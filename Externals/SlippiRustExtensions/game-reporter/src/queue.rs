use std::collections::VecDeque;
use std::sync::mpsc::Receiver;
use std::sync::{Arc, Mutex, OnceLock};
use std::thread;
use std::time::Duration;

use serde_json::json;

use dolphin_logger::Log;

use crate::types::{GameReport, GameReportRequestPayload};
use crate::ProcessingEvent;

/// Trimmed-down user information - if more things move in to the Rust side,
/// this will likely move and/or get expanded.
#[derive(Debug)]
pub struct UserInfo {
    pub uid: String,
    pub play_key: String,
}

const REPORT_URL: &str = "https://rankings-dot-slippi.uc.r.appspot.com/report";
const ABANDON_URL: &str = "https://rankings-dot-slippi.uc.r.appspot.com/abandon";
const COMPLETE_URL: &str = "https://rankings-dot-slippi.uc.r.appspot.com/complete";

/// How many times a report should attempt to send.
const MAX_REPORT_ATTEMPTS: i32 = 5;

/// Expected response payload when saving a report to the server.
#[derive(Debug, serde::Deserialize)]
struct ReportResponse {
    success: bool,
    upload_url: Option<String>,
}

/// An "inner" struct that holds shared points of data that we need to
/// access from multiple threads in this module.
///
/// By storing this separately, it both somewhat mimics how the original
/// C++ class was set up and makes life easier in terms of passing pieces
/// of data around various threads.
#[derive(Clone, Debug)]
pub struct GameReporterQueue {
    pub http_client: ureq::Agent,
    pub user: Arc<UserInfo>,
    pub iso_hash: Arc<OnceLock<String>>,
    inner: Arc<Mutex<VecDeque<GameReport>>>,
}

impl GameReporterQueue {
    /// Initializes and returns a new game reporter.
    pub(crate) fn new(uid: String, play_key: String) -> Self {
        let http_client = ureq::AgentBuilder::new()
            .https_only(true)
            .max_idle_connections(5)
            .user_agent("SlippiGameReporterContext/Rust v0.1")
            .build();

        Self {
            http_client,

            user: Arc::new(UserInfo { uid, play_key }),

            iso_hash: Arc::new(OnceLock::new()),
            inner: Arc::new(Mutex::new(VecDeque::new())),
        }
    }

    /// Adds a new report to the back of the queue.
    ///
    /// (The processing thread pulls from the front)
    pub(crate) fn add_report(&self, report: GameReport) {
        let mut lock = self.inner.lock().expect("Ruh roh");
        (*lock).push_back(report);
    }

    /// Report a completed match.
    ///
    /// This doesn't necessarily need to be here, but it's easier to grok the codebase
    /// if we keep all reporting network calls in one module.
    pub fn report_completion(&self, match_id: String, end_mode: u8) {
        let res = self.http_client.post(COMPLETE_URL).send_json(json!({
            "matchId": match_id,
            "uid": self.user.uid,
            "playKey": self.user.play_key,
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
    ///
    /// This doesn't necessarily need to be here, but it's easier to grok the codebase
    /// if we keep all reporting network calls in one module.
    pub fn report_abandonment(&self, match_id: String) {
        let res = self.http_client.post(ABANDON_URL).send_json(json!({
            "matchId": match_id,
            "uid": self.user.uid,
            "playKey": self.user.play_key
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

/// The main loop that processes reports.
pub(crate) fn run(reporter: GameReporterQueue, receiver: Receiver<ProcessingEvent>) {
    loop {
        // Watch for notification to do work
        match receiver.recv() {
            Ok(ProcessingEvent::ReportAvailable) => {
                process_reports(&reporter, ProcessingEvent::ReportAvailable);
            },

            Ok(ProcessingEvent::Shutdown) => {
                tracing::info!(target: Log::GameReporter, "Processing thread winding down");

                process_reports(&reporter, ProcessingEvent::Shutdown);

                break;
            },

            // This should realistically never happen, since it means the Sender
            // that's held a level up has been dropped entirely - but we'll log
            // for the hell of it in case anyone's tweaking the logic.
            Err(e) => {
                tracing::error!(
                    target: Log::GameReporter,
                    error = ?e,
                    "Failed to receive ProcessingEvent, thread will exit"
                );

                break;
            },
        }
    }
}

/// The true inner error, minus any metadata.
#[derive(Debug)]
enum ReportSendErrorKind {
    Net(ureq::Error),
    IO(std::io::Error),
    NotSuccessful,
}

/// Wraps errors that can occur during report sending.
#[derive(Debug)]
struct ReportSendError {
    is_last_attempt: bool,
    sleep_ms: Duration,
    kind: ReportSendErrorKind,
}

/// Process jobs from the queue.
fn process_reports(queue: &GameReporterQueue, event: ProcessingEvent) {
    // This `.expect()` should realistically never trigger, as we'd need
    // some real odd cases to occur (can't read the file, etc) which are
    // unlikely to happen given the nature of the application itself.
    let iso_hash = queue.iso_hash.get().expect("ISO md5 hash is somehow missing");

    let mut report_queue = queue.inner.lock().expect("If this fails we have bigger issues");

    // Process all reports currently in the queue.
    while !report_queue.is_empty() {
        // We only want to pop if we're successful in sending or if we encounter an error
        // (e.g, max attempts). We pass the locked queue over to work with the borrow checker
        // here, since otherwise we can't pop without some ugly block work to coerce letting
        // a mutable borrow drop.
        match try_send_next_report(&mut *report_queue, event, &queue.http_client, &queue.user, &iso_hash) {
            Ok(upload_url) => {
                // Pop the front of the queue. If we have a URL, chuck it all over
                // to the replay uploader.
                let report = report_queue.pop_front();

                if let (Some(report), Some(upload_url)) = (report, upload_url) {
                    try_upload_replay_data(report.replay_data, upload_url);
                }

                thread::sleep(Duration::ZERO)
            },

            Err(e) => {
                tracing::error!(
                    target: Log::GameReporter,
                    error = ?e.kind,
                    "Failed to send report"
                );

                if e.is_last_attempt {
                    tracing::error!(target: Log::GameReporter, "Hit max retry limit, dropping report");
                    let _ = report_queue.pop_front();
                }

                thread::sleep(e.sleep_ms)
            },
        }
    }
}

/// Builds a request payload and sends it.
///
/// If this is successful, it yields back an upload URL endpoint. This can be
/// passed to the upload call for processing.
fn try_send_next_report(
    queue: &mut VecDeque<GameReport>,
    event: ProcessingEvent,
    http_client: &ureq::Agent,
    user: &UserInfo,
    iso_hash: &str,
) -> Result<Option<String>, ReportSendError> {
    let report = (*queue)
        .front_mut()
        .expect("We checked if it's empty, this should never fail");

    report.attempts += 1;

    // If we're shutting the thread down, limit max attempts to just 1.
    let max_attempts = match event {
        ProcessingEvent::Shutdown => 1,
        _ => MAX_REPORT_ATTEMPTS,
    };

    let is_last_attempt = report.attempts >= max_attempts;

    let payload = GameReportRequestPayload::with(&user.uid, &user.play_key, iso_hash, &report);

    let error_sleep_ms = match is_last_attempt {
        true => Duration::ZERO,
        false => Duration::from_millis((report.attempts as u64) * 100),
    };

    let response: ReportResponse = http_client
        .post(REPORT_URL)
        .send_json(payload)
        .map_err(|e| ReportSendError {
            is_last_attempt,
            sleep_ms: error_sleep_ms,
            kind: ReportSendErrorKind::Net(e),
        })?
        .into_json()
        .map_err(|e| ReportSendError {
            is_last_attempt,
            sleep_ms: error_sleep_ms,
            kind: ReportSendErrorKind::IO(e),
        })?;

    if !response.success {
        return Err(ReportSendError {
            is_last_attempt,
            sleep_ms: error_sleep_ms,
            kind: ReportSendErrorKind::NotSuccessful,
        });
    }

    Ok(response.upload_url)
}

/// Attempts to compress and upload replay data to the url at `upload_url`.
fn try_upload_replay_data(data: Vec<u8>, upload_url: String) {
    //X-Goog-Content-Length-Range: 0,10000000
}
