//! This module implements a custom `tracing_subscriber::Layer` that facilitates
//! routing logs through the Dolphin log viewer. 

use std::ffi::CString;
use std::fmt::Write;
use std::os::raw::{c_int, c_char};

use tracing::Level;
use tracing_subscriber::Layer;

/// Corresponds to Dolphin's `LogTypes::LOG_LEVELS::LERROR` value.
const LOG_LEVEL_ERROR: c_int = 2;

/// Corresponds to Dolphin's `LogTypes::LOG_LEVELS::LWARNING` value.
const LOG_LEVEL_WARNING: c_int = 3;

/// Corresponds to Dolphin's `LogTypes::LOG_LEVELS::LINFO` value.
const LOG_LEVEL_INFO: c_int = 4;

/// A type that mirrors a function over on the C++ side; because the library exists as
/// a dylib, it can't depend on any functions from the host application - but we _can_
/// pass in a hook/callback fn.
pub type ForeignLoggerFn = unsafe extern "C" fn(c_int, *const c_char, c_int, *const c_char);

/// A type that wraps a `ForeignLoggerFn` and manages any unsafe ffi calls.
///
/// This implements `tracing_subscriber::Layer` and is the default way to log in this library.
#[derive(Debug)]
pub struct DolphinLoggerLayer {
    handler: ForeignLoggerFn
}

impl DolphinLoggerLayer {
    /// Wraps a `ForeignLoggerFn` and returns a type implementing `Layer` that can be installed
    /// for tracing usage.
    pub fn new(handler: ForeignLoggerFn) -> Self {
        Self { handler }
    }
}

impl<S> Layer<S> for DolphinLoggerLayer
where
    S: tracing::Subscriber,
{
    /// Unpacks a tracing event and routes it to the appropriate Dolphin log handler.
    ///
    /// At the moment, this is somewhat "dumb" and may allocate more than we want to. Consider
    /// it a hook for safely improving performance later. ;P
    fn on_event(
        &self,
        event: &tracing::Event<'_>,
        _ctx: tracing_subscriber::layer::Context<'_, S>,
    ) {
        let mut visitor = DolphinLoggerVisitor::new();
        event.record(&mut visitor);
        
        let metadata = event.metadata();

        match CString::new(visitor.0) {
            Ok(c_str_msg) => {
                let log_level = match *metadata.level() {
                    Level::INFO => LOG_LEVEL_INFO,
                    Level::WARN => LOG_LEVEL_WARNING,
                    Level::ERROR => LOG_LEVEL_ERROR,

                    // Fix to map to trace/debug/etc
                    _ => LOG_LEVEL_INFO
                };

                // Todo: is this an Ishiiruka bug that filenames don't render in the logger?
                // This does produce the correct module path... not a blocker, but annoying.
                let filename = metadata.file().unwrap_or_else(|| "");

                match CString::new(filename) {
                    Ok(c_filename) => {
                        let line_number = metadata.line().unwrap_or_else(|| 0);

                        unsafe {
                            (self.handler)(
                                log_level,
                                c_filename.as_ptr() as *const c_char,
                                line_number.try_into().unwrap_or_else(|_| 0),
                                c_str_msg.as_ptr() as *const c_char
                            );
                        }
                    },

                    // This should never happen, but on the off chance it does, just dump it
                    // to stderr.
                    Err(e) => {
                        eprintln!("Failed to convert filename msg to CString: {:?}", e);
                    }
                }
            },

            // This should never happen, but on the off chance it does, I guess
            // just dump it to stderr?
            Err(e) => {
                eprintln!("Failed to convert info msg to CString: {:?}", e);
            }
        }

    }
}

/// Implements a visitor that builds a type for the logger functionality in Dolphin
/// to consume.
#[derive(Debug)]
struct DolphinLoggerVisitor(String);

impl DolphinLoggerVisitor {
    /// Creates and returns a new `DolphinLoggerVisitor`.
    pub fn new() -> Self {
        Self(String::new())
    }
}

impl tracing::field::Visit for DolphinLoggerVisitor {
    fn record_f64(&mut self, field: &tracing::field::Field, value: f64) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_i64(&mut self, field: &tracing::field::Field, value: i64) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_u64(&mut self, field: &tracing::field::Field, value: u64) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_i128(&mut self, field: &tracing::field::Field, value: i128) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_u128(&mut self, field: &tracing::field::Field, value: u128) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_bool(&mut self, field: &tracing::field::Field, value: bool) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_bool: {:?}", e);
        }
    }

    fn record_str(&mut self, field: &tracing::field::Field, value: &str) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_str: {:?}", e);
        }
    }

    fn record_error(
        &mut self,
        field: &tracing::field::Field,
        value: &(dyn std::error::Error + 'static),
    ) {
        if let Err(e) = write!(&mut self.0, "{}={} ", field.name(), value) {
            eprintln!("Failed to record_error: {:?}", e);
        }
    }

    fn record_debug(&mut self, field: &tracing::field::Field, value: &dyn std::fmt::Debug) {
        if let Err(e) = write!(&mut self.0, "{}={:?} ", field.name(), value) {
            eprintln!("Failed to record_debug: {:?}", e);
        }
    }
}
