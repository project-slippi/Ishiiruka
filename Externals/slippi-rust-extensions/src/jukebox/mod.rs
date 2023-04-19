//! A stub that will be filled out by someone else later, lol

use std::ffi::{c_uint, c_short};

use crate::logger::Log;

/// This handler definition represents a passed-in function for pushing audio samples
/// into the current Dolphin SoundStream interface.
pub type ForeignAudioSamplerFn = unsafe extern "C" fn(samples: *const c_short, num_samples: c_uint);

/// Soontm.
#[derive(Debug)]
pub struct Jukebox {
    m_pRAM: usize,
    sampler_fn: ForeignAudioSamplerFn
}

impl Jukebox {
    /// Returns a new configured Jukebox, ready to play.
    pub fn new(m_pRAM: usize, sampler_fn: ForeignAudioSamplerFn) -> Self {
        tracing::info!(
            target: Log::Jukebox,
            m_pRAM,
            "Initializing Jukebox"
        );

        Self { m_pRAM, sampler_fn }
    }

    /// Starts the Jukebox. This may be able to be condensed, above my pay grade.
    pub fn start(&self) {
        tracing::info!(
            target: Log::Jukebox,
            "Starting Jukebox"
        );
    }
}
