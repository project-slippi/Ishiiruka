#pragma once

#include <SlippiLib/SlippiGame.h>
#include <climits>
#include <memory>
#include <queue>
#include <string>

#include "Common/CommonTypes.h"

class SlippiReplayComm
{
  public:
	typedef struct WatchSettings
	{
		std::string path;
		int startFrame = Slippi::GAME_FIRST_FRAME;
		int endFrame = INT_MAX;
		std::string gameStartAt = "";
		std::string gameStation = "";
		int index = 0;
	} WatchSettings;

	// Loaded file contents
	typedef struct CommSettings
	{
		std::string mode;
		std::string replayPath;
		int startFrame = Slippi::GAME_FIRST_FRAME;
		int endFrame = INT_MAX;
		bool outputOverlayFiles = false;
		bool isRealTimeMode = false;
		bool shouldResync = true;          // If true, logic will attempt to resync games
		std::string rollbackDisplayMethod; // off, normal, visible
		std::string commandId;
		std::string gameStation;
		std::queue<WatchSettings> queue;
	} CommSettings;

	SlippiReplayComm();
	explicit SlippiReplayComm(const std::string &config_file_path);
	~SlippiReplayComm();

	WatchSettings current;

	CommSettings getSettings();
	void nextReplay();
	bool isNewReplay();
	std::unique_ptr<Slippi::SlippiGame> loadGame();

  private:
	void loadFile();
	std::string getReplayPath();

	std::string configFilePath;
	std::string previousReplayLoaded;
	std::string previousCommandId;
	int previousIndex = -1;

	u64 configLastLoadModTime = 0;

	// Queue stuff
	bool queueWasEmpty = true;

	CommSettings commFileSettings;
};

extern std::unique_ptr<SlippiReplayComm> g_replayComm;
