//! A stub that will be filled out by someone else later, lol

use std::ffi::{c_uint, c_short};

use dolphin_logger::Log;

/// This handler definition represents a passed-in function for pushing audio samples
/// into the current Dolphin SoundStream interface.
pub type ForeignAudioSamplerFn = unsafe extern "C" fn(samples: *const c_short, num_samples: c_uint);

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

/// Soontm.
#[derive(Debug)]
pub struct Jukebox {
    m_p_ram: usize,
    sampler_fn: ForeignAudioSamplerFn
}

impl Jukebox {
    /// Returns a new configured Jukebox, ready to play.
    pub fn new(m_p_ram: *const u8, sampler_fn: ForeignAudioSamplerFn) -> Result<Self> {
        let m_p_ram = m_p_ram as usize;

        tracing::info!(
            target: Log::Jukebox,
            m_p_ram,
            "Initializing Jukebox"
        );

        Ok(Self { m_p_ram, sampler_fn })
    }
}
