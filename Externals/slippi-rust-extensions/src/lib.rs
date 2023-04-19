#![allow(non_snake_case)]
//! This library implements Slippi-specific logic used in Dolphin. It's linked into Dolphin
//! at build time and uses the C FFI to communicate over the Rust/C++ boundary.
//!
//! The core idea behind it all is that this "extension" is launched when the Slippi EXI Device
//! becomes active. The EXI Device (on the C++ side) holds the pointer for the Rust shadow EXI
//! Device, and is responsible for calling through to the destructor when the corresponding C++
//! destructor is run.

pub(crate) mod exi;
pub use exi::{
    slprs_exi_device_create, slprs_exi_device_destroy,
    slprs_exi_device_dma_write, slprs_exi_device_dma_read,
    slprs_exi_device_start_jukebox
};


pub(crate) mod logger;
pub use logger::{
    slprs_logging_init, slprs_logging_register_container,
    slprs_logging_update_container
};

pub(crate) mod jukebox;

/// Simple shorthand for Result types.
pub(crate) type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;
