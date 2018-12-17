#include "SlippiReplayComm.h"
#include "Common/FileUtil.h"

#include "DolphinWX\Main.h"

SlippiReplayComm::SlippiReplayComm()
{
	auto thePath = getCommFilePath();
	configFilePath = thePath;
}

std::string SlippiReplayComm::getCommFilePath()
{
	auto args = wxGetApp().argv.GetArguments();
	auto len = args.GetCount();

	int commFlagIdx = -1;

	// TODO: This is not the greatest way to do this and
	// TODO: also there is no real error handling. Probably
	// TODO: this logic should go elsewhere?
	int idx = 0;
	while (idx < len) {
		auto arg = args[idx];
		if (arg == "/c" || arg == "-c") {
			commFlagIdx = idx;
			break;
		}

		idx++;
	}

	bool notFound = commFlagIdx < 0;
	bool pathOutOfBounds = commFlagIdx + 1 >= len;
	if (notFound || pathOutOfBounds) {
		// Default comm file
		return "Slippi/playback.txt";
	}

	return (std::string)args[commFlagIdx + 1];
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
