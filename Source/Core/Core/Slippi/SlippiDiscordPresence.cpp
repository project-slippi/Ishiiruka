#include "Core/Slippi/SlippiDiscordPresence.h"
#include "Common/Logging/Log.h"

SlippiDiscordPresence::~SlippiDiscordPresence() {
	// connect to discord here
}

SlippiDiscordPresence::SlippiDiscordPresence() {
	// disconnect from discord here
}

void SlippiDiscordPresence::ReportGame(SlippiGameReporter::GameReport report) {
	INFO_LOG(SLIPPI_ONLINE, "Got a game report: %d stocks and %d stocks",
	 report.players[0].stocksRemaining, report.players[0].stocksRemaining);
};

void SlippiDiscordPresence::UpdateGameInfo(Slippi::GameSettings* gameInfo) {
	INFO_LOG(SLIPPI_ONLINE, "Playing stage %d", gameInfo->stage);
	INFO_LOG(SLIPPI_ONLINE, "Playing character %d", gameInfo->players[0].characterId);
};