#pragma once

#include <string>

class SlippiReplayComm
{
public:
	SlippiReplayComm();
	~SlippiReplayComm();
	bool isReplayReady();
	std::string getReplay();
private:
	std::string configFilePath;
};

