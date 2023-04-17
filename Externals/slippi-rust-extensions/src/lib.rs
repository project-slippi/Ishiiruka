//! This library implements Slippi-specific logic used in Dolphin. It's linked into Dolphin
//! at build time and uses the C FFI to communicate over the Rust/C++ boundary.
//!
//! The core idea behind it all is that this "extension" is launched when the Slippi EXI Device
//! becomes active. The EXI Device (on the C++ side) holds the pointer for the Rust shadow EXI
//! Device, and is responsible for calling through to the destructor when the corresponding C++
//! destructor is run.

use std::ffi::{c_char, c_int};

mod dolphin;
use dolphin::DolphinAdapter;

mod exi;
use exi::SlippiEXIDevice;

mod jukebox;

/// A generic Result type alias that just wraps in our error type.
pub(crate) type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

/// This is the entry point for the library - the C++ (Dolphin) side of things should call this
/// and pass the appropriate pointers and functions. At that point, everything on the Rust side
/// is its own universe, and should be told to shut down (at whatever point) via the corresponding 
/// `slprs_exi_device_destroy` function.
///
/// The returned pointer from this should be used for any of the subsequent `slprs_exi_device_`
/// calls, and should *not* be used after calling `slprs_exi_device_destroy`.
#[no_mangle]
pub extern "C" fn slprs_exi_device_create(
    ram_offset: *const u8,
    logger_fn: unsafe extern "C" fn(c_int, *const c_char, c_int, *const c_char)
) -> usize {
    let dolphin_adapter = DolphinAdapter::new(ram_offset, logger_fn);
    let device = Box::new(SlippiEXIDevice::new(dolphin_adapter));
    let ptr = Box::into_raw(device);
    ptr as usize
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
        device.dolphin.logger.error(format!("{:?}", e));
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
        device.dolphin.logger.error(format!("{:?}", e));
    }

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}

/// The "exit point" for the library - the C++ (Dolphin) side of things should call this to
/// notify the Rust side that it can safely shut down and clean up.
#[no_mangle]
pub extern "C" fn slprs_exi_device_destroy(exi_device_instance: usize) {
    unsafe {
        // Coerce ownership back, then let standard Drop semantics apply
        let _device = Box::from_raw(exi_device_instance as *mut SlippiEXIDevice);
    }
}
