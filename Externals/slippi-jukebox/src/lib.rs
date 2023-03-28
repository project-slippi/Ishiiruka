//! A basic test integration to kick the tires on getting Cargo
//! working with CMake, plus exposing some necessary types from
//! the Ishiiruka codebase into Rust.

/// The main entry point of the library - Ishiiruka calls into this and
/// passes the `ram_offset` to work with.
#[no_mangle]
pub extern "C" fn start_slippi_jukebox(ram_offset: *const u8) {
    println!("Jukebox started! {:p}", ram_offset);
}

/// Call this to end the jukebox, I guess.
#[no_mangle]
pub extern "C" fn shutdown_slippi_jukebox() {
    println!("Shutting down the jukebox");
}
