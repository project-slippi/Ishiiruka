// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/arrstr.h>
#include <wx/panel.h>
#include <wx/textctrl.h>

enum TEXIDevices : int;

class wxButton;
class wxCheckBox;
class wxChoice;
class wxDirPickerCtrl;
class wxSpinCtrl;
class wxString;
class wxStaticText;
class wxTextCtrl;

class SlippiPlaybackConfigPane final : public wxPanel
{
public:
	SlippiPlaybackConfigPane(wxWindow* parent, wxWindowID id);

private:
	void InitializeGUI();
	void LoadGUIValues();
	void BindEvents();

	wxStaticText* m_slippi_delay_frames_txt;
	wxTextCtrl *m_slippi_netplay_lan_ip_ctrl;
	wxCheckBox *m_slippi_display_frame_index;


};
