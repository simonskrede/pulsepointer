fn main() {
    // Explicitly link against X11 and Xfixes for XDefineCursor and related symbols.
    println!("cargo:rustc-link-lib=dylib=X11");
    println!("cargo:rustc-link-lib=dylib=Xfixes");
    println!("cargo:rustc-link-lib=dylib=Xcursor");
    // Ensure the linker keeps these even with --as-needed defaults.
    println!("cargo:rustc-link-arg=-lX11");
    println!("cargo:rustc-link-arg=-lXfixes");
    println!("cargo:rustc-link-arg=-lXcursor");
}
