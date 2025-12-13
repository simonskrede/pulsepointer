pub mod bar;
pub mod circle;
pub mod spectrum;
pub mod oscilloscope;

use crate::config::{AudioData, VisualizationMode};
use crate::image::CursorImage;

pub fn apply(base: &CursorImage, data: &AudioData, mode: VisualizationMode, color: u32) -> Vec<u32> {
    match mode {
        VisualizationMode::Bar => bar::draw(base, data.level, color),
        VisualizationMode::Circle => circle::draw(base, &data.waveform, color), // Use waveform for Circle
        VisualizationMode::Spectrum => spectrum::draw(base, &data.spectrum, color),
        VisualizationMode::Oscilloscope => oscilloscope::draw(base, &data.waveform, color),
    }
}