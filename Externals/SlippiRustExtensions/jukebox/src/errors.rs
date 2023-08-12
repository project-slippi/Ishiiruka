use thiserror::Error;

#[derive(Error, Debug)]
pub enum JukeboxError {
    #[error("{0}")]
    GenericIOError(#[from] std::io::Error),

    #[error("Failed to spawn thread: {0}")]
    ThreadSpawnFailure(std::io::Error),

    #[error("Unexpected null pointer or unaligned read from Dolphin's memory: {0}")]
    DolphinMemoryReadError(std::io::Error),

    #[error("Failed to decode music file: {0}")]
    MusicFileDecodingError(#[from] hps_decode::hps::HpsParseError),

    #[error("Unable to get an audio device handle: {0}")]
    AudioDeviceError(#[from] rodio::StreamError),

    #[error("Unable to play sound with rodio: {0}")]
    AudioPlaybackError(#[from] rodio::PlayError),

    #[error("Failed to parse ISO's Filesystem Table: {0}")]
    FstParseError(String),

    #[error("Failed to seek the ISO: {0}")]
    IsoSeekError(std::io::Error),

    #[error("Failed to read the ISO: {0}")]
    IsoReadError(std::io::Error),

    #[error("The provided game file is not supported")]
    UnsupportedIso,

    #[error("Unknown Jukebox Error")]
    Unknown,
}
