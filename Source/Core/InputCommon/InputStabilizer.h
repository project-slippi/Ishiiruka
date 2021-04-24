#pragma once

#include <chrono>
#include <list>
#include <mutex>

class InputStabilizer
{
  public:
	using time_point = std::chrono::high_resolution_clock::time_point;

  private:
	// Parameters
	const size_t sizeLimit;
	const int64_t delay;
	const int64_t leniency;

	// Transition state
	std::list<time_point> pollTimings;
	int64_t offsetsSum;
	// period used to be an input but is now dynamically controlled by the 59.94/60Hz switch

	// Steady state
	time_point steadyStateOrigin;
	int64_t incrementsSinceOrigin=0;

	// Kristal
	int32_t frameCount = 0;
	bool isCountingFrames = false;
	std::mutex mutex;
	// An instance can be used from both the polling thread that computing a Kristal input's
	// timestamp, and the main thread doing normal pacing stuff - this warrants synchronizing
	// accesses to pollTimings
	int version = 1;
	int frameOfHigherVersion = -10;
	int isNewFrameCounter = 0;
	// to account for stall inputs, we send a timing, and a "version" - receiving an input for
	// timing n=42.5 version 2 makes it more recent than anything in [42,43[ version 1
	// version is incremented when frame count is decremented

  public:
	InputStabilizer(size_t sizeLimit=100, int64_t delay=1'400'000, int64_t leniency=3'333'333);
	InputStabilizer(const InputStabilizer &);
	void feedPollTiming(time_point tp);
	time_point computeNextPollTiming();

	// Kristal
	time_point computeNextPollTimingInternal(bool init = false, bool alter = true);
	void startFrameCount(int32_t initialValue=0);
	void endFrameCount();
	void decrementFrameCount();
	std::pair<float, u8> evaluateTiming(const time_point &);
};