fn main() {
    println!("cargo:rustc-link-lib=X11");     // Link X11
    println!("cargo:rustc-link-lib=Xfixes");  // Link Xfixes
}
