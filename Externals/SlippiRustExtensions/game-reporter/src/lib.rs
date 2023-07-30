use std::collections::{BTreeMap, VecDeque};
use std::sync::{Arc, Mutex};
use std::thread;

use serde_json::json;
use ureq::{Agent, AgentBuilder};

const REPORT_URL: &str = "https://rankings-dot-slippi.uc.r.appspot.com/report";
const ABANDON_URL: &str = "https://rankings-dot-slippi.uc.r.appspot.com/abandon";
const COMPLETE_URL: &str = "https://rankings-dot-slippi.uc.r.appspot.com/complete";

const KNOWN_DESYNC_ISOS: [&'static str; 4] = [
    "23d6baef06bd65989585096915da20f2",
    "27a5668769a54cd3515af47b8d9982f3",
    "5805fa9f1407aedc8804d0472346fc5f",
    "9bb3e275e77bb1a160276f2330f93931"
];

#[derive(Debug, Default)]
pub struct PlayerReport {
    pub uid: Option<String>,
    pub slot_type: Option<u8>,
    pub damage_done: Option<f64>,
    pub stocks_remaining: Option<u8>,
    pub character_id: Option<u8>,
    pub color_id: Option<u8>,
    pub starting_stocks: Option<i64>,
    pub starting_percent: Option<i64>
}

#[derive(Debug)]
#[repr(C)]
pub enum OnlinePlayMode {
    Ranked = 0,
    Unranked = 1,
    Direct = 2,
    Teams = 3
}

#[derive(Debug, Default)]
pub struct GameReport {
    pub online_mode: Option<OnlinePlayMode>,
    pub match_id: Option<String>,
    pub report_attempts: Option<i32>,
    pub duration_frames: Option<u32>,
    pub game_index: Option<u32>,
    pub tie_break_index: Option<u32>,
    pub winner_index: Option<i8>,
    pub game_end_method: Option<u8>,
    pub lras_initiator: Option<i8>,
    pub stage_id: Option<i32>,
    pub players: Vec<PlayerReport>
}

#[derive(Debug)]
struct UserInfo {
    uid: String,
    play_key: String
}

/// A GameReporter etc.
#[derive(Debug)]
pub struct SlippiGameReporter {
    http_client: Agent,
    user_info: Arc<UserInfo>,
    iso_hash: Arc<Mutex<String>>,
    reporting_thread: Option<thread::JoinHandle<()>>,
    md5_thread: Option<thread::JoinHandle<()>>,
    player_uids: Vec<String>,
    report_queue: VecDeque<GameReport>,
    replay_data: BTreeMap<i64, Vec<u8>>,
    replay_write_index: i64,
    replay_last_completed_index: i64
}

impl SlippiGameReporter {
    /// Initializes and returns new game reporter.
    pub fn new(uid: String, play_key: String, iso_path: String) -> Self {
        let http_client = AgentBuilder::new()
            .build();

        let user_info = Arc::new(UserInfo {
            uid,
            play_key
        });

        let reporter_user_info = user_info.clone();
        let reporter_http_client = http_client.clone();

        let reporting_thread = thread::Builder::new()
            .name("SlippiGameReporter".into())
            .spawn(move || {
                handle_reports(reporter_user_info, reporter_http_client);
            })
            .expect("Ruh roh");
        
        let iso_hash = Arc::new(Mutex::new(String::new()));
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
            report_queue: VecDeque::new(),
            replay_data: BTreeMap::new(),
            replay_write_index: 0,
            replay_last_completed_index: -1
        }
    }

    /// Sends a new report to the thread that handles persisting them to the server.
    pub fn start_report(&mut self, report: GameReport) {
        self.report_queue.push_front(report);
    }

    /// Currently unused.
    pub fn start_new_session(&mut self) {
        // Maybe we could do stuff here? We used to initialize gameIndex but 
        // that isn't required anymore
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
            eprintln!("[GameReport] Got error executing abandonment request: {:?}", e);
        }
    }

    /// Report a completed match.
    pub fn report_completion(&mut self, match_id: String, end_mode: u8) {
        let res = self.http_client.post(COMPLETE_URL)
            .send_json(json!({
                "matchId": match_id,
                "uid": self.user_info.uid,
                "playKey": self.user_info.play_key,
                "endMode": end_mode
            }));

        if let Err(e) = res {
            eprintln!("[GameReport] Got error executing completion request: {:?}", e);
        }
    }

    pub fn push_replay_data(&mut self, data: *const u8, length: u32, action: String) {

    }

    pub fn upload_replay_data(&mut self, index: i32, url: String) {

    }
}

impl Drop for SlippiGameReporter {
    fn drop(&mut self) {

    }
}

fn handle_reports(user_info: Arc<UserInfo>, http_client: Agent) {
    loop {
        println!("Duh");
        std::thread::sleep_ms(5000);
    }
}

/// Computes an MD5 hash of the ISO at `iso_path` and writes it back to the value
/// behind `iso_hash`.
fn run_md5(iso_hash: Arc<Mutex<String>>, iso_path: String) {
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
    let mut lock = iso_hash.lock().expect("This should not fail");
    *lock = hash;
}
