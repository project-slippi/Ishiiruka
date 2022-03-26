// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Input/GCPadInputConfigDiag.h"

#include <Windows.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/button.h>
#include <wx/string.h>

#include "Core/ConfigManager.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "InputCommon/ControllerInterface/DInput/DInputKeyboardMouse.h"

void Sensitivity_Spin_Control_Callback(wxSpinDoubleEvent &_event)
{
	ciface::DInput::cursor_sensitivity = _event.GetValue();
	ciface::DInput::Save_Keyboard_and_Mouse_Settings();
}

void Center_Mouse_Key_Button_Callback(wxCommandEvent &_event)
{
	static_cast<wxButton*>(_event.GetEventObject())->SetLabel("[ waiting ]");
	static constexpr unsigned char highest_virtual_key_hex = 0xFE;
	bool listening = true;
	while (listening)
	{
		for (unsigned char i = 0; i < highest_virtual_key_hex; i++) 
		{
			if (GetAsyncKeyState(i) & 0x8000) 
			{
				ciface::DInput::center_mouse_key = i;
				listening = false;
				break;
				
			}
			
		}
		
	}
	static_cast<wxButton *>(_event.GetEventObject())->SetLabel(wxGetTranslation((char)ciface::DInput::center_mouse_key));
	ciface::DInput::Save_Keyboard_and_Mouse_Settings();
}

GCPadInputConfigDialog::GCPadInputConfigDialog(wxWindow* const parent, InputConfig& config,
	const wxString& name, const int port_num)
	: InputConfigDialog(parent, config, name, port_num)
{
	const int space5 = FromDIP(5);

	auto* const device_chooser = CreateDeviceChooserGroupBox();
	auto* const reset_sizer = CreaterResetGroupBox(wxHORIZONTAL);
	auto* const profile_chooser = CreateProfileChooserGroupBox();

	auto* const group_box_buttons =
		new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::Buttons), this, this);
	auto* const group_box_main_stick =
		new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::MainStick), this, this);
	auto* const group_box_c_stick =
		new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::CStick), this, this);
	auto* const group_box_dpad =
		new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::DPad), this, this);
	auto* const group_box_triggers =
		new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::Triggers), this, this);
	auto* const group_box_rumble =
		new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::Rumble), this, this);
	auto* const group_box_options =
		new ControlGroupBox(Pad::GetGroup(port_num, PadGroup::Options), this, this);

	// Keyboard and Mouse Settings
	ciface::DInput::Load_Keyboard_and_Mouse_Settings();
	auto*sensitivity_sizer = new wxBoxSizer(wxVERTICAL);
	auto* sensitivity_static_box = new wxStaticBoxSizer{wxHORIZONTAL, this, "Keyboard and Mouse"};
	sensitivity_static_box->Add(new wxStaticText(this, wxID_ANY, wxGetTranslation("Sensitivity")), 0, wxALIGN_CENTER_VERTICAL);
	sensitivity_static_box->AddSpacer(space5);
	auto* senstivity_spinctrl = new wxSpinCtrlDouble{this, -1,"Sensitivity",wxDefaultPosition,wxDefaultSize,16384L,0,100.00,ciface::DInput::cursor_sensitivity,1.0,"Sensitivity"};
	senstivity_spinctrl->Bind(wxEVT_SPINCTRLDOUBLE, &Sensitivity_Spin_Control_Callback);
	sensitivity_static_box->Add(senstivity_spinctrl);
	sensitivity_static_box->AddSpacer(space5);
	sensitivity_static_box->Add(new wxStaticText(this, wxID_ANY, wxGetTranslation("Center Mouse")), 0, wxALIGN_CENTER_VERTICAL);
	auto* center_mouse_key_button = new wxButton{this, wxID_ANY, wxGetTranslation(std::string{(char)ciface::DInput::center_mouse_key})};
	center_mouse_key_button->SetToolTip(wxGetTranslation("Left-click to detect input."));
	center_mouse_key_button->Bind(wxEVT_BUTTON, &Center_Mouse_Key_Button_Callback);
	sensitivity_static_box->Add(center_mouse_key_button);
	sensitivity_sizer->Add(sensitivity_static_box);
	// End Keyboard and Mouse Settings

	auto* const triggers_rumble_sizer = new wxBoxSizer(wxVERTICAL);
	triggers_rumble_sizer->Add(group_box_triggers, 0, wxEXPAND);
	triggers_rumble_sizer->AddSpacer(space5);
	triggers_rumble_sizer->Add(group_box_rumble, 0, wxEXPAND);

	auto* const dpad_options_sizer = new wxBoxSizer(wxVERTICAL);
	dpad_options_sizer->Add(group_box_dpad, 0, wxEXPAND);
	dpad_options_sizer->AddSpacer(space5);
	dpad_options_sizer->Add(group_box_options, 0, wxEXPAND);

	auto* const controls_sizer = new wxBoxSizer(wxHORIZONTAL);
	controls_sizer->AddSpacer(space5);
	controls_sizer->Add(group_box_buttons, 0, wxEXPAND | wxTOP, space5);
	controls_sizer->AddSpacer(space5);
	controls_sizer->Add(group_box_main_stick, 0, wxEXPAND | wxTOP, space5);
	controls_sizer->AddSpacer(space5);
	controls_sizer->Add(group_box_c_stick, 0, wxEXPAND | wxTOP, space5);
	controls_sizer->AddSpacer(space5);
	controls_sizer->Add(triggers_rumble_sizer, 0, wxEXPAND | wxTOP, space5);
	controls_sizer->AddSpacer(space5);
	controls_sizer->Add(dpad_options_sizer, 0, wxEXPAND | wxTOP, space5);
	controls_sizer->AddSpacer(space5);

	auto* const dio = new wxBoxSizer(wxHORIZONTAL);
	dio->AddSpacer(space5);
	dio->Add(device_chooser, 2, wxEXPAND);
	dio->AddSpacer(space5);
	dio->Add(reset_sizer, 1, wxEXPAND);
	dio->AddSpacer(space5);
	dio->Add(profile_chooser, 2, wxEXPAND);
	dio->AddSpacer(space5);

	auto* const szr_main = new wxBoxSizer(wxVERTICAL);
	szr_main->AddSpacer(space5);
	szr_main->Add(dio);
	szr_main->AddSpacer(space5);
	szr_main->Add(controls_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
	szr_main->AddSpacer(space5);
	szr_main->Add(sensitivity_sizer, 0, wxALIGN_CENTER_HORIZONTAL);
	szr_main->AddSpacer(space5);
	szr_main->Add(CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	szr_main->AddSpacer(space5);

	SetSizerAndFit(szr_main);
	Center();

	UpdateDeviceComboBox();
	UpdateProfileComboBox();

	UpdateGUI();
}
