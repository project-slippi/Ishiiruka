// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/dialog.h>
#include <wx/timer.h>

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

	void OnAdapterRumble(wxCommandEvent& event);
	void OnUpdateRate(wxTimerEvent& ev);
};
