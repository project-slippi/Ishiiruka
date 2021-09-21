#ifdef HAVE_DISCORD_RPC

#include "Core/Slippi/SlippiDiscordPresence.h"
#include "Common/Logging/Log.h"
#include <discord-rpc/include/discord_rpc.h>
#include <time.h>

SlippiDiscordPresence::SlippiDiscordPresence() {

	INFO_LOG(SLIPPI_ONLINE, "SlippiDiscordPresence()");

	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));
	handlers.ready = SlippiDiscordPresence::DiscordReady;
	// handlers.disconnected = handleDiscordDisconnected;
	// handlers.errored = handleDiscordError;
	// handlers.joinGame = handleDiscordJoin;
	// handlers.spectateGame = handleDiscordSpectate;
	// handlers.joinRequest = handleDiscordJoinRequest;
	Discord_Initialize(ApplicationID, &handlers, 1, NULL);
}

SlippiDiscordPresence::~SlippiDiscordPresence() {
	// disconnect from discord here
}

void SlippiDiscordPresence::DiscordReady(const DiscordUser*) {
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
}

// void SlippiDiscordPresence::ReportGame(SlippiGameReporter::GameReport report) {
	// INFO_LOG(SLIPPI_ONLINE, "Got a game report: %d stocks and %d stocks",
	//  report.players[0].stocksRemaining, report.players[0].stocksRemaining);
// };

// void SlippiDiscordPresence::UpdateGameInfo(Slippi::GameSettings* gameInfo) {
	// INFO_LOG(SLIPPI_ONLINE, "Playing stage %d", gameInfo->stage);
	// INFO_LOG(SLIPPI_ONLINE, "Playing character %d", gameInfo->players[0].characterId);
// };

#endif