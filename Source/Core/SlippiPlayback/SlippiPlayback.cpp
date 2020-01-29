#include <memory>

#include "SlippiPlayback.h"
std::unique_ptr<SlippiPlaybackStatus> g_playback_status;

SlippiPlaybackStatus::SlippiPlaybackStatus()
{
  shouldJumpBack = false;
  shouldJumpForward = false;
  inSlippiPlayback = false;
  shouldRunThreads = false;
  currentPlaybackFrame = INT_MIN;
  targetFrameNum = INT_MAX;
  latestFrame = -123;
}

SlippiPlaybackStatus::~SlippiPlaybackStatus() {}
