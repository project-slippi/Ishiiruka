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
	
	return !!replayFilePath.length();
	//return true;
}

std::string SlippiReplayComm::getReplay() {
	std::string replayFilePath;
	File::ReadFileToString(configFilePath, replayFilePath);

	return replayFilePath;
	//return "C:/Dolphin/FM-v5.9-Slippi-r10-Win/Slippi/Console/file1.bin";
}
