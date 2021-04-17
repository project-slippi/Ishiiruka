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

  public:
	InputStabilizer(size_t sizeLimit=100, int64_t delay=1'400'000, int64_t leniency=3'333'333);
	InputStabilizer(const InputStabilizer &);
	void feedPollTiming(time_point tp);
	time_point computeNextPollTiming(bool init=false);

	// Kristal
	time_point computeNextPollTimingInternal(bool init = false);
	void startFrameCount(int32_t initialValue=0);
	void endFrameCount();
	void decrementFrameCount();
	float evaluateTiming(const time_point &);
};