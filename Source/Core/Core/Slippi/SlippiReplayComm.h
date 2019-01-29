#pragma once

#include <SlippiGame.h>
#include <string>

class SlippiReplayComm
{
  public:
	SlippiReplayComm();
	~SlippiReplayComm();
	bool isNewReplay();
	Slippi::SlippiGame *loadGame();

  private:
	std::string getReplay();

	std::string configFilePath;
	std::string previousReplayLoaded;
};
