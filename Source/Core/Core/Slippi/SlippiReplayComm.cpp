#include "SlippiReplayComm.h"
#include "Common/FileUtil.h"

SlippiReplayComm::SlippiReplayComm()
{
	configFilePath = "Slippi/playback.txt";
}


SlippiReplayComm::~SlippiReplayComm()
{
}

bool SlippiReplayComm::isReplayReady() {
	std::string replayFilePath;
	File::ReadFileToString(configFilePath, replayFilePath);
	
	// TODO: This logic for detecting a new replay isn't quite good enough
	// TODO: wont work in the case where someone tries to load the same
	// TODO: replay twice in a row
	bool isNewReplay = replayFilePath != previousReplayLoaded;
	bool isReplay = !!replayFilePath.length();

	return isReplay && isNewReplay;
	//return true;
}

std::string SlippiReplayComm::getReplay() {
	std::string replayFilePath;
	File::ReadFileToString(configFilePath, replayFilePath);

	previousReplayLoaded = replayFilePath;

	return replayFilePath;
	//return "C:/Dolphin/FM-v5.9-Slippi-r10-Win/Slippi/Console/file1.bin";
}
