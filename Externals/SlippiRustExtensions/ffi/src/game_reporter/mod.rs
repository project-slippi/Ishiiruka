use std::ffi::{c_char, CStr};

use slippi_game_reporter::{GameReport, SlippiGameReporter};

pub mod game_report;
pub mod player_report;

/// Initializes a new SlippiGameReporter and leaks it, returning the instance
/// pointer after doing so.
#[no_mangle]
pub extern "C" fn slprs_game_reporter_create(
    uid: *const c_char,
    play_key: *const c_char,
    iso_path: *const c_char
) -> usize {
    let fn_label = "slprs_game_reporter_create";

    let uid = unpack_str(uid, fn_label, "uid"); 
    let play_key = unpack_str(play_key, fn_label, "play_key");
    let iso_path = unpack_str(iso_path, fn_label, "iso_path");

    let reporter = Box::new(SlippiGameReporter::new(uid, play_key, iso_path));
    let reporter_instance_ptr = Box::into_raw(reporter) as usize;
    reporter_instance_ptr
}

/// Moves ownership of the `GameReport` at the specified address to the
/// `SlippiGameReporter` at the corresponding address.
///
/// The reporter will manage the actual... reporting.
#[no_mangle]
pub extern "C" fn slprs_game_reporter_start_report(
    instance_ptr: usize,
    game_report_instance_ptr: usize
) {
    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `game_report_instance_ptr` is only owned
    // by us, and is created/destroyed with the corresponding lifetimes.
    let game_report = unsafe {
        Box::from_raw(game_report_instance_ptr as *mut GameReport)
    };

    set::<SlippiGameReporter, _>(instance_ptr, move |reporter| {
        reporter.start_report(*game_report);
    });
}

/// A small helper method for moving in and out of our known types.
fn set<T, F>(instance_ptr: usize, handler: F)
where
    F: FnOnce(&mut T),
{
    // This entire method could possibly be a macro but I'm way too tired
    // to deal with that syntax right now.

    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `instance_ptr` is only owned
    // by us, and is created/destroyed with the corresponding lifetimes.
    let mut instance = unsafe { Box::from_raw(instance_ptr as *mut T) };

    handler(&mut instance);

    // Fall back into a raw pointer so Rust doesn't obliterate the object.
    let _leak = Box::into_raw(instance);
}

/// A helper function for converting c str types to Rust ones with
/// some optional args for aiding in debugging should this ever be a problem.
///
/// This will panic if the strings being passed over cannot be converted, as
/// we need the game reporter to be able to run without question.
fn unpack_str(string: *const c_char, fn_label: &str, err_label: &str) -> String {
    // This is theoretically safe as we control the strings being passed from 
    // the C++ side, and can mostly guarantee that we know what we're getting.
    //
    // As more things get converted to pure Rust, this will probably go away.
    let slice = unsafe { CStr::from_ptr(string) };

    // What we *can't* guarantee is that it's proper UTF-8 etc. 
    //
    // If we can't parse it into a Rust String, then we'll go ahead and dump
    // some logs and then just... panic.
    //
    // The code path that may encounter this has never proven to be an issue, but
    // we realistically need the reporter to run without question and bailing out
    // may be the best move here.
    //
    // @TODO: Can we even safely panic into the C++ side...? Need to research
    // best approach here.
    match slice.to_str() {
        Ok(path) => path.to_string(),

        Err(e) => {
            tracing::error!(
                error = ?e,
                "[{}] Failed to bridge {}, will panic",
                fn_label,
                err_label
            );

            panic!("Unable to bridge necessary type, panicing");
        },
    }
}
