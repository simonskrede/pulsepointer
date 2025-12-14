use crate::image::{self, CursorImage};
use std::f32::consts::PI;

pub fn draw(base: &CursorImage, waveform: &[f32], color: u32) -> Vec<u32> {
    let mut pixels = base.pixels.clone();
    
    if waveform.is_empty() {
        return pixels;
    }

    let width = base.width as usize;
    let height = base.height as usize;
    let cx = width as f32 / 2.0;
    let cy = height as f32 / 2.0;
    let radius = (width.min(height) as f32 / 2.0) - 5.0;

    // Radius follows the waveform wrapped around the circle.
    
    let num_points = 64; 
    let step = PI * 2.0 / num_points as f32;

    for i in 0..num_points {
        let angle = i as f32 * step;
        let next_angle = (i + 1) as f32 * step;
        
        let sample_idx = (i * waveform.len() / num_points) % waveform.len();
        let sample = waveform[sample_idx].abs(); // use magnitude
        
        let r = radius + (sample * 40.0);
        
        let x0 = (cx + r * angle.cos()) as i32;
        let y0 = (cy + r * angle.sin()) as i32;
        
        let x1 = (cx + r * next_angle.cos()) as i32;
        let y1 = (cy + r * next_angle.sin()) as i32;
        
        image::draw_line(&mut pixels, width, height, x0, y0, x1, y1, color);
    }
    
    pixels
}