#pragma once

#include <chrono>
#include <list>
#include <mutex>

class InputStabilizer
{
  public:
	using time_point = std::chrono::high_resolution_clock::time_point;

  private:
	std::list<time_point> pollTimings;
	int64_t offsetsSum;
	const size_t sizeLimit;
	const int64_t delay;
	const int64_t leniency;
	// period used to be an input but is now dynamically controlled by the 59.94/60Hz switch
	time_point steadyStateOrigin;
	int64_t incrementsSinceOrigin=0;

  public:
	InputStabilizer(size_t sizeLimit=100, int64_t delay=1'400'000, int64_t leniency=3'333'333);
	InputStabilizer(const InputStabilizer &);
	void feedPollTiming(time_point tp);
	time_point computeNextPollTiming(bool init=false);
};