use std::fs::File;
use std::sync::{Arc, OnceLock};

use chksum::prelude::*;

use dolphin_integrations::{Color, Dolphin, Duration};

/// ISO hashes that are known to cause problems. We alert the player
/// if we detect that they're running one.
const KNOWN_DESYNC_ISOS: [&'static str; 4] = [
    "23d6baef06bd65989585096915da20f2",
    "27a5668769a54cd3515af47b8d9982f3",
    "5805fa9f1407aedc8804d0472346fc5f",
    "9bb3e275e77bb1a160276f2330f93931",
];

/// Computes an MD5 hash of the ISO at `iso_path` and writes it back to the value
/// behind `iso_hash`.
pub fn run(iso_hash: Arc<OnceLock<String>>, iso_path: String) {
    let digest = File::open(&iso_path)
        .expect("Dolphin would crash if this was invalid")
        .chksum(HashAlgorithm::MD5)
        .expect("This might be worth handling later");

    let hash = format!("{:x}", digest);

    if KNOWN_DESYNC_ISOS.contains(&hash.as_str()) {
        // This has more line breaks in the C++ version and I frankly do not have the context as to
        // why there were there, but if it's some weird string parsing issue...?
        //
        // Settle on 2 (4 before) as a middle ground I guess.
        Dolphin::add_osd_message(
            Color::Red,
            Duration::Custom(20000),
            "\n\nCAUTION: You are using an ISO that is known to cause desyncs",
        );
    }

    println!("MD5 Hash: {}", hash);
    iso_hash.set(hash).expect("This should not fail");
}
