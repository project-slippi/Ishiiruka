#include <mutex>
#include <memory>
#include <SlippiPlayback\SlippiPlayback.h>

#ifdef _WIN32
#include <share.h>
#endif

#include "Common/Logging/Log.h"
#include "Core/NetPlayClient.h"
#include "Core/Core.h"
#include "Core/State.h"
#include "Core/HW/EXI_DeviceSlippi.h"

#define FRAME_INTERVAL 900
#define SLEEP_TIME_MS 8

std::unique_ptr<SlippiPlaybackStatus> g_playback_status;

extern std::unique_ptr<SlippiReplayComm> g_replay_comm;

static std::mutex mtx;
static std::mutex seekMtx;
static std::mutex diffMtx;
static std::unique_lock<std::mutex> processingLock(diffMtx);
static std::condition_variable condVar;
static std::condition_variable cv_waitingForTargetFrame;
static std::condition_variable cv_processingDiff;
static std::atomic<int> numDiffsProcessing(0);

int32_t emod(int32_t a, int32_t b)
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
  latestFrame = -123;
}

void SlippiPlaybackStatus::startThreads()
{
	shouldRunThreads = true;
	m_savestateThread = std::thread(&SlippiPlaybackStatus::SavestateThread, this);
	m_seekThread = std::thread(&SlippiPlaybackStatus::SeekThread, this);
}

void SlippiPlaybackStatus::prepareSlippiPlayback()
{
	// block if there's too many diffs being processed
	while (numDiffsProcessing > 3)
	{
		INFO_LOG(SLIPPI, "Processing too many diffs, blocking main process");
		cv_processingDiff.wait(processingLock);
	}

	if (inSlippiPlayback &&
	    currentPlaybackFrame == targetFrameNum)
	{
		INFO_LOG(SLIPPI, "Reached frame to seek to, unblock");
		cv_waitingForTargetFrame.notify_one();
	}

	// Unblock thread to save a state every interval
	if ((currentPlaybackFrame + 123) % FRAME_INTERVAL == 0)
		condVar.notify_one();
}

void SlippiPlaybackStatus::resetPlayback()
{
	shouldRunThreads = false;

	if (m_savestateThread.joinable())
		m_savestateThread.detach();

	if (m_seekThread.joinable())
		m_seekThread.detach();

	condVar.notify_one(); // Will allow thread to kill itself

	shouldJumpBack = false;
	shouldJumpForward = false;
	targetFrameNum = INT_MAX;
	inSlippiPlayback = false;
	futureDiffs.clear();
	futureDiffs.rehash(0);
}

void SlippiPlaybackStatus::processInitialState(std::vector<u8> &iState)
{
	INFO_LOG(SLIPPI, "saving iState");
	State::SaveToBuffer(iState);
	SConfig::GetInstance().bHideCursor = false;
};

void SlippiPlaybackStatus::SavestateThread()
{
	Common::SetCurrentThreadName("Savestate thread");
	std::unique_lock<std::mutex> intervalLock(mtx);

	INFO_LOG(SLIPPI, "Entering savestate thread");

	while (shouldRunThreads)
	{
		// Wait to hit one of the intervals
		while (shouldRunThreads &&
		       (currentPlaybackFrame + 123) % FRAME_INTERVAL != 0)
			condVar.wait(intervalLock);

		if (!shouldRunThreads)
			break;

		int32_t fixedFrameNumber = currentPlaybackFrame;
		if (fixedFrameNumber == INT_MAX)
			continue;

		bool isStartFrame = fixedFrameNumber == Slippi::GAME_FIRST_FRAME;
		bool hasStateBeenProcessed = futureDiffs.count(fixedFrameNumber) > 0;

		if (!inSlippiPlayback && isStartFrame)
		{
			processInitialState(iState);
			inSlippiPlayback = true;
		}
		else if (!hasStateBeenProcessed && !isStartFrame)
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
		bool shouldSeek = inSlippiPlayback &&
		                  (shouldJumpBack || shouldJumpForward ||
		                   targetFrameNum != INT_MAX);

		if (shouldSeek)
		{
			auto replayCommSettings = g_replay_comm->getSettings();
			if (replayCommSettings.mode == "queue")
				clearWatchSettingsStartEnd();

			bool paused = (Core::GetState() == Core::CORE_PAUSE);
			Core::SetState(Core::CORE_PAUSE);

			uint32_t jumpInterval = 300; // 5 seconds;

			if (shouldJumpForward)
				targetFrameNum = currentPlaybackFrame + jumpInterval;

			if (shouldJumpBack)
				targetFrameNum = currentPlaybackFrame - jumpInterval;

			// Handle edgecases for trying to seek before start or past end of game
			if (targetFrameNum < Slippi::GAME_FIRST_FRAME)
				targetFrameNum = Slippi::GAME_FIRST_FRAME;

			if (targetFrameNum > latestFrame)
			{
				targetFrameNum = latestFrame;
			}

			int32_t closestStateFrame =
			    targetFrameNum - emod(targetFrameNum + 123, FRAME_INTERVAL);

			bool isLoadingStateOptimal = targetFrameNum < currentPlaybackFrame ||
			                             closestStateFrame > currentPlaybackFrame;

			if (isLoadingStateOptimal)
			{
				if (closestStateFrame <= Slippi::GAME_FIRST_FRAME)
				{
					State::LoadFromBuffer(iState);
				}
				else
				{
					// If this diff has been processed, load it
					if (futureDiffs.count(closestStateFrame) > 0)
					{
						std::string stateString;
						decoder.Decode((char *)iState.data(), iState.size(), futureDiffs[closestStateFrame].get(),
						               &stateString);
						std::vector<u8> stateToLoad(stateString.begin(), stateString.end());
						State::LoadFromBuffer(stateToLoad);
					};
				}
			}

			// Fastforward until we get to the frame we want
			if (targetFrameNum != closestStateFrame &&
			    targetFrameNum != latestFrame)
			{
				isHardFFW = true;
				SConfig::GetInstance().m_OCEnable = true;
				SConfig::GetInstance().m_OCFactor = 4.0f;

				Core::SetState(Core::CORE_RUN);
				cv_waitingForTargetFrame.wait(seekLock);
				Core::SetState(Core::CORE_PAUSE);

				SConfig::GetInstance().m_OCFactor = 1.0f;
				SConfig::GetInstance().m_OCEnable = false;
				isHardFFW = false;
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

void SlippiPlaybackStatus::clearWatchSettingsStartEnd()
{
	int startFrame = g_replay_comm->current.startFrame;
	int endFrame = g_replay_comm->current.endFrame;
	if (startFrame != Slippi::GAME_FIRST_FRAME || endFrame != INT_MAX)
	{
		if (g_playback_status->targetFrameNum < startFrame)
			g_replay_comm->current.startFrame = g_playback_status->targetFrameNum;
		if (g_playback_status->targetFrameNum > endFrame)
			g_replay_comm->current.endFrame = INT_MAX;
	}
}

SlippiPlaybackStatus::~SlippiPlaybackStatus() {}
