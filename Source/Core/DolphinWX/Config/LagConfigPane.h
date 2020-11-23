// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/arrstr.h>
#include <wx/panel.h>

class wxButton;
class wxCheckBox;
class wxRadioButton;
class wxChoice;

class LagConfigPane final : public wxPanel
{
public:
	LagConfigPane(wxWindow* parent, wxWindowID id);

private:
	void InitializeGUI();
	void LoadGUIValues();
	void BindEvents();

	void OnIncreaseProcessPriorityCheckbox(wxCommandEvent &);
	void OnUseSaturatePollingThreadPriorityChechBoxChanged(wxCommandEvent &);
	void OnUseEngineStabilizationCheckBoxChanged(wxCommandEvent &);
    void OnUseSteadyStateEngineStabilizationCheckBoxChanged(wxCommandEvent&);
    void OnUseUsbPollingStabilizationCheckBoxChanged(wxCommandEvent&);
	void OnUseOfficialAdapterTimingReconstructionIfApplicableCheckBoxChanged(wxCommandEvent&);
	void On5994HzSelected(wxCommandEvent &);
	void On60HzSelected(wxCommandEvent &);

	wxCheckBox *m_increase_process_priority_checkbox;
	wxCheckBox *m_saturate_polling_thread_priority_checkbox;
	wxCheckBox *m_use_engine_stabilization_checkbox; // Configs: delay leniency [can fine tune global lag addition by
	                                                 // playing on delay]
	wxRadioButton *m_engine_frequency_radio_button_60Hz;
	wxRadioButton *m_engine_frequency_radio_button_5994Hz;

	wxCheckBox *m_use_steady_state_engine_stabilization_checkbox;           // Configs: a voir
	wxCheckBox *m_use_usb_polling_stabilization_checkbox;         // Configs: leniency
	wxCheckBox *m_use_official_adapter_timing_reconstruction_if_applicable_checkbox; // Grise si steady state engine
	                                                                                 // stabilization off
};
