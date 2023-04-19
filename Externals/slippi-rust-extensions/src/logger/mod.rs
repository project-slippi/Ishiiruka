//! This module provides a tracing subscriber configuration that works with the
//! Dolphin logging setup.
//!
//! It essentially maps the concept of a `LogContainer` over to Rust, and provides
//! C FFI hooks to forward state change calls in. On top of that, this module contains
//! a custom `tracing_subscriber::Layer` that will pass logs back to Dolphin.
//!
//! Ultimately this should mean no log fragmentation or confusion.

use std::ffi::{c_char, c_int, CStr};
use std::sync::{Arc, Once, RwLock};

use once_cell::sync::OnceCell;
use tracing::Level;
use tracing_subscriber::prelude::*;

mod layer;
use layer::{DolphinLoggerLayer, convert_dolphin_log_level_to_tracing_level};

/// A marker for where logs should be routed to.
///
/// Rust enum variants can't be strings, but we want to be able to pass an
/// enum to the tracing macro `target` field - which requires a static str.
///
/// Thus we'll fake things a bit and just expose a module that keys things
/// accordingly. The syntax will be the same as if using an enum.
///
/// If you want to add a new logger type, you will need to add a new value here
/// and create a corresponding `RustLogContainer` on the Dolphin side. The rest
/// should "just work".
#[allow(non_upper_case_globals)]
pub(crate) mod Log {
    /// The default target for all tracing (the crate name).
    pub const General: &'static str = "slippi_rust_extensions";

    /// Can be used to segment Jukebox logs.
    pub const Jukebox: &'static str = "slippi_rust_jukebox";
}

/// Represents a `LogContainer` on the Dolphin side.
#[derive(Debug)]
pub(crate) struct LogContainer {
    pub kind: String,
    pub log_type: c_int,
    pub is_enabled: bool,
    pub level: Level
}

/// A global stack of `LogContainers`.
/// 
/// All logger registrations (which require `write`) should happen up-front due to how
/// Dolphin itself works. RwLock here should provide us parallel reader access after.
static LOG_CONTAINERS: OnceCell<Arc<RwLock<Vec<LogContainer>>>> = OnceCell::new();

/// This should be called from the Dolphin LogManager initialization to ensure that
/// all logging needs on the Rust side are configured appropriately.
///
/// *Usually* you do not want a library installing a global logger, however our use case is
/// not so standard: this library does in a sense act as an application due to the way it's
/// called into, and we *want* a global subscriber.
/// 
/// Note that `logger_fn` cannot be type-aliased here, otherwise cbindgen will
/// mess up the header output. That said, the function type represents:
///
/// ```
/// void Log(level, log_type, filename, line_number, msg);
/// ```
#[no_mangle]
pub extern "C" fn slprs_logging_init(
    logger_fn: unsafe extern "C" fn(c_int, c_int, *const c_char, c_int, *const c_char),
) {
    let _containers = LOG_CONTAINERS.get_or_init(|| {
        Arc::new(RwLock::new(Vec::new()))
    });

    // A guard so that we don't double-init logging layers.
    static LOGGER: Once = Once::new();

    // We don't use `try_init` here because we do want to
    // know if something else, somehow, registered before us.
    LOGGER.call_once(|| {
        tracing_subscriber::registry()
            .with(DolphinLoggerLayer::new(logger_fn))
            .init();
    });
}

/// Registers a log container, which mirrors a Dolphin `LogContainer` (`RustLogContainer`).
///
/// This enables passing a configured log level and/or enabled status across the boundary from
/// Dolphin to our tracing subscriber setup. This is important as we want to short-circuit any
/// allocations during log handling that aren't necessary (e.g if a log is outright disabled).
#[no_mangle]
pub extern "C" fn slprs_logging_register_container(
    kind: *const c_char,
    log_type: c_int,
    is_enabled: bool,
    default_log_level: c_int
) {
    // We control the other end of the registration flow, so we can ensure this ptr's valid UTF-8.
    let c_kind_str = unsafe {
        CStr::from_ptr(kind)
    };
    
    let kind = c_kind_str
        .to_str()
        .expect("[slprs_logging_register_container]: Failed to convert kind c_char to str")
        .to_string();

    let containers = LOG_CONTAINERS.get()
        .expect("[slprs_logging_register_container]: Attempting to get `LOG_CONTAINERS` before init");

    let mut writer = containers.write()
        .expect("[slprs_logging_register_container]: Unable to acquire write lock on `LOG_CONTAINERS`?");

    (*writer).push(LogContainer {
        kind,
        log_type,
        is_enabled,
        level: convert_dolphin_log_level_to_tracing_level(default_log_level)
    });
}

/// Sets a particular log container to a new enabled state. When a log container is in a disabled
/// state, no allocations will happen behind the scenes for any logging period.
#[no_mangle]
pub extern "C" fn slprs_logging_update_container(kind: *const c_char, enabled: bool, level: c_int) {
    // We control the other end of the registration flow, so we can ensure this ptr's valid UTF-8.
    let c_kind_str = unsafe {
        CStr::from_ptr(kind)
    };
    
    let kind = c_kind_str
        .to_str()
        .expect("[slprs_logging_update_container]: Failed to convert kind c_char to str");

    let containers = LOG_CONTAINERS.get()
        .expect("[slprs_logging_update_container]: Attempting to get `LOG_CONTAINERS` before init");

    let mut writer = containers.write()
        .expect("[slprs_logging_set_container_enabled]: Unable to acquire write lock on `LOG_CONTAINERS`?");

    for container in (*writer).iter_mut() {
        if container.kind == kind {
            container.is_enabled = enabled;
            container.level = convert_dolphin_log_level_to_tracing_level(level);
            break;
        }
    }
}
