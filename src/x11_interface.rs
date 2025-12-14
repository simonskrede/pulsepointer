use std::{
    collections::HashSet,
    ffi::{CStr, CString},
    os::raw::c_int,
    ptr,
};

use libc::c_ulong;
use x11::{xcursor, xfixes, xlib};

use crate::config::{XC_LEFT_PTR, XFIXES_DISPLAY_CURSOR_NOTIFY_MASK};
use crate::image::{self, CursorImage};

extern "C" {
    fn XcursorLibraryLoadImage(name: *const i8, theme: *const i8, size: i32) -> *mut xcursor::XcursorImage;
    fn XcursorGetDefaultSize(dpy: *mut xlib::Display) -> c_int;
    fn XcursorGetTheme(dpy: *mut xlib::Display) -> *const i8;
    pub fn XFixesSelectCursorInput(dpy: *mut xlib::Display, win: xlib::Window, event_mask: c_ulong);
}

extern "C" fn error_handler(display: *mut xlib::Display, event: *mut xlib::XErrorEvent) -> c_int {
    unsafe {
        let mut error_text = [0i8; 1024];
        xlib::XGetErrorText(
            display,
            (*event).error_code as c_int,
            error_text.as_mut_ptr(),
            error_text.len() as c_int,
        );
        let error_description = CStr::from_ptr(error_text.as_ptr()).to_string_lossy();
        eprintln!(
            "X Error: type={}, error_code={}, request_code={}, minor_code={}, description={}",
            (*event).type_, (*event).error_code, (*event).request_code, (*event).minor_code, error_description
        );
    }
    0
}

pub struct X11Context {
    pub display: *mut xlib::Display,
    pub root: xlib::Window,
    pub event_base: i32,
    old_handler: Option<unsafe extern "C" fn(*mut xlib::Display, *mut xlib::XErrorEvent) -> c_int>,
}

// Allow sharing the context, even though this loop stays single-threaded.
unsafe impl Send for X11Context {}

impl X11Context {
    pub fn new() -> Result<Self, String> {
        unsafe {
            let old_handler = xlib::XSetErrorHandler(Some(error_handler));

            let display = xlib::XOpenDisplay(ptr::null());
            if display.is_null() {
                // Restore handler if we failed
                xlib::XSetErrorHandler(old_handler);
                return Err("Failed to open display".into());
            }

            let screen = xlib::XDefaultScreen(display);
            let root = xlib::XRootWindow(display, screen);

            let mut event_base = 0;
            let mut error_base = 0;
            if xfixes::XFixesQueryExtension(display, &mut event_base, &mut error_base) == 0 {
                eprintln!("XFixes extension not available; cursor shape changes will not be tracked.");
            } else {
                XFixesSelectCursorInput(display, root, XFIXES_DISPLAY_CURSOR_NOTIFY_MASK as c_ulong);
            }

            // Track new windows so the cursor stays consistent as apps appear.
            xlib::XSelectInput(display, root, xlib::SubstructureNotifyMask | xlib::StructureNotifyMask);
            xlib::XFlush(display);

            Ok(Self {
                display,
                root,
                event_base,
                old_handler,
            })
        }
    }

    pub fn fetch_current_cursor(&self) -> Option<CursorImage> {
        fetch_current_cursor(self.display)
    }

    pub fn load_cursor_image_by_name(&self, name: &str) -> Option<CursorImage> {
        load_cursor_image_by_name(self.display, name)
    }

    pub fn create_cursor_from_image(&self, image: &CursorImage) -> Result<xlib::Cursor, String> {
        create_cursor_from_image(self.display, image)
    }

    pub fn create_font_cursor(&self, shape: u32) -> xlib::Cursor {
        unsafe { xlib::XCreateFontCursor(self.display, shape) }
    }

    pub fn free_cursor(&self, cursor: xlib::Cursor) {
        unsafe { xlib::XFreeCursor(self.display, cursor); }
    }

    pub fn collect_windows(&self) -> HashSet<xlib::Window> {
        collect_windows(self.display, self.root)
    }

    pub fn apply_cursor(&self, cursor: xlib::Cursor, windows: &HashSet<xlib::Window>) {
        apply_cursor_to_windows(self.display, self.root, cursor, windows)
    }

    pub fn pending(&self) -> i32 {
        unsafe { xlib::XPending(self.display) }
    }

    pub fn next_event(&self) -> xlib::XEvent {
        unsafe {
            let mut event: xlib::XEvent = std::mem::zeroed();
            xlib::XNextEvent(self.display, &mut event);
            event
        }
    }
    
    pub fn get_default_cursor(&self) -> (CursorImage, xlib::Cursor) {
         let base_image = self.fetch_current_cursor()
            .or_else(|| self.load_cursor_image_by_name("left_ptr"))
            .unwrap_or_else(image::fallback_cursor_image);
         let cursor = self.create_cursor_from_image(&base_image)
            .unwrap_or_else(|_| self.create_font_cursor(XC_LEFT_PTR));
         (base_image, cursor)
    }
}

impl Drop for X11Context {
    fn drop(&mut self) {
        unsafe {
            xlib::XCloseDisplay(self.display);
            xlib::XSetErrorHandler(self.old_handler);
        }
    }
}


// --- Internal Helpers (kept for implementation details) ---

fn fetch_current_cursor(display: *mut xlib::Display) -> Option<CursorImage> {
    unsafe {
        let img = xfixes::XFixesGetCursorImage(display);
        if img.is_null() {
            return None;
        }
        let width = (*img).width as u32;
        let height = (*img).height as u32;
        let xhot = (*img).xhot as u32;
        let yhot = (*img).yhot as u32;
        // serial was removed from CursorImage
        let len = width.saturating_mul(height) as usize;
        let src = std::slice::from_raw_parts((*img).pixels, len);
        let pixels = src.iter().map(|p| *p as u32).collect();
        xlib::XFree(img as *mut _);
        Some(CursorImage {
            pixels,
            width,
            height,
            xhot,
            yhot,
        })
    }
}

fn create_cursor_from_image(display: *mut xlib::Display, image: &CursorImage) -> Result<xlib::Cursor, String> {
    unsafe {
        let ximg = xcursor::XcursorImageCreate(image.width as i32, image.height as i32);
        if ximg.is_null() {
            return Err("Failed to allocate XcursorImage".into());
        }
        (*ximg).xhot = image.xhot;
        (*ximg).yhot = image.yhot;
        (*ximg).delay = 0;

        let len = image.width.saturating_mul(image.height) as usize;
        let dst = std::slice::from_raw_parts_mut((*ximg).pixels, len);
        dst.copy_from_slice(&image.pixels[..len.min(image.pixels.len())]);

        let cursor = xcursor::XcursorImageLoadCursor(display, ximg);
        xcursor::XcursorImageDestroy(ximg);

        if cursor == 0 {
            Err("Failed to create ARGB cursor".into())
        } else {
            Ok(cursor)
        }
    }
}

fn load_cursor_image_by_name(display: *mut xlib::Display, name: &str) -> Option<CursorImage> {
    let cstr = CString::new(name).ok()?;
    unsafe {
        let size = XcursorGetDefaultSize(display);
        let theme = XcursorGetTheme(display);
        
        let img = XcursorLibraryLoadImage(cstr.as_ptr(), theme, size);
        if img.is_null() {
            return None;
        }
        let width = (*img).width as u32;
        let height = (*img).height as u32;
        let xhot = (*img).xhot as u32;
        let yhot = (*img).yhot as u32;
        let len = width.saturating_mul(height) as usize;
        let src = std::slice::from_raw_parts((*img).pixels, len);
        let pixels = src.iter().map(|p| *p as u32).collect();
        xcursor::XcursorImageDestroy(img);
        Some(CursorImage {
            pixels,
            width,
            height,
            xhot,
            yhot,
        })
    }
}

fn collect_windows(display: *mut xlib::Display, root: xlib::Window) -> HashSet<xlib::Window> {
    let mut stack = vec![root];
    let mut windows = HashSet::new();

    while let Some(window) = stack.pop() {
        let mut root_return = 0;
        let mut parent_return = 0;
        let mut children_return: *mut xlib::Window = ptr::null_mut();
        let mut nchildren_return = 0;

        unsafe {
            if xlib::XQueryTree(
                display,
                window,
                &mut root_return,
                &mut parent_return,
                &mut children_return,
                &mut nchildren_return,
            ) != 0
            {
                if nchildren_return > 0 && !children_return.is_null() {
                    let children_slice = std::slice::from_raw_parts(children_return, nchildren_return as usize);
                    for &child in children_slice {
                        windows.insert(child);
                        stack.push(child);
                    }
                }
                if !children_return.is_null() {
                    xlib::XFree(children_return as *mut _);
                }
            }
        }
    }

    windows
}

fn apply_cursor_to_windows(
    display: *mut xlib::Display,
    root: xlib::Window,
    cursor: xlib::Cursor,
    windows: &HashSet<xlib::Window>,
) {
    unsafe {
        xlib::XDefineCursor(display, root, cursor);
        for &window in windows {
            xlib::XDefineCursor(display, window, cursor);
        }
        xlib::XFlush(display);
    }
}
