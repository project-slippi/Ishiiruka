#ifdef HAVE_DISCORD_RPC

#include "Core/Slippi/SlippiDiscordPresence.h"
#include "Common/Logging/Log.h"
#include <discord-rpc/include/discord_rpc.h>
#include <time.h>
#include <thread>
#include <chrono>
#include <stdio.h>

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

void SlippiDiscordPresence::UpdateGameInfo(SlippiMatchInfo* gameInfo) {
	int stageId = -1;
	for(int i = 0; i < SLIPPI_REMOTE_PLAYER_MAX; i++) {
		if(gameInfo->remotePlayerSelections[i].playerIdx == 0) {
			INFO_LOG(SLIPPI_ONLINE, "remotePlayerSelections[%d].stageId: %d", i, gameInfo->localPlayerSelections.stageId);
			stageId = gameInfo->remotePlayerSelections[i].stageId;
			break;
		}
	}

	INFO_LOG(SLIPPI_ONLINE, "Playing stage %d", stageId);
	INFO_LOG(SLIPPI_ONLINE, "Playing character %d", gameInfo->localPlayerSelections.characterId);

	char state[256];
	snprintf(state, 256, "%s (%s) vs. (%s) (%s)",
	"Player 1", characters[gameInfo->remotePlayerSelections[0].characterId],
 	"Player 2", characters[gameInfo->remotePlayerSelections[1].characterId]);

	char details[256];
	snprintf(details, 256, "Stocks: %d - %d", 4, 4);

	char largeImageKey[256] = "custom";
	if(stageId != -1) {
		snprintf(largeImageKey, 256, "%d_map", stageId);
	}

	char smallImageKey[256];
	snprintf(smallImageKey, 3, "c_%d_%d", gameInfo->localPlayerSelections.characterId,
																				gameInfo->localPlayerSelections.characterColor);

	INFO_LOG(SLIPPI_ONLINE, "Displaying icon %s",  largeImageKey);

	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.state = state;
	discordPresence.details = details;
	discordPresence.startTimestamp = time(0);
	discordPresence.endTimestamp = time(0) + 7 * 60;
	discordPresence.largeImageKey = largeImageKey;
	discordPresence.smallImageKey = smallImageKey;
	discordPresence.instance = 0;
	Discord_UpdatePresence(&discordPresence);
};

#endif