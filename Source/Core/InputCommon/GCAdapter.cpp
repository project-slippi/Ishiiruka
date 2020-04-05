// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <libusb.h>
#include <mutex>
#include <iostream>

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

static std::mutex s_mutex;
static u8 s_controller_payload[37];
static u8 s_controller_payload_swap[37];

static std::atomic<int> s_controller_payload_size = { 0 };

static std::thread s_adapter_input_thread;
static std::thread s_adapter_output_thread;
static Common::Flag s_adapter_thread_running;

static Common::Event s_rumble_data_available;
static unsigned char s_latest_rumble_data[5];

static std::mutex s_init_mutex;
static std::thread s_adapter_detect_thread;
static Common::Flag s_adapter_detect_thread_running;

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

bool adapter_error = false;

bool AdapterError()
{
	return adapter_error && s_adapter_thread_running.IsSet();
}

static void Read()
{
	adapter_error = false;

	int payload_size = 0;
	while (s_adapter_thread_running.IsSet())
	{
		adapter_error = libusb_interrupt_transfer(s_handle, s_endpoint_in, s_controller_payload_swap,
			sizeof(s_controller_payload_swap), &payload_size, 16) != LIBUSB_SUCCESS && SConfig::GetInstance().bAdapterWarning;

		{
			std::lock_guard<std::mutex> lk(s_mutex);
			std::swap(s_controller_payload_swap, s_controller_payload);
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
			libusb_interrupt_transfer(s_handle, s_endpoint_out, s_latest_rumble_data, sizeof(s_latest_rumble_data), &size, 16);
	}

	s_rumble_data_available.Reset();
}

#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
static int HotplugCallback(libusb_context* ctx, libusb_device* dev, libusb_hotplug_event event,
	void* user_data)
{
	if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
	{
		if (s_handle == nullptr && CheckDeviceAccess(dev))
		{
			std::lock_guard<std::mutex> lk(s_init_mutex);
			AddGCAdapter(dev);
		}
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
		if (s_libusb_hotplug_enabled)
		{
			static timeval tv = { 0, 500000 };
			libusb_handle_events_timeout(s_libusb_context, &tv);
		}
		else
		{
			if (s_handle == nullptr)
			{
				std::lock_guard<std::mutex> lk(s_init_mutex);
				Setup();
				if (s_detected && s_detect_callback != nullptr)
					s_detect_callback();
			}
			Common::SleepCurrentThread(500);
		}
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
		s_adapter_detect_thread.join();
	}
}

static void Setup()
{
	libusb_device** list;
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
	libusb_interrupt_transfer(s_handle, s_endpoint_out, &payload, sizeof(payload), &tmp, 16);

	s_adapter_thread_running.Set(true);
    s_adapter_input_thread = std::thread(Read);
    s_adapter_output_thread = std::thread(Write);

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

GCPadStatus Input(int chan)
{
	if (!UseAdapter())
		return{};

	if (s_handle == nullptr || !s_detected)
		return{};

	if(AdapterError())
	{
		GCPadStatus centered_status = GCPadStatus();
		centered_status.stickX = centered_status.stickY =
		centered_status.substickX = centered_status.substickY =
		/* these are all the same */ GCPadStatus::MAIN_STICK_CENTER_X;

		return centered_status;
	}

	int payload_size = 0;
	u8 controller_payload_copy[37];

	{
		std::lock_guard<std::mutex> lk(s_mutex);
		std::copy(std::begin(s_controller_payload), std::end(s_controller_payload),
			std::begin(controller_payload_copy));
		payload_size = s_controller_payload_size.load();
	}

	GCPadStatus pad = {};
	if (payload_size != sizeof(controller_payload_copy) ||
		controller_payload_copy[0] != LIBUSB_DT_HID)
	{
		ERROR_LOG(SERIALINTERFACE, "error reading payload (size: %d, type: %02x)", payload_size,
			controller_payload_copy[0]);
		Reset();
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
			GCPadStatus centered_status = GCPadStatus();
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

	s_latest_rumble_data[0] = 0x11;
	for (int i = 0; i < 4; i++)
		s_latest_rumble_data[i + 1] = s_controller_rumble[i];

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

		unsigned char rumble[5] = { 0x11, s_controller_rumble[0], s_controller_rumble[1],
															 s_controller_rumble[2], s_controller_rumble[3] };
		int size = 0;

		libusb_interrupt_transfer(s_handle, s_endpoint_out, rumble, sizeof(rumble), &size, 16);
		// Netplay sends invalid data which results in size = 0x00.  Ignore it.
		if (size != 0x05 && size != 0x00)
		{
			ERROR_LOG(SERIALINTERFACE, "error writing rumble (size: %d)", size);
			Reset();
		}
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
