#pragma once

#include <SlippiGame.h>
#include <string>

#include <json.hpp>
using json = nlohmann::json;

class SlippiReplayComm
{
  public:
	// Loaded file contents
	typedef struct {
		std::string replayPath;
		bool isRealTimeMode;
		std::string commandId;
	} CommSettings;
	
	SlippiReplayComm();
	~SlippiReplayComm();

	CommSettings getSettings();
	bool isNewReplay();
	Slippi::SlippiGame *loadGame();

  private:
	void loadFile();

	std::string configFilePath;
	json fileData;
	std::string previousReplayLoaded;
	std::string previousCommandId;

	CommSettings commFileSettings;
};
