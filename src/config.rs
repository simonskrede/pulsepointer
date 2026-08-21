pub const AUDIO_CHUNK_FRAMES: usize = 1024; // Must be power of 2 for FFT
pub const SPECTRUM_BUCKETS: usize = 32;
pub const WAVEFORM_SAMPLES: usize = 128;
pub const LEVEL_LOG_INTERVAL_MS: u64 = 1000;
pub const CONTROL_POLL_INTERVAL_MS: u64 = 100;

#[derive(Debug)]
pub struct AudioData {
    pub level: f32,
    pub bass: f32,
    pub midrange: f32,
    pub treble: f32,
    pub onset: f32,
    pub spectrum: [f32; SPECTRUM_BUCKETS],
    pub waveform: [f32; WAVEFORM_SAMPLES],
}
