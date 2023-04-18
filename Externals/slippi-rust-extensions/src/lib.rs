//! This library implements Slippi-specific logic used in Dolphin. It's linked into Dolphin
//! at build time and uses the C FFI to communicate over the Rust/C++ boundary.
//!
//! The core idea behind it all is that this "extension" is launched when the Slippi EXI Device
//! becomes active. The EXI Device (on the C++ side) holds the pointer for the Rust shadow EXI
//! Device, and is responsible for calling through to the destructor when the corresponding C++
//! destructor is run.

use std::ffi::{c_char, c_int};
use std::sync::Once;

use tracing_subscriber::prelude::*;

mod exi;
use exi::SlippiEXIDevice;

mod logger;
use logger::DolphinLoggerLayer;

mod jukebox;

/// A generic Result type alias that just wraps in our error type.
pub(crate) type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

/// A guard so that we don't double-init logging layers.
static LOGGER: Once = Once::new();

/// This is the entry point for the library - the C++ (Dolphin) side of things should call this
/// and pass the appropriate pointers and functions. At that point, everything on the Rust side
/// is its own universe, and should be told to shut down (at whatever point) via the corresponding 
/// `slprs_exi_device_destroy` function.
///
/// The returned pointer from this should be used for any of the subsequent `slprs_exi_device_`
/// calls, and should *not* be used after calling `slprs_exi_device_destroy`.
#[no_mangle]
pub extern "C" fn slprs_exi_device_create(
    m_pRAM: *const u8,
    logger_fn: unsafe extern "C" fn(c_int, *const c_char, c_int, *const c_char)
) -> usize {
    // We install our custom logger up-front. Since the EXI device can be restarted, we guard
    // against this running multiple times. We don't use `try_init` here because we do want to
    // know if something else, somehow, registered before us.
    //
    // *Usually* you do not want a library installing a global logger, however our use case is
    // not so standard: this library does in a sense act as an application due to the way it's
    // called into, and we *want* a global subscriber.
    LOGGER.call_once(|| {
        tracing_subscriber::registry()
            .with(DolphinLoggerLayer::new(logger_fn))
            .init();
    });

    let device = Box::new(SlippiEXIDevice::new(m_pRAM as usize));
    let ptr = Box::into_raw(device);
    let ptr = ptr as usize;
    ptr
}

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMAWrite(u32 _uAddr, u32 _uSize);`
#[no_mangle]
pub extern "C" fn slprs_exi_device_dma_write(
    exi_device_instance: usize,
    address: *const u8,
    size: *const u8
) {
    // Coerce the instance back from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance` pointer is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    let mut device = unsafe {
        Box::from_raw(exi_device_instance as *mut SlippiEXIDevice)
    };

    if let Err(e) = device.dma_write(address as usize, size as usize) {
        tracing::error!(error = ?e, "dma_write failure");
    }

    // Fall back into a raw pointer so Rust doesn't obliterate the object
    let _leak = Box::into_raw(device);
}

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMARead(u32 _uAddr, u32 _uSize);`
#[no_mangle]
pub extern "C" fn slprs_exi_device_dma_read(
    exi_device_instance: usize,
    address: *const u8,
    size: *const u8
) {
    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance` pointer is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    let mut device = unsafe {
        Box::from_raw(exi_device_instance as *mut SlippiEXIDevice)
    };

    if let Err(e) = device.dma_read(address as usize, size as usize) {
        tracing::error!(error = ?e, "dma_read failure");
    }

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}

/// The "exit point" for the library - the C++ (Dolphin) side of things should call this to
/// notify the Rust side that it can safely shut down and clean up.
#[no_mangle]
pub extern "C" fn slprs_exi_device_destroy(exi_device_instance: usize) {
    tracing::warn!(instance = exi_device_instance, "Destroy");

    unsafe {
        // Coerce ownership back, then let standard Drop semantics apply
        let _device = Box::from_raw(exi_device_instance as *mut SlippiEXIDevice);
    }
}

