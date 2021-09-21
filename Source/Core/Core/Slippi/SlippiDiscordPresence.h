#include "Core/Slippi/SlippiGameReporter.h"
#include <SlippiGame.h>

class SlippiDiscordPresence {
	public:
		SlippiDiscordPresence();
		~SlippiDiscordPresence();
		void ReportGame(SlippiGameReporter::GameReport);
		void UpdateGameInfo(Slippi::GameSettings*);
	private:
		const long int ApplicationID = 635924792893112320;
};