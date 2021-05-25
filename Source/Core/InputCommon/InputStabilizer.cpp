#include "InputStabilizer.h"
#include "Common/Logging/Log.h"
#include <numeric>
#include <sstream>
#include "Core/ConfigManager.h"

using time_point = std::chrono::high_resolution_clock::time_point;

InputStabilizer::InputStabilizer(size_t sizeLimit, int64_t delay, int64_t leniency)
    : pollTimings{std::list<time_point>()}
    , sizeLimit{sizeLimit}
    , delay{delay}
    , leniency{leniency}
    , offsetsSum{0}
{
}

InputStabilizer::InputStabilizer(const InputStabilizer& target)
	: pollTimings{target.pollTimings}
    , sizeLimit{target.sizeLimit}
    , delay{target.delay}
    , leniency{target.leniency}
    , offsetsSum{target.offsetsSum}
{
}

/* Class dedicated to storing the history of timings at which something is polled and compute the
 * next estimated timing using a provided period and using the mean of timings[i] - i*period as x0 value.
 * computeNextPollTiming provides the next estimated poll timing.
 * feedPollTiming is used to feed the poll timing that would've been used were we not using the stabilizer.
 *
 * The unit is the nanosecond.
 * The history stores at most @sizeLimit entries. The period is either 1/59.94s or 1/60s depending on the
 * relevant setting of SConfig.
 * When the difference between the new entry and the previous one is farther than @leniency from the computed,
 * timing, it is considered that some exceptional event occured (ex. frame drop) and this invalidates all
 * previous data, resulting in clearing the history and starting over.
 * @delay nanoseconds will be substracted from all values obtained from computeNextPollTimings. If we're to
 * reconstruct a polling with stable periodicity, it makes sense that in some instances ("half of them"), the
 * computed timing is superior to the current one ("now", which we would've used were we not using this class)
 * in which case we want to query whatever timestamp-supporting data buffer with a timestamp that is effectively
 * in the future. Naturally, the data isn't there yet. This means that to always be able to obtain the data of
 * the timestamp of our choices, our computed timestamps must always be in the past. Hence the need to substract
 * a @delay, which must be chosen to be superior to the maximum "lookahead" we expect to face on a non-exceptional
 * basis.
 * It's not fully determined currently what the leniency should be for the average user. My current tests (-Arte)
 * point to the delay barely ever straying more than 1.33ms from the trend. So 1.4ms delay sounds good. But it
 * could use more testing, plus that's just on my machine.
 * 
 * Steady state algorithm: under normal operation, the timing is subject to small variations induced by the
 * replacement of the oldest value by the newest one, whose offset positions to the underlying trend may differ.
 * This is bad for the user (although by a 1/100 amount of the usual damage) so we may consider strictly
 * enforcing a stable increment. But to do that we need enough data to accurately estimate the underlying trend.
 * When the queue is full, provided steady state stabilization is on, we will switch to strictly enforcing
 * periodic increments.
 *
 * Computation details: It's preferable not to iterate over the full history every computation as they need to 
 * be as light as possible. The strategy used is to use a "reference" in time, which is what we'll reason based on.
 * We store the sum of the (timepoints - reference), and update it only as necessary when removing or adding an
 * element. The reference we use is the latest entry. Note that since reference > all stored timepoints, the
 * @offsetsSum is negative - is is expected that only timestamps of increasing values are fed.
 * 
 * We operate with integers except for period multiplications
 * One InputStabilizer shouldn't be used by different threads so it's not synchronized
 * 
 * The next poll timing computed is
 * [reference + period + mean of differences of the offset (+ period*i ) entries to the reference - delay]
 * Which results in : reference + (offsetsSum + n*(n+1)/2*period)/#entries - delay
 */

void InputStabilizer::feedPollTiming(std::chrono::high_resolution_clock::time_point tp)
{
	std::lock_guard<std::mutex> lock(mutex);

	{ // Kristal stuff
		frameCount++;
		if (isNewFrameCounter != 0)
			isNewFrameCounter--;
		if (isNewFrameCounter == 0)
			version = 1;
	}

	const SConfig& sconfig = SConfig::GetInstance();
	double period = 1'000'000'000 / 59.94;

	if (pollTimings.size() == sizeLimit)
	{
		// If we are in steady state, the fed timing is ignored except for error checking
		// It is supposed that feed is called before compute, and incrementsSinceOrigin is
		// incremented after each computation
		if (std::abs((tp - steadyStateOrigin).count() - (int64_t)(incrementsSinceOrigin * period)  ) > leniency)
		{
			offsetsSum = 0;
			pollTimings.clear();
			pollTimings.push_front(tp);
		}
		return;
	}
	if (pollTimings.size())
	{
		if (std::chrono::duration_cast<std::chrono::nanoseconds>(tp - pollTimings.front()).count() >
			period + leniency ||
			std::chrono::duration_cast<std::chrono::nanoseconds>(tp - pollTimings.front()).count() < period - leniency)
		{ // Too high a mistake, reset
			offsetsSum = 0;
			pollTimings.clear();
		}
		else
		{
			offsetsSum -= pollTimings.size() * (tp - pollTimings.front()).count(); // sets reference to tp
		}
	}
	pollTimings.push_front(tp);
	if (pollTimings.size() == sizeLimit) // Initialize steady state algorithm
	{
		incrementsSinceOrigin = 0;
		steadyStateOrigin = computeNextPollTimingInternal(true) + std::chrono::nanoseconds(delay);
		// The origin is compared to real time points and therefore doesn't contain the delay
	}
}

time_point InputStabilizer::computeNextPollTimingInternal(bool init, bool alter) //TODO Why the fuck is there a state change in the read method what was I thinking
{
	const SConfig& sconfig = SConfig::GetInstance();
	double period = 1'000'000'000 / 59.94;

	size_t size = pollTimings.size();

	if (!size)
		return std::chrono::high_resolution_clock::now() - std::chrono::nanoseconds(delay);

	if ((!init) && (size == sizeLimit))
	{
		auto result = steadyStateOrigin + std::chrono::nanoseconds((int64_t)(incrementsSinceOrigin * period - delay));
		if (alter) incrementsSinceOrigin++;
		return result;
	}

	std::chrono::high_resolution_clock::time_point ref = pollTimings.front();
	int64_t actualization = (int64_t)((size) * (size - 1) / 2 * period);
	int64_t actualizedOffsetsMean = (offsetsSum + actualization) / (int64_t)size;
	return ref + std::chrono::nanoseconds(actualizedOffsetsMean - delay);
}

time_point InputStabilizer::computeNextPollTiming()
{
	std::lock_guard<std::mutex> lock(mutex);
	return computeNextPollTimingInternal(false, false);
}

void InputStabilizer::startFrameCount(int32_t initialValue)
{
	frameCount = initialValue;
	isCountingFrames = true;
}

void InputStabilizer::endFrameCount() {
	frameCount = 0;
	isCountingFrames = false;
}

void InputStabilizer::decrementFrameCount() {
	frameCount--;
	isNewFrameCounter = 2;
}

std::pair<float, u8> InputStabilizer::evaluateTiming(const time_point& tp) {
	std::lock_guard<std::mutex> lock(mutex);

	const SConfig &sconfig = SConfig::GetInstance();
	double period = (int64_t)1'000'000'000 / 59.94;

	// It is assumed the last provided timing matches the frame number we currently have
	
	// What we're looking here is when tp is relatively to the stabilizer timings
	// The stabilizer timings are natively offset by the delay
	// But the parameter isn't
	// the time point returned by computeNextPollTiming isn't the "real" timing for the integer that is frameCount
	// We must compensate the delay by adding it again
	time_point previousPoll = computeNextPollTimingInternal() + std::chrono::nanoseconds(delay);

	long long diff = (tp - previousPoll).count();

	int inputVersion = 1;
	double timing = frameCount + (diff / period);
	if (timing >= frameOfHigherVersion && timing < frameOfHigherVersion + 1)
		inputVersion = version;

	// DEBUG
	auto now = std::chrono::high_resolution_clock::now();
	long long diffDebug = (now - previousPoll).count();
	int inputVersionDebug = 1;
	double timingDebug = frameCount + (diffDebug / period);
	if (timingDebug >= frameOfHigherVersion && timingDebug < frameOfHigherVersion + 1)
		inputVersionDebug = version;
	// /DEBUG


	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2) << "Evaluated tp " << tp.time_since_epoch().count() << " to " << (float)timing << " v" << inputVersion << " on "
	    << now.time_since_epoch().count() << " " << (float)timingDebug << " v" << inputVersionDebug;

	INFO_LOG(KRISTAL, oss.str().c_str());

	return std::pair<float, u8>((float)timing, (u8)inputVersion);
}