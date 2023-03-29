//! A basic test integration to kick the tires on getting Cargo
//! working with CMake, plus exposing some necessary types from
//! the Ishiiruka codebase into Rust.

mod log;
use log::{ForeignLoggerFn, Logger};

/// The main entry point of the library - Ishiiruka calls into this and
/// passes the `ram_offset` to work with.
#[no_mangle]
pub extern "C" fn start_slippi_jukebox(
    ram_offset: *const u8,
    logger_fn: ForeignLoggerFn
) {
    let logger = Logger::new(logger_fn);
    logger.info(format!("Jukebox started! {:p}", ram_offset));
    logger.warn(format!("Jukebox started! {:p}", ram_offset));
    logger.error(format!("Jukebox started! {:p}", ram_offset));
}

/// Call this to end the jukebox, I guess.
#[no_mangle]
pub extern "C" fn shutdown_slippi_jukebox() {
    // log::info("Shutting down the jukebox");
}
