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
	if(!SConfig::GetInstance().m_DiscordPresence) return;

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

	InGame = false;
	ActionThread = std::thread([&] (auto* obj) { obj->Action(); }, this);
}

SlippiDiscordPresence::~SlippiDiscordPresence() {
	if(!SConfig::GetInstance().m_DiscordPresence) return;

	ActionThreadCV.notify_all();
	Discord_Shutdown();
	ActionThread.join();
	// disconnect from discord here
}

void SlippiDiscordPresence::Action() {
	INFO_LOG(SLIPPI, "SlippiDiscordPresence::Action()");
	std::mutex mtx;
	std::unique_lock<std::mutex> lk(mtx);
	do {
		#ifdef DISCORD_DISABLE_IO_THREAD
			Discord_UpdateConnection();
		#endif
		Discord_RunCallbacks();
	} while(ActionThreadCV.wait_for(lk, Interval) == std::cv_status::timeout);
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
	if(!SConfig::GetInstance().m_DiscordPresence) return;
	Idle();
}

void SlippiDiscordPresence::GameStart(SlippiMatchInfo* gameInfo, SlippiMatchmaking* matchmaking) {
	if(!SConfig::GetInstance().m_DiscordPresence) return;
	std::vector<SlippiPlayerSelections> players(SLIPPI_REMOTE_PLAYER_MAX+1);
	players[gameInfo->localPlayerSelections.playerIdx] = gameInfo->localPlayerSelections;
	for(int i = 0; i < SLIPPI_REMOTE_PLAYER_MAX; i++) {
		players[gameInfo->remotePlayerSelections[i].playerIdx] = gameInfo->remotePlayerSelections[i];
	}
	players.shrink_to_fit();

	int stageId = players[0].stageId ? players[0].stageId : players[1].stageId;
	INFO_LOG(SLIPPI_ONLINE, "Playing stage %d", stageId);
	INFO_LOG(SLIPPI_ONLINE, "Playing character %d", gameInfo->localPlayerSelections.characterId);

	std::ostringstream details;
	std::vector<std::vector<int>> playerTeams(players.size());
	int maxTeam = 0;
	for(int i = 0; i < players.size(); i++) {
		playerTeams[players[i].teamId].push_back(players[i].playerIdx);
		maxTeam = maxTeam > players[i].teamId ? maxTeam : players[i].teamId;
	}
	playerTeams.resize(maxTeam+1);
	for(auto &team : playerTeams) {
		for(int &i : team) {
			details << matchmaking->GetPlayerName(i) << " (" << characters[players[i].characterId] << ") ";
			if(&i != &team.back()) details << "and ";
		}
		if(&team != &playerTeams.back()) details << "vs. ";
	}


	std::string details_str = details.str();

	// INFO_LOG(SLIPPI_ONLINE, "Discord state: %s", state.str().c_str());

	char largeImageKey[5];
	const char* largeImageText = "Unknown Stage";
	if(stageId > -1 && stageId <= 32) {
		snprintf(largeImageKey, 5, "m_%d", stageId);
		largeImageText = stages[stageId];
	}

	int characterId = gameInfo->localPlayerSelections.characterId;
	char smallImageKey[7];
	const char* smallImageText = "Unknown Character";
	if(characterId > -1 && characterId <= 25) {
		snprintf(smallImageKey, 7, "c_%d_%d", characterId, gameInfo->localPlayerSelections.characterColor);
		smallImageText = characters[characterId];
	}
	INFO_LOG(SLIPPI_ONLINE, "Displaying icon %s",  largeImageKey);

	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.state = 0;
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
