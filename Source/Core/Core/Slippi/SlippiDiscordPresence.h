#ifdef HAVE_DISCORD_RPC

#include "Core/Slippi/SlippiGameReporter.h"
#include "Core/Slippi/SlippiNetplay.h"
#include "Core/Slippi/SlippiMatchmaking.h"
#include <discord-rpc/include/discord_rpc.h>
#include <thread>
#include <chrono>
#include <atomic>

class SlippiDiscordPresence {
	public:
		SlippiDiscordPresence();
		~SlippiDiscordPresence();
		// void ReportGame(SlippiGameReporter::GameReport);
		void GameEnd();
		void GameStart(SlippiMatchInfo*, SlippiMatchmaking*);
		static void DiscordError(int, const char*);
		static void DiscordReady(const DiscordUser*);
		void Action();
	private:
		static void Idle();
		inline static time_t StartTime;
		inline static DiscordRichPresence discordPresence; 
		const char* ApplicationID = "635924792893112320";
		const std::chrono::milliseconds Interval = std::chrono::milliseconds(5'000);
		std::condition_variable ActionThreadCV;
		std::atomic<bool> InGame;
		std::thread ActionThread;
};

#endif
