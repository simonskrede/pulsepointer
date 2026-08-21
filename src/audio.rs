use std::{
    array, env,
    mem::size_of,
    sync::{
        atomic::{AtomicBool, Ordering},
        mpsc::{SyncSender, TrySendError},
        Arc,
    },
    time::{Duration, Instant},
};

use libpulse_binding::{
    def::BufferAttr,
    sample::{Format, Spec},
    stream::Direction,
};
use libpulse_simple_binding as pulse;
use rustfft::{num_complex::Complex, Fft, FftPlanner};

use crate::config::{
    AudioData, AUDIO_CHUNK_FRAMES, LEVEL_LOG_INTERVAL_MS, SPECTRUM_BUCKETS, WAVEFORM_SAMPLES,
};

const CHANNELS: usize = 2;
const SAMPLE_RATE: u32 = 48_000;
const MAX_SPECTRUM_FREQUENCY: f32 = 16_000.0;

type AudioResult<T> = Result<T, Box<dyn std::error::Error + Send + Sync>>;

struct SpectrumBucket {
    start_bin: usize,
    end_bin: usize,
    boost: f32,
    center_hz: f32,
}

struct AudioProcessor {
    fft: Arc<dyn Fft<f32>>,
    fft_buffer: Vec<Complex<f32>>,
    window: Vec<f32>,
    buckets: [SpectrumBucket; SPECTRUM_BUCKETS],
    previous_spectrum: [f32; SPECTRUM_BUCKETS],
    average_flux: f32,
    have_previous_spectrum: bool,
    smoothed_level: f32,
    smoothed_spectrum: [f32; SPECTRUM_BUCKETS],
}

impl AudioProcessor {
    fn new() -> Self {
        let mut planner = FftPlanner::new();
        let fft = planner.plan_fft_forward(AUDIO_CHUNK_FRAMES);
        let fft_buffer = vec![Complex::default(); AUDIO_CHUNK_FRAMES];
        let window = (0..AUDIO_CHUNK_FRAMES)
            .map(|index| {
                let phase =
                    2.0 * std::f32::consts::PI * index as f32 / (AUDIO_CHUNK_FRAMES - 1) as f32;
                0.5 * (1.0 - phase.cos())
            })
            .collect();

        let nyquist_bin = AUDIO_CHUNK_FRAMES / 2;
        let frequency_per_bin = SAMPLE_RATE as f32 / AUDIO_CHUNK_FRAMES as f32;
        let max_bin = (MAX_SPECTRUM_FREQUENCY / frequency_per_bin) as usize;
        let max_bin = max_bin.min(nyquist_bin);
        let log_max = (max_bin as f32).ln();
        let buckets = array::from_fn(|index| {
            let start = (index as f32 / SPECTRUM_BUCKETS as f32 * log_max)
                .exp()
                .floor() as usize;
            let end = ((index + 1) as f32 / SPECTRUM_BUCKETS as f32 * log_max)
                .exp()
                .ceil() as usize;
            let start = start.clamp(1, max_bin - 1);
            let end = end.clamp(start + 1, max_bin);
            SpectrumBucket {
                start_bin: start,
                end_bin: end,
                boost: index as f32 * 0.5 + 1.0,
                center_hz: ((start * end) as f32).sqrt() * frequency_per_bin,
            }
        });

        Self {
            fft,
            fft_buffer,
            window,
            buckets,
            previous_spectrum: [0.0; SPECTRUM_BUCKETS],
            average_flux: 0.0,
            have_previous_spectrum: false,
            smoothed_level: 0.0,
            smoothed_spectrum: [0.0; SPECTRUM_BUCKETS],
        }
    }

    fn process(&mut self, bytes: &[u8]) -> AudioData {
        debug_assert_eq!(
            bytes.len(),
            AUDIO_CHUNK_FRAMES * CHANNELS * size_of::<i16>()
        );

        let mut waveform = [0.0; AUDIO_CHUNK_FRAMES];
        for (sample, frame) in waveform.iter_mut().zip(bytes.chunks_exact(4)) {
            let left = i16::from_ne_bytes([frame[0], frame[1]]) as f32 / 32_768.0;
            let right = i16::from_ne_bytes([frame[2], frame[3]]) as f32 / 32_768.0;
            *sample = (left + right) * 0.5;
        }

        let (sum_of_squares, peak) = waveform.iter().fold((0.0_f32, 0.0_f32), |acc, sample| {
            (acc.0 + sample * sample, acc.1.max(sample.abs()))
        });
        let rms = (sum_of_squares / AUDIO_CHUNK_FRAMES as f32).sqrt();
        let current_level = (rms * 3.0).min(peak * 1.2).min(1.0);
        self.smoothed_level = self.smoothed_level * 0.7 + current_level * 0.3;

        for ((fft_sample, sample), window) in
            self.fft_buffer.iter_mut().zip(&waveform).zip(&self.window)
        {
            *fft_sample = Complex::new(sample * window, 0.0);
        }
        self.fft.process(&mut self.fft_buffer);

        let mut current_spectrum = [0.0; SPECTRUM_BUCKETS];
        for ((current, smoothed), bucket) in current_spectrum
            .iter_mut()
            .zip(&mut self.smoothed_spectrum)
            .zip(&self.buckets)
        {
            let peak_squared = self.fft_buffer[bucket.start_bin..bucket.end_bin]
                .iter()
                .map(Complex::norm_sqr)
                .fold(0.0_f32, f32::max);
            let sensitivity = 8.0;
            let value =
                (peak_squared.sqrt() / AUDIO_CHUNK_FRAMES as f32 * sensitivity * bucket.boost)
                    .clamp(0.0, 1.0);
            *current = value;

            let smoothing = if value > *smoothed { 0.6 } else { 0.75 };
            *smoothed = *smoothed * smoothing + value * (1.0 - smoothing);
        }

        let spectral_flux = current_spectrum
            .iter()
            .zip(&self.previous_spectrum)
            .map(|(current, previous)| (current - previous).max(0.0))
            .sum::<f32>()
            / SPECTRUM_BUCKETS as f32;
        let onset = if self.have_previous_spectrum {
            let threshold = self.average_flux * 1.5 + 0.002;
            ((spectral_flux - threshold) * 8.0).clamp(0.0, 1.0)
        } else {
            0.0
        };
        self.average_flux = self.average_flux * 0.92 + spectral_flux * 0.08;
        self.previous_spectrum = current_spectrum;
        self.have_previous_spectrum = true;

        let bass = band_level(&self.smoothed_spectrum, &self.buckets, 0.0, 250.0);
        let midrange = band_level(&self.smoothed_spectrum, &self.buckets, 250.0, 2_000.0);
        let treble = band_level(
            &self.smoothed_spectrum,
            &self.buckets,
            2_000.0,
            MAX_SPECTRUM_FREQUENCY,
        );

        if let Some(trigger) = waveform
            .windows(2)
            .position(|pair| pair[0] <= 0.0 && pair[1] > 0.0)
        {
            waveform.rotate_left(trigger);
        }

        AudioData {
            level: self.smoothed_level,
            bass,
            midrange,
            treble,
            onset,
            spectrum: self.smoothed_spectrum,
            waveform: decimate_waveform(&waveform),
        }
    }
}

fn decimate_waveform(waveform: &[f32; AUDIO_CHUNK_FRAMES]) -> [f32; WAVEFORM_SAMPLES] {
    let samples_per_output = AUDIO_CHUNK_FRAMES / WAVEFORM_SAMPLES;
    array::from_fn(|output_index| {
        let start = output_index * samples_per_output;
        waveform[start..start + samples_per_output]
            .iter()
            .sum::<f32>()
            / samples_per_output as f32
    })
}

fn band_level(
    spectrum: &[f32; SPECTRUM_BUCKETS],
    buckets: &[SpectrumBucket; SPECTRUM_BUCKETS],
    minimum_hz: f32,
    maximum_hz: f32,
) -> f32 {
    let (sum, count) = spectrum
        .iter()
        .zip(buckets)
        .filter(|(_, bucket)| bucket.center_hz >= minimum_hz && bucket.center_hz < maximum_hz)
        .fold((0.0, 0_u32), |(sum, count), (value, _)| {
            (sum + value, count + 1)
        });
    if count == 0 {
        0.0
    } else {
        (sum / count as f32).clamp(0.0, 1.0)
    }
}

pub fn audio_loop(
    running: Arc<AtomicBool>,
    tx: SyncSender<AudioData>,
    requested_source: Option<String>,
    log_levels_requested: bool,
) -> AudioResult<()> {
    let spec = Spec {
        format: Format::S16NE,
        channels: CHANNELS as u8,
        rate: SAMPLE_RATE,
    };
    if !spec.is_valid() {
        return Err("invalid PulseAudio sample specification".into());
    }

    let source = requested_source
        .or_else(|| env::var("PULSEPOINTER_SOURCE").ok())
        .unwrap_or_else(|| "@DEFAULT_MONITOR@".to_string());
    let chunk_bytes = AUDIO_CHUNK_FRAMES * CHANNELS * size_of::<i16>();
    let attributes = BufferAttr {
        maxlength: u32::MAX,
        tlength: u32::MAX,
        prebuf: u32::MAX,
        minreq: u32::MAX,
        fragsize: chunk_bytes as u32,
    };
    let recorder = pulse::Simple::new(
        None,
        "pulsepointer",
        Direction::Record,
        Some(&source),
        "system-audio-monitor",
        &spec,
        None,
        Some(&attributes),
    )?;

    let mut processor = AudioProcessor::new();
    let mut buffer = vec![0; chunk_bytes];
    let mut last_log = Instant::now();
    let log_levels = log_levels_requested || env::var_os("PULSEPOINTER_LOG").is_some();
    let mut pending_onset = 0.0_f32;

    while running.load(Ordering::Relaxed) {
        recorder.read(&mut buffer)?;
        let mut data = processor.process(&buffer);
        data.onset = data.onset.max(pending_onset);
        let level = data.level;
        match tx.try_send(data) {
            Ok(()) => pending_onset = 0.0,
            Err(TrySendError::Full(data)) => pending_onset = data.onset,
            Err(TrySendError::Disconnected(_)) => return Ok(()),
        }

        if log_levels && last_log.elapsed() >= Duration::from_millis(LEVEL_LOG_INTERVAL_MS) {
            last_log = Instant::now();
            eprintln!("PulsePointer audio level: {level:.3}");
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn processor_decodes_stereo_without_allocating_sample_vectors() {
        let mut bytes = Vec::with_capacity(AUDIO_CHUNK_FRAMES * 4);
        for index in 0..AUDIO_CHUNK_FRAMES {
            let sample = if index % 2 == 0 { i16::MAX } else { 0 };
            bytes.extend_from_slice(&sample.to_ne_bytes());
            bytes.extend_from_slice(&sample.to_ne_bytes());
        }

        let data = AudioProcessor::new().process(&bytes);

        assert!(data.level > 0.0);
        assert_eq!(data.waveform.len(), WAVEFORM_SAMPLES);
        assert_eq!(data.spectrum.len(), SPECTRUM_BUCKETS);
        assert!(data.bass.is_finite());
        assert!(data.midrange.is_finite());
        assert!(data.treble.is_finite());
    }

    #[test]
    fn processor_detects_a_sudden_spectral_onset() {
        let mut processor = AudioProcessor::new();
        let silence = vec![0; AUDIO_CHUNK_FRAMES * CHANNELS * size_of::<i16>()];
        processor.process(&silence);

        let mut tone = Vec::with_capacity(silence.len());
        for index in 0..AUDIO_CHUNK_FRAMES {
            let phase = 2.0 * std::f32::consts::PI * 110.0 * index as f32 / SAMPLE_RATE as f32;
            let sample = (phase.sin() * i16::MAX as f32 * 0.8) as i16;
            tone.extend_from_slice(&sample.to_ne_bytes());
            tone.extend_from_slice(&sample.to_ne_bytes());
        }

        let data = processor.process(&tone);

        assert!(data.onset > 0.1);
        assert!(data.bass > data.treble);
    }

    #[test]
    fn waveform_decimation_averages_each_input_region() {
        let waveform = array::from_fn(|index| index as f32);

        let decimated = decimate_waveform(&waveform);

        assert_eq!(decimated[0], 3.5);
        assert_eq!(decimated[WAVEFORM_SAMPLES - 1], 1019.5);
    }
}
