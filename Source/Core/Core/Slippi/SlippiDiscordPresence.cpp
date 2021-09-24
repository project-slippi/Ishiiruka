#ifdef HAVE_DISCORD_RPC

#include "Core/Slippi/SlippiDiscordPresence.h"
#include "Common/Logging/Log.h"
#include <discord-rpc/include/discord_rpc.h>
#include <time.h>
#include <thread>
#include <chrono>

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
	// discordPresence.state = "West of House";
	discordPresence.details = "Gaming";
	discordPresence.startTimestamp = time(0);
	discordPresence.endTimestamp = time(0) + 5 * 60;
	discordPresence.largeImageKey = "canary-large";
	discordPresence.smallImageKey = "ptb-small";
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

void SlippiDiscordPresence::UpdateGameInfo(SlippiMatchInfo* gameInfo) {
	INFO_LOG(SLIPPI_ONLINE, "Playing stage %d", gameInfo->localPlayerSelections.stageId);
	INFO_LOG(SLIPPI_ONLINE, "Playing character %d", gameInfo->localPlayerSelections.characterId);
};

#endif