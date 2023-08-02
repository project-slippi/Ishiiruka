use std::collections::VecDeque;
use std::sync::{Arc, OnceLock, Mutex};
use std::thread;

use serde_json::json;
use ureq::{Agent, AgentBuilder};

use dolphin_logger::Log;

const REPORT_URL: &str = "https://rankings-dot-slippi.uc.r.appspot.com/report";
const ABANDON_URL: &str = "https://rankings-dot-slippi.uc.r.appspot.com/abandon";
const COMPLETE_URL: &str = "https://rankings-dot-slippi.uc.r.appspot.com/complete";

/// ISO hashes that are known to cause problems. We alert the player
/// if we detect that they're running one.
const KNOWN_DESYNC_ISOS: [&'static str; 4] = [
    "23d6baef06bd65989585096915da20f2",
    "27a5668769a54cd3515af47b8d9982f3",
    "5805fa9f1407aedc8804d0472346fc5f",
    "9bb3e275e77bb1a160276f2330f93931",
];

/// The different modes that a player could be in.
#[derive(Debug)]
pub enum OnlinePlayMode {
    Ranked = 0,
    Unranked = 1,
    Direct = 2,
    Teams = 3,
}

/// Game metadata payload that we log to the server.
#[derive(Debug)]
pub struct GameReport {
    pub online_mode: OnlinePlayMode,
    pub match_id: String,
    pub report_attempts: Option<i32>,
    pub duration_frames: u32,
    pub game_index: u32,
    pub tie_break_index: u32,
    pub winner_index: i8,
    pub game_end_method: u8,
    pub lras_initiator: i8,
    pub stage_id: i32,
    pub players: Vec<PlayerReport>,
}

/// Player metadata payload that's logged with game info.
#[derive(Debug)]
pub struct PlayerReport {
    pub uid: String,
    pub slot_type: u8,
    pub damage_done: f64,
    pub stocks_remaining: u8,
    pub character_id: u8,
    pub color_id: u8,
    pub starting_stocks: i64,
    pub starting_percent: i64,
}

/// Trimmed-down user information - if more things move in to the Rust side,
/// this will likely move and/or get expanded.
#[derive(Debug)]
struct UserInfo {
    uid: String,
    play_key: String,
}

/// Different actions that we branch on when replay data is pushed
/// to a `SlippiGameReporter`.
#[derive(Debug)]
pub enum ReplayDataAction {
    /// Just push the data, nothing special.
    Blank,

    /// Start writing a new Replay.
    Create,

    /// Finish the current Replay.
    Close,
}

/// Tracks replay data that gets passed over during gameplay.
///
/// This holds replay data in an increasing stack of entries. If
/// `ReplayDataAction` is passed to `push`, then this will add a new
/// entry in the underlying data stack and begin writing data there.
#[derive(Debug)]
struct ReplayData {
    // This was an `std::map<int, std::vector<u8>>` in C++ and I'm unsure why.
    // @Fizzi or @Nikki - is there some nuance I'm missing? Maybe avoiding something
    // expensive during cleanup...?
    pub data: Vec<Vec<u8>>,
    pub write_index: usize,
    pub last_completed_index: Option<usize>
}

impl ReplayData {
    /// Creates a new `ReplayData` instance.
    pub fn new() -> Self {
        Self {
            data: vec![Vec::new()],
            write_index: 0,
            last_completed_index: None
        }
    }

    /// Copies data from the provided slice into the proper bucket.
    pub fn push(&mut self, data: &[u8], action: ReplayDataAction) {
        if let ReplayDataAction::Create = action {
            self.write_index += 1;
        }

        self.data[self.write_index].extend_from_slice(data);

        if let ReplayDataAction::Close = action {
            self.last_completed_index = Some(self.write_index);
        }
    }
}

/// Implements multi-threaded queues and handlers for saving game reports 
/// and replays.
#[derive(Debug)]
pub struct SlippiGameReporter {
    http_client: Agent,
    user_info: Arc<UserInfo>,
    iso_hash: Arc<OnceLock<String>>,
    reporting_thread: Option<thread::JoinHandle<()>>,
    md5_thread: Option<thread::JoinHandle<()>>,
    player_uids: Vec<String>,
    report_queue: Arc<Mutex<VecDeque<GameReport>>>,
    replay_data: ReplayData
}

impl SlippiGameReporter {
    /// Initializes and returns a new game reporter.
    ///
    /// This spawns a few background threads to handle things like report and
    /// upload processing, along with checking for troublesome ISOs.
    pub fn new(uid: String, play_key: String, iso_path: String) -> Self {
        let http_client = AgentBuilder::new()
            .https_only(true)
            .max_idle_connections(5)
            .user_agent("SlippiGameReporter/Rust v0.1")
            .build();

        let user_info = Arc::new(UserInfo { uid, play_key });

        let reporter_user_info = user_info.clone();
        let reporter_http_client = http_client.clone();

        let reporting_thread = thread::Builder::new()
            .name("SlippiGameReporter".into())
            .spawn(move || {
                // Temporary appeasement of the compiler while scaffolding, ignore.
                let mut xxx = VecDeque::new();
                handle_reports(&mut xxx, reporter_user_info, reporter_http_client);
            })
            .expect("Ruh roh");

        let iso_hash = Arc::new(OnceLock::new());
        let md5_hash = iso_hash.clone();

        let md5_thread = thread::Builder::new()
            .name("SlippiGameReporterMD5".into())
            .spawn(move || {
                run_md5(md5_hash, iso_path);
            })
            .expect("Ruh roh");

        Self {
            http_client,
            user_info,
            iso_hash,
            reporting_thread: Some(reporting_thread),
            md5_thread: Some(md5_thread),
            player_uids: Vec::new(),
            report_queue: Arc::new(Mutex::new(VecDeque::new())),
            replay_data: ReplayData::new()
        }
    }

    /// Adds a new report to the queue and notifies the background thread that
    /// there's work to be done.
    pub fn start_report(&mut self, report: GameReport) {
        let mut lock = self.report_queue.lock().expect("Ruh roh");
        (*lock).push_front(report);
        // notify
    }

    /// Currently unused.
    pub fn start_new_session(&mut self) {
        // Maybe we could do stuff here? We used to initialize gameIndex but
        // that isn't required anymore
    }

    /// Logs replay data that's passed to it. The background processing thread
    /// will pick it up when ready.
    pub fn push_replay_data(&mut self, data: &[u8], action: ReplayDataAction) {
        self.replay_data.push(data, action);
    }

    /// Report a completed match.
    pub fn report_completion(&mut self, match_id: String, end_mode: u8) {
        let res = self.http_client.post(COMPLETE_URL).send_json(json!({
            "matchId": match_id,
            "uid": self.user_info.uid,
            "playKey": self.user_info.play_key,
            "endMode": end_mode
        }));

        if let Err(e) = res {
            tracing::error!(
                target: Log::GameReporter,
                error = ?e,
                "Error executing completion request"
            );
        }
    }

    /// Report an abandoned match.
    pub fn report_abandonment(&mut self, match_id: String) {
        let res = self.http_client.post(ABANDON_URL)
            .send_json(json!({
                "matchId": match_id,
                "uid": self.user_info.uid,
                "playKey": self.user_info.play_key
            }));

        if let Err(e) = res {
            tracing::error!(
                target: Log::GameReporter,
                error = ?e,
                "Error executing abandonment request"
            );
        }
    }
}

impl Drop for SlippiGameReporter {
    /// Cleans up background threads and does some last minute cleanup attempts.
    fn drop(&mut self) {}
}

/// The main loop that processes reports.
fn handle_reports(
    queue: &mut VecDeque<GameReport>,
    user_info: Arc<UserInfo>, 
    http_client: Agent
) {
    loop {
        let has_data = queue.len() > 0;

        // Process all reports currently in the queue.
        while let Some(mut report) = queue.pop_front() {
        }

        // If we had data, do any cleanup 

        std::thread::sleep_ms(0);
    }
}

/// Uploads.
fn upload_replay_data(index: i32, url: String) {
    //X-Goog-Content-Length-Range: 0,10000000
}

/// Computes an MD5 hash of the ISO at `iso_path` and writes it back to the value
/// behind `iso_hash`.
fn run_md5(iso_hash: Arc<OnceLock<String>>, iso_path: String) {
    use chksum::prelude::*;

    use std::fs::File;

    let digest = File::open(&iso_path)
        .expect("Dolphin would crash if this was invalid")
        .chksum(HashAlgorithm::MD5)
        .expect("This might be worth handling later");

    let hash = format!("{:x}", digest);

    if KNOWN_DESYNC_ISOS.contains(&hash.as_str()) {
        // This should be an OSD message but that can be handled later
        println!("Desync warning!");
    }

    println!("MD5 Hash: {}", hash);
    iso_hash.set(hash).expect("This should not fail");
}
