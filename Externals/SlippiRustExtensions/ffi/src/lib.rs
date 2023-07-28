//! This library is the core interface for the Rust side of things, and consists
//! predominantly of C FFI bridging functions that can be called from the Dolphin
//! side of things.
//!
//! This library auto-generates C headers on build, and Slippi Dolphin is pre-configured
//! to locate these headers and link the entire dylib.

use std::ffi::{c_char, c_int, CStr};

use dolphin_logger::Log;
use slippi_exi_device::SlippiEXIDevice;

/// Creates and leaks a shadow EXI device.
///
/// The C++ (Dolphin) side of things should call this and pass the appropriate arguments. At
/// that point, everything on the Rust side is its own universe, and should be told to shut
/// down (at whatever point) via the corresponding `slprs_exi_device_destroy` function.
///
/// The returned pointer from this should *not* be used after calling `slprs_exi_device_destroy`.
#[no_mangle]
pub extern "C" fn slprs_exi_device_create() -> usize {
    let exi_device = Box::new(SlippiEXIDevice::new());
    let exi_device_instance_ptr = Box::into_raw(exi_device) as usize;

    tracing::warn!(target: Log::EXI, ptr = exi_device_instance_ptr, "Creating Device");

    exi_device_instance_ptr
}

/// The C++ (Dolphin) side of things should call this to notify the Rust side that it
/// can safely shut down and clean up.
#[no_mangle]
pub extern "C" fn slprs_exi_device_destroy(exi_device_instance_ptr: usize) {
    tracing::warn!(target: Log::EXI, ptr = exi_device_instance_ptr, "Destroying Device");

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
pub extern "C" fn slprs_exi_device_dma_write(exi_device_instance_ptr: usize, address: *const u8, size: *const u8) {
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
pub extern "C" fn slprs_exi_device_dma_read(exi_device_instance_ptr: usize, address: *const u8, size: *const u8) {
    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance_ptr` pointer is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    let mut device = unsafe { Box::from_raw(exi_device_instance_ptr as *mut SlippiEXIDevice) };

    device.dma_read(address as usize, size as usize);

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(device);
}

/// Configures the Jukebox process. This needs to be called after the EXI device is created
/// in order for certain pieces of Dolphin to be properly initalized; this may change down
/// the road though and is not set in stone.
#[no_mangle]
pub extern "C" fn slprs_exi_device_configure_jukebox(
    exi_device_instance_ptr: usize,
    is_enabled: bool,
    m_p_ram: *const u8,
    iso_path: *const c_char,
    get_dolphin_volume_fn: unsafe extern "C" fn() -> c_int,
) {
    // Convert the provided ISO path to an owned Rust string.
    // This is theoretically safe since we control the C++ side and can can mostly guarantee
    // the validity of what is being passed in.
    let slice = unsafe { CStr::from_ptr(iso_path) };

    // What we *can't* guarantee is that it's proper UTF-8 etc. If we can't parse it into a
    // Rust String, then we'll just avoid running the Jukebox entirely and log an error message
    // for people to debug with.
    let iso_path = match slice.to_str() {
        Ok(path) => path.to_string(),

        Err(e) => {
            tracing::error!(error = ?e, "Failed to bridge iso_path, jukebox not initializing");
            return;
        },
    };

    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `exi_device_instance_ptr` is only owned
    // by the C++ EXI device, and is created/destroyed with the corresponding lifetimes.
    let mut device = unsafe { Box::from_raw(exi_device_instance_ptr as *mut SlippiEXIDevice) };

    device.configure_jukebox(is_enabled, m_p_ram, iso_path, get_dolphin_volume_fn);

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
/// See `dolphin_logger::register_container` for more information.
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
