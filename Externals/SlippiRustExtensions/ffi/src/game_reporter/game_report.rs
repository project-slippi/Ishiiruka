use std::ffi::c_char;

use slippi_game_reporter::{OnlinePlayMode, GameReport, PlayerReport};

use super::{set, unpack_str};

/*#[derive(Debug)]
#[repr(C)]
pub enum OnlinePlayMode {
    Ranked = 0,
    Unranked = 1,
    Direct = 2,
    Teams = 3
}

#[derive(Debug)]
pub struct GameReport {
    pub online_mode: OnlinePlayMode,
}*/

/// Initializes a new GameReport and leaks it, returning the instance pointer
/// after doing so.
///
/// This is expected to ultimately be passed to the game reporter, which will handle
/// destruction and cleanup.
#[no_mangle]
pub extern "C" fn slprs_game_report_create() -> usize {
    let report = Box::new(GameReport::default());
    let report_instance_ptr = Box::into_raw(report) as usize;
    report_instance_ptr
}

/// Takes ownership of the `PlayerReport` at the specified address, adding it to the
/// `GameReport` at the corresponding address.
#[no_mangle]
pub extern "C" fn slprs_game_report_add_player_report(
    instance_ptr: usize,
    player_report_instance_ptr: usize
) {
    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `game_report_instance_ptr` is only owned
    // by us, and is created/destroyed with the corresponding lifetimes.
    let player_report = unsafe {
        Box::from_raw(player_report_instance_ptr as *mut PlayerReport)
    };

    set::<GameReport, _>(instance_ptr, move |report| {
        report.players.push(*player_report);
    });
}

/// Sets the `match_id` on the game report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_game_report_set_match_id(instance_ptr: usize, match_id: *const c_char) {
    set::<GameReport, _>(instance_ptr, move |report| {
        let match_id = unpack_str(match_id, "slprs_game_report_set_match_id", "match_id");
        report.match_id = Some(match_id);
    });
}

/// Sets the `duration_frames` on the game report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_game_report_set_duration_frames(instance_ptr: usize, duration: u32) {
    set::<GameReport, _>(instance_ptr, move |report| {
        report.duration_frames = Some(duration);
    });
}

/// Sets the `game_index` on the game report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_game_report_set_game_index(instance_ptr: usize, index: u32) {
    set::<GameReport, _>(instance_ptr, move |report| {
        report.game_index = Some(index);
    });
}

/// Sets the `tie_break_index` on the game report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_game_report_set_tie_break_index(instance_ptr: usize, index: u32) {
    set::<GameReport, _>(instance_ptr, move |report| {
        report.tie_break_index = Some(index);
    });
}

/// Sets the `winner_index` on the game report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_game_report_set_winner_index(instance_ptr: usize, index: i8) {
    set::<GameReport, _>(instance_ptr, move |report| {
        report.winner_index = Some(index);
    });
}

/// Sets the `game_end_method` on the game report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_game_report_set_game_end_method(instance_ptr: usize, method: u8) {
    set::<GameReport, _>(instance_ptr, move |report| {
        report.game_end_method = Some(method);
    });
}

/// Sets the `lras_initiator` on the game report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_game_report_set_lras_initiator(instance_ptr: usize, initiator: i8) {
    set::<GameReport, _>(instance_ptr, move |report| {
        report.lras_initiator = Some(initiator);
    });
}

/// Sets the `stage_id` on the game report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_game_report_set_stage_id(instance_ptr: usize, stage_id: i32) {
    set::<GameReport, _>(instance_ptr, move |report| {
        report.stage_id = Some(stage_id);
    });
}
