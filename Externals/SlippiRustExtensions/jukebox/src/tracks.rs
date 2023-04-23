use once_cell::sync::Lazy;
use rand::Rng;

// 25% chance upon initialization to use the
// alternate menu theme for the rest of the session
pub(crate) static MENU_TRACK: Lazy<TrackId> = Lazy::new(|| {
    if rand::thread_rng().gen_range(0..4) == 0 {
        TrackId::Menu2
    } else {
        TrackId::Menu1
    }
});

// 50% chance upon initialization to use the alternate tournament
// mode track for the rest of the session
pub(crate) static TOURNAMENT_MODE_TRACK: Lazy<TrackId> = Lazy::new(|| {
    if rand::thread_rng().gen_range(0..2) == 0 {
        TrackId::TournamentMode1
    } else {
        TrackId::TournamentMode2
    }
});

/// IDs for all the songs that Slippi Jukebox can play. Any track that
/// exists in vanilla can be added
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub(crate) enum TrackId {
    Menu1,
    Menu2,
    Lottery,
    TournamentMode1,
    TournamentMode2,
    VsOpponent,
    PeachsCastle,
    RainbowCruise,
    KongoJungle,
    JungleJapes,
    GreatBay,
    Saria,
    Temple,
    FireEmblem,
    Brinstar,
    BrinstarDepths,
    YoshisStory,
    YoshisIsland,
    SuperMario3,
    FountainOfDreams,
    GreenGreens,
    Corneria,
    Venom,
    PokemonStadium,
    PokemonBattle,
    Pokefloats,
    MuteCity,
    BigBlue,
    MachRider,
    Onett,
    Mother2,
    Fourside,
    IcicleMountain,
    BalloonFighter,
    MushroomKingdom,
    DrMario,
    MushroomKingdomII,
    FlatZone,
    DreamLand64,
    YoshisIsland64,
    KongoJungle64,
    Battlefield,
    MultimanMelee,
    FinalDestination,
    MultimanMelee2,
    BreakTheTargets,
    BrinstarEscape,
    AllStarRestArea,
}

/// The first 8 bytes of the left channel coefficients within an .hps file are
/// used to identify which TrackId is associated
pub(crate) fn identify_coefficients(coefficients: [u8; 8]) -> Option<(TrackId, usize)> {
    use self::TrackId::*;

    // Matches the first bytes of left channel coefficients with (TrackId, .hps file length)
    match coefficients {
        [0x0A, 0x96, 0xFA, 0x86, 0x0E, 0xBE, 0xF8, 0x9A] => Some((Menu1, 2085824)),
        [0x02, 0x90, 0xFE, 0x04, 0x0B, 0x3F, 0xFA, 0x9C] => Some((Menu2, 2222976)),
        [0x04, 0x4C, 0xFF, 0xD8, 0x0B, 0x7D, 0xFA, 0xE3] => Some((Lottery, 2020960)),
        [0x09, 0xED, 0xF9, 0x83, 0x0D, 0x43, 0xF9, 0xC9] => Some((TournamentMode1, 1127808)),
        [0x0A, 0x45, 0xF9, 0x9B, 0x0D, 0x5D, 0xF9, 0xBD] => Some((TournamentMode2, 1127104)),
        [0xFF, 0xF1, 0x07, 0x96, 0x07, 0x10, 0xFB, 0xCC] => Some((VsOpponent, 125120)),
        [0x02, 0xD1, 0xFD, 0x91, 0x09, 0xFF, 0xFB, 0x74] => Some((PeachsCastle, 3290784)),
        [0x02, 0xFD, 0xFD, 0xD2, 0x0B, 0x12, 0xFA, 0xBE] => Some((RainbowCruise, 3308192)),
        [0x00, 0xEA, 0xFD, 0x80, 0x0A, 0x62, 0xFA, 0x44] => Some((KongoJungle, 8006176)),
        [0x05, 0x4E, 0xFD, 0x6A, 0x0C, 0x68, 0xFA, 0x34] => Some((JungleJapes, 7054208)),
        [0x0B, 0x9D, 0xF9, 0xD7, 0x0E, 0xE5, 0xF8, 0x68] => Some((GreatBay, 1735968)),
        [0x07, 0x53, 0xF9, 0x4F, 0x0D, 0xDC, 0xF9, 0x15] => Some((Saria, 1324512)),
        [0x03, 0x1A, 0xFE, 0xC6, 0x0A, 0x9A, 0xFB, 0x12] => Some((Temple, 3795904)),
        [0xFE, 0x0B, 0xFE, 0xA3, 0x09, 0xC5, 0xFB, 0x5D] => Some((FireEmblem, 4518752)),
        [0x04, 0xA9, 0xFE, 0x3F, 0x0C, 0x18, 0xF9, 0xF2] => Some((Brinstar, 6538304)),
        [0x04, 0x0A, 0xFF, 0xA8, 0x0B, 0xC9, 0xFA, 0x25] => Some((BrinstarDepths, 5439904)),
        [0x03, 0xB1, 0x00, 0xE2, 0x0B, 0xAF, 0xFA, 0x75] => Some((YoshisStory, 4347232)),
        [0x0A, 0x45, 0xFB, 0x0D, 0x0E, 0xD2, 0xF8, 0x8F] => Some((YoshisIsland, 1983264)),
        [0x01, 0x3D, 0x00, 0xC6, 0x0A, 0x2B, 0xFA, 0xE4] => Some((SuperMario3, 3508800)),
        [0x02, 0x2A, 0x02, 0x94, 0x0D, 0x18, 0xF9, 0x1D] => Some((FountainOfDreams, 5944864)),
        [0x08, 0x04, 0xFD, 0x1F, 0x0D, 0xF9, 0xF8, 0xC2] => Some((GreenGreens, 3732448)),
        [0x08, 0xE5, 0xFB, 0xFA, 0x0E, 0x59, 0xF8, 0xBF] => Some((Corneria, 2569344)),
        [0x07, 0xDD, 0xFD, 0x6E, 0x0E, 0x03, 0xF8, 0xCA] => Some((Venom, 2303552)),
        [0xFC, 0x40, 0xFD, 0x07, 0x0B, 0xEB, 0xFA, 0x07] => Some((PokemonStadium, 2303488)),
        [0x08, 0xF8, 0xFB, 0x05, 0x0D, 0xF0, 0xF8, 0xE6] => Some((PokemonBattle, 3581408)),
        [0x07, 0x08, 0xFB, 0xCE, 0x0C, 0xED, 0xF9, 0x5C] => Some((Pokefloats, 5514720)),
        [0x05, 0xA5, 0xFD, 0x9B, 0x0B, 0xD9, 0xF9, 0xF4] => Some((MuteCity, 3878784)),
        [0x06, 0xF5, 0xFB, 0x6F, 0x0C, 0xDB, 0xF9, 0xC4] => Some((BigBlue, 4174272)),
        [0x07, 0x87, 0xFC, 0x49, 0x0D, 0x56, 0xF9, 0x0F] => Some((MachRider, 4794944)),
        [0x04, 0x03, 0xFF, 0x4E, 0x0B, 0xBE, 0xFA, 0x76] => Some((Onett, 4216544)),
        [0x06, 0xC2, 0xFD, 0xE1, 0x0D, 0x5D, 0xF9, 0x16] => Some((Mother2, 6802176)),
        [0x04, 0xF1, 0xFD, 0xC4, 0x0C, 0x63, 0xFA, 0x0B] => Some((Fourside, 4326208)),
        [0x03, 0xA3, 0xFE, 0x21, 0x0A, 0xDF, 0xFB, 0x1D] => Some((IcicleMountain, 4033472)),
        [0xFF, 0xDC, 0x06, 0x76, 0x07, 0x71, 0xFD, 0x32] => Some((BalloonFighter, 2138208)),
        [0x02, 0x4B, 0xFF, 0x90, 0x0A, 0x23, 0xFB, 0xB7] => Some((MushroomKingdom, 3287136)),
        [0x02, 0x4D, 0xFD, 0x6D, 0x0A, 0x25, 0xFB, 0x63] => Some((DrMario, 2888736)),
        [0xFF, 0xD7, 0x07, 0x2E, 0x08, 0xCB, 0xFC, 0x9B] => Some((MushroomKingdomII, 1861664)),
        [0x01, 0x6F, 0x01, 0xEB, 0x0B, 0xC6, 0xF9, 0x6A] => Some((FlatZone, 3970656)),
        [0x08, 0xB6, 0xFB, 0x2E, 0x0E, 0x17, 0xF8, 0xEF] => Some((DreamLand64, 2482240)),
        [0x04, 0x0C, 0xFB, 0x97, 0x0B, 0xFD, 0xFA, 0x0D] => Some((YoshisIsland64, 2164672)),
        [0x04, 0xCD, 0xFB, 0x16, 0x0D, 0x08, 0xF9, 0xA6] => Some((KongoJungle64, 5750272)),
        [0x04, 0x8D, 0xFC, 0xF6, 0x0B, 0xCD, 0xFA, 0x19] => Some((Battlefield, 3146688)),
        [0x03, 0x2A, 0xFF, 0xE6, 0x0A, 0x81, 0xFA, 0xE6] => Some((MultimanMelee, 3717984)),
        [0x09, 0x8D, 0xF9, 0xE0, 0x0E, 0x32, 0xF8, 0xCC] => Some((FinalDestination, 3270336)),
        [0x03, 0x43, 0xFE, 0x31, 0x0A, 0x6A, 0xFA, 0xF5] => Some((MultimanMelee2, 3354112)),
        [0x05, 0xDB, 0xFD, 0x18, 0x0D, 0x50, 0xF9, 0x6D] => Some((BrinstarEscape, 2739968)),
        [0x0C, 0x73, 0xF9, 0xDB, 0x0F, 0x75, 0xF8, 0x3C] => Some((AllStarRestArea, 1004288)),
        [0x02, 0xB7, 0xFE, 0xA0, 0x09, 0xFA, 0xFB, 0x3C] => Some((BreakTheTargets, 1542976)),
        _ => None,
    }
}

/// Given a stage ID, retrieve the ID of the track that should play
pub(crate) fn get_stage_track_id(stage_id: u8) -> Option<TrackId> {
    use self::TrackId::*;

    // Stage IDs and their associated tracks
    let track_ids: Option<(TrackId, Option<TrackId>)> = match stage_id {
        0x02 | 0x1F => Some((PeachsCastle, None)),
        0x03 => Some((RainbowCruise, None)),
        0x04 => Some((KongoJungle, None)),
        0x05 => Some((JungleJapes, None)),
        0x06 => Some((GreatBay, Some(Saria))),
        0x07 => Some((Temple, Some(FireEmblem))),
        0x08 => Some((Brinstar, None)),
        0x09 => Some((BrinstarDepths, None)),
        0x0A => Some((YoshisStory, None)),
        0x0B => Some((YoshisIsland, Some(SuperMario3))),
        0x0C => Some((FountainOfDreams, None)),
        0x0D => Some((GreenGreens, None)),
        0x0E => Some((Corneria, None)),
        0x0F => Some((Venom, None)),
        0x10 => Some((PokemonStadium, Some(PokemonBattle))),
        0x11 => Some((Pokefloats, None)),
        0x12 => Some((MuteCity, None)),
        0x13 => Some((BigBlue, Some(MachRider))),
        0x14 => Some((Onett, Some(Mother2))),
        0x15 => Some((Fourside, None)),
        0x16 => Some((IcicleMountain, Some(BalloonFighter))),
        0x18 => Some((MushroomKingdom, Some(DrMario))),
        0x19 => Some((MushroomKingdomII, Some(DrMario))),
        0x1B => Some((FlatZone, None)),
        0x1C => Some((DreamLand64, None)),
        0x1D => Some((YoshisIsland64, None)),
        0x1E => Some((KongoJungle64, None)),
        0x24 => Some((Battlefield, Some(MultimanMelee))),
        0x25 => Some((FinalDestination, Some(MultimanMelee2))),
        // Snag trophies
        0x26 => Some((Lottery, None)),
        // Race to the Finish
        0x27 => Some((Battlefield, None)),
        // Adventure Mode Field Stages
        0x20 => Some((Temple, None)),
        0x21 => Some((BrinstarEscape, None)),
        0x22 => Some((BigBlue, None)),
        // All-Star Rest Area
        0x42 => Some((AllStarRestArea, None)),
        // Break the Targets + Home Run Contest
        0x2C | 0x28 | 0x43 | 0x33 | 0x31 | 0x37 | 0x3D | 0x2B | 0x29 | 0x41 | 0x2D | 0x2E
        | 0x36 | 0x2F | 0x30 | 0x3B | 0x3E | 0x32 | 0x2A | 0x38 | 0x39 | 0x3A | 0x35 | 0x3F
        | 0x34 | 0x40 => Some((BreakTheTargets, None)),
        _ => None,
    };

    // If the stage has an alternate track associated, there's a 12.5% chance it will be selected
    match track_ids {
        Some(track_ids) => match track_ids {
            (_, Some(id)) if rand::thread_rng().gen_range(0..8) == 0 => Some(id),
            (id, _) => Some(id),
        },
        None => None,
    }
}
