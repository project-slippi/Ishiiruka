use std::ffi::c_char;

use slippi_game_reporter::PlayerReport;

use super::{set, unpack_str};

/// Initializes a new PlayerReport and leaks it, returning the instance pointer
/// after doing so.
#[no_mangle]
pub extern "C" fn slprs_player_report_create() -> usize {
    let report = Box::new(PlayerReport::default());
    let report_instance_ptr = Box::into_raw(report) as usize;
    report_instance_ptr
}

/// Sets the `uid` on the player report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_player_report_set_uid(instance_ptr: usize, uid: *const c_char) {
    set::<PlayerReport, _>(instance_ptr, move |report| {
        let uid = unpack_str(uid, "slprs_player_report_set_uid", "uid");
        report.uid = Some(uid);
    });
}

/// Sets the `slot_type` on the player report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_player_report_set_slot_type(instance_ptr: usize, slot_type: u8) {
    set::<PlayerReport, _>(instance_ptr, |report| {
        report.slot_type = Some(slot_type);
    });
}

/// Sets the `damage_done` on the player report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_player_report_set_damage_done(instance_ptr: usize, damage: f64) {
    set::<PlayerReport, _>(instance_ptr, |report| {
        report.damage_done = Some(damage);
    });
}

/// Sets the `stocks_remaining` on the player report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_player_report_set_stocks_remaining(instance_ptr: usize, stocks: u8) {
    set::<PlayerReport, _>(instance_ptr, |report| {
        report.stocks_remaining = Some(stocks);
    });
}

/// Sets the `character_id` on the player report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_player_report_set_character_id(instance_ptr: usize, character_id: u8) {
    set::<PlayerReport, _>(instance_ptr, |report| {
        report.character_id = Some(character_id);
    });
}

/// Sets the `color_id` on the player report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_player_report_set_color_id(instance_ptr: usize, color_id: u8) {
    set::<PlayerReport, _>(instance_ptr, |report| {
        report.slot_type = Some(color_id);
    });
}

/// Sets the `starting_stocks` on the player report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_player_report_set_starting_stocks(instance_ptr: usize, stocks: i64) {
    set::<PlayerReport, _>(instance_ptr, |report| {
        report.starting_stocks = Some(stocks);
    });
}

/// Sets the `starting_percent` on the player report at the address of `instance_ptr`.
#[no_mangle]
pub extern "C" fn slprs_player_report_set_starting_percent(instance_ptr: usize, percent: i64) {
    set::<PlayerReport, _>(instance_ptr, |report| {
        report.starting_percent = Some(percent);
    });
}

