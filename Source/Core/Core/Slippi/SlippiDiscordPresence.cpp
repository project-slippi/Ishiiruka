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
	StartTime = time(0);

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

	Idle();
}

void SlippiDiscordPresence::Idle() {
	// connect to discord here
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.state = "Idle";
	discordPresence.startTimestamp = StartTime;
	discordPresence.largeImageKey = "menu";
	discordPresence.instance = 0;
	Discord_UpdatePresence(&discordPresence);
}

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

void SlippiDiscordPresence::GameEnd() {
	Idle();
}

void SlippiDiscordPresence::GameStart(SlippiMatchInfo* gameInfo, SlippiMatchmaking* matchmaking) {
	std::vector<SlippiPlayerSelections> players(matchmaking->RemotePlayerCount()+1);
	players[gameInfo->localPlayerSelections.playerIdx] = gameInfo->localPlayerSelections;
	for(int i = 0; i < matchmaking->RemotePlayerCount()+1; i++) {
		players[gameInfo->remotePlayerSelections[i].playerIdx] = gameInfo->remotePlayerSelections[i];
	}

	int stageId = players[0].stageId;
	INFO_LOG(SLIPPI_ONLINE, "Playing stage %d", stageId);
	INFO_LOG(SLIPPI_ONLINE, "Playing character %d", gameInfo->localPlayerSelections.characterId);

	std::ostringstream details;
	if(matchmaking->RemotePlayerCount() >= 1) {
		details << matchmaking->GetPlayerName(0) << " (" << characters[players[0].characterId] << ") ";
		details << "vs. ";
		details << matchmaking->GetPlayerName(1) << " (" << characters[players[1].characterId] << ") ";
	} else {
		// for(auto &team : playerTeams) {
		// 	for(int &i : team) {
		// 		details << matchmaking->GetPlayerName(i) << " (" << characters[players[i].characterId] << ") ";
		// 	}
		// 	details << "vs. ";
		// }
	}
	const std::string details_str = details.str();
	const char* tmp = details_str.data();

	INFO_LOG(SLIPPI_ONLINE, "Discord state: %s", tmp);

	// char state[14];
	// snprintf(state, 14, "Stocks: %d - %d", 4, 4);

	std::string largeImageKey = "custom";
	std::string largeImageText = "Unknown Stage";
	if(stageId != -1) {
		largeImageKey = std::to_string(stageId) + "_map";
		largeImageText = stages[stageId];
	}

	std::string smallImageKey = 
		"c_"+std::to_string(gameInfo->localPlayerSelections.characterId)+"_"+std::to_string(gameInfo->localPlayerSelections.characterColor);
	std::string smallImageText = characters[gameInfo->localPlayerSelections.characterId];

	std::cout << "\nDisplaying icon " << largeImageKey << "\n";
	// memset(&discordPresence, 0, sizeof(discordPresence));
	// discordPresence.state = "Stocks";
	// discordPresence.details = details_str.data();
	// discordPresence.startTimestamp = time(0);
	// discordPresence.endTimestamp = time(0) + 8 * 60;
	// discordPresence.largeImageKey = largeImageKey.data();
	// discordPresence.largeImageText = largeImageText.data();
	// discordPresence.smallImageKey = smallImageKey.data();
	// discordPresence.smallImageText = smallImageText.data();
	// discordPresence.instance = 0;
	// Discord_UpdatePresence(&discordPresence);
};

#endif