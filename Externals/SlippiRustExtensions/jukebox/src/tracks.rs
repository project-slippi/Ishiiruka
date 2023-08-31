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

/// Given the filename of a melee music track, return the associated track ID
pub(crate) fn get_track_id_by_filename(track_filename: &str) -> Option<TrackId> {
    use self::TrackId::*;

    match track_filename {
        "menu01.hps" => Some(Menu1),
        "menu3.hps" => Some(Menu2),
        "menu02.hps" => Some(Lottery),
        "vs_hyou1.hps" => Some(TournamentMode1),
        "vs_hyou2.hps" => Some(TournamentMode2),
        "intro_es.hps" => Some(VsOpponent),
        "castle.hps" => Some(PeachsCastle),
        "rcruise.hps" => Some(RainbowCruise),
        "garden.hps" => Some(KongoJungle),
        "kongo.hps" => Some(JungleJapes),
        "greatbay.hps" => Some(GreatBay),
        "saria.hps" => Some(Saria),
        "shrine.hps" => Some(Temple),
        "akaneia.hps" => Some(FireEmblem),
        "zebes.hps" => Some(Brinstar),
        "kraid.hps" => Some(BrinstarDepths),
        "ystory.hps" => Some(YoshisStory),
        "yorster.hps" => Some(YoshisIsland),
        "smari3.hps" => Some(SuperMario3),
        "izumi.hps" => Some(FountainOfDreams),
        "greens.hps" => Some(GreenGreens),
        "corneria.hps" => Some(Corneria),
        "venom.hps" => Some(Venom),
        "pstadium.hps" => Some(PokemonStadium),
        "pokesta.hps" => Some(PokemonBattle),
        "pura.hps" => Some(Pokefloats),
        "mutecity.hps" => Some(MuteCity),
        "bigblue.hps" => Some(BigBlue),
        "mrider.hps" => Some(MachRider),
        "onetto.hps" => Some(Onett),
        "onetto2.hps" => Some(Mother2),
        "fourside.hps" => Some(Fourside),
        "icemt.hps" => Some(IcicleMountain),
        "baloon.hps" => Some(BalloonFighter),
        "inis1_01.hps" => Some(MushroomKingdom),
        "docmari.hps" => Some(DrMario),
        "inis2_01.hps" => Some(MushroomKingdomII),
        "flatzone.hps" => Some(FlatZone),
        "old_kb.hps" => Some(DreamLand64),
        "old_ys.hps" => Some(YoshisIsland64),
        "old_dk.hps" => Some(KongoJungle64),
        "sp_zako.hps" => Some(Battlefield),
        "hyaku.hps" => Some(MultimanMelee),
        "sp_end.hps" => Some(FinalDestination),
        "hyaku2.hps" => Some(MultimanMelee2),
        "target.hps" => Some(BreakTheTargets),
        "siren.hps" => Some(BrinstarEscape),
        "1p_qk.hps" => Some(AllStarRestArea),
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
        0x2C | 0x28 | 0x43 | 0x33 | 0x31 | 0x37 | 0x3D | 0x2B | 0x29 | 0x41 | 0x2D | 0x2E | 0x36 | 0x2F | 0x30 | 0x3B | 0x3E
        | 0x32 | 0x2A | 0x38 | 0x39 | 0x3A | 0x35 | 0x3F | 0x34 | 0x40 => Some((BreakTheTargets, None)),
        _ => None,
    };

    // If the stage has an alternate track associated, there's a 12.5% chance it will be selected
    match track_ids {
        Some(track_ids) => match track_ids {
            (_, Some(id)) if fastrand::u8(0..8) == 0 => Some(id),
            (id, _) => Some(id),
        },
        None => None,
    }
}

/// Returns a tuple containing a randomly selected menu track tournament track
/// to play
pub(crate) fn get_random_menu_tracks() -> (TrackId, TrackId) {
    // 25% chance to use the alternate menu theme
    let menu_track = if fastrand::u8(0..4) == 0 {
        TrackId::Menu2
    } else {
        TrackId::Menu1
    };

    // 50% chance to use the alternate tournament mode theme
    let tournament_track = if fastrand::u8(0..2) == 0 {
        TrackId::TournamentMode1
    } else {
        TrackId::TournamentMode2
    };

    (menu_track, tournament_track)
}
