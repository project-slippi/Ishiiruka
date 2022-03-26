// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#include "InputCommon/ControllerInterface/DInput/DInputKeyboardMouse.h"

#include <algorithm>
#include <string>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/Core.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/DInput/DInput.h"

// (lower would be more sensitive) user can lower sensitivity by setting range
// seems decent here ( at 8 ), I don't think anyone would need more sensitive than this
// and user can lower it much farther than they would want to with the range
#define MOUSE_AXIS_SENSITIVITY 8

// if input hasn't been received for this many ms, mouse input will be skipped
// otherwise it is just some crazy value
#define DROP_INPUT_TIME 250

namespace ciface
{
namespace DInput
{
extern double cursor_sensitivity = 15.0;
extern unsigned char center_mouse_key = 'K';

static const struct
{
	const BYTE code;
	const char* const name;
} named_keys[] = {
#include "InputCommon/ControllerInterface/DInput/NamedKeys.h"  // NOLINT
};

// lil silly
static HWND m_hwnd;

void InitKeyboardMouse(IDirectInput8* const idi8, HWND _hwnd)
{
	m_hwnd = _hwnd;

	// mouse and keyboard are a combined device, to allow shift+click and stuff
	// if that's dumb, I will make a VirtualDevice class that just uses ranges of inputs/outputs from
	// other devices
	// so there can be a separated Keyboard and mouse, as well as combined KeyboardMouse

	LPDIRECTINPUTDEVICE8 kb_device = nullptr;
	LPDIRECTINPUTDEVICE8 mo_device = nullptr;

	if (SUCCEEDED(idi8->CreateDevice(GUID_SysKeyboard, &kb_device, nullptr)) &&
		SUCCEEDED(kb_device->SetDataFormat(&c_dfDIKeyboard)) &&
		SUCCEEDED(kb_device->SetCooperativeLevel(nullptr, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)) &&
		SUCCEEDED(idi8->CreateDevice(GUID_SysMouse, &mo_device, nullptr)) &&
		SUCCEEDED(mo_device->SetDataFormat(&c_dfDIMouse2)) &&
		SUCCEEDED(mo_device->SetCooperativeLevel(nullptr, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
	{
		g_controller_interface.AddDevice(std::make_shared<KeyboardMouse>(kb_device, mo_device));
		return;
	}

	if (kb_device)
		kb_device->Release();
	if (mo_device)
		mo_device->Release();
}

void Pass_Main_Frame_to_Keyboard_and_Mouse(CFrame* _main_frame)
{
	main_frame = _main_frame;
}
void Save_Keyboard_and_Mouse_Settings()
{
	std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + "Mouse_and_Keyboard_Settings.ini";
	IniFile inifile;
	inifile.Load(ini_filename);
	IniFile::Section *section = inifile.GetOrCreateSection("MouseAndKeyboardSettings");
	section->Set("MouseCursorSensitivity", cursor_sensitivity);
	section->Set("CenterMouseKey", std::to_string(center_mouse_key));
	inifile.Save(ini_filename);
}

void Load_Keyboard_and_Mouse_Settings()
{
	std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + "Mouse_and_Keyboard_Settings.ini";
	IniFile inifile;
	inifile.Load(ini_filename);

	IniFile::Section *section = inifile.GetOrCreateSection("MouseAndKeyboardSettings");

	// Sage 3/26/2022: The extra faffing about with the string instead of reading it in with a default value
	//				  was added because Inifile doesn't support chars as numbers for some reason.
	if (section->Exists("CenterMouseKey"))
	{
		std::string temp_string{};
		section->Get("CenterMouseKey", &temp_string);
		if (temp_string.size() == 0)
		{
			center_mouse_key = 'K';
		}
		else
		{
			center_mouse_key = static_cast<unsigned char>(std::stoul(temp_string));
		}
	}
	else
	{
		center_mouse_key = 'K';
	}

	section->Get("MouseCursorSensitivity", &cursor_sensitivity, 15.0);
}

KeyboardMouse::~KeyboardMouse()
{
	// kb
	m_kb_device->Unacquire();
	m_kb_device->Release();
	// mouse
	m_mo_device->Unacquire();
	m_mo_device->Release();
}

KeyboardMouse::KeyboardMouse(const LPDIRECTINPUTDEVICE8 kb_device,
	const LPDIRECTINPUTDEVICE8 mo_device)
	: m_kb_device(kb_device), m_mo_device(mo_device)
{
	m_kb_device->Acquire();
	m_mo_device->Acquire();

	m_last_update = GetTickCount();

	ZeroMemory(&m_state_in, sizeof(m_state_in));

	// KEYBOARD
	// add keys
	for (u8 i = 0; i < sizeof(named_keys) / sizeof(*named_keys); ++i)
		AddInput(new Key(i, m_state_in.keyboard[named_keys[i].code]));

	// MOUSE
	// get caps
	DIDEVCAPS mouse_caps;
	ZeroMemory(&mouse_caps, sizeof(mouse_caps));
	mouse_caps.dwSize = sizeof(mouse_caps);
	m_mo_device->GetCapabilities(&mouse_caps);
	// mouse buttons
	for (u8 i = 0; i < mouse_caps.dwButtons; ++i)
		AddInput(new Button(i, m_state_in.mouse.rgbButtons[i]));
	// mouse axes
	for (unsigned int i = 0; i < mouse_caps.dwAxes; ++i)
	{
		const LONG& ax = (&m_state_in.mouse.lX)[i];

		// each axis gets a negative and a positive input instance associated with it
		AddInput(new Axis(i, ax, (2 == i) ? -1 : -MOUSE_AXIS_SENSITIVITY));
		AddInput(new Axis(i, ax, -(2 == i) ? 1 : MOUSE_AXIS_SENSITIVITY));
	}
	// cursor, with a hax for-loop
	for (unsigned int i = 0; i < 4; ++i)
		AddInput(new Cursor(!!(i & 2), (&m_state_in.cursor.x)[i / 2], !!(i & 1)));
}

void KeyboardMouse::GetMousePos(ControlState* const x, ControlState* const y)
{
	POINT temporary_point = {0, 0};
	GetCursorPos(&temporary_point);

	// Sage 3/20/2022: This was for debugging before I figured out how to get the main frame renderer focus
	/*if (!(GetAsyncKeyState(release_mouse_from_screen_jail) & 0x8000)
	    && ::Core::IsRunningAndStarted()
	    && main_frame->RendererHasFocus())*/
	if (::Core::IsRunningAndStarted() && main_frame->RendererHasFocus())
	{
		// Sage 3/20/2020: I hate the way this Show Cursor function works. This is the only function I have ever
		// actively despised

		ShowCursor(FALSE);                         // decrement so I can acquire the actual value
		int show_cursor_number = ShowCursor(TRUE); // acquire actual value on increment so the number is unchanged
		if (show_cursor_number >= 0) // check to see if the number is greater than or equal to 0 because the cursor is
		                             // shown if the number is greater than or equal to 0
			ShowCursor(FALSE); // decrement the value so the cursor is actually invisible

		// Sage 3/20/2022: I didn't know that ClipCursor existed when I wrote this, but that might be able
		//				  to replace the code below this comment.
		double fraction_of_screen_to_lock_mouse_in_x = screen_width / (cursor_sensitivity * screen_ratio);
		double fraction_of_screen_to_lock_mouse_in_y = screen_height / cursor_sensitivity;

		// bind x value of mouse pos to within a fraction of the screen from center
		if (temporary_point.x > (center_of_screen.x + fraction_of_screen_to_lock_mouse_in_x))
			temporary_point.x = static_cast<long>(center_of_screen.x + fraction_of_screen_to_lock_mouse_in_x);

		if (temporary_point.x < (center_of_screen.x - fraction_of_screen_to_lock_mouse_in_x))
			temporary_point.x = static_cast<long>(center_of_screen.x - fraction_of_screen_to_lock_mouse_in_x);

		// bind y value of mouse pos to within a fraction of the screen from center
		if (temporary_point.y > (center_of_screen.y + fraction_of_screen_to_lock_mouse_in_y))
			temporary_point.y = static_cast<long>(center_of_screen.y + fraction_of_screen_to_lock_mouse_in_y);

		if (temporary_point.y < (center_of_screen.y - fraction_of_screen_to_lock_mouse_in_y))
			temporary_point.y = static_cast<long>(center_of_screen.y - fraction_of_screen_to_lock_mouse_in_y);

		SetCursorPos(temporary_point.x, temporary_point.y);
	}
	else
	{
		ShowCursor(FALSE);                         // decrement so I can acquire the actual value
		int show_cursor_number = ShowCursor(TRUE); // acquire actual value on increment so the number is unchanged
		if (show_cursor_number <= 0) // check to see if the number is less than or equal to 0 because the cursor is
		                             // shown if the number is greater than or equal to 0
			ShowCursor(TRUE); // decrement the value so the cursor is actually invisible
	}

	// See If Origin Reset Is Pressed
	if (player_requested_mouse_center ||
	    (::Core::GetState() == ::Core::CORE_UNINITIALIZED &&
	     main_frame->RendererHasFocus())) // Sage 3/20/2022: I don't think this works very well with boot to pause, but
	                                      // it does work with normal boot
	{
		// Move cursor to the center of the screen if the origin reset key is pressed
		SetCursorPos(center_of_screen.x, center_of_screen.y);
		temporary_point.x = center_of_screen.x;
		temporary_point.y = center_of_screen.y;
	}

	// Sage 3/7/2022: Everything more than assigning point.x and point.y directly to the controller's state
	//				 is to normalize the coordinates since it seems like dolphin wants the inputs from -1.0 to 1.0i
	*x = ((ControlState)((temporary_point.x) / (ControlState)screen_width) - 0.5) * (cursor_sensitivity * screen_ratio);
	*y = ((ControlState)((temporary_point.y) / (ControlState)screen_height) - 0.5) * cursor_sensitivity;
}

void KeyboardMouse::UpdateInput()
{
	DIMOUSESTATE2 temporary_mouse_state;

	// Sage 3/8/2022: I think the code below causes some instability in the axis control inputs.
	//				 I am disabling it temporarily and will only re-enable it if the controls stop working.

	//// if mouse position hasn't been updated in a short while, skip a dev state
	// DWORD cur_time = GetTickCount();
	// if (cur_time - last_update > DROP_INPUT_TIME)
	//{
	//	// set axes to zero
	//	ZeroMemory(&current_state.mouse, sizeof(current_state.mouse));
	//	// skip this input state
	//	mouse->GetDeviceState(sizeof(temporary_mouse_state), &temporary_mouse_state);
	// }
	// last_update = cur_time;

	HRESULT keyboard_error_state = m_kb_device->GetDeviceState(sizeof(current_state.keyboard), &current_state.keyboard);
	HRESULT mouse_error_state = m_mo_device->GetDeviceState(sizeof(temporary_mouse_state), &temporary_mouse_state);

	if (DIERR_INPUTLOST == keyboard_error_state || DIERR_NOTACQUIRED == keyboard_error_state)
		m_kb_device->Acquire();

	if (DIERR_INPUTLOST == mouse_error_state || DIERR_NOTACQUIRED == mouse_error_state)
		m_mo_device->Acquire();

	if (SUCCEEDED(keyboard_error_state) && SUCCEEDED(mouse_error_state))
	{
		////////////////////////old comment/////////////////////////////////
		// need to smooth out the axes, otherwise it doesn't work for shit//
		////////////////////////////////////////////////////////////////////
		// old comment is correct and all I've done is add a sens modifier to
		// the averaging value (1.0 sensitivity was what the old version used)

		// Sage 3/25/2022: I don't expect these to be used for competition so I'm not adding any UI for
		// inverse_axis_sensitivity 				  It works very well as programmed, axis controls just make it hard to play Melee. I
		//will add UI if 				  there are people who make it known to me they'd like to use axis controls.

		clamp<double>(inverse_axis_sensitivity, 0.51, 100.0);
		for (unsigned int i = 0; i < 3; ++i)
		{
			double post_calculation_value =
			    static_cast<double>((&current_state.mouse.lX)[i] += (&temporary_mouse_state.lX)[i]);
			post_calculation_value /= (2.0 * inverse_axis_sensitivity);
			(&current_state.mouse.lX)[i] = static_cast<long>(post_calculation_value);
		}

		// copy over the buttons
		memcpy(current_state.mouse.rgbButtons, temporary_mouse_state.rgbButtons,
		       sizeof(current_state.mouse.rgbButtons));

		// update mouse cursor
		GetMousePos(&current_state.cursor.x, &current_state.cursor.y);
	}
	// Sage 3/20/2022: This needs to stay at the end of UpdateInput() to create a 2-frame delay which
	//				  is roughly what I measured on a real controller. (It was variable from 1.5ish-2.5ish frames on my
	//controller)
	if (GetAsyncKeyState(center_mouse_key) && 0x8000)
		player_requested_mouse_center = true;
	else
		player_requested_mouse_center = false;
}

std::string KeyboardMouse::GetName() const
{
	return "Keyboard Mouse";
}

std::string KeyboardMouse::GetSource() const
{
	return DINPUT_SOURCE_NAME;
}

// names
std::string KeyboardMouse::Key::GetName() const
{
	return named_keys[m_index].name;
}

std::string KeyboardMouse::Button::GetName() const
{
	return std::string("Click ") + char('0' + m_index);
}

std::string KeyboardMouse::Axis::GetName() const
{
	static char tmpstr[] = "Axis ..";
	tmpstr[5] = (char)('X' + m_index);
	tmpstr[6] = (m_range < 0 ? '-' : '+');
	return tmpstr;
}

std::string KeyboardMouse::Cursor::GetName() const
{
	static char tmpstr[] = "Cursor ..";
	tmpstr[7] = (char)('X' + m_index);
	tmpstr[8] = (m_positive ? '+' : '-');
	return tmpstr;
}

// get/set state
ControlState KeyboardMouse::Key::GetState() const
{
	return (m_key != 0);
}

ControlState KeyboardMouse::Button::GetState() const
{
	return (m_button != 0);
}

ControlState KeyboardMouse::Axis::GetState() const
{
	return std::max(0.0, ControlState(m_axis) / m_range);
}

ControlState KeyboardMouse::Cursor::GetState() const
{
	return std::max(0.0, ControlState(m_axis) / (m_positive ? 1.0 : -1.0));
}
}
}
