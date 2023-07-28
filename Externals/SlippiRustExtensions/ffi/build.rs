//! This build script simply generates C FFI bindings for the freestanding
//! functions in `lib.rs` and dumps them into a header that the Dolphin
//! project is pre-configured to find.

use std::env;

fn main() {
    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();

    cbindgen::Builder::new()
        .with_crate(crate_dir)
        .generate()
        .expect("Unable to generate bindings")
        .write_to_file("includes/SlippiRustExtensions.h");
}
