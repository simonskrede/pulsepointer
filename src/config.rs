use clap::ValueEnum;

pub const XC_LEFT_PTR: u32 = 68;

pub const UPDATE_HZ: u64 = 75;
pub const AUDIO_CHUNK_FRAMES: usize = 1024; // Must be power of 2 for FFT
pub const SPECTRUM_BUCKETS: usize = 32;

// XFixes constants
pub const XFIXES_CURSOR_NOTIFY: i32 = 1;
pub const XFIXES_DISPLAY_CURSOR_NOTIFY_MASK: u64 = 1;
pub const XFIXES_DISPLAY_CURSOR_NOTIFY: i32 = 0;

pub const CURSOR_POLL_MS: u64 = 250;
pub const LEVEL_LOG_INTERVAL_MS: u64 = 1000;
pub const HEARTBEAT_MS: u64 = 400;

#[derive(Copy, Clone, Debug, ValueEnum, PartialEq)]
pub enum VisualizationMode {
    Bar,
    Circle,
    Spectrum,
    Oscilloscope,
}

#[derive(Clone, Debug)]
pub struct AudioData {
    pub level: f32,                // RMS volume (0.0 - 1.0)
    pub spectrum: Vec<f32>,        // Frequency buckets (normalized 0.0 - 1.0)
    pub waveform: Vec<f32>,        // Raw time-domain samples (normalized -1.0 - 1.0)
}

impl Default for AudioData {
    fn default() -> Self {
        Self {
            level: 0.0,
            spectrum: vec![0.0; SPECTRUM_BUCKETS],
            waveform: Vec::new(),
        }
    }
}