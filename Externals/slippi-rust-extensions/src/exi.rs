//! This module houses the `SlippiEXIDevice`, which is in effect a "shadow subclass" of the C++
//! Slippi EXI device.
//!
//! What this means is that the Slippi EXI Device (C++) holds a pointer to the Rust
//! `SlippiEXIDevice` and forwards calls over the C FFI. This has a fairly clean mapping to "when
//! Slippi stuff is happening" and enables us to let the Rust side live in its own world.

use crate::jukebox::Jukebox;

/// An EXI Device subclass specific to managing and interacting with the game itself.
#[derive(Debug)]
pub(crate) struct SlippiEXIDevice {
    jukebox: Jukebox
}

impl SlippiEXIDevice {
    /// Creates and returns a new `SlippiEXIDevice` with default values.
    ///
    /// At the moment you should never need to call this yourself.
    pub fn new(m_pRAM: usize) -> Self {
        tracing::info!("Starting SlippiEXIDevice");

        let jukebox = Jukebox::new(m_pRAM);

        Self { jukebox }
    }

    /// Stubbed for now, but this would get called by the C++ EXI device on DMAWrite.
    pub fn dma_write(&mut self, _address: usize, _size: usize) -> crate::Result<()> {
        Ok(())
    }

    /// Stubbed for now, but this would get called by the C++ EXI device on DMARead.
    pub fn dma_read(&mut self, _address: usize, _size: usize) -> crate::Result<()> {
        Ok(())
    }
}
