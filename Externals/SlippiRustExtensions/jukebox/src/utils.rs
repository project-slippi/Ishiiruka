use crate::scenes::scene_ids::*;
use crate::tracks::{identify_coefficients, TrackId};
use anyhow::Result;
use directories::BaseDirs;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::io::prelude::*;

#[derive(Serialize, Deserialize, Debug)]
pub(crate) struct SlippiConfig {
    settings: SlippiSettings,
}

#[derive(Serialize, Deserialize, Debug)]
pub(crate) struct SlippiSettings {
    #[serde(rename = "isoPath")]
    iso_path: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub(crate) struct DolphinConfig {
    #[serde(rename = "DSP")]
    dsp: std::collections::HashMap<String, String>,
}

/// Get the path of the Melee ISO configured in Slippi
pub(crate) fn get_iso_path(base_dirs: &BaseDirs) -> Result<String> {
    let slippi_config_file = base_dirs.config_dir().join("Slippi Launcher/Settings");
    let slippi_config = std::fs::read_to_string(slippi_config_file)?;
    Ok(serde_json::from_str::<SlippiConfig>(&slippi_config)?
        .settings
        .iso_path)
}

/// Get the volume from Dolphin -> Config -> Audio as a value from 0 - 100
pub(crate) fn get_dolphin_volume(base_dirs: &BaseDirs) -> f32 {
    let default_volume = 100;
    let dolphin_config_file = base_dirs
        .config_dir()
        .join("Slippi Launcher/netplay/User/Config/Dolphin.ini");
    let dolphin_config =
        std::fs::read_to_string(dolphin_config_file).unwrap_or_else(|_| String::new());
    let volume = serde_ini::from_str::<DolphinConfig>(&dolphin_config)
        .ok()
        .and_then(|config| {
            config
                .dsp
                .get("Volume")
                .and_then(|volume| volume.parse::<u8>().ok())
        })
        .unwrap_or(default_volume);

    volume as f32 / 100.0
}

/// Produces a hashmap containing offsets and lengths of .hps files contained within the iso
/// These can be looked up by TrackId
pub(crate) fn create_track_map(
    iso: &mut std::fs::File,
) -> Result<HashMap<TrackId, (usize, usize)>> {
    let file_size = iso.metadata()?.len();

    // Locate .hps sections of the iso by scanning it 60mb at a time
    let chunk_size = 1024 * 1024 * 60;
    let chunk_count = file_size / chunk_size + 1;
    let mut buffer = vec![0; chunk_size as usize];

    // Locations (offsets) of all .hps files on the iso
    let locations = (0..chunk_count)
        .map(|chunk_index| {
            // Since the last chunk won't have as many bytes as the rest of
            // them, we need to clear the buffer before we load data into it off
            // the disk
            if chunk_index == chunk_count - 1 {
                buffer.fill(0);
            }

            let chunk_start_address = chunk_size * chunk_index;
            iso.seek(std::io::SeekFrom::Start(chunk_start_address))?;
            iso.read(&mut buffer)?;

            Ok(memchr::memmem::find_iter(buffer.as_slice(), b" HALPST\0")
                .map(|address| chunk_start_address as usize + address)
                .collect::<Vec<_>>())
        })
        .collect::<Result<Vec<_>>>()?;

    // Using known 8-byte sequences from .hps files in the OST, identify each of
    // the locations, and insert them into a hashmap so we can look them up
    // using TrackIds later on
    Ok(locations
        .into_iter()
        .flatten()
        .filter_map(|location| {
            Some({
                // read the first 8 bytes of the left channel coefficients for
                // the .hps file at `location`
                let coef_offset = 0x20;
                iso.seek(std::io::SeekFrom::Start((location as u64) + coef_offset))
                    .ok()?;
                let mut buf = [0; 8];
                iso.read_exact(&mut buf).ok()?;
                // Identify which TrackId is associated and populate the hashmap
                let (id, size) = identify_coefficients(buf)?;
                (id, (location, size))
            })
        })
        .collect::<HashMap<TrackId, (usize, usize)>>())
}

/// Returns true if the user is in an actual match
/// Sourced from M'Overlay: https://github.com/bkacjios/m-overlay/blob/d8c629d/source/melee.lua#L1177
pub(crate) fn is_in_game(scene_major: u8, scene_minor: u8) -> bool {
    if scene_major == SCENE_ALL_STAR_MODE && scene_minor < SCENE_ALL_STAR_CSS {
        return true;
    }
    if scene_major == SCENE_VS_MODE || scene_major == SCENE_VS_ONLINE {
        return scene_minor == SCENE_VS_INGAME;
    }
    if (SCENE_TRAINING_MODE..=SCENE_STAMINA_MODE).contains(&scene_major)
        || scene_major == SCENE_FIXED_CAMERA_MODE
    {
        return scene_minor == SCENE_TRAINING_INGAME;
    }
    if scene_major == SCENE_EVENT_MATCH {
        return scene_minor == SCENE_EVENT_MATCH_INGAME;
    }
    if scene_major == SCENE_CLASSIC_MODE && scene_minor < SCENE_CLASSIC_CONTINUE {
        return scene_minor % 2 == 1;
    }
    if scene_major == SCENE_ADVENTURE_MODE {
        return scene_minor == SCENE_ADVENTURE_MUSHROOM_KINGDOM
            || scene_minor == SCENE_ADVENTURE_MUSHROOM_KINGDOM_BATTLE
            || scene_minor == SCENE_ADVENTURE_MUSHROOM_KONGO_JUNGLE_TINY_BATTLE
            || scene_minor == SCENE_ADVENTURE_MUSHROOM_KONGO_JUNGLE_GIANT_BATTLE
            || scene_minor == SCENE_ADVENTURE_UNDERGROUND_MAZE
            || scene_minor == SCENE_ADVENTURE_HYRULE_TEMPLE_BATTLE
            || scene_minor == SCENE_ADVENTURE_BRINSTAR
            || scene_minor == SCENE_ADVENTURE_ESCAPE_ZEBES
            || scene_minor == SCENE_ADVENTURE_GREEN_GREENS_KIRBY_BATTLE
            || scene_minor == SCENE_ADVENTURE_GREEN_GREENS_KIRBY_TEAM_BATTLE
            || scene_minor == SCENE_ADVENTURE_GREEN_GREENS_GIANT_KIRBY_BATTLE
            || scene_minor == SCENE_ADVENTURE_CORNERIA_BATTLE_1
            || scene_minor == SCENE_ADVENTURE_CORNERIA_BATTLE_2
            || scene_minor == SCENE_ADVENTURE_CORNERIA_BATTLE_3
            || scene_minor == SCENE_ADVENTURE_POKEMON_STADIUM_BATTLE
            || scene_minor == SCENE_ADVENTURE_FZERO_GRAND_PRIX_RACE
            || scene_minor == SCENE_ADVENTURE_FZERO_GRAND_PRIX_BATTLE
            || scene_minor == SCENE_ADVENTURE_ONETT_BATTLE
            || scene_minor == SCENE_ADVENTURE_ICICLE_MOUNTAIN_CLIMB
            || scene_minor == SCENE_ADVENTURE_BATTLEFIELD_BATTLE
            || scene_minor == SCENE_ADVENTURE_BATTLEFIELD_METAL_BATTLE
            || scene_minor == SCENE_ADVENTURE_FINAL_DESTINATION_BATTLE;
    }
    if scene_major == SCENE_TARGET_TEST {
        return scene_minor == SCENE_TARGET_TEST_INGAME;
    }
    if (SCENE_SUPER_SUDDEN_DEATH..=MENU_LIGHTNING_MELEE).contains(&scene_major) {
        return scene_minor == SCENE_SSD_INGAME;
    }
    if (SCENE_HOME_RUN_CONTEST..=SCENE_CRUEL_MELEE).contains(&scene_major) {
        return scene_minor == SCENE_HOME_RUN_CONTEST_INGAME;
    }
    if scene_major == SCENE_TITLE_SCREEN_IDLE {
        return scene_minor == SCENE_TITLE_SCREEN_IDLE_FIGHT_1
            || scene_minor == SCENE_TITLE_SCREEN_IDLE_FIGHT_2;
    }

    false
}

/// Returns true if the player navigating the menus (including CSS and SSS)
/// Sourced from M'Overlay: https://github.com/bkacjios/m-overlay/blob/d8c629d/source/melee.lua#L1243
pub(crate) fn is_in_menus(scene_major: u8, scene_minor: u8) -> bool {
    if scene_major == SCENE_MAIN_MENU {
        return true;
    }
    if scene_major == SCENE_VS_MODE {
        return scene_minor == SCENE_VS_CSS || scene_minor == SCENE_VS_SSS;
    }
    if scene_major == SCENE_VS_ONLINE {
        return scene_minor == SCENE_VS_ONLINE_CSS
            || scene_minor == SCENE_VS_ONLINE_SSS
            || scene_minor == SCENE_VS_ONLINE_RANKED;
    }
    if (SCENE_TRAINING_MODE..=SCENE_STAMINA_MODE).contains(&scene_major)
        || scene_major == SCENE_FIXED_CAMERA_MODE
    {
        return scene_minor == SCENE_TRAINING_CSS || scene_minor == SCENE_TRAINING_SSS;
    }
    if scene_major == SCENE_EVENT_MATCH {
        return scene_minor == SCENE_EVENT_MATCH_SELECT;
    }
    if scene_major == SCENE_CLASSIC_MODE
        || scene_major == SCENE_ADVENTURE_MODE
        || scene_major == SCENE_ALL_STAR_MODE
    {
        return scene_minor == SCENE_CLASSIC_CSS;
    }
    if scene_major == SCENE_TARGET_TEST {
        return scene_minor == SCENE_TARGET_TEST_CSS;
    }
    if (SCENE_SUPER_SUDDEN_DEATH..=MENU_LIGHTNING_MELEE).contains(&scene_major) {
        return scene_minor == SCENE_SSD_CSS || scene_minor == SCENE_SSD_SSS;
    }
    if (SCENE_HOME_RUN_CONTEST..=SCENE_CRUEL_MELEE).contains(&scene_major) {
        return scene_minor == SCENE_HOME_RUN_CONTEST_CSS;
    }
    false
}
