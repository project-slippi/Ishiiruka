// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/checkbox.h>
#include <wx/radiobut.h>
#include <wx/event.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>

#include "Core/ConfigManager.h"

#include "DolphinWX/Config/LagConfigPane.h"

LagConfigPane::LagConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	BindEvents();
}

void LagConfigPane::InitializeGUI() {
    
    m_increase_process_priority_checkbox = new wxCheckBox(this, wxID_ANY, _("Increase process priority"));
	m_saturate_polling_thread_priority_checkbox = new wxCheckBox(this, wxID_ANY, _("Saturate polling thread priority"));
	m_use_engine_stabilization_checkbox = new wxCheckBox(this, wxID_ANY, _("Use engine stabilization"));
    // Configs: delay leniency [can fine tune global lag addition by playing on delay] + queue length ?
    m_use_steady_state_engine_stabilization_checkbox = new wxCheckBox(this, wxID_ANY, _("Use steady state stabilization"));
    // Configs: validation-length leniency
    m_use_usb_polling_stabilization_checkbox = new wxCheckBox(this, wxID_ANY, _("Use USB polling stabilization"));
    m_use_official_adapter_timing_reconstruction_if_applicable_checkbox
        = new wxCheckBox(this, wxID_ANY, _("Use poll timing reconstruction when applicable"));
    // Grise si steady state engine stabilization off

	m_engine_frequency_radio_button_5994Hz =
	    new wxRadioButton(this, 1, _("59.94 Hz"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	m_engine_frequency_radio_button_60Hz =
	    new wxRadioButton(this, 2, _("60 Hz"), wxDefaultPosition, wxDefaultSize, 0);

	m_increase_process_priority_checkbox->SetToolTip(_("Increases the priority of the Dolphin process (to High on Windows).\n"
	"Empirical tests have shown Dolphin running slightly slower than it should (ex. 0.02 to 0.03%) with normal priority.\n"
	"This is invisible to the eye but breaks steady state stabilization."));
    m_saturate_polling_thread_priority_checkbox->SetToolTip(
		_("Sets the priority of the adapter polling thread to the maximum of the non-realtime priority span "
          "for user processes. (15 on Windows)"));
    m_use_engine_stabilization_checkbox->SetToolTip(
		_("Manipulate what controller data is used by the game engine based on their time of arrival in order "
          "to use controller data one frame length apart in the controller timeline despite the variance in "
          "game engine read timings induced by volatile wake-up timings in operating system processes.\n"
		  "Current input lag cost: 1,4ms."));
    m_use_steady_state_engine_stabilization_checkbox->SetToolTip(
		_("Enter a steady state operation mode for engine stabilization when enough data about the underlying "
		  "wake-up trend is available."));
    m_use_usb_polling_stabilization_checkbox->SetToolTip(
		_("Enforce a virtual millisecond-atomic schedule for USB polling data.\n"
	      "Current input lag cost: 200us."));
    m_use_official_adapter_timing_reconstruction_if_applicable_checkbox->SetToolTip(
		_("Attempt to use the history of presence or absence of new data to reconstruct the timing the adapter "
          " must have polled the controller at and incorporate this into the controller data history.\n"
          "Currently only available for the official adapter. Should trigger for any other port usage combination "
          "than [P1+P2] and [P1+P2+P3+P4]. Recognition of applicability is automated.\n"
		  "Input lag cost: 400us."));

	m_engine_frequency_radio_button_5994Hz->SetToolTip(
		_("The default setting.\n"
		"Should be used for 59.94Hz games i.e Melee with the polling drift fix code, "
		"which is present in the default Melee iso / Unclepunch's training mode / "
		"the 20XX training pack."));
	m_engine_frequency_radio_button_60Hz->SetToolTip(
	    _("Should be used for 60Hz games. Currently, the default Melee iso, Unclepunch's training mode "
	      "and the 20XX training pack are all clocked at 59.94Hz.\n"
	      "In particular, do not use the 60Hz option for Slippi netplay as that would worsen the experience.\n"
		  "If you use it for whatever reason , do not forget to revert to the 59.94Hz one before playing online."));

	const int space5 = FromDIP(5);

	wxBoxSizer *const main_sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer *const lag_control_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Latency stability enhancements"));

	lag_control_sizer->Add(
	    new wxStaticText(
	        this, wxID_ANY,
	        _("As of today, all the following enhancements are only applicable when using the WUP-028 device\n"
	          "(GameCube Adapter for Wii U in the controllers panel), historically referred to as 'Native Control'.")),
	    0, wxTOP, space5);

	lag_control_sizer->Add(m_increase_process_priority_checkbox, 0, wxLEFT | wxRIGHT | wxUP, space5);
	lag_control_sizer->Add(m_saturate_polling_thread_priority_checkbox, 0, wxLEFT | wxRIGHT | wxUP, space5);

	wxBoxSizer *const engine_frequency_radio_buttons_box_sizer =
	    new wxBoxSizer(wxHORIZONTAL);
	engine_frequency_radio_buttons_box_sizer->Add(m_use_engine_stabilization_checkbox);
	engine_frequency_radio_buttons_box_sizer->Add(m_engine_frequency_radio_button_5994Hz, 0, wxLEFT, space5);
	engine_frequency_radio_buttons_box_sizer->Add(m_engine_frequency_radio_button_60Hz, 0, wxLEFT, space5);

	//lag_control_sizer->AddSpacer(space5);
	lag_control_sizer->Add(engine_frequency_radio_buttons_box_sizer, 0, wxLEFT | wxRIGHT | wxUP, space5);

	lag_control_sizer->Add(m_use_steady_state_engine_stabilization_checkbox, 0, wxLEFT | wxRIGHT | wxUP, space5);
	lag_control_sizer->Add(m_use_usb_polling_stabilization_checkbox, 0, wxLEFT | wxRIGHT | wxUP, space5);
	lag_control_sizer->Add(m_use_official_adapter_timing_reconstruction_if_applicable_checkbox, 0,
	                       wxLEFT | wxRIGHT | wxUP,
	                       space5);
	lag_control_sizer->AddSpacer(space5);

	main_sizer->Add(lag_control_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);

	SetSizer(main_sizer);
}

void LagConfigPane::LoadGUIValues()
{
	const SConfig& startup_params = SConfig::GetInstance();

	m_increase_process_priority_checkbox->SetValue(startup_params.bIncreaseProcessPriority);
	m_saturate_polling_thread_priority_checkbox->SetValue(startup_params.bSaturatePollingThreadPriority);
	m_use_engine_stabilization_checkbox->SetValue(startup_params.bUseEngineStabilization);
	m_use_steady_state_engine_stabilization_checkbox->SetValue(startup_params.bUseSteadyStateEngineStabilization);
	m_use_usb_polling_stabilization_checkbox->SetValue(startup_params.bUseUsbPollingStabilization);
	m_use_official_adapter_timing_reconstruction_if_applicable_checkbox->SetValue(startup_params.bUseAdapterTimingReconstructionWhenApplicable);

	m_engine_frequency_radio_button_60Hz->SetValue(!startup_params.bUse5994HzStabilization);

	if (!m_increase_process_priority_checkbox->IsChecked())
	{
		m_use_steady_state_engine_stabilization_checkbox->Disable();
	}

	if (!m_use_engine_stabilization_checkbox->IsChecked())
	{
		m_use_steady_state_engine_stabilization_checkbox->Disable();
		m_use_usb_polling_stabilization_checkbox->Disable();
		m_use_official_adapter_timing_reconstruction_if_applicable_checkbox->Disable();
		m_engine_frequency_radio_button_5994Hz->Disable();
		m_engine_frequency_radio_button_60Hz->Disable();
	}
}

void LagConfigPane::BindEvents()
{
	m_increase_process_priority_checkbox->Bind(wxEVT_CHECKBOX,
	&LagConfigPane::OnIncreaseProcessPriorityCheckbox, this);

	m_saturate_polling_thread_priority_checkbox->Bind(wxEVT_CHECKBOX,
    &LagConfigPane::OnUseSaturatePollingThreadPriorityChechBoxChanged, this);
	
    m_use_engine_stabilization_checkbox->Bind(wxEVT_CHECKBOX,
    &LagConfigPane::OnUseEngineStabilizationCheckBoxChanged, this);
	
	m_engine_frequency_radio_button_5994Hz->Bind(wxEVT_RADIOBUTTON, &LagConfigPane::On5994HzSelected, this);
	m_engine_frequency_radio_button_60Hz->Bind(wxEVT_RADIOBUTTON, &LagConfigPane::On60HzSelected, this);

    m_use_steady_state_engine_stabilization_checkbox->Bind(wxEVT_CHECKBOX,
    &LagConfigPane::OnUseSteadyStateEngineStabilizationCheckBoxChanged, this);
	
    m_use_usb_polling_stabilization_checkbox->Bind(wxEVT_CHECKBOX,
    &LagConfigPane::OnUseUsbPollingStabilizationCheckBoxChanged, this);

	m_use_official_adapter_timing_reconstruction_if_applicable_checkbox->Bind(wxEVT_CHECKBOX,
    &LagConfigPane::OnUseOfficialAdapterTimingReconstructionIfApplicableCheckBoxChanged, this);
}

void LagConfigPane::OnIncreaseProcessPriorityCheckbox(wxCommandEvent&) {
	SConfig::GetInstance().bIncreaseProcessPriority = m_increase_process_priority_checkbox->IsChecked();
	if (!m_increase_process_priority_checkbox->IsChecked())
	{
		m_use_steady_state_engine_stabilization_checkbox->Disable();
	}
	else
	{
		if (m_use_engine_stabilization_checkbox->IsChecked())
			m_use_steady_state_engine_stabilization_checkbox->Enable();
	}
}

void LagConfigPane::OnUseSaturatePollingThreadPriorityChechBoxChanged(wxCommandEvent &)
{
	SConfig::GetInstance().bSaturatePollingThreadPriority
        = m_saturate_polling_thread_priority_checkbox->IsChecked();
}

void LagConfigPane::OnUseEngineStabilizationCheckBoxChanged(wxCommandEvent&)
{
	SConfig::GetInstance().bUseEngineStabilization
        = m_use_engine_stabilization_checkbox->IsChecked();
	if (!m_use_engine_stabilization_checkbox->IsChecked())
	{
		m_use_steady_state_engine_stabilization_checkbox->Disable();
		m_use_usb_polling_stabilization_checkbox->Disable();
		m_use_official_adapter_timing_reconstruction_if_applicable_checkbox->Disable();
		m_engine_frequency_radio_button_5994Hz->Disable();
		m_engine_frequency_radio_button_60Hz->Disable();
	}
	else
	{
		if (m_increase_process_priority_checkbox->IsChecked())
			m_use_steady_state_engine_stabilization_checkbox->Enable();
		m_use_usb_polling_stabilization_checkbox->Enable();
		m_use_official_adapter_timing_reconstruction_if_applicable_checkbox->Enable();
		m_engine_frequency_radio_button_5994Hz->Enable();
		m_engine_frequency_radio_button_60Hz->Enable();
	}
}

void LagConfigPane::OnUseSteadyStateEngineStabilizationCheckBoxChanged(wxCommandEvent&)
{
	SConfig::GetInstance().bUseSteadyStateEngineStabilization
        = m_use_steady_state_engine_stabilization_checkbox->IsChecked();
}

void LagConfigPane::OnUseUsbPollingStabilizationCheckBoxChanged(wxCommandEvent&)
{
	SConfig::GetInstance().bUseUsbPollingStabilization
        = m_use_usb_polling_stabilization_checkbox->IsChecked();
	if (!m_use_usb_polling_stabilization_checkbox->IsChecked())
	{
		m_use_official_adapter_timing_reconstruction_if_applicable_checkbox->Disable();
	}
	else
	{
		m_use_official_adapter_timing_reconstruction_if_applicable_checkbox->Enable();
	}
}

void LagConfigPane::OnUseOfficialAdapterTimingReconstructionIfApplicableCheckBoxChanged(wxCommandEvent&)
{
	SConfig::GetInstance().bUseAdapterTimingReconstructionWhenApplicable
        = m_use_official_adapter_timing_reconstruction_if_applicable_checkbox->IsChecked();
}

void LagConfigPane::On5994HzSelected(wxCommandEvent &)
{
	SConfig::GetInstance().bUse5994HzStabilization = true;
}

void LagConfigPane::On60HzSelected(wxCommandEvent &)
{

	wxMessageDialog m_60HzConfirmationDialog(this,
		_("Switching the engine stabilization mode to 60Hz makes it suited for playing 60Hz games. "
		  "The default Melee ISO configuration used for Slippi Netplay is 59.94Hz. Unclepunch's "
		  "training mode and the 20XX training pack are also clocked at 59.94Hz. As long as this "
		  "setting is on 60Hz, you shouldn't netplay.\n"
		  "If you do switch to 60Hz, don't forget to switch back to 59.94Hz before using either "
		  "of these ISOs.\n"
	      "Continue ?"),
	    _("Please confirm you know what you're doing."), wxYES_NO | wxSTAY_ON_TOP | wxICON_WARNING, wxDefaultPosition);

	int Ret = m_60HzConfirmationDialog.ShowModal();

	if (Ret == wxID_YES)
		SConfig::GetInstance().bUse5994HzStabilization = false;
	else
		m_engine_frequency_radio_button_5994Hz->SetValue(true); // Fixes UI
}
