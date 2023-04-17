//! A stub that will be filled out by someone else later, lol

use crate::dolphin::DolphinAdapter;

/// Soontm.
#[derive(Debug)]
pub struct Jukebox {
    dolphin: DolphinAdapter
}

impl Jukebox {
    /// Returns a new configured Jukebox, ready to play.
    pub fn new(dolphin: DolphinAdapter) -> Self {
        Self { dolphin }
    }
}
