
#pragma once

class SlippiPlaybackStatus
{
  public:
	SlippiPlaybackStatus();
	virtual ~SlippiPlaybackStatus();

	bool shouldJumpBack = false;
	bool shouldJumpForward = false;
	bool inSlippiPlayback = false;
	volatile bool shouldRunThreads = false;
	int32_t currentPlaybackFrame = INT_MIN;
	int32_t targetFrameNum = INT_MAX;
	int32_t latestFrame = -123;

  private:
};
