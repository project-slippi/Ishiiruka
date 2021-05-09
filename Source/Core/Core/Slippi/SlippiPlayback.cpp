#include <memory>
#include <mutex>

#ifdef _WIN32
#include <share.h>
#endif

#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/HW/EXI_DeviceSlippi.h"
#include "Core/NetPlayClient.h"
#include "Core/State.h"
#include "SlippiPlayback.h"
#include <VideoCommon/OnScreenDisplay.h>

#define FRAME_INTERVAL 900
#define SLEEP_TIME_MS 8

std::unique_ptr<SlippiPlaybackStatus> g_playbackStatus;
extern std::unique_ptr<SlippiReplayComm> g_replayComm;

static std::mutex mtx;
static std::mutex seekMtx;
static std::mutex diffMtx;
static std::unique_lock<std::mutex> processingLock(diffMtx);
static std::condition_variable condVar;
static std::condition_variable cv_waitingForTargetFrame;
static std::condition_variable cv_processingDiff;
static std::atomic<int> numDiffsProcessing(0);

s32 emod(s32 a, s32 b)
{
	assert(b != 0);
	int r = a % b;
	return r >= 0 ? r : r + std::abs(b);
}

std::string processDiff(std::vector<u8> iState, std::vector<u8> cState)
{
	INFO_LOG(SLIPPI, "Processing diff");
	numDiffsProcessing += 1;
	cv_processingDiff.notify_one();
	std::string diff = std::string();
	open_vcdiff::VCDiffEncoder encoder((char *)iState.data(), iState.size());
	encoder.Encode((char *)cState.data(), cState.size(), &diff);

	INFO_LOG(SLIPPI, "done processing");
	numDiffsProcessing -= 1;
	cv_processingDiff.notify_one();
	return diff;
}

SlippiPlaybackStatus::SlippiPlaybackStatus()
{
	shouldJumpBack = false;
	shouldJumpForward = false;
	inSlippiPlayback = false;
	shouldRunThreads = false;
	isHardFFW = false;
	isSoftFFW = false;
	lastFFWFrame = INT_MIN;
	currentPlaybackFrame = INT_MIN;
	targetFrameNum = INT_MAX;
	latestFrame = Slippi::GAME_FIRST_FRAME;
	prevOCEnable = SConfig::GetInstance().m_OCEnable;
	prevOCFactor = SConfig::GetInstance().m_OCFactor;
}

void SlippiPlaybackStatus::startThreads()
{
	shouldRunThreads = true;
	m_savestateThread = std::thread(&SlippiPlaybackStatus::SavestateThread, this);
	m_seekThread = std::thread(&SlippiPlaybackStatus::SeekThread, this);
}

void SlippiPlaybackStatus::prepareSlippiPlayback(s32 &frameIndex)
{
	// block if there's too many diffs being processed
	while (shouldRunThreads && numDiffsProcessing > 3)
	{
		INFO_LOG(SLIPPI, "Processing too many diffs, blocking main process");
		cv_processingDiff.wait(processingLock);
	}

	// Unblock thread to save a state every interval
	if (shouldRunThreads && ((currentPlaybackFrame + 122) % FRAME_INTERVAL == 0))
		condVar.notify_one();

	if (SConfig::GetInstance().m_slippiEnableFrameIndex)
	{
		std::stringstream frameDisplay;
		frameDisplay << "Frame: " + std::to_string(frameIndex);
		INFO_LOG(SLIPPI_ONLINE, "Replay Frame: %d", frameIndex);

		OSD::AddTypedMessage(OSD::MessageType::FrameIndex, frameDisplay.str(), 1000, OSD::Color::CYAN);
	}

	// TODO: figure out why sometimes playback frame increments past targetFrameNum
	if (inSlippiPlayback && frameIndex >= targetFrameNum)
	{
		if (targetFrameNum < currentPlaybackFrame)
		{
			// Since playback logic only goes up in currentPlaybackFrame now due to handling rollback
			// playback, we need to rewind the currentPlaybackFrame here instead such that the playback
			// cursor will show up in the correct place
			currentPlaybackFrame = targetFrameNum;
		}

		if (currentPlaybackFrame > targetFrameNum)
		{
			INFO_LOG(SLIPPI, "Reached frame %d. Target was %d. Unblocking", currentPlaybackFrame, targetFrameNum);
		}
		cv_waitingForTargetFrame.notify_one();
	}
}

void SlippiPlaybackStatus::resetPlayback()
{
	if (shouldRunThreads)
	{
		shouldRunThreads = false;

		if (m_savestateThread.joinable())
			m_savestateThread.detach();

		if (m_seekThread.joinable())
			m_seekThread.detach();

		condVar.notify_one(); // Will allow thread to kill itself
		futureDiffs.clear();
		futureDiffs.rehash(0);
	}

	shouldJumpBack = false;
	shouldJumpForward = false;
	isHardFFW = false;
	isSoftFFW = false;
	targetFrameNum = INT_MAX;
	inSlippiPlayback = false;
}

void SlippiPlaybackStatus::processInitialState(std::vector<u8> &iState)
{
	INFO_LOG(SLIPPI, "saving iState");
	State::SaveToBuffer(iState);
};

void SlippiPlaybackStatus::SavestateThread()
{
	Common::SetCurrentThreadName("Savestate thread");
	std::unique_lock<std::mutex> intervalLock(mtx);

	INFO_LOG(SLIPPI, "Entering savestate thread");

	while (shouldRunThreads)
	{
		// Wait to hit one of the intervals
		// Possible while rewinding that we hit this wait again.
		while (shouldRunThreads && (currentPlaybackFrame - Slippi::PLAYBACK_FIRST_SAVE) % FRAME_INTERVAL != 0)
			condVar.wait(intervalLock);

		if (!shouldRunThreads)
			break;

		s32 fixedFrameNumber = currentPlaybackFrame;
		if (fixedFrameNumber == INT_MAX)
			continue;

		bool isStartFrame = fixedFrameNumber == Slippi::PLAYBACK_FIRST_SAVE;
		bool hasStateBeenProcessed = futureDiffs.count(fixedFrameNumber) > 0;

		if (!inSlippiPlayback && isStartFrame)
		{
			processInitialState(iState);
			inSlippiPlayback = true;
		}
		else if (SConfig::GetInstance().m_InterfaceSeekbar && !hasStateBeenProcessed && !isStartFrame)
		{
			INFO_LOG(SLIPPI, "saving diff at frame: %d", fixedFrameNumber);
			State::SaveToBuffer(cState);

			futureDiffs[fixedFrameNumber] = std::async(processDiff, iState, cState);
		}
		Common::SleepCurrentThread(SLEEP_TIME_MS);
	}

	INFO_LOG(SLIPPI, "Exiting savestate thread");
}

void SlippiPlaybackStatus::SeekThread()
{
	Common::SetCurrentThreadName("Seek thread");
	std::unique_lock<std::mutex> seekLock(seekMtx);

	INFO_LOG(SLIPPI, "Entering seek thread");

	while (shouldRunThreads)
	{
		bool shouldSeek = inSlippiPlayback && (shouldJumpBack || shouldJumpForward || targetFrameNum != INT_MAX);

		if (shouldSeek)
		{
			auto replayCommSettings = g_replayComm->getSettings();
			if (replayCommSettings.mode == "queue")
				updateWatchSettingsStartEnd();

			bool paused = (Core::GetState() == Core::CORE_PAUSE);
			Core::SetState(Core::CORE_PAUSE);

			u32 jumpInterval = 300; // 5 seconds;

			if (shouldJumpForward)
				targetFrameNum = currentPlaybackFrame + jumpInterval;

			if (shouldJumpBack)
				targetFrameNum = currentPlaybackFrame - jumpInterval;

			// Handle edgecases for trying to seek before start or past end of game
			if (targetFrameNum < Slippi::PLAYBACK_FIRST_SAVE)
				targetFrameNum = Slippi::PLAYBACK_FIRST_SAVE;

			if (targetFrameNum > latestFrame)
			{
				targetFrameNum = latestFrame;
			}

			s32 closestStateFrame = targetFrameNum - emod(targetFrameNum - Slippi::PLAYBACK_FIRST_SAVE, FRAME_INTERVAL);

			// Somtimes prepareSlippiPlayback sets currentPlaybackFrame = targetFrameNum so check if target is <=
			bool isLoadingStateOptimal =
			    targetFrameNum <= currentPlaybackFrame || closestStateFrame > currentPlaybackFrame;

			if (isLoadingStateOptimal)
			{
				if (closestStateFrame <= Slippi::PLAYBACK_FIRST_SAVE)
				{
					State::LoadFromBuffer(iState);
				}
				else
				{
					// If this diff has been processed, load it
					if (futureDiffs.count(closestStateFrame) > 0)
					{
						loadState(closestStateFrame);
					}
					else if (targetFrameNum < currentPlaybackFrame)
					{
						s32 closestActualStateFrame = closestStateFrame - FRAME_INTERVAL;
						while (closestActualStateFrame > Slippi::PLAYBACK_FIRST_SAVE &&
						       futureDiffs.count(closestActualStateFrame) == 0)
							closestActualStateFrame -= FRAME_INTERVAL;
						loadState(closestActualStateFrame);
					}
					else if (targetFrameNum > currentPlaybackFrame)
					{
						s32 closestActualStateFrame = closestStateFrame - FRAME_INTERVAL;
						while (closestActualStateFrame > currentPlaybackFrame &&
						       futureDiffs.count(closestActualStateFrame) == 0)
							closestActualStateFrame -= FRAME_INTERVAL;

						// only load a savestate if we find one past our current frame since we are seeking forwards
						if (closestActualStateFrame > currentPlaybackFrame)
							loadState(closestActualStateFrame);
					}
				}
			}

			// Fastforward until we get to the frame we want
			if (targetFrameNum != closestStateFrame && targetFrameNum != latestFrame)
			{
				setHardFFW(true);

				Core::SetState(Core::CORE_RUN);
				cv_waitingForTargetFrame.wait(seekLock);
				Core::SetState(Core::CORE_PAUSE);

				setHardFFW(false);
			}

			if (!paused)
				Core::SetState(Core::CORE_RUN);

			shouldJumpBack = false;
			shouldJumpForward = false;
			targetFrameNum = INT_MAX;
		}

		Common::SleepCurrentThread(SLEEP_TIME_MS);
	}

	INFO_LOG(SLIPPI, "Exit seek thread");
}

// Set isHardFFW and update OC settings to speed up the FFW
void SlippiPlaybackStatus::setHardFFW(bool enable)
{
	isHardFFW = enable;
	if (isHardFFW)
	{
		SConfig::GetInstance().m_OCEnable = true;
		SConfig::GetInstance().m_OCFactor = 4.0f;
	}
	else
	{
		SConfig::GetInstance().m_OCFactor = prevOCFactor;
		SConfig::GetInstance().m_OCEnable = prevOCEnable;
	}
}

void SlippiPlaybackStatus::loadState(s32 closestStateFrame)
{
	if (closestStateFrame == Slippi::PLAYBACK_FIRST_SAVE)
		State::LoadFromBuffer(iState);
	else
	{
		std::string stateString;
		decoder.Decode((char *)iState.data(), iState.size(), futureDiffs[closestStateFrame].get(), &stateString);
		std::vector<u8> stateToLoad(stateString.begin(), stateString.end());
		State::LoadFromBuffer(stateToLoad);
	}
}

bool SlippiPlaybackStatus::shouldFFWFrame(int32_t frameIndex) const
{
	if (!isSoftFFW && !isHardFFW)
	{
		// If no FFW at all, don't FFW this frame
		return false;
	}

	if (isHardFFW)
	{
		// For a hard FFW, always FFW until it's turned off
		return true;
	}

	// Here we have a soft FFW, we only want to turn on FFW for single frames once
	// every X frames to FFW in a more smooth manner
	return (frameIndex - lastFFWFrame) >= 15;
}

void SlippiPlaybackStatus::updateWatchSettingsStartEnd()
{
	int startFrame = g_replayComm->current.startFrame;
	int endFrame = g_replayComm->current.endFrame;
	if (startFrame != Slippi::GAME_FIRST_FRAME || endFrame != INT_MAX)
	{
		if (g_playbackStatus->targetFrameNum < startFrame)
			g_replayComm->current.startFrame = g_playbackStatus->targetFrameNum;
		if (g_playbackStatus->targetFrameNum > endFrame)
			g_replayComm->current.endFrame = INT_MAX;
	}
}

SlippiPlaybackStatus::~SlippiPlaybackStatus()
{
	// Kill threads to prevent cleanup crash
	resetPlayback();
}
