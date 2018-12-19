#include "SlippiReplayComm.h"
#include "Common/FileUtil.h"

#include "Core/ConfigManager.h"
#include "Common/Logging/LogManager.h"

SlippiReplayComm::SlippiReplayComm()
{
	INFO_LOG(EXPANSIONINTERFACE, "SlippiReplayComm: Using playback config path: %s", SConfig::GetInstance().m_strSlippiInput.c_str());
	configFilePath = SConfig::GetInstance().m_strSlippiInput.c_str();
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
