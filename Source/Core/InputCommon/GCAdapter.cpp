// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <libusb.h>
#include <mutex>
#include <iostream>
#include <chrono>
#include <deque>
#include <numeric>
#include <fstream>
#include <time.h>

#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SI.h"
#include "Core/HW/SystemTimers.h"
#include "Core/NetPlayProto.h"

#include "InputCommon/GCAdapter.h"
#include "InputCommon/GCPadStatus.h"

namespace GCAdapter
{
static bool CheckDeviceAccess(libusb_device* device);
static void AddGCAdapter(libusb_device* device);
static void ResetRumbleLockNeeded();
static void Reset();
static void Setup();

static bool s_detected = false;
static libusb_device_handle* s_handle = nullptr;
static u8 s_controller_type[MAX_SI_CHANNELS] = {
		ControllerTypes::CONTROLLER_NONE, ControllerTypes::CONTROLLER_NONE,
		ControllerTypes::CONTROLLER_NONE, ControllerTypes::CONTROLLER_NONE };
static u8 s_controller_rumble[4];

static const int adapter_payload_size = 37;

static std::mutex s_mutex;
static u8 s_controller_payload[adapter_payload_size];
static u8 s_controller_payload_swap[adapter_payload_size];
static u8 default_controller_payload[adapter_payload_size]{};

static std::atomic<int> s_controller_payload_size = { 0 };

static std::thread s_adapter_input_thread;
static std::thread s_adapter_output_thread;
static Common::Flag s_adapter_thread_running;

static Common::Event s_rumble_data_available;

static std::mutex s_init_mutex;
static std::thread s_adapter_detect_thread;
static Common::Flag s_adapter_detect_thread_running;
static Common::Event s_hotplug_event;

static std::function<void(void)> s_detect_callback;

static bool s_libusb_driver_not_supported = false;
static libusb_context* s_libusb_context = nullptr;
static bool s_libusb_hotplug_enabled = false;
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
static libusb_hotplug_callback_handle s_hotplug_handle;
#endif

static u8 s_endpoint_in = 0;
static u8 s_endpoint_out = 0;

static u64 s_last_init = 0;

static u64 s_consecutive_slow_transfers = 0;
static double s_read_rate = 0.0;

// Schmidtt trigger style, start applying if effective report rate > 290Hz, stop if < 260Hz
static const int stopApplyingEILVOptimsHz = 260;
static const int startApplyingEILVOptimsHz = 290;
volatile static bool applyEILVOptims = false;

// Outputs a file with the time points queried by the engine
#define MEASURE_POLLS_FROM_ENGINE 0

#if MEASURE_POLLS_FROM_ENGINE
std::deque<std::chrono::high_resolution_clock::time_point> engine_polls;
int engine_polls_counter = 0;

std::ofstream engine_polls_file;
#endif

bool adapter_error = false;

bool AdapterError()
{
	return adapter_error && s_adapter_thread_running.IsSet();
}

struct controller_payload_entry
{
	std::chrono::high_resolution_clock::time_point raw_timing;
	std::chrono::high_resolution_clock::time_point estimated_timing;
	u8 controller_payload[adapter_payload_size];
	controller_payload_entry(std::chrono::high_resolution_clock::time_point tp, u8 *controller_payload)
	    : raw_timing{tp}
	{
		std::copy(controller_payload, controller_payload + adapter_payload_size, this->controller_payload);
	}
};

std::deque<controller_payload_entry> controller_payload_entries;
int controller_payload_limit = 50;

int usbPollingStabilizationDelay = 200'000;
bool timingReconstructionUsageHistory[1000]{};
int truhIndex = 0;
int truhSum = 0;

bool beenUsingTR() {
	return truhSum > 500;
}
void feedTruh(bool usedTR) {
	truhSum -= timingReconstructionUsageHistory[truhIndex] ? 1 : 0;
	truhSum += usedTR ? 1 : 0;
	timingReconstructionUsageHistory[truhIndex] = usedTR;
	truhIndex = (truhIndex + 1) % 1000;
}

void judgeEILVOptimsApplicability() {
	if (controller_payload_entries.size() > 10)
	{
		double diff =
		    (controller_payload_entries.front().raw_timing - controller_payload_entries.back().raw_timing).count() /
		    1'000'000.;
		double hz = 1000. / ( diff / controller_payload_entries.size() );

		if (!applyEILVOptims && hz > startApplyingEILVOptimsHz)
			applyEILVOptims = true;
		else if (applyEILVOptims && hz < stopApplyingEILVOptimsHz)
			applyEILVOptims = false;
	}
}

static void Feed(std::chrono::high_resolution_clock::time_point tp, u8 *controller_payload)
{
	const SConfig &sconfig = SConfig::GetInstance();

	controller_payload_entries.push_front(controller_payload_entry(tp, controller_payload));

	controller_payload_entry &newEntry = controller_payload_entries.front();
	judgeEILVOptimsApplicability();
	
	size_t size = controller_payload_entries.size();

	// Do we use USB Polling Stabilization ?
	// Once we get to having identified differences, do we use TR ? If so, is TR applicable ?

	if (sconfig.bUseEngineStabilization && sconfig.bUseUsbPollingStabilization)
	{
		std::vector<int64_t> measures;
		std::vector<int64_t> offsetsModulo;
		measures.reserve(size);
		offsetsModulo.reserve(size);

		int64_t latestMeasure = newEntry.raw_timing.time_since_epoch().count();

		for (controller_payload_entry entry : controller_payload_entries)
		{
			int64_t measure = entry.raw_timing.time_since_epoch().count(); // difference to the last poll in ns
			measures.push_back(measure);
		}

		for (int64_t measure : measures)
		{
			// measure = 2.4 ; latestMeasure = 3.3 => gets pushed back : 0.1
			// measure = 2.2 ; latestMeasure = 3.3 => gets pushed back : -0.1
			// ]-0.5;0.5]
			offsetsModulo.push_back((int64_t)fmod(measure - latestMeasure - 500'000, 1'000'000) + 500'000);
		}

		int64_t mean =
		    (int64_t)(std::accumulate(offsetsModulo.begin(), offsetsModulo.end(), (int64_t)0) / (int64_t)size);

		std::vector<int64_t> measuresCorrected;
		for (int64_t measure : measures)
		{
			// measure is 2.4, latestMeasure is 3.5, mean is .1, meaning that timings should be corrected to .5+.1 = .6
			// measure - latestMeasure - mean = -1.2
			// entire division by 1ms then * 1ms : -1
			// + latestMeasure + mean : 2.6
			measuresCorrected.push_back((int64_t)(round((measure - latestMeasure - mean) / 1'000'000.0)) * 1'000'000 +
			                            latestMeasure + mean);
		}

		// Step 2 : we now have polls whose mutual differences are multiples of 1ms
		// We will now figure out when polls truly happened
		// In order to do this we will compute the timing differences and look for differences of 2ms
		// When there's a difference of 2ms, it means no polling happened during the first 1ms,
		// and that in turn means that a polling therefore happened during the x+1;x+1.2 period
		// We will assume the polling happened at x+1.1
		std::vector<int64_t> differences;
		for (int64_t i = 0; i < (int64_t)(measuresCorrected.size()) - 1; i++)
		{
			differences.push_back((measuresCorrected[i] - measuresCorrected[i + 1]) / (int64_t)1'000'000);
		}

		// We consider TR is applicable is we find a 111121111211112 pattern
		// The idea being that when a realignment due to the real period being 1.19971 and not 1.2 happens, the 50 entries won't be a repeating 11112 pattern
		// We might also have errors. 2 consecutive 11112 can happen randomly, 3 is less likely
		// If we could tell the official WUP-028 from its clones we could simply check for which ports are active
		
		bool appliedTR = false;

		if (sconfig.bUseAdapterTimingReconstructionWhenApplicable)
		{
			int64_t index1pattern = -1;
			int64_t index3patterns = -1;

			for (int64_t i = 0; i < (int64_t)(differences.size()) - 14; i++)
			{
				if (differences[i] == 2 && std::all_of(differences.begin() + i + 1, differences.begin() + i + 4 + 1,
				                                       [](const int64_t &x) { return x == 1; }))
				{
					if (index1pattern == -1)
						index1pattern = i;
					if (differences[i + 5] == 2 &&
					    std::all_of(differences.begin() + i + 5 + 1, differences.begin() + i + 5 + 4 + 1,
					                [](const int64_t &x) { return x == 1; }) &&
					    differences[i + 10] == 2 &&
					    std::all_of(differences.begin() + i + 10 + 1, differences.begin() + i + 10 + 4 + 1,
					                [](const int64_t &x) { return x == 1; }))
					{
						index3patterns = i;
						break;
					}
				}
			}

			bool shouldUseTR = index3patterns != -1;
			
			feedTruh(shouldUseTR);

			// 3 cases:
			// A We've been using TR and we should use TR this time => TR algorithm
			// B We've been using TR but we shouldn't use it this time => mean TR offset
			// C We haven't been using TR => regardless of whether we concluded that we should use it, don't use it

			// If we don't use TR in general, we don't need to apply the TR offset
			// If we keep switching between TR and not, we're going to switch between adding the offset or not which is terrible
			// Hence the use of a history of TR use, if we could've used (and perhaps we did) TR for 500 out of the 1000 poll feed,
			// then we are "using it" overall

			if (beenUsingTR()) // A or B
			{
				if (shouldUseTR) // A
				{
					// The poll with a 2ms difference is assumed to have happened 0.9ms on average before the timing we obtained
					// We remove 0.9ms but we correct that to 0.8ms later to match the end of the 0.2ms wide eligibility window for this poll
					// This is so that the most recent timing possible (11112 case; 1ms - 4*0.2ms) is "now"

					int64_t latestDiff2Estimation = measuresCorrected[index1pattern] - 800'000;

					// We can't just multiply the index of differences by 1.2 ; we could do that if we were sure there
					// are only 1s after the 2. But missing polls happen when the CPU is under strain, so we have to
					// account for that.

					int64_t diff = (int64_t)std::llround(
					    ((measuresCorrected[0] - 300'000.) - latestDiff2Estimation) /
					    1'200'000.);
					int64_t newPollEstimation = diff * 1'200'000 + latestDiff2Estimation;
					newEntry.estimated_timing =
					    std::chrono::high_resolution_clock::time_point(std::chrono::nanoseconds(newPollEstimation));

					// Full example scenario:
					// Measures corrected:         9         8         7         6         5         4         3         1      (i.e, a poll
					// came in at t=10ms, t=8ms, etc) Note that here the 8 is unexpected
					// measuresCorrected[index1pattern] : 3
					// latestDiff2Estimation : 2.2 (upper extremity of the [2.2;2.0] range the poll belongs to
					// Measures corrected -0.3:    8.7       7.7       6.7       5.7       4.7       3.7       2.7
					// Minus latestDiffEstimation: 6.5       5.5       4.5       3.5       2.5       1.5       0.5
					// Associated 1.2 range:       [6.6;5.4] [6.6;5.4] [5.4;4.2] [4.2;3]   [3;1.8]   [1.8;0.6] [0.6;-0.6]
					// Diff:                       5         5         4         3         2         1         0
					// newPollEstimation:          8.2       8.2(yes)  7         5.8       4.6       3.4       2.2
					// The rationale behind estimating 8 to 8.2 is that it is probably a correction from the fact the real polling period is 1.19971ms
					// and not 1.2ms: the next poll come in 2ms later (example is for demonstration purpose, you should never have both the 8 and 9 here)
					// and we will estimate the incoming 10 as 9.2 from now on (the 8 would therefore have been 8) 8.2 is closer to 8 than 7
				}
				else // B
				{
					newEntry.estimated_timing = newEntry.raw_timing - std::chrono::nanoseconds(400'000);
					// On average, it's -0.4
					// Reason is simple: in a proper cycle of 5, we correct the entry after a 2ms silence by -0.8
					// Then the 4 subsequent entries by -0.6 -0.4 -0.2 0
					// We correct entries by -0.4 on average, what matters is what happens to the entries
					// There is no "weighting" to do based on how much time an entry is the last available
				}

				appliedTR = true;
			}
		}
		if (!appliedTR) // C
		{
			int64_t mean =
			    (int64_t)(std::accumulate(offsetsModulo.begin(), offsetsModulo.end(), (int64_t)0) / (int64_t)size);

			int64_t newEntryEstimatedTiming = latestMeasure + mean;
			newEntry.estimated_timing =
			    std::chrono::high_resolution_clock::time_point(std::chrono::nanoseconds(newEntryEstimatedTiming));
		}
	}
	else
	{
		newEntry.estimated_timing = newEntry.raw_timing; // Will be used if ES is used, otherwise not used. We fill it either way
	}

	if (controller_payload_entries.size() > controller_payload_limit)
		controller_payload_entries.pop_back();
}

const u8 *Fetch(std::chrono::high_resolution_clock::time_point *tp)
{
	const SConfig &sconfig = SConfig::GetInstance();

	#if MEASURE_POLLS_FROM_ENGINE
	if (tp)
	{
		engine_polls.push_back(*tp);
		std::ostringstream str;
		str << tp->time_since_epoch().count() << ";";
		engine_polls_file << str.str();
	}
	#endif

	if (applyEILVOptims && sconfig.bUseEngineStabilization && tp != nullptr)
	{
		for (auto entry = controller_payload_entries.begin(); entry != controller_payload_entries.end(); entry++)
		{
			// We also have to account for small variations in reception time, plus processing time, hence the offset.
			// Our estimation assumes the initial "2ms difference" true poll timing is at the end of the 0.2ms wide window.
			// *tp - offset > x <=> *tp > x + offset
			// The more you pretend things haven't happened yet when they have, the more room you have to work with.
			// Finally, we are, under normal circumstances, reconstructing timings between 0 and 0.8ms ago.
			// So we need to delay the timings by 0.8ms, otherwise, we would be writing the past.
			// Plus some offset to account for the 1000Hz alignment of controller timings done in the process.

			if (*tp > ( (sconfig.bUseAdapterTimingReconstructionWhenApplicable && beenUsingTR())
			               ? entry->estimated_timing + std::chrono::nanoseconds(800'000) +
			                                 std::chrono::nanoseconds(usbPollingStabilizationDelay)
			               : (sconfig.bUseUsbPollingStabilization
			                      ? entry->estimated_timing + std::chrono::nanoseconds(usbPollingStabilizationDelay)
			                      : entry->raw_timing)
			               ))
			{ // tp is the time queried for, if it is more recent than the one
				// stored and we've got to this point, we should return
				return entry->controller_payload;
			}
		}
	}
	if (controller_payload_entries.size() == 0)
	{
		return default_controller_payload;
	}
	return controller_payload_entries.front().controller_payload;
}

bool IsReadingAtReducedRate()
{
	return s_consecutive_slow_transfers > 80;
}

double ReadRate() 
{
	return s_read_rate;
}

static void Read()
{
	const SConfig &sconfig = SConfig::GetInstance();
	
	s_consecutive_slow_transfers = 0;
	adapter_error = false;

	u8 bkp_payload_swap[adapter_payload_size];
	int bkp_payload_size = 0;
	bool has_prev_input = false;
	s_read_rate = 0.0;

	int payload_size = 0;
	while (s_adapter_thread_running.IsSet())
	{
		bool reuseOldInputsEnabled = SConfig::GetInstance().bAdapterWarning;
		std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();
		adapter_error = libusb_interrupt_transfer(s_handle, s_endpoint_in, s_controller_payload_swap,
			sizeof(s_controller_payload_swap), &payload_size, 32) != LIBUSB_SUCCESS && reuseOldInputsEnabled;
		std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();

		double elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1000000.0;

		// Store previous input and restore in the case of an adapter error
		if (reuseOldInputsEnabled)
		{
			if (!adapter_error)
			{
				memcpy(bkp_payload_swap, s_controller_payload_swap, adapter_payload_size);
				bkp_payload_size = payload_size;
				has_prev_input = true;
			}
			else if (has_prev_input)
			{
				memcpy(s_controller_payload_swap, bkp_payload_swap, adapter_payload_size);
				payload_size = bkp_payload_size;
			}
		}

		if(elapsed > 15.0)
			s_consecutive_slow_transfers++;
		else
			s_consecutive_slow_transfers = 0;

		s_read_rate = elapsed;
		
		{
			std::lock_guard<std::mutex> lk(s_mutex);
			Feed(now,
			      s_controller_payload_swap); // TODO Clean usage of payload swap since we don't use it anymore. Also rn
			                                  // we get old input from s_controller_payload_swap which would be empty
			// Reading the last available input is implemented naturally in the input queue, do it this way
			s_controller_payload_size.store(payload_size);
		}

		Common::YieldCPU();
	}
}

static void Write()
{
	int size = 0;
	while (s_adapter_thread_running.IsSet())
	{
		if (s_rumble_data_available.WaitFor(std::chrono::milliseconds(100)))
		{
			unsigned char rumble[5] = {0x11, s_controller_rumble[0], s_controller_rumble[1], s_controller_rumble[2],
			                           s_controller_rumble[3]};
			libusb_interrupt_transfer(s_handle, s_endpoint_out, rumble, sizeof(rumble), &size, 32);
		}
	}

	s_rumble_data_available.Reset();
}

#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
static int HotplugCallback(libusb_context* ctx, libusb_device* dev, libusb_hotplug_event event,
	void* user_data)
{
	if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
	{
		if (s_handle == nullptr)
			s_hotplug_event.Set();
	}
	else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)
	{
		if (s_handle != nullptr && libusb_get_device(s_handle) == dev)
			Reset();
	}
	return 0;
}
#endif

static void ScanThreadFunc()
{
	Common::SetCurrentThreadName("GC Adapter Scanning Thread");
	NOTICE_LOG(SERIALINTERFACE, "GC Adapter scanning thread started");

#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
	s_libusb_hotplug_enabled = libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) != 0;
	if (s_libusb_hotplug_enabled)
	{
		if (libusb_hotplug_register_callback(
			s_libusb_context, (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
				LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
			LIBUSB_HOTPLUG_ENUMERATE, 0x057e, 0x0337, LIBUSB_HOTPLUG_MATCH_ANY, HotplugCallback,
			nullptr, &s_hotplug_handle) != LIBUSB_SUCCESS)
			s_libusb_hotplug_enabled = false;
		if (s_libusb_hotplug_enabled)
			NOTICE_LOG(SERIALINTERFACE, "Using libUSB hotplug detection");
	}
#endif

	while (s_adapter_detect_thread_running.IsSet())
	{
		if (s_handle == nullptr)
		{
			std::lock_guard<std::mutex> lk(s_init_mutex);
			Setup();
		}

		if (s_libusb_hotplug_enabled)
			s_hotplug_event.Wait();
		else
			Common::SleepCurrentThread(500);
	}
	NOTICE_LOG(SERIALINTERFACE, "GC Adapter scanning thread stopped");
}

void SetAdapterCallback(std::function<void(void)> func)
{
	s_detect_callback = func;
}

void Init()
{
	if (s_handle != nullptr)
		return;

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if ((CoreTiming::GetTicks() - s_last_init) < SystemTimers::GetTicksPerSecond())
			return;

		s_last_init = CoreTiming::GetTicks();
	}

	s_libusb_driver_not_supported = false;

	int ret = libusb_init(&s_libusb_context);

	if (ret)
	{
		ERROR_LOG(SERIALINTERFACE, "libusb_init failed with error: %d", ret);
		s_libusb_driver_not_supported = true;
		Shutdown();
	}
	else
	{
		if (UseAdapter())
			StartScanThread();
	}
}

void StartScanThread()
{
	if (s_adapter_detect_thread_running.IsSet())
		return;

	s_adapter_detect_thread_running.Set(true);
	s_adapter_detect_thread = std::thread(ScanThreadFunc);
}

void StopScanThread()
{
	if (s_adapter_detect_thread_running.TestAndClear())
	{
		s_hotplug_event.Set();
		s_adapter_detect_thread.join();
	}
}

static void Setup()
{
#if MEASURE_POLLS_FROM_ENGINE
	__time64_t long_time;
	_time64(&long_time);
	struct tm newtime;
	_localtime64_s(&newtime, &long_time);

	std::ostringstream fileNameStream;
	fileNameStream << "Engine polls ";
	fileNameStream << std::put_time(&newtime, "%Y-%m-%d %H-%M-%S");
	fileNameStream << ".txt";

	std::string fileName = fileNameStream.str();

	engine_polls_file.open(fileName, std::ios_base::out);
#endif
	
	libusb_device **list;
	ssize_t cnt = libusb_get_device_list(s_libusb_context, &list);

	for (int i = 0; i < MAX_SI_CHANNELS; i++)
	{
		s_controller_type[i] = ControllerTypes::CONTROLLER_NONE;
		s_controller_rumble[i] = 0;
	}

	for (int d = 0; d < cnt; d++)
	{
		libusb_device* device = list[d];
		if (CheckDeviceAccess(device))
		{
			// Only connect to a single adapter in case the user has multiple connected
			AddGCAdapter(device);
			break;
		}
	}

	libusb_free_device_list(list, 1);
}

static bool CheckDeviceAccess(libusb_device* device)
{
	int ret;
	libusb_device_descriptor desc;
	int dRet = libusb_get_device_descriptor(device, &desc);
	if (dRet)
	{
		// could not acquire the descriptor, no point in trying to use it.
		ERROR_LOG(SERIALINTERFACE, "libusb_get_device_descriptor failed with error: %d", dRet);
		return false;
	}

	if (desc.idVendor == 0x057e && desc.idProduct == 0x0337)
	{
		NOTICE_LOG(SERIALINTERFACE, "Found GC Adapter with Vendor: %X Product: %X Devnum: %d",
			desc.idVendor, desc.idProduct, 1);

		u8 bus = libusb_get_bus_number(device);
		u8 port = libusb_get_device_address(device);
		ret = libusb_open(device, &s_handle);
		if (ret)
		{
			if (ret == LIBUSB_ERROR_ACCESS)
			{
				if (dRet)
				{
					ERROR_LOG(SERIALINTERFACE, "Dolphin does not have access to this device: Bus %03d Device "
						"%03d: ID ????:???? (couldn't get id).",
						bus, port);
				}
				else
				{
					ERROR_LOG(
						SERIALINTERFACE,
						"Dolphin does not have access to this device: Bus %03d Device %03d: ID %04X:%04X.",
						bus, port, desc.idVendor, desc.idProduct);
				}
			}
			else
			{
				ERROR_LOG(SERIALINTERFACE, "libusb_open failed to open device with error = %d", ret);
				if (ret == LIBUSB_ERROR_NOT_SUPPORTED)
					s_libusb_driver_not_supported = true;
			}
			return false;
		}
		else if ((ret = libusb_kernel_driver_active(s_handle, 0)) == 1)
		{
			if ((ret = libusb_detach_kernel_driver(s_handle, 0)) && ret != LIBUSB_ERROR_NOT_SUPPORTED)
			{
				ERROR_LOG(SERIALINTERFACE, "libusb_detach_kernel_driver failed with error: %d", ret);
			}
		}
		// This call makes Nyko-brand (and perhaps other) adapters work.
		// However it returns LIBUSB_ERROR_PIPE with Mayflash adapters.
		const int transfer = libusb_control_transfer(s_handle, 0x21, 11, 0x0001, 0, nullptr, 0, 1000);
		if (transfer < 0)
			WARN_LOG(SERIALINTERFACE, "libusb_control_transfer failed with error: %d", transfer);

		// this split is needed so that we don't avoid claiming the interface when
		// detaching the kernel driver is successful
		if (ret != 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED)
		{
			return false;
		}
		else if ((ret = libusb_claim_interface(s_handle, 0)))
		{
			ERROR_LOG(SERIALINTERFACE, "libusb_claim_interface failed with error: %d", ret);
		}
		else
		{
			return true;
		}
	}
	return false;
}

static void AddGCAdapter(libusb_device* device)
{
	libusb_config_descriptor* config = nullptr;
	libusb_get_config_descriptor(device, 0, &config);
	for (u8 ic = 0; ic < config->bNumInterfaces; ic++)
	{
		const libusb_interface* interfaceContainer = &config->interface[ic];
		for (int i = 0; i < interfaceContainer->num_altsetting; i++)
		{
			const libusb_interface_descriptor* interface = &interfaceContainer->altsetting[i];
			for (u8 e = 0; e < interface->bNumEndpoints; e++)
			{
				const libusb_endpoint_descriptor* endpoint = &interface->endpoint[e];
				if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)
					s_endpoint_in = endpoint->bEndpointAddress;
				else
					s_endpoint_out = endpoint->bEndpointAddress;
			}
		}
	}

	int tmp = 0;
	unsigned char payload = 0x13;
	libusb_interrupt_transfer(s_handle, s_endpoint_out, &payload, sizeof(payload), &tmp, 32);

	s_adapter_thread_running.Set(true);

	const SConfig &sconfig = SConfig::GetInstance();

	if (sconfig.bIncreaseProcessPriority)
	{
		#if defined(_WIN32)
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
		#elif defined(__linux__)
		#elif defined(__APPLE__)
		#endif
	}

	s_adapter_input_thread = std::thread(Read);
	if (sconfig.bSaturatePollingThreadPriority)
	{
		#if defined(_WIN32)
		SetThreadPriority(s_adapter_input_thread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
		#elif defined(__linux__)
		#elif defined(__APPLE__)
		#endif
	}

	s_adapter_output_thread = std::thread(Write);
	if (sconfig.bSaturatePollingThreadPriority)
	{
		#if defined(_WIN32)
		SetThreadPriority(s_adapter_output_thread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
		#elif defined(__linux__)
		#elif defined(__APPLE__)
		#endif
	}

	s_detected = true;
	if (s_detect_callback != nullptr)
		s_detect_callback();
	ResetRumbleLockNeeded();
}

void Shutdown()
{
	StopScanThread();
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
	if (s_libusb_hotplug_enabled)
		libusb_hotplug_deregister_callback(s_libusb_context, s_hotplug_handle);
#endif
	Reset();

	if (s_libusb_context)
	{
		libusb_exit(s_libusb_context);
		s_libusb_context = nullptr;
	}

	s_libusb_driver_not_supported = false;
}

static void Reset()
{
	std::unique_lock<std::mutex> lock(s_init_mutex, std::defer_lock);
	if (!lock.try_lock())
		return;
	if (!s_detected)
		return;

	if (s_adapter_thread_running.TestAndClear())
	{
		s_adapter_input_thread.join();
		s_adapter_output_thread.join();
	}

	for (int i = 0; i < MAX_SI_CHANNELS; i++)
		s_controller_type[i] = ControllerTypes::CONTROLLER_NONE;

	s_detected = false;

	if (s_handle)
	{
		libusb_release_interface(s_handle, 0);
		libusb_close(s_handle);
		s_handle = nullptr;
	}
	if (s_detect_callback != nullptr)
		s_detect_callback();
	NOTICE_LOG(SERIALINTERFACE, "GC Adapter detached");
}

GCPadStatus Input(int chan, std::chrono::high_resolution_clock::time_point *tp)
{
	if (!UseAdapter())
		return{};

	if (s_handle == nullptr || !s_detected)
		return{};

	int payload_size = 0;
	u8 controller_payload_copy[adapter_payload_size];

	{
		std::lock_guard<std::mutex> lk(s_mutex);
		const u8 *controller_payload = Fetch(tp);
		std::copy(controller_payload, controller_payload + adapter_payload_size, controller_payload_copy);
		payload_size = s_controller_payload_size.load();
	}

	GCPadStatus pad = {};
	if (payload_size != sizeof(controller_payload_copy) ||
		controller_payload_copy[0] != LIBUSB_DT_HID)
	{
		// This can occur for a few frames on initialization.
		ERROR_LOG(SERIALINTERFACE, "error reading payload (size: %d, type: %02x)", payload_size,
			controller_payload_copy[0]);
	}
	else
	{
		bool get_origin = false;
		u8 type = controller_payload_copy[1 + (9 * chan)] >> 4;
		if (type != ControllerTypes::CONTROLLER_NONE &&
			s_controller_type[chan] == ControllerTypes::CONTROLLER_NONE)
		{
			NOTICE_LOG(SERIALINTERFACE, "New device connected to Port %d of Type: %02x", chan + 1,
				controller_payload_copy[1 + (9 * chan)]);
			get_origin = true;
		}

		s_controller_type[chan] = type;

		if (s_controller_type[chan] != ControllerTypes::CONTROLLER_NONE)
		{
			u8 b1 = controller_payload_copy[1 + (9 * chan) + 1];
			u8 b2 = controller_payload_copy[1 + (9 * chan) + 2];

			if (b1 & (1 << 0))
				pad.button |= PAD_BUTTON_A;
			if (b1 & (1 << 1))
				pad.button |= PAD_BUTTON_B;
			if (b1 & (1 << 2))
				pad.button |= PAD_BUTTON_X;
			if (b1 & (1 << 3))
				pad.button |= PAD_BUTTON_Y;

			if (b1 & (1 << 4))
				pad.button |= PAD_BUTTON_LEFT;
			if (b1 & (1 << 5))
				pad.button |= PAD_BUTTON_RIGHT;
			if (b1 & (1 << 6))
				pad.button |= PAD_BUTTON_DOWN;
			if (b1 & (1 << 7))
				pad.button |= PAD_BUTTON_UP;

			if (b2 & (1 << 0))
				pad.button |= PAD_BUTTON_START;
			if (b2 & (1 << 1))
				pad.button |= PAD_TRIGGER_Z;
			if (b2 & (1 << 2))
				pad.button |= PAD_TRIGGER_R;
			if (b2 & (1 << 3))
				pad.button |= PAD_TRIGGER_L;

			if (get_origin)
				pad.button |= PAD_GET_ORIGIN;

			pad.stickX = controller_payload_copy[1 + (9 * chan) + 3];
			pad.stickY = controller_payload_copy[1 + (9 * chan) + 4];
			pad.substickX = controller_payload_copy[1 + (9 * chan) + 5];
			pad.substickY = controller_payload_copy[1 + (9 * chan) + 6];
			pad.triggerLeft = controller_payload_copy[1 + (9 * chan) + 7];
			pad.triggerRight = controller_payload_copy[1 + (9 * chan) + 8];
		}
		else
		{
			GCPadStatus centered_status = {0};
			centered_status.stickX = centered_status.stickY =
			centered_status.substickX = centered_status.substickY =
			/* these are all the same */ GCPadStatus::MAIN_STICK_CENTER_X;

			return centered_status;
		}
	}

	return pad;
}

bool DeviceConnected(int chan)
{
	return s_controller_type[chan] != ControllerTypes::CONTROLLER_NONE;
}

bool UseAdapter()
{
	return SConfig::GetInstance().m_SIDevice[0] == SIDEVICE_WIIU_ADAPTER ||
		SConfig::GetInstance().m_SIDevice[1] == SIDEVICE_WIIU_ADAPTER ||
		SConfig::GetInstance().m_SIDevice[2] == SIDEVICE_WIIU_ADAPTER ||
		SConfig::GetInstance().m_SIDevice[3] == SIDEVICE_WIIU_ADAPTER;
}

void ResetRumble()
{
	std::unique_lock<std::mutex> lock(s_init_mutex, std::defer_lock);
	if (!lock.try_lock())
		return;
	ResetRumbleLockNeeded();
}

// Needs to be called when s_init_mutex is locked in order to avoid
// being called while the libusb state is being reset
static void ResetRumbleLockNeeded()
{
	if (!UseAdapter() || (s_handle == nullptr || !s_detected))
	{
		return;
	}

	std::fill(std::begin(s_controller_rumble), std::end(s_controller_rumble), 0);

	s_rumble_data_available.Set();
}

void Output(int chan, u8 rumble_command)
{
	if (s_handle == nullptr || !UseAdapter() || !SConfig::GetInstance().m_AdapterRumble[chan])
		return;

	// Skip over rumble commands if it has not changed or the controller is wireless
	if (rumble_command != s_controller_rumble[chan] &&
		s_controller_type[chan] != ControllerTypes::CONTROLLER_WIRELESS)
	{
		s_controller_rumble[chan] = rumble_command;
		s_rumble_data_available.Set();
	}
}

bool IsDetected()
{
	return s_detected;
}

bool IsDriverDetected()
{
	return !s_libusb_driver_not_supported;
}

}  // end of namespace GCAdapter
