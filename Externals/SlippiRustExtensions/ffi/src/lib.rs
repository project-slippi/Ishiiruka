//! This library is the core interface for the Rust side of things, and consists
//! predominantly of C FFI bridging functions that can be called from the Dolphin
//! side of things.
//!
//! This library auto-generates C headers on build, and Slippi Dolphin is pre-configured
//! to locate these headers and link the entire dylib.

pub mod exi;
pub mod game_reporter;
pub mod logger;
