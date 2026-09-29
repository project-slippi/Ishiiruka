// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/GCPadEmu.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Host.h"

static const u16 button_bitmasks[] = {
	PAD_BUTTON_A,
	PAD_BUTTON_B,
	PAD_BUTTON_X,
	PAD_BUTTON_Y,
	PAD_TRIGGER_Z,
	PAD_BUTTON_START,
	0  // MIC HAX
};

static const u16 trigger_bitmasks[] = {
	PAD_TRIGGER_L, PAD_TRIGGER_R,
};

static const u16 dpad_bitmasks[] = { PAD_BUTTON_UP, PAD_BUTTON_DOWN, PAD_BUTTON_LEFT,
PAD_BUTTON_RIGHT };

static const char* const named_buttons[] = { "A", "B", "X", "Y", "Z", _trans("Start") };

static const char* const named_triggers[] = {
	// i18n: The left trigger button (labeled L on real controllers)
	_trans("L"),
	// i18n: The right trigger button (labeled R on real controllers)
	_trans("R"),
	// i18n: The left trigger button (labeled L on real controllers) used as an analog input
	_trans("L-Analog"),
	// i18n: The right trigger button (labeled R on real controllers) used as an analog input
	_trans("R-Analog") };

// Digital "rectangle" controller inputs. A/B/X/Y/Z/Start, L/R and the D-Pad come from their usual groups.
static const char* const named_rectangle_inputs[] = {
	_trans("Up"), _trans("Down"), _trans("Left"), _trans("Right"),
	_trans("Mod X"), _trans("Mod Y"),
	_trans("C-Up"), _trans("C-Down"), _trans("C-Left"), _trans("C-Right"),
	_trans("Lightshield"), _trans("Midshield") };

enum RectangleInput
{
	RECTANGLE_UP,
	RECTANGLE_DOWN,
	RECTANGLE_LEFT,
	RECTANGLE_RIGHT,
	RECTANGLE_MOD_X,
	RECTANGLE_MOD_Y,
	RECTANGLE_C_UP,
	RECTANGLE_C_DOWN,
	RECTANGLE_C_LEFT,
	RECTANGLE_C_RIGHT,
	RECTANGLE_LIGHTSHIELD,
	RECTANGLE_MIDSHIELD,
};

enum RectangleSetting
{
	RECTANGLE_ENABLE,
	RECTANGLE_RULESET_LIMITS,
	RECTANGLE_CROUCH_WALK_OS,
};

// Key names for the default keyboard layout, which differ per input backend.
struct DefaultKeys
{
	const char* left_bracket;
	const char* right_bracket;
	const char* space;
	const char* comma;
	const char* minus;
	const char* equals;
	const char* up;
	const char* down;
	const char* left;
	const char* right;
};

#ifdef _WIN32
static const DefaultKeys default_keys = { "LBRACKET", "RBRACKET", "SPACE", "COMMA", "MINUS", "EQUALS",
	"UP", "DOWN", "LEFT", "RIGHT" };
#elif __APPLE__
static const DefaultKeys default_keys = { "[", "]", "Space", ",", "-", "=",
	"Up Arrow", "Down Arrow", "Left Arrow", "Right Arrow" };
#else
static const DefaultKeys default_keys = { "bracketleft", "bracketright", "space", "comma", "minus", "equal",
	"Up", "Down", "Left", "Right" };
#endif

GCPad::GCPad(const unsigned int index) : m_index(index)
{
	// buttons
	groups.emplace_back(m_buttons = new Buttons(_trans("Buttons")));
	for (unsigned int i = 0; i < sizeof(named_buttons) / sizeof(*named_buttons); ++i)
		m_buttons->controls.emplace_back(new ControlGroup::Input(named_buttons[i]));

	// sticks
	groups.emplace_back(m_main_stick = new AnalogStick("Main Stick", _trans("Control Stick"),
		DEFAULT_PAD_STICK_RADIUS));
	groups.emplace_back(m_c_stick =
		new AnalogStick("C-Stick", _trans("C Stick"), DEFAULT_PAD_STICK_RADIUS));

	// triggers
	groups.emplace_back(m_triggers = new MixedTriggers(_trans("Triggers")));
	for (auto& named_trigger : named_triggers)
		m_triggers->controls.emplace_back(new ControlGroup::Input(named_trigger));

	// rumble
	groups.emplace_back(m_rumble = new ControlGroup(_trans("Rumble")));
	m_rumble->controls.emplace_back(new ControlGroup::Output(_trans("Motor")));

	// Microphone
	groups.emplace_back(m_mic = new Buttons(_trans("Microphone")));
	m_mic->controls.emplace_back(new ControlGroup::Input(_trans("Button")));

	// dpad
	groups.emplace_back(m_dpad = new Buttons(_trans("D-Pad")));
	for (auto& named_direction : named_directions)
		m_dpad->controls.emplace_back(new ControlGroup::Input(named_direction));

	// options
	groups.emplace_back(m_options = new ControlGroup(_trans("Options")));
	m_options->boolean_settings.emplace_back(
		std::make_unique<ControlGroup::BackgroundInputSetting>(_trans("Background Input")));
	m_options->boolean_settings.emplace_back(std::make_unique<ControlGroup::BooleanSetting>(
		_trans("Iterative Input"), false, ControlGroup::SettingType::VIRTUAL));

	// rectangle (digital buttons in place of the control stick and C-stick, for Melee)
	groups.emplace_back(m_rectangle = new ControlGroup("Rectangle", _trans("Rectangle (Melee)")));
	for (auto& named_input : named_rectangle_inputs)
		m_rectangle->controls.emplace_back(new ControlGroup::Input(named_input));
	m_rectangle->boolean_settings.emplace_back(
		std::make_unique<ControlGroup::BooleanSetting>(_trans("Enable"), false));
	m_rectangle->boolean_settings.emplace_back(
		std::make_unique<ControlGroup::BooleanSetting>(_trans("Controller Ruleset Limits"), true));
	m_rectangle->boolean_settings.emplace_back(
		std::make_unique<ControlGroup::BooleanSetting>(_trans("Crouch Walk Option Select"), false));
}

std::string GCPad::GetName() const
{
	return std::string("GCPad") + char('1' + m_index);
}

ControllerEmu::ControlGroup* GCPad::GetGroup(PadGroup group)
{
	switch (group)
	{
	case PadGroup::Buttons:
		return m_buttons;
	case PadGroup::MainStick:
		return m_main_stick;
	case PadGroup::CStick:
		return m_c_stick;
	case PadGroup::DPad:
		return m_dpad;
	case PadGroup::Triggers:
		return m_triggers;
	case PadGroup::Rumble:
		return m_rumble;
	case PadGroup::Mic:
		return m_mic;
	case PadGroup::Options:
		return m_options;
	case PadGroup::Rectangle:
		return m_rectangle;
	default:
		return nullptr;
	}
}

GCPadStatus GCPad::GetInput() const
{
	auto lock = ControllerEmu::GetStateLock();

	if (IsRectangleEnabled())
		return GetRectangleInput();
	m_rectangle_controller.Reset();

	ControlState x, y, triggers[2];
	GCPadStatus pad = {};

	// buttons
	m_buttons->GetState(&pad.button, button_bitmasks);

	// set analog A/B analog to full or w/e, prolly not needed
	if (pad.button & PAD_BUTTON_A)
		pad.analogA = 0xFF;
	if (pad.button & PAD_BUTTON_B)
		pad.analogB = 0xFF;

	// dpad
	m_dpad->GetState(&pad.button, dpad_bitmasks);

	// sticks
	m_main_stick->GetState(&x, &y);
	pad.stickX =
		static_cast<u8>(GCPadStatus::MAIN_STICK_CENTER_X + (x * GCPadStatus::MAIN_STICK_RADIUS));
	pad.stickY =
		static_cast<u8>(GCPadStatus::MAIN_STICK_CENTER_Y + (y * GCPadStatus::MAIN_STICK_RADIUS));

	m_c_stick->GetState(&x, &y);
	pad.substickX =
		static_cast<u8>(GCPadStatus::C_STICK_CENTER_X + (x * GCPadStatus::C_STICK_RADIUS));
	pad.substickY =
		static_cast<u8>(GCPadStatus::C_STICK_CENTER_Y + (y * GCPadStatus::C_STICK_RADIUS));

	// triggers
	m_triggers->GetState(&pad.button, trigger_bitmasks, triggers);
	pad.triggerLeft = static_cast<u8>(triggers[0] * 0xFF);
	pad.triggerRight = static_cast<u8>(triggers[1] * 0xFF);

	return pad;
}

void GCPad::SampleRectangleInput() const
{
	auto lock = ControllerEmu::GetStateLock();

	if (IsRectangleEnabled())
		GetRectangleInput();
}

bool GCPad::IsRectangleEnabled() const
{
	return m_rectangle->boolean_settings[RECTANGLE_ENABLE]->GetValue();
}

GCPadStatus GCPad::GetRectangleInput() const
{
	u16 buttons = 0;
	ControlState analog_triggers[2];
	m_buttons->GetState(&buttons, button_bitmasks);
	m_dpad->GetState(&buttons, dpad_bitmasks);
	m_triggers->GetState(&buttons, trigger_bitmasks, analog_triggers);

	const auto held = [this](RectangleInput input) {
		return m_rectangle->controls[input]->control_ref->State() > 0.5;
	};

	MeleeRectangle::Inputs inputs;
	inputs.up = held(RECTANGLE_UP);
	inputs.down = held(RECTANGLE_DOWN);
	inputs.left = held(RECTANGLE_LEFT);
	inputs.right = held(RECTANGLE_RIGHT);
	inputs.mod_x = held(RECTANGLE_MOD_X);
	inputs.mod_y = held(RECTANGLE_MOD_Y);
	inputs.c_up = held(RECTANGLE_C_UP);
	inputs.c_down = held(RECTANGLE_C_DOWN);
	inputs.c_left = held(RECTANGLE_C_LEFT);
	inputs.c_right = held(RECTANGLE_C_RIGHT);
	inputs.lightshield = held(RECTANGLE_LIGHTSHIELD);
	inputs.midshield = held(RECTANGLE_MIDSHIELD);
	inputs.a = (buttons & PAD_BUTTON_A) != 0;
	inputs.b = (buttons & PAD_BUTTON_B) != 0;
	inputs.x = (buttons & PAD_BUTTON_X) != 0;
	inputs.y = (buttons & PAD_BUTTON_Y) != 0;
	inputs.z = (buttons & PAD_TRIGGER_Z) != 0;
	inputs.l = (buttons & PAD_TRIGGER_L) != 0;
	inputs.r = (buttons & PAD_TRIGGER_R) != 0;
	inputs.start = (buttons & PAD_BUTTON_START) != 0;
	inputs.dpad_up = (buttons & PAD_BUTTON_UP) != 0;
	inputs.dpad_down = (buttons & PAD_BUTTON_DOWN) != 0;
	inputs.dpad_left = (buttons & PAD_BUTTON_LEFT) != 0;
	inputs.dpad_right = (buttons & PAD_BUTTON_RIGHT) != 0;

	MeleeRectangle::Settings settings;
	settings.ruleset_limits = m_rectangle->boolean_settings[RECTANGLE_RULESET_LIMITS]->GetValue();
	settings.crouch_walk_os = m_rectangle->boolean_settings[RECTANGLE_CROUCH_WALK_OS]->GetValue();

	// Emulated time, so travel time and lockouts are relative to what the game sees regardless of
	// emulation speed.
	const s64 now_us = static_cast<s64>(static_cast<double>(CoreTiming::GetTicks()) * 1000000.0 /
		SystemTimers::GetTicksPerSecond());

	return m_rectangle_controller.Update(inputs, settings, now_us);
}

void GCPad::SetOutput(const ControlState strength)
{
	auto lock = ControllerEmu::GetStateLock();
	m_rumble->controls[0]->control_ref->State(strength);
}

void GCPad::LoadDefaults(const ControllerInterface& ciface)
{
	ControllerEmu::LoadDefaults(ciface);

	// The keyboard defaults to the community B0XX keyboard layout (as used by b0xx-ahk), played as a
	// rectangle controller. The Control Stick and C Stick groups are left unbound since the Rectangle
	// group replaces them.
	m_rectangle->boolean_settings[RECTANGLE_ENABLE]->SetValue(true);

	// stick modifiers to 50 %
	m_main_stick->controls[4]->control_ref->range = 0.5f;
	m_c_stick->controls[4]->control_ref->range = 0.5f;

	// Left hand: L, Left, Down, Right; thumb on Mod X and Mod Y
	m_triggers->SetControlExpression(0, "Q");  // L
	m_rectangle->SetControlExpression(RECTANGLE_LEFT, "2");
	m_rectangle->SetControlExpression(RECTANGLE_DOWN, "3");
	m_rectangle->SetControlExpression(RECTANGLE_RIGHT, "4");
	m_rectangle->SetControlExpression(RECTANGLE_MOD_X, "V");
	m_rectangle->SetControlExpression(RECTANGLE_MOD_Y, "B");

	m_buttons->SetControlExpression(5, "7");  // Start

	// Right hand: R, Y, Lightshield, Midshield over B, X, Z, Up
	m_triggers->SetControlExpression(1, "9");  // R
	m_buttons->SetControlExpression(3, "0");   // Y
	m_rectangle->SetControlExpression(RECTANGLE_LIGHTSHIELD, default_keys.minus);
	m_rectangle->SetControlExpression(RECTANGLE_MIDSHIELD, default_keys.equals);
	m_buttons->SetControlExpression(1, "O");   // B
	m_buttons->SetControlExpression(2, "P");   // X
	m_buttons->SetControlExpression(4, default_keys.left_bracket);  // Z
	m_rectangle->SetControlExpression(RECTANGLE_UP, default_keys.right_bracket);

	// Right thumb: A surrounded by the C buttons, with C-Down below them
	m_buttons->SetControlExpression(0, "M");  // A
	m_rectangle->SetControlExpression(RECTANGLE_C_LEFT, "N");
	m_rectangle->SetControlExpression(RECTANGLE_C_RIGHT, default_keys.comma);
	m_rectangle->SetControlExpression(RECTANGLE_C_UP, "K");
	m_rectangle->SetControlExpression(RECTANGLE_C_DOWN, default_keys.space);

	// D-Pad
	m_dpad->SetControlExpression(0, default_keys.up);
	m_dpad->SetControlExpression(1, default_keys.down);
	m_dpad->SetControlExpression(2, default_keys.left);
	m_dpad->SetControlExpression(3, default_keys.right);
}

bool GCPad::GetMicButton() const
{
	auto lock = ControllerEmu::GetStateLock();
	return (0.0f != m_mic->controls.back()->control_ref->State());
}
