//! This module implements an adapter for interfacing with Dolphin
//! core pieces over the FFI boundary. It wraps `unsafe` operations and
//! acts as a clean bridge where we can inject our own logic as needed.
//!
//! (This is partly necessary as this library is exposed as a dylib, which means
//! it can't "see" the host application's code - but we _can_ pass in function
//! references to call back to.)

mod logger;
use logger::{ForeignLoggerFn, Logger};

/// An adapter, of sorts, for interacting with Dolphin over the FFI boundary.
///
/// This holds unsafe references to a number of functions and provides "safe"
/// adapters to use throughout this library. If you need to interact with something
/// on the Dolphin side, this is generally where you want to handle the bridging.
///
/// (I have resisted the urge to call this "EchoLocation" or "Ecco")
#[derive(Clone, Debug)]
pub struct DolphinAdapter {
    ram_offset: usize,
    pub logger: Logger,
}

impl DolphinAdapter {
    /// Wraps various function handlers and pointers into a `DolphinAdapter` instance.
    pub fn new(ram_offset: *const u8, logger_fn: ForeignLoggerFn) -> Self {
        Self {
            ram_offset: ram_offset as usize,
            logger: Logger::new(logger_fn)
        }
    }
}
