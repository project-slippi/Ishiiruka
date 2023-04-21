//! This library is the core interface for the Rust side of things, and consists
//! predominantly of C FFI bridging functions that can be called from the Dolphin
//! side of things.
//!
//! This library auto-generates C headers on build, and Slippi Dolphin is pre-configured
//! to locate these headers and link the entire dylib.

use std::ffi::{c_char, c_int, c_short, c_uint};

use dolphin_logger::Log;
use slippi_exi_device::SlippiEXIDevice;

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
    let exi_device_instance_ptr = Box::into_raw(exi_device) as usize;

    tracing::warn!(
        target: Log::EXI,
        ptr = exi_device_instance_ptr,
        "Creating Device"
    );

    exi_device_instance_ptr
}

/// The C++ (Dolphin) side of things should call this to notify the Rust side that it
/// can safely shut down and clean up.
#[no_mangle]
pub extern "C" fn slprs_exi_device_destroy(exi_device_instance_ptr: usize) {
    tracing::warn!(
        target: Log::EXI,
        ptr = exi_device_instance_ptr,
        "Destroying Device"
    );

    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance_ptr` is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    unsafe {
        // Coerce ownership back, then let standard Drop semantics apply
        let _device = Box::from_raw(exi_device_instance_ptr as *mut SlippiEXIDevice);
    }
}

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMAWrite(u32 _uAddr, u32 _uSize);`
#[no_mangle]
pub extern "C" fn slprs_exi_device_dma_write(
    exi_device_instance_ptr: usize,
    address: *const u8,
    size: *const u8,
) {
    // Coerce the instance back from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance_ptr` pointer is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    let mut device = unsafe { Box::from_raw(exi_device_instance_ptr as *mut SlippiEXIDevice) };

    device.dma_write(address as usize, size as usize);

    // Fall back into a raw pointer so Rust doesn't obliterate the object
    let _leak = Box::into_raw(device);
}

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMARead(u32 _uAddr, u32 _uSize);`
#[no_mangle]
pub extern "C" fn slprs_exi_device_dma_read(
    exi_device_instance_ptr: usize,
    address: *const u8,
    size: *const u8,
) {
    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance_ptr` pointer is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    let mut device = unsafe { Box::from_raw(exi_device_instance_ptr as *mut SlippiEXIDevice) };

    device.dma_read(address as usize, size as usize);

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}

/// Kicks off the Jukebox process. This needs to be called after the EXI device is created
/// in order for certain pieces of Dolphin to be properly initalized; this may change down
/// the road though and is not set in stone.
#[no_mangle]
pub extern "C" fn slprs_exi_device_start_jukebox(
    exi_device_instance_ptr: usize,
    m_p_ram: *const u8,
    sample_handler_fn: unsafe extern "C" fn(samples: *const c_short, num_samples: c_uint),
) {
    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance_ptr` is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    let mut device = unsafe { Box::from_raw(exi_device_instance_ptr as *mut SlippiEXIDevice) };

    device.start_jukebox(m_p_ram, sample_handler_fn);

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}

/// This should be called from the Dolphin LogManager initialization to ensure that
/// all logging needs on the Rust side are configured appropriately.
///
/// For more information, consult `dolphin_logger::init`.
///
/// Note that `logger_fn` cannot be type-aliased here, otherwise cbindgen will
/// mess up the header output. That said, the function type represents:
///
/// ```
/// void Log(level, log_type, msg);
/// ```
#[no_mangle]
pub extern "C" fn slprs_logging_init(logger_fn: unsafe extern "C" fn(c_int, c_int, *const c_char)) {
    dolphin_logger::init(logger_fn);
}

/// Registers a log container, which mirrors a Dolphin `LogContainer` (`RustLogContainer`).
///
/// See `dolphin_loger::register_container` for more information.
#[no_mangle]
pub extern "C" fn slprs_logging_register_container(
    kind: *const c_char,
    log_type: c_int,
    is_enabled: bool,
    default_log_level: c_int,
) {
    dolphin_logger::register_container(kind, log_type, is_enabled, default_log_level);
}

/// Updates the configuration for a registered logging container.
///
/// For more information, see `dolphin_logger::update_container`.
#[no_mangle]
pub extern "C" fn slprs_logging_update_container(kind: *const c_char, enabled: bool, level: c_int) {
    dolphin_logger::update_container(kind, enabled, level);
}
