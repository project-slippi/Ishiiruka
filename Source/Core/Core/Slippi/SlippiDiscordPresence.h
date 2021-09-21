#ifdef HAVE_DISCORD_RPC

#include "Core/Slippi/SlippiGameReporter.h"
#include <discord-rpc/include/discord_rpc.h>

class SlippiDiscordPresence {
	public:
		SlippiDiscordPresence();
		~SlippiDiscordPresence();
		// void ReportGame(SlippiGameReporter::GameReport);
		// void UpdateGameInfo(Slippi::GameSettings*);
		static void DiscordReady(const DiscordUser*);
	private:
		const char* ApplicationID = "635924792893112320";
};

#endif