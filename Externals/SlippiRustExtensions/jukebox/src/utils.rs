use std::fs::File;
use std::io::{Read, Seek};

use crate::scenes::scene_ids::*;
use crate::{JukeboxError::*, Result};

/// Get a copy of the `size` bytes in `file` at `offset`
pub(crate) fn copy_bytes_from_file(file: &mut File, offset: u64, size: usize) -> Result<Vec<u8>> {
    file.seek(std::io::SeekFrom::Start(offset)).map_err(IsoSeek)?;
    let mut bytes = vec![0; size];
    file.read_exact(&mut bytes).map_err(IsoRead)?;
    Ok(bytes)
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
    if (SCENE_TRAINING_MODE..=SCENE_STAMINA_MODE).contains(&scene_major) || scene_major == SCENE_FIXED_CAMERA_MODE {
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
        return scene_minor == SCENE_TITLE_SCREEN_IDLE_FIGHT_1 || scene_minor == SCENE_TITLE_SCREEN_IDLE_FIGHT_2;
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
        return scene_minor == SCENE_VS_ONLINE_CSS || scene_minor == SCENE_VS_ONLINE_SSS || scene_minor == SCENE_VS_ONLINE_RANKED;
    }
    if (SCENE_TRAINING_MODE..=SCENE_STAMINA_MODE).contains(&scene_major) || scene_major == SCENE_FIXED_CAMERA_MODE {
        return scene_minor == SCENE_TRAINING_CSS || scene_minor == SCENE_TRAINING_SSS;
    }
    if scene_major == SCENE_EVENT_MATCH {
        return scene_minor == SCENE_EVENT_MATCH_SELECT;
    }
    if scene_major == SCENE_CLASSIC_MODE || scene_major == SCENE_ADVENTURE_MODE || scene_major == SCENE_ALL_STAR_MODE {
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
