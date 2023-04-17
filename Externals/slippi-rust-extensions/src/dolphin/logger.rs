use std::ffi::CString;
use std::os::raw::{c_int, c_char};

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
#[derive(Clone, Debug)]
pub struct Logger {
    handler: ForeignLoggerFn
}

impl Logger {
    /// Wraps a `ForeignLoggerFn`.
    pub fn new(handler: ForeignLoggerFn) -> Self {
        Self { handler }
    }

    /// The core logging functionality. Handles converting Rust types into C types
    /// and sending them over the bridge.
    fn log(&self, log_level: c_int, msg: Vec<u8>) {
        match CString::new(msg) {
            Ok(c_str_msg) => {
                // Temp, since it's again almost 3AM lol
                let c_filename = CString::new("slippi")
                    .expect("No reason this should fail");
                
                let line_number = 0;

                unsafe {
                    (self.handler)(
                        log_level,
                        c_filename.as_ptr() as *const c_char,
                        line_number,
                        c_str_msg.as_ptr() as *const c_char
                    );
                }
            },

            // This should never happen, but on the off chance it does, I guess
            // just dump it to stderr?
            Err(e) => {
                eprintln!("Failed to convert info msg to CString: {:?}", e);
            }
        }
    }

    /// Log a message at the `LINFO` level. If you need to format and add any data
    /// to the log message, you are responsible for doing so before passing it into
    /// this function.
    pub fn info<S>(&self, msg: S)
    where
        S: Into<Vec<u8>>,
    {
        self.log(LOG_LEVEL_INFO, msg.into())
    }

    /// Log a message at the `LWARN` level. If you need to format and add any data
    /// to the log message, you are responsible for doing so before passing it into
    /// this function.
    pub fn warn<S>(&self, msg: S)
    where
        S: Into<Vec<u8>>,
    {
        self.log(LOG_LEVEL_WARNING, msg.into())
    }

    /// Log a message at the `LINFO` level. If you need to format and add any data
    /// to the log message, you are responsible for doing so before passing it into
    /// this function.
    pub fn error<S>(&self, msg: S)
    where
        S: Into<Vec<u8>>,
    {
        self.log(LOG_LEVEL_ERROR, msg.into())
    }
}
