use crate::image::{self, CursorImage};

pub fn draw(base: &CursorImage, spectrum: &[f32], color: u32) -> Vec<u32> {
    let mut pixels = base.pixels.clone();
    
    if spectrum.is_empty() {
        return pixels;
    }

    let width = base.width as usize;
    let height = base.height as usize;
    
    let bar_width = (width / spectrum.len()).max(1);
    
    for (i, val) in spectrum.iter().enumerate() {
        let h = (val * height as f32) as usize;
        let x0 = (i * bar_width) as i32;
        let y0 = height as i32;
        let x1 = x0;
        let y1 = (height.saturating_sub(h)) as i32;
        
        // Draw vertical line for each bucket
        // For a nicer look, we could draw a filled rect, but lines are simpler for now
         image::draw_line(&mut pixels, width, height, x0, y0, x1, y1, color);
    }
    
    pixels
}