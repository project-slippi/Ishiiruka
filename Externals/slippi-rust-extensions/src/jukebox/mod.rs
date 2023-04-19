//! A stub that will be filled out by someone else later, lol

use crate::logger::Log;

/// Soontm.
#[derive(Debug)]
pub struct Jukebox {
    m_pRAM: usize
}

impl Jukebox {
    /// Returns a new configured Jukebox, ready to play.
    pub fn new(m_pRAM: usize) -> Self {
        tracing::info!(
            target: Log::Jukebox,
            m_pRAM,
            "Initializing Jukebox"
        );

        Self { m_pRAM }
    }

    /// Starts the Jukebox. This may be able to be condensed, above my pay grade.
    pub fn start(&self) {
        tracing::info!(
            target: Log::Jukebox,
            "Starting Jukebox"
        );
    }
}
