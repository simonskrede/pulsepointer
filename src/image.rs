#[derive(Clone, Debug)]
pub struct CursorImage {
    pub pixels: Vec<u32>, // ARGB format
    pub width: u32,
    pub height: u32,
    pub xhot: u32,
    pub yhot: u32,
}

impl PartialEq for CursorImage {
    fn eq(&self, other: &Self) -> bool {
        // We care about visual identity.
        self.width == other.width
            && self.height == other.height
            && self.xhot == other.xhot
            && self.yhot == other.yhot
            && self.pixels == other.pixels
    }
}

pub fn blend_pixel(dst: u32, src: u32) -> u32 {
    let sa = ((src >> 24) & 0xff) as u32;
    if sa == 0 {
        return dst;
    }
    let sr = (src >> 16) & 0xff;
    let sg = (src >> 8) & 0xff;
    let sb = src & 0xff;

    let da = ((dst >> 24) & 0xff) as u32;
    let dr = (dst >> 16) & 0xff;
    let dg = (dst >> 8) & 0xff;
    let db = dst & 0xff;

    let out_a = sa + da * (255 - sa) / 255;
    let out_r = (sr * sa + dr * (255 - sa)) / 255;
    let out_g = (sg * sa + dg * (255 - sa)) / 255;
    let out_b = (sb * sa + db * (255 - sa)) / 255;

    (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b
}

pub fn draw_line(pixels: &mut [u32], width: usize, height: usize, x0: i32, y0: i32, x1: i32, y1: i32, color: u32) {
    let dx = (x1 - x0).abs();
    let dy = -(y1 - y0).abs();
    let sx = if x0 < x1 { 1 } else { -1 };
    let sy = if y0 < y1 { 1 } else { -1 };
    let mut err = dx + dy;

    let mut x = x0;
    let mut y = y0;

    loop {
        if x >= 0 && x < width as i32 && y >= 0 && y < height as i32 {
            let idx = (y as usize) * width + (x as usize);
            if idx < pixels.len() {
                pixels[idx] = blend_pixel(pixels[idx], color);
            }
        }
        if x == x1 && y == y1 {
            break;
        }
        let e2 = 2 * err;
        if e2 >= dy {
            err += dy;
            x += sx;
        }
        if e2 <= dx {
            err += dx;
            y += sy;
        }
    }
}

pub fn fallback_cursor_image() -> CursorImage {
    let width = 32;
    let height = 32;
    let mut pixels = vec![0u32; (width * height) as usize];
    // Tiny white dot so the cursor stays visible if fetch fails.
    if width > 1 && height > 1 {
        pixels[1 * width as usize + 1] = 0xFFFFFFFF;
    }
    CursorImage {
        pixels,
        width,
        height,
        xhot: 1,
        yhot: 1,
    }
}