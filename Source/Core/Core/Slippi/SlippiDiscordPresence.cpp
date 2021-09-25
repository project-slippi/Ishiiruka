#ifdef HAVE_DISCORD_RPC

#include "Core/Slippi/SlippiDiscordPresence.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include <discord-rpc/include/discord_rpc.h>
#include <time.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <array>
#include <stdio.h>

#define MAX_NAME_LENGTH 15

SlippiDiscordPresence::SlippiDiscordPresence() {
	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));
	handlers.ready = SlippiDiscordPresence::DiscordReady;
	// handlers.disconnected = handleDiscordDisconnect;
	handlers.errored = SlippiDiscordPresence::DiscordError;
	// handlers.joinGame = handleDiscordJoin;
	// handlers.spectateGame = handleDiscordSpectate;
	// handlers.joinRequest = handleDiscordJoinRequest;
	Discord_Initialize(ApplicationID, &handlers, 1, NULL);

	RunActionThread = true;
	InGame = false;
	ActionThread = std::thread([&] (auto* obj) { obj->Action(); }, this);
}

SlippiDiscordPresence::~SlippiDiscordPresence() {
	RunActionThread = false;
	// disconnect from discord here
}

void SlippiDiscordPresence::Action() {
	INFO_LOG(SLIPPI, "SlippiDiscordPresence::Action()");
	while(RunActionThread) {
		#ifdef DISCORD_DISABLE_IO_THREAD
			Discord_UpdateConnection();
		#endif
		Discord_RunCallbacks();
		std::this_thread::sleep_for(Interval);
	}
}

void SlippiDiscordPresence::DiscordError(int errcode, const char* message)
{
    ERROR_LOG(SLIPPI, "Could not connected to discord: error (%d: %s)", errcode, message);
}

void SlippiDiscordPresence::DiscordReady(const DiscordUser* user) {
	INFO_LOG(SLIPPI, "Discord: connected to user %s#%s - %s",
		user->username,
		user->discriminator,
		user->userId);

	// connect to discord here
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.state = "Idle";
	// discordPresence.details = "Idle";
	discordPresence.startTimestamp = time(0);
	// discordPresence.endTimestamp = time(0) + 5 * 60;
	discordPresence.largeImageKey = "menu";
	// discordPresence.smallImageKey = "ptb-small";
	// discordPresence.partyId = "party1234";
	// discordPresence.partySize = 1;
	// discordPresence.partyMax = 6;
	// discordPresence.partyPrivacy = DISCORD_PARTY_PUBLIC;
	// discordPresence.matchSecret = "xyzzy";
	// discordPresence.joinSecret = "join";
	// discordPresence.spectateSecret = "look";
	discordPresence.instance = 0;
	Discord_UpdatePresence(&discordPresence);

	INFO_LOG(SLIPPI, "SlippiDiscordPresence::DiscordReady()");
}

// void SlippiDiscordPresence::ReportGame(SlippiGameReporter::GameReport report) {
	// INFO_LOG(SLIPPI_ONLINE, "Got a game report: %d stocks and %d stocks",
	//  report.players[0].stocksRemaining, report.players[0].stocksRemaining);
// };

const char* characters[] = {
  "Captain Falcon",
  "Donkey Kong",
  "Fox",
  "Game and Watch",
  "Kirby",
  "Bowser",
  "Young Link",
  "Luigi",
  "Mario",
  "Marth",
  "Mewtwo",
  "Ness",
  "Peach",
  "Pikachu",
  "Ice Climbers",
  "Jigglypuff",
  "Samus",
  "Yoshi",
  "Zelda",
  "Sheik",
  "Falco",
  "Link",
  "Dr. Mario",
  "Roy",
  "Pichu",
  "Ganondorf",
};

const char* stages[] = {
  "Unknown Stage",
  "Unknown Stage",
  "Fountains of Dreams",
  "Pokemon Stadium",
  "Princess Peach's Castle",
  "Kongo Jungle",
  "Brinstar",
  "Corneria",
  "Yoshi's Story",
  "Onett",
  "Mute City",
  "Rainbow Cruise",
  "Jungle Japes",
  "Great Bay",
  "Temple",
  "Brinstar Depths",
  "Yoshi's Island",
  "Green Greens",
  "Fourside",
  "Mushroom Kingdom",
  "Mushroom Kingdom II",
  "Unknown Stage",
  "Venom",
  "Poke Floats",
  "Big Blue",
  "Icicle Mountain",
  "Unknown Stage",
  "Flat Zone",
  "Dream Land 64",
  "Yoshi's Island 64",
  "Kongo Jungle 64",
  "Battlefield",
  "Final Destination"
};

void SlippiDiscordPresence::UpdateGameInfo(SlippiMatchInfo* gameInfo, SlippiMatchmaking* matchmaking) {

	std::vector<SlippiPlayerSelections> players(SLIPPI_REMOTE_PLAYER_MAX+1);
	players[gameInfo->localPlayerSelections.playerIdx] = gameInfo->localPlayerSelections;
	for(int i = 0; i < SLIPPI_REMOTE_PLAYER_MAX; i++) {
		players[gameInfo->remotePlayerSelections[i].playerIdx] = gameInfo->remotePlayerSelections[i];
	}
	players.shrink_to_fit();

	int stageId = players[0].stageId;
	INFO_LOG(SLIPPI_ONLINE, "Playing stage %d", stageId);
	INFO_LOG(SLIPPI_ONLINE, "Playing character %d", gameInfo->localPlayerSelections.characterId);

	std::ostringstream details;
	for(int i = 0; i < matchmaking->RemotePlayerCount()+1; i++) {
		details << matchmaking->GetPlayerName(i) << " (" << characters[players[i].characterId] << ") ";
		if(i < matchmaking->RemotePlayerCount()) {
			details << "vs. ";
		}
	}
	std::string details_str = details.str();

	// INFO_LOG(SLIPPI_ONLINE, "Discord state: %s", state.str().c_str());

	char state[14];
	snprintf(state, 14, "Stocks: %d - %d", 4, 4);

	char largeImageKey[7] = "custom";
	const char* largeImageText = "Unknown Stage";
	if(stageId != -1) {
		snprintf(largeImageKey, 7, "%d_map", stageId);
		largeImageText = stages[stageId];
	}

	char smallImageKey[7];
	const char* smallImageText;
	snprintf(smallImageKey, 7, "c_%d_%d", gameInfo->localPlayerSelections.characterId,
																				gameInfo->localPlayerSelections.characterColor);
	smallImageText = characters[gameInfo->localPlayerSelections.characterId];

	INFO_LOG(SLIPPI_ONLINE, "Displaying icon %s",  largeImageKey);

	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.state = state;
	discordPresence.details = details_str.c_str();
	discordPresence.startTimestamp = time(0);
	discordPresence.endTimestamp = time(0) + 8 * 60;
	discordPresence.largeImageKey = largeImageKey;
	discordPresence.largeImageText = largeImageText;
	discordPresence.smallImageKey = smallImageKey;
		discordPresence.smallImageText = smallImageText;
	discordPresence.instance = 0;
	Discord_UpdatePresence(&discordPresence);
};

#endif