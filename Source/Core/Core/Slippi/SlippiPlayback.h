#pragma once

#include <SlippiLib/SlippiGame.h>
#include <climits>
#include <future>
#include <open-vcdiff/src/google/vcdecoder.h>
#include <open-vcdiff/src/google/vcencoder.h>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"

class SlippiPlaybackStatus
{
  public:
	SlippiPlaybackStatus();
	~SlippiPlaybackStatus();

	bool shouldJumpBack = false;
	bool shouldJumpForward = false;
	bool inSlippiPlayback = false;
	volatile bool shouldRunThreads = false;
	bool isHardFFW = false;
	bool isSoftFFW = false;
	s32 lastFFWFrame = INT_MIN;
	s32 currentPlaybackFrame = INT_MIN;
	s32 targetFrameNum = INT_MAX;
	s32 latestFrame = Slippi::GAME_FIRST_FRAME;

	bool prevOCEnable;
	float prevOCFactor;

	std::thread m_savestateThread;
	std::thread m_seekThread;

	void startThreads(void);
	void resetPlayback(void);
	bool shouldFFWFrame(int32_t frameIndex) const;
	void prepareSlippiPlayback(s32 &frameIndex);
	void setHardFFW(bool enable);
	std::unordered_map<u32, bool> getDenylist();
	std::vector<u8> getLegacyCodelist();

  private:
	void SavestateThread(void);
	void SeekThread(void);
	void loadState(s32 closestStateFrame);
	void processInitialState(std::vector<u8> &iState);
	void updateWatchSettingsStartEnd();
	void generateDenylist();
	void generateLegacyCodelist();

	std::unordered_map<int32_t, std::shared_future<std::string>>
	    futureDiffs;        // State diffs keyed by frameIndex, processed async
	std::vector<u8> iState; // The initial state
	std::vector<u8> cState; // The current (latest) state

	std::unordered_map<u32, bool> denylist;
	std::vector<u8> legacyCodelist;

	open_vcdiff::VCDiffDecoder decoder;
	open_vcdiff::VCDiffEncoder *encoder = NULL;
};
