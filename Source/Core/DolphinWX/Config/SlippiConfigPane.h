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

class SlippiConfigPane final : public wxPanel
{
public:
	SlippiConfigPane(wxWindow* parent, wxWindowID id);

private:
	void InitializeGUI();
	void LoadGUIValues();
	void BindEvents();

	void OnReplaySavingToggle(wxCommandEvent& event);
	void OnReplayMonthFoldersToggle(wxCommandEvent& event);
	void OnReplayDirChanged(wxCommandEvent& event);
	void OnDelayFramesChanged(wxCommandEvent &event);
	void OnQuickChatToggle(wxCommandEvent& event);

	wxCheckBox* m_replay_enable_checkbox;
	wxDirPickerCtrl* m_replay_directory_picker;
	wxCheckBox* m_replay_month_folders_checkbox;
	wxStaticText* m_slippi_delay_frames_txt;
	wxSpinCtrl *m_slippi_delay_frames_ctrl;
	wxCheckBox* m_slippi_enable_quick_chat;

};
