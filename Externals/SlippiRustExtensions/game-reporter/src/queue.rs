use std::collections::VecDeque;
use std::sync::mpsc::Receiver;
use std::sync::{Arc, Mutex, OnceLock};
use std::thread;
use std::time::Duration;

use serde_json::{json, Value};

use dolphin_logger::Log;

use crate::types::{GameReport, GameReportRequestPayload};
use crate::ProcessingEvent;

const GRAPHQL_URL: &str = "https://gql-gateway-dev-dot-slippi.uc.r.appspot.com/graphql";

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
    pub iso_hash: Arc<OnceLock<String>>,
    inner: Arc<Mutex<VecDeque<GameReport>>>,
}

impl GameReporterQueue {
    /// Initializes and returns a new game reporter.
    pub(crate) fn new() -> Self {
        // `max_idle_connections` is set to `0` to mimic how the CURL setup in the
        // C++ version was done - i.e, I don't want to introduce connection pooling
        // without Fizzi/Nikki opting in to it.
        let http_client = ureq::AgentBuilder::new()
            //.https_only(true)
            .max_idle_connections(0)
            .user_agent("SlippiGameReporter/Rust v0.1")
            .build();

        Self {
            http_client,
            iso_hash: Arc::new(OnceLock::new()),
            inner: Arc::new(Mutex::new(VecDeque::new())),
        }
    }

    /// Adds a new report to the back of the queue.
    ///
    /// (The processing thread pulls from the front)
    pub(crate) fn add_report(&self, report: GameReport) {
        match self.inner.lock() {
            Ok(mut lock) => {
                (*lock).push_back(report);
            },

            Err(e) => {
                // This should never happen.
                tracing::error!(
                    target: Log::GameReporter,
                    error = ?e,
                    "Unable to lock queue, dropping report"
                );
            },
        }
    }

    /// Report a completed match.
    ///
    /// This doesn't necessarily need to be here, but it's easier to grok the codebase
    /// if we keep all reporting network calls in one module.
    pub fn report_completion(&self, uid: String, play_key: String, match_id: String, end_mode: u8) {
        let mutation = r#"
            mutation ($report: OnlineGameCompleteInput!) {
                completeOnlineGame (report: $report)
            }
        "#;

        let variables = Some(json!({
            "report": {
                "matchId": match_id,
                "fbUid": uid,
                "playKey": play_key,
                "endMode": end_mode,
            }
        }));

        let res = execute_graphql_query(&self.http_client, mutation, variables, Some("completeOnlineGame".to_string()));

        match res {
            Ok(value) if value == "true" => {
                tracing::info!(target: Log::GameReporter, "Successfully executed abandonment request")
            },
            Ok(value) => tracing::error!(target: Log::GameReporter, ?value, "Error executing abandonment request",),
            Err(e) => tracing::error!(
                target: Log::GameReporter,
                error = ?e,
                "Error executing abandonment request"
            ),
        }
    }

    /// Report an abandoned match.
    ///
    /// This doesn't necessarily need to be here, but it's easier to grok the codebase
    /// if we keep all reporting network calls in one module.
    pub fn report_abandonment(&self, uid: String, play_key: String, match_id: String) {
        let mutation = r#"
            mutation ($report: OnlineGameAbandonInput!) {
                abandonOnlineGame (report: $report)
            }
        "#;

        let variables = Some(json!({
            "report": {
                "matchId": match_id,
                "fbUid": uid,
                "playKey": play_key,
            }
        }));

        let res = execute_graphql_query(&self.http_client, mutation, variables, Some("abandonOnlineGame".to_string()));

        match res {
            Ok(value) if value == "true" => {
                tracing::info!(target: Log::GameReporter, "Successfully executed abandonment request")
            },
            Ok(value) => tracing::error!(target: Log::GameReporter, ?value, "Error executing abandonment request",),
            Err(e) => tracing::error!(
                target: Log::GameReporter,
                error = ?e,
                "Error executing abandonment request"
            ),
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

/// Process jobs from the queue.
fn process_reports(queue: &GameReporterQueue, event: ProcessingEvent) {
    // This `.expect()` should realistically never trigger, as we'd need
    // some real odd cases to occur (can't read the file, etc) which are
    // unlikely to happen given the nature of the application itself.
    let iso_hash = queue.iso_hash.get().expect("ISO MD5 hash is somehow missing");

    let mut report_queue = queue.inner.lock().expect("Failed to lock queue for game report processing");

    // Process all reports currently in the queue.
    while !report_queue.is_empty() {
        // We only want to pop if we're successful in sending or if we encounter an error
        // (e.g, max attempts). We pass the locked queue over to work with the borrow checker
        // here, since otherwise we can't pop without some ugly block work to coerce letting
        // a mutable borrow drop.
        match try_send_next_report(&mut *report_queue, event, &queue.http_client, &iso_hash) {
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
                    backoff = ?e.sleep_ms,
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

/// The true inner error, minus any metadata.
#[derive(Debug)]
enum ReportSendErrorKind {
    Net(ureq::Error),
    JSON(serde_json::Error),
    GraphQL(String),
    NotSuccessful(String),
}

fn execute_graphql_query(
    http_client: &ureq::Agent,
    query: &str,
    variables: Option<Value>,
    field: Option<String>,
) -> Result<String, ReportSendErrorKind> {
    // Prepare the GraphQL request payload
    let request_body = match variables {
        Some(vars) => json!({
            "query": query,
            "variables": vars,
        }),
        None => json!({
            "query": query,
        }),
    };

    // Make the GraphQL request
    let response = http_client
        .post(GRAPHQL_URL)
        .send_json(ureq::json!(&request_body))
        .map_err(ReportSendErrorKind::Net)?;

    // Parse the response JSON
    let response_json: Value =
        serde_json::from_str(&response.into_string().unwrap_or_default()).map_err(ReportSendErrorKind::JSON)?;

    // Check for GraphQL errors
    if let Some(errors) = response_json.get("errors") {
        if errors.is_array() && !errors.as_array().unwrap().is_empty() {
            let error_message = serde_json::to_string_pretty(errors).unwrap();
            return Err(ReportSendErrorKind::GraphQL(error_message));
        }
    }

    // Return the data response
    if let Some(data) = response_json.get("data") {
        let result = match field {
            Some(field) => data.get(&field).unwrap_or(data),
            None => data,
        };
        Ok(result.to_string())
    } else {
        Err(ReportSendErrorKind::GraphQL(
            "No 'data' field in the GraphQL response.".to_string(),
        ))
    }
}

/// Wraps errors that can occur during report sending.
#[derive(Debug)]
struct ReportSendError {
    is_last_attempt: bool,
    sleep_ms: Duration,
    kind: ReportSendErrorKind,
}

/// Builds a request payload and sends it.
///
/// If this is successful, it yields back an upload URL endpoint. This can be
/// passed to the upload call for processing.
fn try_send_next_report(
    queue: &mut VecDeque<GameReport>,
    event: ProcessingEvent,
    http_client: &ureq::Agent,
    iso_hash: &str,
) -> Result<Option<String>, ReportSendError> {
    let report = (*queue).front_mut().expect("Reporter queue is empty yet it shouldn't be");

    report.attempts += 1;

    // If we're shutting the thread down, limit max attempts to just 1.
    let max_attempts = match event {
        ProcessingEvent::Shutdown => 1,
        _ => MAX_REPORT_ATTEMPTS,
    };

    let is_last_attempt = report.attempts >= max_attempts;

    let payload = GameReportRequestPayload::with(&report, iso_hash);

    let error_sleep_ms = match is_last_attempt {
        true => Duration::ZERO,
        false => Duration::from_millis((report.attempts as u64) * 100),
    };

    let mutation = r#"
        mutation ($report: OnlineGameReportInput!) {
            reportOnlineGame (report: $report) {
                success
                uploadUrl
            }
        }
    "#;

    let variables = Some(json!({
        "report": payload,
    }));

    // Call execute_graphql_query and get the response body as a String.
    let response_body =
        execute_graphql_query(http_client, mutation, variables, Some("reportOnlineGame".to_string())).map_err(|e| {
            ReportSendError {
                is_last_attempt,
                sleep_ms: error_sleep_ms,
                kind: e,
            }
        })?;

    // tracing::error!(target: Log::GameReporter, "Response {}", response_body);

    // Now, parse the response JSON to get the data you need.
    let response: ReportResponse = serde_json::from_str(&response_body).map_err(|e| ReportSendError {
        is_last_attempt,
        sleep_ms: error_sleep_ms,
        kind: ReportSendErrorKind::JSON(e),
    })?;

    if !response.success {
        return Err(ReportSendError {
            is_last_attempt,
            sleep_ms: error_sleep_ms,
            kind: ReportSendErrorKind::NotSuccessful(response_body.to_string()),
        });
    }

    Ok(response.upload_url)
}

/// Attempts to compress and upload replay data to the url at `upload_url`.
fn try_upload_replay_data(data: Vec<u8>, upload_url: String) {
    //X-Goog-Content-Length-Range: 0,10000000
}
