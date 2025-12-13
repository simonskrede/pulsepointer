use clap::Parser;
use ctrlc;
use std::{
    collections::VecDeque,
    sync::{
        atomic::{AtomicBool, Ordering},
        mpsc, Arc,
    },
    thread,
    time::{Duration, Instant},
};
use x11::{xfixes, xlib};

mod audio;
mod config;
mod image;
mod visualization;
mod x11_interface;

use crate::config::*;
use crate::image::CursorImage;
use crate::x11_interface::*;
use crate::visualization::apply;

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    #[arg(short, long, value_enum, default_value_t = VisualizationMode::Oscilloscope)]
    mode: VisualizationMode,

    /// Color in hex format (RRGGBB or AARRGGBB). Default is intense bright orange.
    #[arg(short, long, default_value = "CCFF4500")]
    color: String,
}

fn parse_color(s: &str) -> Result<u32, String> {
    let s = s.trim_start_matches('#');
    let val = u32::from_str_radix(s, 16).map_err(|_| "Invalid hex color".to_string())?;
    
    if s.len() == 6 {
        // Assume full opacity (or specifically, we usually want some transparency for cursor overlay, 
        // but if user gave 6 chars, let's just prefix with CC (translucent) or FF (opaque).
        // The existing code used CC (approx 80%). Let's stick to that if 6 chars provided?
        // Or maybe FF. Let's do FF (opaque) if they explicitly asked for a color, 
        // OR CC if we want to maintain the "overlay" feel.
        // The default is 8 chars "CCFF4500".
        // If user says "FF0000", maybe they want solid red.
        // Let's assume FF (solid) for 6 chars, unless we want to force transparency.
        // Actually, let's default to CC (translucent) if 6 chars to keep it usable as a cursor.
        Ok(val | 0xCC000000)
    } else if s.len() == 8 {
        Ok(val)
    } else {
        Err("Color must be 6 or 8 hex digits".to_string())
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();
    let mode = args.mode;
    let color_val = parse_color(&args.color).unwrap_or_else(|e| {
        eprintln!("Warning: {e}. Using default color.");
        0xCCFF4500
    });

    // Initialize Safe X11 Context
    let ctx = X11Context::new()?;

    let (mut base_image, original_cursor) = ctx.get_default_cursor();
            
    // Prepare a dedicated "safe" cursor for restoring the root window on exit.
    // We explicitly ask for "left_ptr" (standard arrow) to avoid restoring a text beam
    // if the app was started while hovering over a terminal.
    let safe_restore_cursor = {
         let img = ctx.load_cursor_image_by_name("left_ptr")
             .unwrap_or_else(crate::image::fallback_cursor_image);
         ctx.create_cursor_from_image(&img)
             .unwrap_or_else(|_| ctx.create_font_cursor(XC_LEFT_PTR))
    };

    let running = Arc::new(AtomicBool::new(true));
    let running_audio = running.clone();
    let running_signal = running.clone();
    ctrlc::set_handler(move || {
        running_signal.store(false, Ordering::SeqCst);
    })?;

    let (tx, rx) = mpsc::channel();
    let audio_handle = thread::spawn(move || {
        if let Err(err) = audio::audio_loop(running_audio, tx) {
            eprintln!("Audio loop error: {err}");
        }
    });

    let mut windows = ctx.collect_windows();
    let mut current_audio_data = AudioData::default();
    let mut last_level_time = Instant::now();
    let mut heartbeat_toggle = false;
    let mut last_cursor_refresh = Instant::now();
    
    let mut dynamic_cursor: Option<xlib::Cursor> = None;
    let mut cursor_history: VecDeque<CursorImage> = VecDeque::new();
    const HISTORY_SIZE: usize = 20;

    println!("Cursor equalizer active (Mode: {:?}, Color: {:08X}). Press Ctrl+C to restore the default cursor.", mode, color_val);

    while running.load(Ordering::SeqCst) {
        // 1. Process X11 Events (Check for external cursor changes)
        while ctx.pending() > 0 {
            let event = ctx.next_event();
            // Accessing union fields is unsafe
            let etype = unsafe { event.type_ };
            
            if etype == xlib::MapNotify || etype == xlib::CreateNotify {
                windows = ctx.collect_windows();
            } else if etype == ctx.event_base + crate::config::XFIXES_CURSOR_NOTIFY {
                let ce = unsafe { *(&event as *const xlib::XEvent as *const xfixes::XFixesCursorNotifyEvent) };
                if ce.subtype == crate::config::XFIXES_DISPLAY_CURSOR_NOTIFY {
                    if let Some(ci) = ctx.fetch_current_cursor() {
                        // If the new cursor is NOT one of our recent frames, it's an external change.
                        if ci != base_image && !cursor_history.contains(&ci) {
                            base_image = ci; 
                            cursor_history.clear(); // Base changed, history is irrelevant
                        }
                    }
                }
            }
        }

        // 2. Polling fallback for cursor changes
        if last_cursor_refresh.elapsed() >= Duration::from_millis(CURSOR_POLL_MS) {
            last_cursor_refresh = Instant::now();
            if let Some(ci) = ctx.fetch_current_cursor() {
                if ci != base_image && !cursor_history.contains(&ci) {
                    base_image = ci;
                    cursor_history.clear();
                }
            }
        }

        // 3. Receive latest Audio Data
        while let Ok(new_data) = rx.try_recv() {
            current_audio_data = new_data;
            last_level_time = Instant::now();
        }

        // 4. Heartbeat logic (modify level if silence)
        if last_level_time.elapsed() > Duration::from_millis(HEARTBEAT_MS) {
            heartbeat_toggle = !heartbeat_toggle;
            current_audio_data.level = if heartbeat_toggle { 0.05 } else { 0.0 };
        }

        // 5. Render & Apply Cursor
        let pixels = apply(&base_image, &current_audio_data, mode, color_val);
        let overlay_image = CursorImage {
            pixels,
            width: base_image.width,
            height: base_image.height,
            xhot: base_image.xhot,
            yhot: base_image.yhot,
        };
        
        if let Ok(new_cursor) = ctx.create_cursor_from_image(&overlay_image) {
                ctx.apply_cursor(new_cursor, &windows);
                if let Some(old_c) = dynamic_cursor.replace(new_cursor) {
                    ctx.free_cursor(old_c);
                }
                
                cursor_history.push_back(overlay_image);
                if cursor_history.len() > HISTORY_SIZE {
                    cursor_history.pop_front();
                }
        }

        thread::sleep(Duration::from_millis(1000 / UPDATE_HZ));
    }

    // Restore defaults
    // Restore the root window's cursor to the standard arrow.
    unsafe {
        xlib::XDefineCursor(ctx.display, ctx.root, safe_restore_cursor);
    }

    // For all other windows that we might have modified,
    // explicitly unset their cursors so they revert to their own definitions.
    for &win in &windows {
        // Only modify non-root windows here.
        if win != ctx.root {
            unsafe {
                xlib::XDefineCursor(ctx.display, win, 0); // Unset cursor for this window
            }
        }
    }
    unsafe { xlib::XFlush(ctx.display); }

    if let Some(c) = dynamic_cursor {
         ctx.free_cursor(c);
    }
    // original_cursor is associated with ctx/main-display and will be freed when ctx drops or we can free it now.
    ctx.free_cursor(original_cursor);
    ctx.free_cursor(safe_restore_cursor);
    
    // ctx is dropped here, closing display and restoring error handler.

    if let Err(err) = audio_handle.join() {
        eprintln!("Audio thread terminated: {err:?}");
    }

    Ok(())
}