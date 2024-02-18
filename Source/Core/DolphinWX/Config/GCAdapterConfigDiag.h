// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/dialog.h>
#include <wx/timer.h>
#include <wx/choice.h>
#include "InputCommon/GCPadStatus.h"

class wxStaticText;

class GCAdapterConfigDiag : public wxDialog
{
public:
	GCAdapterConfigDiag(wxWindow* const parent, const wxString& name, const int tab_num = 0);
	~GCAdapterConfigDiag();

	void ScheduleAdapterUpdate();
	void OnUpdateAdapter(wxCommandEvent& ev);

private:
	wxStaticText* m_adapter_status;
	wxTimer m_update_rate_timer;
	
	int m_pad_id;

	PadButton SelectionToPadButton[12] = {PAD_BUTTON_A,     PAD_BUTTON_B,    PAD_BUTTON_X,   PAD_BUTTON_Y,
	                                      PAD_TRIGGER_Z,    PAD_TRIGGER_L,   PAD_TRIGGER_R,  PAD_BUTTON_UP,
	                                      PAD_BUTTON_RIGHT, PAD_BUTTON_DOWN, PAD_BUTTON_LEFT, PAD_BUTTON_NONE};
	int PadButtonToSelection(PadButton button);

	void OnAdapterRumble(wxCommandEvent &event);
	void OnAChoice(wxCommandEvent &event);
	void OnBChoice(wxCommandEvent &event);
	void OnXChoice(wxCommandEvent &event);
	void OnYChoice(wxCommandEvent &event);
	void OnZChoice(wxCommandEvent &event);
	void OnLChoice(wxCommandEvent &event);
	void OnRChoice(wxCommandEvent &event);
	void OnUpChoice(wxCommandEvent &event);
	void OnRightChoice(wxCommandEvent &event);
	void OnDownChoice(wxCommandEvent &event);
	void OnLeftChoice(wxCommandEvent &event);
	void OnUpdateRate(wxTimerEvent& ev);
};
