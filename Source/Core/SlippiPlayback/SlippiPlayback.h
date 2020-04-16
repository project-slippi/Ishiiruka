#pragma once
#include <future>
#include <open-vcdiff\src\google\vcdecoder.h>
#include <open-vcdiff\src\google\vcencoder.h>
#include <unordered_map>

class SlippiPlaybackStatus
{
public:
	SlippiPlaybackStatus();
	virtual ~SlippiPlaybackStatus();

	bool shouldJumpBack = false;
	bool shouldJumpForward = false;
	bool inSlippiPlayback = false;
	volatile bool shouldRunThreads = false;
	bool isHardFFW = false;
	bool isSoftFFW = false;
	int32_t lastFFWFrame = INT_MIN;
	int32_t currentPlaybackFrame = INT_MIN;
	int32_t targetFrameNum = INT_MAX;
	int32_t latestFrame = -123;

	std::thread m_savestateThread;
	std::thread m_seekThread;

	void startThreads(void);
	void resetPlayback(void);
	void prepareSlippiPlayback();

  private:
	void SavestateThread(void);
	void SeekThread(void);
	void processInitialState(std::vector<u8> &iState);

	std::unordered_map<int32_t, std::shared_future<std::string>>
	    futureDiffs;        // State diffs keyed by frameIndex, processed async
	std::vector<u8> iState; // The initial state
	std::vector<u8> cState; // The current (latest) state

	open_vcdiff::VCDiffDecoder decoder;
	open_vcdiff::VCDiffEncoder *encoder = NULL;
};

