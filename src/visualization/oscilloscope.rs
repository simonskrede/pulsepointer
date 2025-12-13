use crate::image::{self, CursorImage};

pub fn draw(base: &CursorImage, waveform: &[f32], color: u32) -> Vec<u32> {
    let mut pixels = base.pixels.clone();
    
    if waveform.is_empty() {
        return pixels;
    }

    let width = base.width as usize;
    let height = base.height as usize;
    let mid_y = (height / 2) as i32;
    
    // Draw waveform
    // We'll just take a slice of the waveform to fit the width
    let samples_per_pixel = (waveform.len() as f32 / width as f32).max(1.0);
    
    for x in 0..(width - 1) {
        let idx = (x as f32 * samples_per_pixel) as usize;
        let next_idx = ((x + 1) as f32 * samples_per_pixel) as usize;
        
        if idx >= waveform.len() || next_idx >= waveform.len() {
            break;
        }

        let y1 = mid_y + (waveform[idx] * (height as f32 / 2.0) * 2.0) as i32;
        let y2 = mid_y + (waveform[next_idx] * (height as f32 / 2.0) * 2.0) as i32;

        image::draw_line(
            &mut pixels, 
            width, 
            height, 
            x as i32, 
            y1, 
            (x + 1) as i32, 
            y2, 
            color
        );
    }
    
    pixels
}
