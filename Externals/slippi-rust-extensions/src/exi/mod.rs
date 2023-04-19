//! This module implements a "shadow" EXI device that should be created and destroyed
//! via the C++ Dolphin-side Slippi EXI device.

use std::ffi::{c_uint, c_short};

use crate::logger::Log;

mod device;
use device::SlippiEXIDevice;

/// Creates and leaks a shadow EXI device.
///
/// The C++ (Dolphin) side of things should call this and pass the appropriate arguments. At
/// that point, everything on the Rust side is its own universe, and should be told to shut 
/// down (at whatever point) via the corresponding `slprs_jukebox_destroy` function.
///
/// The returned pointer from this should *not* be used after calling `slprs_exi_device_destroy`.
#[no_mangle]
pub extern "C" fn slprs_exi_device_create() -> usize {
    let exi_device = Box::new(SlippiEXIDevice::new());
    let ptr = Box::into_raw(exi_device);
    ptr as usize
}

/// The "exit point" for the library - the C++ (Dolphin) side of things should call this to
/// notify the Rust side that it can safely shut down and clean up.
#[no_mangle]
pub extern "C" fn slprs_exi_device_destroy(exi_device_instance: usize) {
    tracing::warn!(instance = exi_device_instance, "Destroying");

    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance` pointer is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    unsafe {
        // Coerce ownership back, then let standard Drop semantics apply
        let _device = Box::from_raw(exi_device_instance as *mut SlippiEXIDevice);
    }
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

/// Kicks off the Jukebox process. This needs to be called after the EXI device is created
/// in order for certain pieces of Dolphin to be properly initalized; this may change down
/// the road though and is not set in stone.
#[no_mangle]
pub extern "C" fn slprs_exi_device_start_jukebox(
    exi_device_instance: usize,
    m_pRAM: *const u8,
    sample_handler_fn: unsafe extern "C" fn(samples: *const c_short, num_samples: c_uint)
) {
    let m_pRAM = m_pRAM as usize;
    //tracing::warn!(target: Log::Jukebox, m_pRAM, "Starting Jukebox");

    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance` pointer is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    let mut device = unsafe {
        Box::from_raw(exi_device_instance as *mut SlippiEXIDevice)
    };

    if let Err(e) = device.start_jukebox(m_pRAM, sample_handler_fn) {
        tracing::error!(
            target: Log::Jukebox,
            error = ?e,
            "start_jukebox failure"
        );
    }

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}
