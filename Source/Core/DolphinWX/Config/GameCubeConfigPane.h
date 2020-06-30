// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/arrstr.h>
#include <wx/panel.h>

enum TEXIDevices : int;

class wxButton;
class wxCheckBox;
class wxChoice;
class wxDirPickerCtrl;
class wxSpinCtrl;
class wxString;
class wxStaticText;

class GameCubeConfigPane final : public wxPanel
{
public:
	GameCubeConfigPane(wxWindow* parent, wxWindowID id);

private:
	void InitializeGUI();
	void LoadGUIValues();
	void BindEvents();

	void OnSystemLanguageChange(wxCommandEvent&);
	void OnOverrideLanguageCheckBoxChanged(wxCommandEvent&);
	void OnSkipBiosCheckBoxChanged(wxCommandEvent&);
	void OnSlotAChanged(wxCommandEvent&);
	void OnSlotBChanged(wxCommandEvent&);
	void OnSP1Changed(wxCommandEvent&);
	void OnSlotAButtonClick(wxCommandEvent&);
	void OnSlotBButtonClick(wxCommandEvent&);

	void ChooseEXIDevice(const wxString& device_name, int device_id);
	void HandleEXISlotChange(int slot, const wxString& title);
	void ChooseSlotPath(bool is_slot_a, TEXIDevices device_type);

	void OnReplaySavingToggle(wxCommandEvent& event);
	void OnReplayMonthFoldersToggle(wxCommandEvent& event);
	void OnReplayDirChanged(wxCommandEvent& event);
	void OnDelayFramesChanged(wxCommandEvent &event);

	wxArrayString m_ipl_language_strings;

	wxChoice* m_system_lang_choice;
	wxCheckBox* m_override_lang_checkbox;
	wxCheckBox* m_skip_bios_checkbox;
	wxChoice* m_exi_devices[3];
	wxButton* m_memcard_path[2];
	wxCheckBox* m_replay_enable_checkbox;
	wxDirPickerCtrl* m_replay_directory_picker;
	wxCheckBox* m_replay_month_folders_checkbox;
	wxStaticText* m_slippi_delay_frames_txt;
	wxSpinCtrl *m_slippi_delay_frames_ctrl;
};
