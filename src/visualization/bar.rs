use crate::image::{blend_pixel, CursorImage};

pub fn draw(base: &CursorImage, level: f32, color: u32) -> Vec<u32> {
    let mut pixels = base.pixels.clone();
    
    if level < 0.01 {
        return pixels;
    }

    let width = base.width as usize;
    let height = base.height as usize;
    
    // Draw a vertical bar on the right side
    let bar_width = 4;
    let max_bar_height = height - 4;
    let bar_height = (max_bar_height as f32 * level).min(max_bar_height as f32) as usize;
    
    let start_x = width.saturating_sub(bar_width + 2);
    let start_y = height.saturating_sub(2);

    for y in 0..bar_height {
        for x in 0..bar_width {
            let py = start_y.saturating_sub(y);
            let px = start_x + x;
            
            let idx = py * width + px;
            if idx < pixels.len() {
                pixels[idx] = blend_pixel(pixels[idx], color);
            }
        }
    }
    
    pixels
}
