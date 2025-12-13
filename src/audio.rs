use std::{
    env,
    sync::{
        atomic::{AtomicBool, Ordering},
        mpsc, Arc,
    },
    time::{Duration, Instant},
};

use bytemuck::cast_slice;
use libpulse_binding::{
    def::BufferAttr,
    sample::{Format, Spec},
    stream::Direction,
};
use libpulse_simple_binding as psimple;
use rustfft::{num_complex::Complex, FftPlanner};

use crate::config::{AudioData, AUDIO_CHUNK_FRAMES, LEVEL_LOG_INTERVAL_MS, SPECTRUM_BUCKETS};

pub fn audio_loop(running: Arc<AtomicBool>, tx: mpsc::Sender<AudioData>) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let spec = Spec {
        format: Format::S16NE,
        channels: 2,
        rate: 48_000,
    };
    if !spec.is_valid() {
        return Err("Audio spec is not valid for PulseAudio".into());
    }

    let source = env::var("CURSOR_EQ_SOURCE").ok();
    let bytes_per_frame = (spec.channels as usize) * std::mem::size_of::<i16>();
    let chunk_bytes = AUDIO_CHUNK_FRAMES * bytes_per_frame;
    let attr = BufferAttr {
        maxlength: u32::MAX,
        tlength: u32::MAX,
        prebuf: u32::MAX,
        minreq: u32::MAX,
        fragsize: chunk_bytes as u32,
    };

    let simple = psimple::Simple::new(
        None,
        "cursor_equalizer",
        Direction::Record,
        source.as_deref(),
        "cursor-eq-monitor",
        &spec,
        None,
        Some(&attr),
    )?;

    let mut buffer = vec![0u8; chunk_bytes];
    let mut smoothed_level = 0.0f32;
    let mut smoothed_spectrum = vec![0.0f32; SPECTRUM_BUCKETS];
    let mut last_log = Instant::now();
    let log_levels = env::var("CURSOR_EQ_LOG").is_ok();

    // FFT Setup
    let mut planner = FftPlanner::new();
    let fft = planner.plan_fft_forward(AUDIO_CHUNK_FRAMES);
    let mut fft_buffer = vec![Complex { re: 0.0, im: 0.0 }; AUDIO_CHUNK_FRAMES];
    let window: Vec<f32> = (0..AUDIO_CHUNK_FRAMES)
        .map(|i| 0.5 * (1.0 - (2.0 * std::f32::consts::PI * i as f32 / (AUDIO_CHUNK_FRAMES - 1) as f32).cos()))
        .collect();

    while running.load(Ordering::SeqCst) {
        simple.read(&mut buffer)?;
        let samples: &[i16] = cast_slice(&buffer);
        if samples.is_empty() {
            continue;
        }

        // --- Waveform Capture (Mono) ---
        let mut waveform = Vec::with_capacity(AUDIO_CHUNK_FRAMES);
        // We'll capture mono samples for both FFT and Oscilloscope
        for chunk in samples.chunks_exact(2) {
            let left = chunk[0] as f32 / i16::MAX as f32;
            let right = chunk[1] as f32 / i16::MAX as f32;
            waveform.push((left + right) / 2.0);
        }

        // --- Triggering / Stabilization ---
        // Simple zero-crossing trigger: Find first positive zero-crossing
        let mut trigger_offset = 0;
        for i in 0..waveform.len().saturating_sub(1) {
            if waveform[i] <= 0.0 && waveform[i+1] > 0.0 {
                trigger_offset = i;
                break;
            }
        }
        
        // --- Level Calculation (RMS) ---
        let mut sum_sq = 0.0f32;
        let mut peak = 0.0f32;
        for &mono in &waveform {
            sum_sq += mono * mono;
            peak = peak.max(mono.abs());
        }

        let rms = (sum_sq / waveform.len() as f32).sqrt();
        let current_level = (rms * 3.0).min(peak * 1.2).min(1.0);
        smoothed_level = smoothed_level * 0.7 + current_level * 0.3;

        // --- FFT Calculation ---
        for (i, &mono) in waveform.iter().enumerate() {
            if i >= AUDIO_CHUNK_FRAMES { break; }
            fft_buffer[i] = Complex {
                re: mono * window[i],
                im: 0.0,
            };
        }
        
        fft.process(&mut fft_buffer);

        // Map FFT bins to SPECTRUM_BUCKETS using a logarithmic scale.
        // Limit to ~16kHz (approx bin 342) to avoid wasting buckets on inaudible/empty ultrasonics.
        // 48kHz sample rate / 1024 frames = 46.875 Hz per bin.
        // 16000 / 46.875 ~= 341.
        let max_bin = 342.min(AUDIO_CHUNK_FRAMES / 2);
        let log_max = (max_bin as f32).ln();
        
        for i in 0..SPECTRUM_BUCKETS {
            // Calculate bin range for this bucket
            let freq_start = (i as f32 / SPECTRUM_BUCKETS as f32 * log_max).exp();
            let freq_end = ((i + 1) as f32 / SPECTRUM_BUCKETS as f32 * log_max).exp();
            
            let start_bin = freq_start.floor() as usize;
            let end_bin = freq_end.ceil() as usize;
            let start_bin = start_bin.clamp(1, max_bin - 1); // Skip DC
            let end_bin = end_bin.clamp(start_bin + 1, max_bin);

            let mut bucket_mag = 0.0f32;
            for idx in start_bin..end_bin {
                if idx < fft_buffer.len() {
                    let norm = fft_buffer[idx].norm();
                    // Use MAX instead of Average. 
                    // High freq buckets span many bins; spectral peaks are sparse. Average dilutes them.
                    bucket_mag = bucket_mag.max(norm);
                }
            }
            
            // Whitening / Pre-emphasis: Strong linear boost for highs.
            // i=0 -> 1.0x
            // i=31 -> 16.5x
            let freq_boost = (i as f32 * 0.5) + 1.0;
            
            // Normalize
            let sensitivity = 8.0; 
            let val = (bucket_mag / (AUDIO_CHUNK_FRAMES as f32) * sensitivity * freq_boost).clamp(0.0, 1.0);
            
            // Fast attack, faster decay for dynamic look
            if val > smoothed_spectrum[i] {
                 smoothed_spectrum[i] = smoothed_spectrum[i] * 0.6 + val * 0.4;
            } else {
                 smoothed_spectrum[i] = smoothed_spectrum[i] * 0.75 + val * 0.25;
            }
        }
        
        // Prepare waveform for display (apply trigger offset)
        // If we found a trigger, rotate/slice so it starts there.
        // Or simpler: just send the raw buffer, visualization can handle cropping.
        // But for stability, let's rotate it here or send a "stable" slice.
        // Let's just rotate the vector so index 0 is the trigger point.
        if trigger_offset > 0 {
             waveform.rotate_left(trigger_offset);
        }


        let _ = tx.send(AudioData {
            level: smoothed_level,
            spectrum: smoothed_spectrum.clone(),
            waveform,
        });

        if log_levels && last_log.elapsed() >= Duration::from_millis(LEVEL_LOG_INTERVAL_MS) {
            last_log = Instant::now();
            eprintln!("cursor_eq level: {:.3}", smoothed_level);
        }
    }

    Ok(())
}
