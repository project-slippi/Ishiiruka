// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <mutex>

#include "Common/Thread.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

#ifdef CIFACE_USE_XINPUT
#include "InputCommon/ControllerInterface/XInput/XInput.h"
#endif
#ifdef CIFACE_USE_DINPUT
#include "InputCommon/ControllerInterface/DInput/DInput.h"
#endif
#ifdef CIFACE_USE_XLIB
#include "InputCommon/ControllerInterface/Xlib/XInput2.h"
#endif
#ifdef CIFACE_USE_OSX
#include "InputCommon/ControllerInterface/OSX/OSX.h"
#include "InputCommon/ControllerInterface/Quartz/Quartz.h"
#endif
#ifdef CIFACE_USE_SDL
#include "InputCommon/ControllerInterface/SDL/SDL.h"
#endif
#ifdef CIFACE_USE_ANDROID
#include "InputCommon/ControllerInterface/Android/Android.h"
#endif
#ifdef CIFACE_USE_EVDEV
#include "InputCommon/ControllerInterface/evdev/evdev.h"
#endif
#ifdef CIFACE_USE_PIPES
#include "InputCommon/ControllerInterface/Pipes/Pipes.h"
#endif

#include "InputCommon/InputConfig.h"
#include "InputCommon/ControllerEmu.h"
#include "Core/HW/GCPad.h"

using namespace ciface::ExpressionParser;

namespace
{
const ControlState INPUT_DETECT_THRESHOLD = 0.55;
}

ControllerInterface g_controller_interface;

//
// Init
//
// Detect devices and inputs outputs / will make refresh function later
//
void ControllerInterface::Initialize(void* const hwnd)
{
	if (m_is_init)
		return;

	m_hwnd = hwnd;

#ifdef CIFACE_USE_DINPUT
	// nothing needed
#endif
#ifdef CIFACE_USE_XINPUT
	ciface::XInput::Init();
#endif
#ifdef CIFACE_USE_XLIB
	// nothing needed
#endif
#ifdef CIFACE_USE_OSX
	ciface::OSX::Init(hwnd);
	// nothing needed for Quartz
#endif
#ifdef CIFACE_USE_SDL
	ciface::SDL::Init();
#endif
#ifdef CIFACE_USE_ANDROID
	// nothing needed
#endif
#ifdef CIFACE_USE_EVDEV
	ciface::evdev::Init();
#endif
#ifdef CIFACE_USE_PIPES
	// nothing needed
#endif

	m_is_init = true;
	RefreshDevices();
}

void ControllerInterface::RefreshDevices()
{
	if (!m_is_init)
		return;

	{
		std::lock_guard<std::mutex> lk(m_devices_mutex);
		m_devices.clear();
	}

#ifdef CIFACE_USE_DINPUT
	ciface::DInput::PopulateDevices(reinterpret_cast<HWND>(m_hwnd));
#endif
#ifdef CIFACE_USE_XINPUT
	ciface::XInput::PopulateDevices();
#endif
#ifdef CIFACE_USE_XLIB
	ciface::XInput2::PopulateDevices(m_hwnd);
#endif
#ifdef CIFACE_USE_OSX
	ciface::OSX::PopulateDevices(m_hwnd);
	ciface::Quartz::PopulateDevices(m_hwnd);
#endif
#ifdef CIFACE_USE_SDL
	ciface::SDL::PopulateDevices();
#endif
#ifdef CIFACE_USE_ANDROID
	ciface::Android::PopulateDevices();
#endif
#ifdef CIFACE_USE_EVDEV
	ciface::evdev::PopulateDevices();
#endif
#ifdef CIFACE_USE_PIPES
	ciface::Pipes::PopulateDevices();
#endif
}

//
// DeInit
//
// Remove all devices/ call library cleanup functions
//
void ControllerInterface::Shutdown()
{
	if (!m_is_init)
		return;

	{
		std::lock_guard<std::mutex> lk(m_devices_mutex);

		for (const auto& d : m_devices)
		{
			// Set outputs to ZERO before destroying device
			for (ciface::Core::Device::Output* o : d->Outputs())
				o->SetState(0);
		}

		m_devices.clear();
	}

#ifdef CIFACE_USE_XINPUT
	ciface::XInput::DeInit();
#endif
#ifdef CIFACE_USE_DINPUT
	// nothing needed
#endif
#ifdef CIFACE_USE_XLIB
// nothing needed
#endif
#ifdef CIFACE_USE_OSX
	ciface::OSX::DeInit();
	ciface::Quartz::DeInit();
#endif
#ifdef CIFACE_USE_SDL
	// TODO: there seems to be some sort of memory leak with SDL, quit isn't freeing everything up
	SDL_Quit();
#endif
#ifdef CIFACE_USE_ANDROID
	// nothing needed
#endif
#ifdef CIFACE_USE_EVDEV
	ciface::evdev::Shutdown();
#endif

	m_is_init = false;
}

void ControllerInterface::AddDevice(std::shared_ptr<ciface::Core::Device> device)
{
	std::lock_guard<std::mutex> lk(m_devices_mutex);
	// Try to find an ID for this device
	int id = 0;
	while (true)
	{
		const auto it = std::find_if(m_devices.begin(), m_devices.end(), [&device, &id](const auto& d) {
			return d->GetSource() == device->GetSource() && d->GetName() == device->GetName() &&
				d->GetId() == id;
		});
		if (it == m_devices.end())  // no device with the same name with this ID, so we can use it
			break;
		else
			id++;
	}
	device->SetId(id);
	m_devices.emplace_back(std::move(device));
}

void ControllerInterface::RemoveDevice(std::function<bool(const ciface::Core::Device*)> callback)
{
	std::lock_guard<std::mutex> lk(m_devices_mutex);
	m_devices.erase(std::remove_if(m_devices.begin(), m_devices.end(),
		[&callback](const auto& dev) { return callback(dev.get()); }),
		m_devices.end());
}

//
// UpdateInput
//
// Update input for all devices
//
void ControllerInterface::UpdateInput()
{
	// Don't block the UI or CPU thread (to avoid a short but noticeable frame drop)
	if (m_devices_mutex.try_lock())
	{
		std::lock_guard<std::mutex> lk(m_devices_mutex, std::adopt_lock);
		for (const auto& d : m_devices)
			d->UpdateInput();
	}
}

//
// RegisterHotplugCallback
//
// Register a callback to be called from the input backends' hotplug thread
// when there is a new device
//
void ControllerInterface::RegisterHotplugCallback(std::function<void()> callback)
{
	m_hotplug_callbacks.emplace_back(std::move(callback));
}

//
// InvokeHotplugCallbacks
//
// Invoke all callbacks that were registered
//
void ControllerInterface::InvokeHotplugCallbacks() const
{
	for (const auto& callback : m_hotplug_callbacks)
		callback();
}

std::map<int, SlippiPad> ControllerInterface::GetSlippiPads()
{
	std::map<int, SlippiPad> pads;
	// Loop through all input devices
	{
		std::lock_guard<std::mutex> lk(m_devices_mutex);

		for (u32 i = 0; i < m_devices.size(); i++)
		{
			std::shared_ptr<ciface::Core::Device> d = m_devices[i];
			if (d->GetSource() == "Pipe")
			{
				ciface::Pipes::PipeDevice* x = (ciface::Pipes::PipeDevice*)d.get();

				// Find which controller this device is attached to
				for(int j = 0; j < 4; j++)
				{
					const auto device_type = SConfig::GetInstance().m_SIDevice[j];
					if (device_type == 6) //TODO
					{
						ciface::Core::DeviceQualifier device = Pad::GetConfig()->GetController(j)->default_device;
						if (device.name == d->GetName())
						{
							pads[j] = (x)->GetSlippiPad();
						}
					}
				}
			}
		}
	}

	return pads;
}

//
// InputReference :: State
//
// Gets the state of an input reference
// override function for ControlReference::State ...
//
ControlState ControllerInterface::InputReference::State(const ControlState ignore)
{
	if (parsed_expression)
		return parsed_expression->GetValue() * range;
	else
		return 0.0;
}

//
// OutputReference :: State
//
// Set the state of all binded outputs
// overrides ControlReference::State .. combined them so I could make the GUI simple / inputs ==
// same as outputs one list
// I was lazy and it works so watever
//
ControlState ControllerInterface::OutputReference::State(const ControlState state)
{
	if (parsed_expression)
		parsed_expression->SetValue(state);
	return 0.0;
}

//
// UpdateReference
//
// Updates a controlreference's binded devices/controls
// need to call this to re-parse a control reference's expression after changing it
//
void ControllerInterface::UpdateReference(ControllerInterface::ControlReference* ref,
	const ciface::Core::DeviceQualifier& default_device) const
{
	delete ref->parsed_expression;
	ref->parsed_expression = nullptr;

	ControlFinder finder(*this, default_device, ref->is_input);
	ref->parse_error = ParseExpression(ref->expression, finder, &ref->parsed_expression);
}

//
// InputReference :: Detect
//
// Wait for input on all binded devices
// supports not detecting inputs that were held down at the time of Detect start,
// which is useful for those crazy flightsticks that have certain buttons that are always held down
// or some crazy axes or something
// upon input, return pointer to detected Control
// else return nullptr
//
ciface::Core::Device::Control*
ControllerInterface::InputReference::Detect(const unsigned int ms,
	ciface::Core::Device* const device)
{
	unsigned int time = 0;
	std::vector<bool> states(device->Inputs().size());

	if (device->Inputs().size() == 0)
		return nullptr;

	// get starting state of all inputs,
	// so we can ignore those that were activated at time of Detect start
	std::vector<ciface::Core::Device::Input *>::const_iterator i = device->Inputs().begin(),
		e = device->Inputs().end();
	for (std::vector<bool>::iterator state = states.begin(); i != e; ++i)
		*state++ = ((*i)->GetState() > (1 - INPUT_DETECT_THRESHOLD));

	while (time < ms)
	{
		device->UpdateInput();
		i = device->Inputs().begin();
		for (std::vector<bool>::iterator state = states.begin(); i != e; ++i, ++state)
		{
			// detected an input
			if ((*i)->IsDetectable() && (*i)->GetState() > INPUT_DETECT_THRESHOLD)
			{
				// input was released at some point during Detect call
				// return the detected input
				if (false == *state)
					return *i;
			}
			else if ((*i)->GetState() < (1 - INPUT_DETECT_THRESHOLD))
			{
				*state = false;
			}
		}
		Common::SleepCurrentThread(10);
		time += 10;
	}

	// no input was detected
	return nullptr;
}

//
// OutputReference :: Detect
//
// Totally different from the inputReference detect / I have them combined so it was simpler to make
// the GUI.
// The GUI doesn't know the difference between an input and an output / it's odd but I was lazy and
// it was easy
//
// set all binded outputs to <range> power for x milliseconds return false
//
ciface::Core::Device::Control*
ControllerInterface::OutputReference::Detect(const unsigned int ms,
	ciface::Core::Device* const device)
{
	// ignore device

	// don't hang if we don't even have any controls mapped
	if (BoundCount() > 0)
	{
		State(1);
		unsigned int slept = 0;

		// this loop is to make stuff like flashing keyboard LEDs work
		while (ms > (slept += 10))
			Common::SleepCurrentThread(10);

		State(0);
	}
	return nullptr;
}
