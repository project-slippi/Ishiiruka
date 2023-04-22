mod scenes;
mod tracks;
mod utils;

use anyhow::{Context, Result};
use bus::Bus;
use directories::BaseDirs;
use dolphin_logger::Log;
use hps_decode::{hps::Hps, pcm_iterator::PcmIterator};
use process_memory::LocalMember;
use process_memory::Memory;
use rodio::{OutputStream, Sink};
use scenes::scene_ids::*;
use std::convert::TryInto;
use std::error::Error;
use std::ffi::{c_short, c_uint};
use std::io::prelude::*;
use std::ops::ControlFlow::{self, Break, Continue};
use std::sync::mpsc;
use std::{thread::sleep, time::Duration};
use tracks::TrackId;

/// This handler definition represents a passed-in function for pushing audio samples
/// into the current Dolphin SoundStream interface.
pub type ForeignSetSampleRateFn = unsafe extern "C" fn(rate: u32);
pub type ForeignSetVolumeFn = unsafe extern "C" fn(left_volume: u32, right_volume: u32);
pub type ForeignPushSamplesFn = unsafe extern "C" fn(samples: *const c_short, num_samples: c_uint);

#[derive(Debug)]
pub struct Jukebox {
    bus: Bus<JukeboxEvent>,
}

const THREAD_LOOP_SLEEP_TIME_MS: u64 = 30;

#[derive(Debug, PartialEq)]
struct DolphinState {
    in_game: bool,
    in_menus: bool,
    scene_major: u8,
    scene_minor: u8,
    stage_id: u8,
    volume: f32,
    is_paused: bool,
    match_info: u8,
}

impl Default for DolphinState {
    fn default() -> Self {
        Self {
            in_game: false,
            in_menus: false,
            scene_major: SCENE_MAIN_MENU,
            scene_minor: 0,
            stage_id: 0,
            volume: 0.0,
            is_paused: false,
            match_info: 0,
        }
    }
}

#[derive(Debug)]
enum MeleeEvent {
    TitleScreenEntered,
    MenuEntered,
    LotteryEntered,
    GameStart(u8), // stage id
    GameEnd,
    RankedStageStrikeEntered,
    VsOnlineOpponent,
    Pause,
    Unpause,
    SetVolume(f32),
    NoOp,
}

#[derive(Debug, Clone)]
enum JukeboxEvent {
    Dropped,
}

impl Jukebox {
    /// Returns a new configured Jukebox, ready to play.
    pub fn new(
        m_p_ram: *const u8,
        set_sample_rate_fn: ForeignSetSampleRateFn,
        set_volume_fn: ForeignSetVolumeFn,
        push_samples_fn: ForeignPushSamplesFn,
    ) -> Result<Self, Box<dyn Error>> {
        let m_p_ram = m_p_ram as usize;

        tracing::info!(target: Log::Jukebox, m_p_ram, "Initializing Slippi Jukebox");

        let base_dirs = BaseDirs::new().context(
            "Home directory path could not be retrieved from the OS. Unable to locate Slippi.",
        )?;

        let iso_path = utils::get_iso_path(&base_dirs)?;
        let dolphin_volume_percent = utils::get_dolphin_volume(&base_dirs);

        let mut iso = std::fs::File::open(iso_path)?;

        tracing::info!(target: Log::Jukebox, "Scanning disc for tracks...");
        let tracks = utils::create_track_map(&mut iso)?;
        tracing::info!(target: Log::Jukebox, "Loaded {} tracks!", tracks.len());

        // This channel is used for the memory-reading thread to communicate with
        // the music-playing thread
        let (tx_melee, rx_melee) = mpsc::channel::<MeleeEvent>();

        // The jukebox bus will be used to notify child threads when the object is
        // about to be dropped
        let mut jukebox_event_bus: Bus<JukeboxEvent> = Bus::new(1);
        let (mut rx_jukebox_1, mut rx_jukebox_2) =
            (jukebox_event_bus.add_rx(), jukebox_event_bus.add_rx());

        // This thread hooks into Dolphin's memory and listens for relevant
        // events
        std::thread::spawn(move || -> Result<()> {
            // Initial state that will get updated over time
            let mut prev_state = DolphinState {
                volume: dolphin_volume_percent,
                ..Default::default()
            };

            loop {
                // Break loop if jukebox has been dropped
                if let Ok(event) = rx_jukebox_1.try_recv() {
                    if matches!(event, JukeboxEvent::Dropped) {
                        break;
                    }
                }

                // Continuously check if the dolphin state has changed
                let state = read_dolphin_state(&m_p_ram, dolphin_volume_percent);

                // If the state has changed,
                if prev_state != state {
                    // send an event to the music player thread
                    let event = produce_melee_event(&prev_state, &state);
                    tracing::info!(target: Log::Jukebox, "{:?}", event);

                    tx_melee.send(event)?;
                    prev_state = state;
                }
                sleep(Duration::from_millis(THREAD_LOOP_SLEEP_TIME_MS));
            }

            Ok(())
        });

        // This thread handles events sent by the dolphin-hooked thread and
        // manages audio playback
        std::thread::spawn(move || -> Result<()> {
            // let (_stream, stream_handle) = OutputStream::try_default()?; // TODO: Stop the program if we can't get a handle to the audio device
            // let sink = Sink::try_new(&stream_handle)?;

            // These values will get updated by the `handle_event` fn
            let mut track_id: Option<TrackId> = None;
            let mut volume = dolphin_volume_percent;

            'outer: loop {
                if let Some(track_id) = track_id {
                    // Lookup the current track_id in the `tracks` hashmap, and
                    // if it's present play it. If not, there will be silence
                    // until a new track_id is set
                    let track = tracks.get(&track_id);
                    if let Some(&(offset, size)) = track {
                        // Seek the location of the track on the ISO
                        iso.seek(std::io::SeekFrom::Start(offset as u64))?;
                        let mut bytes = vec![0; size];
                        iso.read_exact(&mut bytes)?;

                        // Parse data from the ISO into samples
                        let hps: Hps = bytes.try_into()?;
                        let sample_rate = hps.sample_rate;
                        let pcm: PcmIterator = hps.into();

                        // Take two seconds of samples from the PCM stream
                        let mut samples = pcm.take(128_000).collect::<Vec<_>>();
                        samples.shrink_to_fit();
                        // Sanity check
                        tracing::info!(
                            target: Log::Jukebox,
                            "sample rate: {}, samples: {:?}",
                            sample_rate,
                            samples.iter().skip(64_000).take(20).collect::<Vec<_>>()
                        );
                        // Send the samples to Dolphin with `sampler_fn`
                        let len = samples.len() as u32;
                        let ptr = samples.as_ptr();
                        std::mem::forget(samples);
                        unsafe {
                            set_sample_rate_fn(sample_rate);
                            set_volume_fn(u32::MAX / 2, u32::MAX / 2);
                            push_samples_fn(ptr, len);
                        }

                        // Play song
                        // sink.append(audio_source);
                        // sink.play();
                        // sink.set_volume(volume);
                    }
                }

                // Continue to play the song indefinitely while regularly checking
                // for new events from the thread that's hooked into dolphin memory
                loop {
                    // Break loop if jukebox has been dropped
                    if let Ok(event) = rx_jukebox_2.try_recv() {
                        if matches!(event, JukeboxEvent::Dropped) {
                            break 'outer;
                        }
                    }

                    if let Ok(event) = rx_melee.try_recv() {
                        // When we receive an event, handle it. This can include
                        // changing the volume, updating the track and breaking
                        // the loop such that the next track starts to play,
                        // etc.
                        match handle_melee_event(event, /*&sink,*/ &mut track_id, &mut volume) {
                            Break(_) => break,
                            _ => (),
                        }
                    }
                    sleep(Duration::from_millis(THREAD_LOOP_SLEEP_TIME_MS));
                }

                // sink.stop();
            }

            Ok(())
        });

        Ok(Self {
            bus: jukebox_event_bus,
        })
    }
}

impl Drop for Jukebox {
    fn drop(&mut self) {
        tracing::info!(target: Log::Jukebox, "Dropping Slippi Jukebox");
        self.bus.broadcast(JukeboxEvent::Dropped);
    }
}

/// Given the previous dolphin state and current dolphin state, produce an event
fn produce_melee_event(prev_state: &DolphinState, state: &DolphinState) -> MeleeEvent {
    let vs_screen_1 = state.scene_major == SCENE_VS_ONLINE
        && prev_state.scene_minor != SCENE_VS_ONLINE_VERSUS
        && state.scene_minor == SCENE_VS_ONLINE_VERSUS;
    let vs_screen_2 = prev_state.scene_minor == SCENE_VS_ONLINE_VERSUS && state.stage_id == 0;
    let entered_vs_online_opponent_screen = vs_screen_1 || vs_screen_2;

    if state.scene_major == SCENE_VS_ONLINE
        && prev_state.scene_minor != SCENE_VS_ONLINE_RANKED
        && state.scene_minor == SCENE_VS_ONLINE_RANKED
    {
        MeleeEvent::RankedStageStrikeEntered
    } else if !prev_state.in_menus && state.in_menus {
        MeleeEvent::MenuEntered
    } else if prev_state.scene_major != SCENE_TITLE_SCREEN
        && state.scene_major == SCENE_TITLE_SCREEN
    {
        MeleeEvent::TitleScreenEntered
    } else if entered_vs_online_opponent_screen {
        MeleeEvent::VsOnlineOpponent
    } else if prev_state.scene_major != SCENE_TROPHY_LOTTERY
        && state.scene_major == SCENE_TROPHY_LOTTERY
    {
        MeleeEvent::LotteryEntered
    } else if (!prev_state.in_game && state.in_game) || prev_state.stage_id != state.stage_id {
        MeleeEvent::GameStart(state.stage_id)
    } else if prev_state.in_game && state.in_game && state.match_info == 1 {
        MeleeEvent::GameEnd
    } else if prev_state.volume != state.volume {
        MeleeEvent::SetVolume(state.volume)
    } else if !prev_state.is_paused && state.is_paused {
        MeleeEvent::Pause
    } else if prev_state.is_paused && !state.is_paused {
        MeleeEvent::Unpause
    } else {
        MeleeEvent::NoOp
    }
}

/// Handle a events received in the audio playback thread, by changing tracks,
/// adjusting volume etc.
fn handle_melee_event(
    event: MeleeEvent,
    // sink: &Sink,
    track_id: &mut Option<TrackId>,
    volume: &mut f32,
) -> ControlFlow<()> {
    use self::MeleeEvent::*;

    // TODO:
    // - Intro movie
    //
    // - classic vs screen
    // - classic victory screen
    // - classic game over screen
    // - classic credits
    // - classic "congratulations movie"
    //
    // - Adventure mode field intro music
    // - Adventure mode mushroom kingdom
    // - Adventure mode great maze
    // - Adventure mode brinstar escape
    //
    // - All Star Rest Area

    match event {
        TitleScreenEntered | GameEnd => {
            *track_id = None;
        }
        MenuEntered => {
            *track_id = Some(*tracks::MENU_TRACK);
        }
        LotteryEntered => {
            *track_id = Some(tracks::TrackId::Lottery);
        }
        VsOnlineOpponent => {
            *track_id = Some(tracks::TrackId::VsOpponent);
        }
        RankedStageStrikeEntered => {
            *track_id = Some(*tracks::TOURNAMENT_MODE_TRACK);
        }
        GameStart(stage_id) => {
            *track_id = tracks::get_stage_track_id(stage_id);
        }
        Pause => {
            // sink.set_volume(*volume * 0.2);
            return Continue(());
        }
        Unpause => {
            // sink.set_volume(*volume);
            return Continue(());
        }
        SetVolume(received_volume) => {
            // sink.set_volume(received_volume);
            *volume = received_volume;
            return Continue(());
        }
        NoOp => {
            return Continue(());
        }
    };

    Break(())
}

/// Create a `DolphinState` by reading Dolphin's memory
fn read_dolphin_state(m_p_ram: &usize, dolphin_volume_percent: f32) -> DolphinState {
    // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L8
    // let volume = dolphin.read_i8(0x8045C384, None)?;
    let volume: LocalMember<i8> = LocalMember::new_offset(vec![m_p_ram + 0x45C384]);
    let volume = unsafe { volume.read().unwrap() };
    let melee_volume_percent: f32 = ((volume as f32 - 100.0) * -1.0) / 100.0;
    // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L16
    let scene_major: LocalMember<u8> = LocalMember::new_offset(vec![m_p_ram + 0x479D30]);
    let scene_major = unsafe { scene_major.read().unwrap() };
    // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L19
    let scene_minor: LocalMember<u8> = LocalMember::new_offset(vec![m_p_ram + 0x479D33]);
    let scene_minor = unsafe { scene_minor.read().unwrap() };
    // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L357
    let stage_id: LocalMember<u8> = LocalMember::new_offset(vec![m_p_ram + 0x49E753]);
    let stage_id = unsafe { stage_id.read().unwrap() };
    // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L248
    // 0 = in game, 1 = GAME! screen, 2 = Stage clear in 1p mode? (maybe also victory screen), 3 = menu
    let match_info: LocalMember<u8> = LocalMember::new_offset(vec![m_p_ram + 0x46B6A0]);
    let match_info = unsafe { match_info.read().unwrap() };
    // https://github.com/bkacjios/m-overlay/blob/d8c629d/source/modules/games/GALE01-2.lua#L353
    let is_paused: LocalMember<u8> = LocalMember::new_offset(vec![m_p_ram + 0x4D640F]);
    let is_paused = unsafe { is_paused.read().unwrap() } == 1;

    DolphinState {
        in_game: utils::is_in_game(scene_major, scene_minor),
        in_menus: utils::is_in_menus(scene_major, scene_minor),
        scene_major,
        scene_minor,
        volume: dolphin_volume_percent * melee_volume_percent,
        stage_id,
        is_paused,
        match_info,
    }
}

// This wrapper allows us to implement `rodio::Source`
struct HpsAudioSource {
    pcm: PcmIterator,
    padding_length: u32,
}

impl Iterator for HpsAudioSource {
    type Item = i16;

    fn next(&mut self) -> Option<Self::Item> {
        // We need to pad the start of the music playback with a quarter second
        // of silence so when two tracks are loaded in quick succession, we
        // don't hear a quick "blip" from the first track. This happens in
        // practice because scene_minor tells us we're in-game before stage_id
        // has a chance to update from the previously played stage.
        //
        // Return 0s (silence) for the length of the padding
        if self.padding_length > 0 {
            self.padding_length -= 1;
            return Some(0);
        }
        // Then start iterating on the actual samples
        self.pcm.next()
    }
}

impl rodio::Source for HpsAudioSource {
    fn current_frame_len(&self) -> Option<usize> {
        None
    }
    fn channels(&self) -> u16 {
        self.pcm.channel_count as u16
    }
    fn sample_rate(&self) -> u32 {
        self.pcm.sample_rate
    }
    fn total_duration(&self) -> Option<std::time::Duration> {
        None
    }
}
