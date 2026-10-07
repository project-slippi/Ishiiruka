// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/arrstr.h>
#include <wx/panel.h>
#include <wx/textctrl.h>

#include "DolphinWX/DolphinSlider.h"

enum TEXIDevices : int;

class wxButton;
class wxCheckBox;
class wxChoice;
class wxDirPickerCtrl;
class wxSpinCtrl;
class wxString;
class wxStaticText;
class wxTextCtrl;

class SlippiNetplayConfigPane final : public wxPanel
{
  public:
	SlippiNetplayConfigPane(wxWindow *parent, wxWindowID id);

  private:
	void InitializeGUI();
	void LoadGUIValues();
	void BindEvents();

	void OnReplaySavingToggle(wxCommandEvent &event);
	void OnReplayMonthFoldersToggle(wxCommandEvent &event);
	void OnReplayDirChanged(wxCommandEvent &event);
	void OnDelayFramesChanged(wxCommandEvent &event);
	void OnForceNetplayPortToggle(wxCommandEvent &event);
	void OnNetplayPortChanged(wxCommandEvent &event);
	void OnForceNetplayLanIpToggle(wxCommandEvent &event);
	void OnNetplayLanIpChanged(wxCommandEvent &event);
	void OnQuickChatChanged(wxCommandEvent &event);
	void OnReduceTimingDispersionToggle(wxCommandEvent &event);
	void PopulateEnableChatChoiceBox();
	void OnToggleJukeboxEnabled(wxCommandEvent &event);
	void OnJukeboxVolumeUpdate(wxCommandEvent &event);
	void OnToggleDiscordRichPresence(wxCommandEvent &event);
	void OnToggleShowPlayerRank(wxCommandEvent &event);
	void OnToggleShowOpponentRank(wxCommandEvent &event);

	wxArrayString m_slippi_enable_quick_chat_strings;

	wxCheckBox *m_replay_enable_checkbox;
	wxDirPickerCtrl *m_replay_directory_picker;
	wxCheckBox *m_replay_month_folders_checkbox;
	wxStaticText *m_slippi_delay_frames_txt;
	wxSpinCtrl *m_slippi_delay_frames_ctrl;
	wxCheckBox *m_slippi_force_netplay_port_checkbox;
	wxSpinCtrl *m_slippi_force_netplay_port_ctrl;
	wxCheckBox *m_slippi_force_netplay_lan_ip_checkbox;
	wxTextCtrl *m_slippi_netplay_lan_ip_ctrl;
	wxStaticText *m_slippi_enable_quick_chat_txt;
	wxChoice *m_slippi_enable_quick_chat_choice;

	wxCheckBox *m_reduce_timing_dispersion_checkbox;

	wxCheckBox *m_slippi_show_player_rank;
	wxCheckBox *m_slippi_show_opponent_rank;

	wxCheckBox *m_slippi_jukebox_enabled_checkbox;
	wxCheckBox *m_slippi_discord_rich_presence_checkbox;
	DolphinSlider *m_slippi_jukebox_volume_slider;
	wxStaticText *m_jukebox_volume_text;
};

class SlippiPlaybackConfigPane final : public wxPanel
{
  public:
	SlippiPlaybackConfigPane(wxWindow *parent, wxWindowID id);

  private:
	void InitializeGUI();
	void LoadGUIValues();
	void BindEvents();

	wxStaticText *m_slippi_delay_frames_txt;
	wxTextCtrl *m_slippi_netplay_lan_ip_ctrl;
	wxCheckBox *m_replay_regenerate_checkbox;
	wxDirPickerCtrl *m_replay_directory_picker;
	wxCheckBox *m_display_frame_index;

	void OnDisplayFrameIndexToggle(wxCommandEvent &event);
	void OnReplayRegenerateToggle(wxCommandEvent &event);
	void OnReplayDirChanged(wxCommandEvent &event);
};
