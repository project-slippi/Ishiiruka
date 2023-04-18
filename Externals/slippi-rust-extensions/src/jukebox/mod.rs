//! A stub that will be filled out by someone else later, lol

/// Soontm.
#[derive(Debug)]
pub struct Jukebox {
    m_pRAM: usize
}

impl Jukebox {
    /// Returns a new configured Jukebox, ready to play.
    pub fn new(m_pRAM: usize) -> Self {
        tracing::info!(offset = m_pRAM, "Starting jukebox");

        Self { m_pRAM }
    }
}
