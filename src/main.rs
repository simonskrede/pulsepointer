extern crate x11;
extern crate x11_dl;

use x11::xlib;
use std::ptr;
use std::thread;
use std::time::Duration;
use std::ffi::CStr;

extern "C" fn error_handler(display: *mut xlib::Display, event: *mut xlib::XErrorEvent) -> i32 {
    unsafe {
        let mut error_text = [0i8; 1024];
        xlib::XGetErrorText(display, (*event).error_code as i32, error_text.as_mut_ptr(), error_text.len() as i32);
        let error_description = CStr::from_ptr(error_text.as_ptr()).to_string_lossy();

        println!("X Error: type={}, error_code={}, request_code={}, minor_code={}, description={}",
                 (*event).type_, (*event).error_code, (*event).request_code, (*event).minor_code, error_description);
    }
    0
}

fn set_cursor_for_all_windows(display: *mut xlib::Display, window: xlib::Window, cursor: xlib::Cursor) {
    let mut root_return = 0;
    let mut parent_return = 0;
    let mut children_return: *mut xlib::Window = ptr::null_mut();
    let mut nchildren_return = 0;

    unsafe {
        // Query the tree for all windows starting from the root or the current window
        if xlib::XQueryTree(display, window, &mut root_return, &mut parent_return, &mut children_return, &mut nchildren_return) != 0 {
            let children_slice = std::slice::from_raw_parts(children_return, nchildren_return as usize);

            // Set the cursor for each window, including children windows recursively
            for &child in children_slice {
                xlib::XDefineCursor(display, child, cursor);
                set_cursor_for_all_windows(display, child, cursor); // Recurse into sub-windows
            }

            if !children_return.is_null() {
                xlib::XFree(children_return as *mut _);
            }
        }
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    unsafe {
        let old_handler = xlib::XSetErrorHandler(Some(error_handler));

        let display = xlib::XOpenDisplay(ptr::null());
        if display.is_null() {
            return Err("Failed to open display".into());
        }

        let screen = xlib::XDefaultScreen(display);
        let root = xlib::XRootWindow(display, screen);

        // Array of cursor shapes to cycle through
        let cursor_shapes = [
            34,  // XC_crosshair
            58,  // XC_left_ptr
            24,  // XC_arrow
            52,  // XC_hand1
            150, // XC_watch
        ];

        println!("Cycling through different cursor shapes. Press Ctrl+C to exit.");

        for (i, &shape) in cursor_shapes.iter().enumerate() {
            let custom_cursor = xlib::XCreateFontCursor(display, shape);
            if custom_cursor == 0 {
                println!("Failed to create cursor with shape {}", shape);
                continue;
            }

            println!("Attempt {} to set cursor shape {}...", i + 1, shape);

            // Set the cursor for the root window (the desktop background)
            let result = xlib::XDefineCursor(display, root, custom_cursor);
            if result != 0 {
                println!("Failed to define cursor for shape {} on the root window.", shape);
            }

            // Also set the cursor for all child windows recursively
            set_cursor_for_all_windows(display, root, custom_cursor);

            // Flush to make sure the change is applied
            xlib::XFlush(display);

            println!("Cursor should now be changed to shape {}.", shape);

            println!("Sleeping for 5 seconds. Please check if the cursor has changed.");
            thread::sleep(Duration::from_secs(5));

            // Free the cursor after use
            xlib::XFreeCursor(display, custom_cursor);
        }

        // Restore the default cursor
        println!("Restoring default cursor...");
        let undefine_result = xlib::XUndefineCursor(display, root);
        println!("XUndefineCursor result: {}", undefine_result);

        // Flush to apply the restoration of the default cursor
        xlib::XFlush(display);

        // Clean up
        xlib::XCloseDisplay(display);
        xlib::XSetErrorHandler(old_handler);
    }

    Ok(())
}
