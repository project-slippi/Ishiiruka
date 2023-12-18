// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/SlippiConfigPane.h"

#include <cassert>
#include <string>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/filepicker.h>
#include <wx/gbsizer.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/valtext.h>

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/GCMemcard.h"
#include "Core/HW/GCPad.h"
#include "Core/NetPlayProto.h"
#include "DolphinWX/Config/ConfigMain.h"
#include "DolphinWX/Input/MicButtonConfigDiag.h"
#include "DolphinWX/WxEventUtils.h"
#include "DolphinWX/WxUtils.h"

#ifndef IS_PLAYBACK
#include "Core/HW/EXI.h"
#include "Core/HW/EXI_DeviceSlippi.h"
#endif

SlippiNetplayConfigPane::SlippiNetplayConfigPane(wxWindow *parent, wxWindowID id)
    : wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	BindEvents();
}

void SlippiNetplayConfigPane::InitializeGUI()
{

	// Slippi settings
	m_replay_enable_checkbox = new wxCheckBox(this, wxID_ANY, _("Save Slippi Replays"));
	m_replay_enable_checkbox->SetToolTip(
	    _("Enable this to make Slippi automatically save .slp recordings of your games."));

	m_replay_month_folders_checkbox = new wxCheckBox(this, wxID_ANY, _("Save Replays to Monthly Subfolders"));
	m_replay_month_folders_checkbox->SetToolTip(
	    _("Enable this to save your replays into subfolders by month (YYYY-MM)."));

	m_replay_directory_picker =
	    new wxDirPickerCtrl(this, wxID_ANY, wxEmptyString, _("Slippi Replay Folder:"), wxDefaultPosition, wxDefaultSize,
	                        wxDIRP_USE_TEXTCTRL | wxDIRP_SMALL);
	m_replay_directory_picker->SetToolTip(_("Choose where your Slippi replay files are saved."));

	// Online settings
	m_slippi_delay_frames_txt = new wxStaticText(this, wxID_ANY, _("Delay Frames:"));
	m_slippi_delay_frames_ctrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(50, -1));
	m_slippi_delay_frames_ctrl->SetToolTip(
	    _("Leave this at 2 unless consistently playing on 120+ ping. "
	      "Increasing this can cause unplayable input delay, and lowering it can cause visual artifacts/lag."));
	m_slippi_delay_frames_ctrl->SetRange(1, 9);

	m_slippi_enable_quick_chat_txt = new wxStaticText(this, wxID_ANY, _("Quick Chat:"));
	m_slippi_enable_quick_chat_choice =
	    new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_slippi_enable_quick_chat_strings);
	m_slippi_enable_quick_chat_choice->SetToolTip(
	    _("Enable this to send and receive Quick Chat Messages when online."));

	m_slippi_force_netplay_port_checkbox = new wxCheckBox(this, wxID_ANY, _("Force Netplay Port"));
	m_slippi_force_netplay_port_checkbox->SetToolTip(
	    _("Enable this to force Slippi to use a specific network port for online peer-to-peer connections."));
	m_slippi_force_netplay_port_ctrl =
	    new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1));
	m_slippi_force_netplay_port_ctrl->SetRange(1, 65535);

	m_slippi_force_netplay_lan_ip_checkbox = new wxCheckBox(this, wxID_ANY, _("Force LAN IP"));
	m_slippi_force_netplay_lan_ip_checkbox->SetToolTip(
	    _("Enable this to force Slippi to use a specific LAN IP when connecting to users with a matching WAN IP. "
	      "Should not be required for most users."));
	m_slippi_netplay_lan_ip_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(120, -1));
	m_slippi_netplay_lan_ip_ctrl->SetMaxLength(20);
	wxArrayString charsToFilter;
	wxTextValidator ipTextValidator(wxFILTER_INCLUDE_CHAR_LIST);
	charsToFilter.Add(wxT("0123456789."));
	ipTextValidator.SetIncludes(charsToFilter);
	m_slippi_netplay_lan_ip_ctrl->SetValidator(ipTextValidator);

	// Input settings
	m_reduce_timing_dispersion_checkbox = new wxCheckBox(this, wxID_ANY, _("Reduce Timing Dispersion"));
	m_reduce_timing_dispersion_checkbox->SetToolTip(
	    _("Make inputs feel more console-like for overclocked GCC to USB "
	      "adapters at the cost of 1.6ms of input lag (2ms for single-port official adapter)."));

	m_slippi_jukebox_enabled_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Music"));

	// WASAPI does not work with this and we want a note for the user.
#ifdef _WIN32
	m_slippi_jukebox_enabled_checkbox->SetToolTip(
	    _("Toggle in-game music for stages and menus. Changing this does not affect "
	      "other audio like character hits or effects. This option does nothing when "
	      "using the Exclusive WASAPI audio backend."));
#else
	m_slippi_jukebox_enabled_checkbox->SetToolTip(
	    _("Toggle in-game music for stages and menus. Changing this does not affect "
	      "other audio like character hits or effects."));
#endif

	m_slippi_jukebox_volume_slider = new DolphinSlider(this, wxID_ANY, 100, 0, 100);
	m_jukebox_volume_text = new wxStaticText(this, wxID_ANY, "");
	m_jukebox_volume_text->SetMinSize(wxSize(50, 20));

	auto *const jukebox_music_volume_sizer = new wxBoxSizer(wxHORIZONTAL);
	jukebox_music_volume_sizer->Add(new wxStaticText(this, wxID_ANY, _("Music Volume:")), 0, wxALIGN_CENTER_VERTICAL);
	jukebox_music_volume_sizer->Add(m_slippi_jukebox_volume_slider, 1, wxALIGN_CENTER_VERTICAL);
	jukebox_music_volume_sizer->Add(m_jukebox_volume_text, 0, wxALIGN_CENTER_VERTICAL);

	const int space5 = FromDIP(5);
	const int space10 = FromDIP(10);

	wxGridBagSizer *const sSlippiReplaySettings = new wxGridBagSizer(space5, space5);
	sSlippiReplaySettings->Add(m_replay_enable_checkbox, wxGBPosition(0, 0), wxGBSpan(1, 2));
	sSlippiReplaySettings->Add(m_replay_month_folders_checkbox, wxGBPosition(1, 0), wxGBSpan(1, 2),
	                           wxRESERVE_SPACE_EVEN_IF_HIDDEN);
	sSlippiReplaySettings->Add(new wxStaticText(this, wxID_ANY, _("Replay folder:")), wxGBPosition(2, 0), wxDefaultSpan,
	                           wxALIGN_CENTER_VERTICAL);
	sSlippiReplaySettings->Add(m_replay_directory_picker, wxGBPosition(2, 1), wxDefaultSpan, wxEXPAND);
	sSlippiReplaySettings->AddGrowableCol(1);

	wxStaticBoxSizer *const sbSlippiReplaySettings =
	    new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Replay Settings"));
	sbSlippiReplaySettings->AddSpacer(space5);
	sbSlippiReplaySettings->Add(sSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	sbSlippiReplaySettings->AddSpacer(space5);

	wxGridBagSizer *const sSlippiOnlineSettings = new wxGridBagSizer(space10, space5);
	sSlippiOnlineSettings->Add(m_slippi_delay_frames_txt, wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	sSlippiOnlineSettings->Add(m_slippi_delay_frames_ctrl, wxGBPosition(0, 1), wxDefaultSpan, wxALIGN_LEFT);

	sSlippiOnlineSettings->Add(m_slippi_enable_quick_chat_txt, wxGBPosition(1, 0), wxDefaultSpan,
	                           wxALIGN_CENTER_VERTICAL);
	sSlippiOnlineSettings->Add(m_slippi_enable_quick_chat_choice, wxGBPosition(1, 1), wxDefaultSpan, wxALIGN_LEFT);

	sSlippiOnlineSettings->Add(m_slippi_force_netplay_port_checkbox, wxGBPosition(2, 0), wxDefaultSpan,
	                           wxALIGN_CENTER_VERTICAL);
	sSlippiOnlineSettings->Add(m_slippi_force_netplay_port_ctrl, wxGBPosition(2, 1), wxDefaultSpan,
	                           wxALIGN_LEFT | wxRESERVE_SPACE_EVEN_IF_HIDDEN);
	sSlippiOnlineSettings->Add(m_slippi_force_netplay_lan_ip_checkbox, wxGBPosition(3, 0), wxDefaultSpan,
	                           wxALIGN_CENTER_VERTICAL);
	sSlippiOnlineSettings->Add(m_slippi_netplay_lan_ip_ctrl, wxGBPosition(3, 1), wxDefaultSpan,
	                           wxALIGN_LEFT | wxRESERVE_SPACE_EVEN_IF_HIDDEN);

	wxStaticBoxSizer *const sbSlippiOnlineSettings =
	    new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Online Settings"));
	sbSlippiOnlineSettings->AddSpacer(space5);
	sbSlippiOnlineSettings->Add(sSlippiOnlineSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	sbSlippiOnlineSettings->AddSpacer(space5);

	wxStaticBoxSizer *const sbSlippiInputSettings = new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Input Settings"));
	sbSlippiInputSettings->AddSpacer(space5);
	sbSlippiInputSettings->Add(m_reduce_timing_dispersion_checkbox, 0, wxLEFT | wxRIGHT, space5);
	sbSlippiInputSettings->AddSpacer(space5);

	wxBoxSizer *const main_sizer = new wxBoxSizer(wxVERTICAL);

	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiOnlineSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiInputSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);

	wxStaticBoxSizer *const sbSlippiJukeboxSettings =
	    new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Jukebox Settings (Beta)"));
	sbSlippiJukeboxSettings->AddSpacer(space5);
	sbSlippiJukeboxSettings->Add(m_slippi_jukebox_enabled_checkbox, 0, wxLEFT | wxRIGHT, space5);
	sbSlippiJukeboxSettings->AddSpacer(space5);
	sbSlippiJukeboxSettings->Add(jukebox_music_volume_sizer, 2, wxEXPAND, space5);

	main_sizer->Add(sbSlippiJukeboxSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);

	SetSizer(main_sizer);
}

void SlippiNetplayConfigPane::LoadGUIValues()
{
	const SConfig &startup_params = SConfig::GetInstance();

#if HAVE_PORTAUDIO
#endif

	bool enableReplays = startup_params.m_slippiSaveReplays;
	bool forceNetplayPort = startup_params.m_slippiForceNetplayPort;
	bool forceLanIp = startup_params.m_slippiForceLanIp;
	bool enableJukebox = startup_params.bSlippiJukeboxEnabled;

	m_replay_enable_checkbox->SetValue(enableReplays);
	m_replay_month_folders_checkbox->SetValue(startup_params.m_slippiReplayMonthFolders);
	m_replay_directory_picker->SetPath(StrToWxStr(startup_params.m_strSlippiReplayDir));

	if (!enableReplays)
	{
		m_replay_month_folders_checkbox->Hide();
	}

	m_slippi_delay_frames_ctrl->SetValue(startup_params.m_slippiOnlineDelay);
	PopulateEnableChatChoiceBox();

	m_slippi_force_netplay_port_checkbox->SetValue(startup_params.m_slippiForceNetplayPort);
	m_slippi_force_netplay_port_ctrl->SetValue(startup_params.m_slippiNetplayPort);
	if (!forceNetplayPort)
	{
		m_slippi_force_netplay_port_ctrl->Hide();
	}

	m_slippi_force_netplay_lan_ip_checkbox->SetValue(startup_params.m_slippiForceLanIp);
	m_slippi_netplay_lan_ip_ctrl->SetValue(startup_params.m_slippiLanIp);
	if (!forceLanIp)
	{
		m_slippi_netplay_lan_ip_ctrl->Hide();
	}

	m_reduce_timing_dispersion_checkbox->SetValue(startup_params.bReduceTimingDispersion);

	m_slippi_jukebox_enabled_checkbox->SetValue(enableJukebox);
	m_slippi_jukebox_volume_slider->SetValue(startup_params.iSlippiJukeboxVolume);
	m_jukebox_volume_text->SetLabel(wxString::Format("%d %%", startup_params.iSlippiJukeboxVolume));
	if (enableJukebox)
	{
		m_slippi_jukebox_volume_slider->Enable();
	}
	else
	{
		m_slippi_jukebox_volume_slider->Disable();
	}
}

void SlippiNetplayConfigPane::BindEvents()
{
	m_replay_enable_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnReplaySavingToggle, this);

	m_replay_month_folders_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnReplayMonthFoldersToggle, this);

	m_replay_directory_picker->Bind(wxEVT_DIRPICKER_CHANGED, &SlippiNetplayConfigPane::OnReplayDirChanged, this);

	m_slippi_delay_frames_ctrl->Bind(wxEVT_SPINCTRL, &SlippiNetplayConfigPane::OnDelayFramesChanged, this);
	m_slippi_enable_quick_chat_choice->Bind(wxEVT_CHOICE, &SlippiNetplayConfigPane::OnQuickChatChanged, this);
	m_slippi_force_netplay_port_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnForceNetplayPortToggle,
	                                           this);
	m_slippi_force_netplay_port_ctrl->Bind(wxEVT_SPINCTRL, &SlippiNetplayConfigPane::OnNetplayPortChanged, this);

	m_slippi_force_netplay_lan_ip_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnForceNetplayLanIpToggle,
	                                             this);
	m_slippi_netplay_lan_ip_ctrl->Bind(wxEVT_TEXT, &SlippiNetplayConfigPane::OnNetplayLanIpChanged, this);

	m_reduce_timing_dispersion_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnReduceTimingDispersionToggle,
	                                          this);

	m_slippi_jukebox_enabled_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnToggleJukeboxEnabled, this);
	m_slippi_jukebox_volume_slider->Bind(wxEVT_SLIDER, &SlippiNetplayConfigPane::OnJukeboxVolumeUpdate, this);
}

void SlippiNetplayConfigPane::OnQuickChatChanged(wxCommandEvent &event)
{
	auto selectedStr = m_slippi_enable_quick_chat_choice->GetSelection() != wxNOT_FOUND
	                       ? WxStrToStr(m_slippi_enable_quick_chat_choice->GetStringSelection())
	                       : quickChatOptions[SLIPPI_CHAT_ON];

	int selectedChoice = SLIPPI_CHAT_ON; // default is enabled

	for (auto it = quickChatOptions.begin(); it != quickChatOptions.end(); it++)
		if (strcmp(it->second.c_str(), selectedStr.c_str()) == 0)
		{
			selectedChoice = it->first;
			break;
		}

	SConfig::GetInstance().m_slippiEnableQuickChat = selectedChoice;
}

void SlippiNetplayConfigPane::OnReplaySavingToggle(wxCommandEvent &event)
{
	bool enableReplays = m_replay_enable_checkbox->IsChecked();

	SConfig::GetInstance().m_slippiSaveReplays = enableReplays;

	enableReplays ? m_replay_month_folders_checkbox->Show() : m_replay_month_folders_checkbox->Hide();
}

void SlippiNetplayConfigPane::OnReplayMonthFoldersToggle(wxCommandEvent &event)
{
	SConfig::GetInstance().m_slippiReplayMonthFolders =
	    m_replay_enable_checkbox->IsChecked() && m_replay_month_folders_checkbox->IsChecked();
}

void SlippiNetplayConfigPane::OnReplayDirChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().m_strSlippiReplayDir = WxStrToStr(m_replay_directory_picker->GetPath());
}

void SlippiNetplayConfigPane::OnDelayFramesChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().m_slippiOnlineDelay = m_slippi_delay_frames_ctrl->GetValue();
}

void SlippiNetplayConfigPane::OnForceNetplayPortToggle(wxCommandEvent &event)
{
	bool enableForcePort = m_slippi_force_netplay_port_checkbox->IsChecked();

	SConfig::GetInstance().m_slippiForceNetplayPort = enableForcePort;

	if (enableForcePort)
		m_slippi_force_netplay_port_ctrl->Show();
	else
		m_slippi_force_netplay_port_ctrl->Hide();
}

void SlippiNetplayConfigPane::OnNetplayPortChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().m_slippiNetplayPort = m_slippi_force_netplay_port_ctrl->GetValue();
}

void SlippiNetplayConfigPane::OnForceNetplayLanIpToggle(wxCommandEvent &event)
{
	bool enableForceLanIp = m_slippi_force_netplay_lan_ip_checkbox->IsChecked();

	SConfig::GetInstance().m_slippiForceLanIp = enableForceLanIp;

	if (enableForceLanIp)
		m_slippi_netplay_lan_ip_ctrl->Show();
	else
		m_slippi_netplay_lan_ip_ctrl->Hide();
}

void SlippiNetplayConfigPane::OnNetplayLanIpChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().m_slippiLanIp = m_slippi_netplay_lan_ip_ctrl->GetValue().c_str();
}

void SlippiNetplayConfigPane::OnReduceTimingDispersionToggle(wxCommandEvent &event)
{
	SConfig::GetInstance().bReduceTimingDispersion = m_reduce_timing_dispersion_checkbox->GetValue();
}

void SlippiNetplayConfigPane::OnToggleJukeboxEnabled(wxCommandEvent &event)
{
	bool isEnabled = m_slippi_jukebox_enabled_checkbox->GetValue();

	SConfig::GetInstance().bSlippiJukeboxEnabled = isEnabled;

	if (isEnabled)
	{
		m_slippi_jukebox_volume_slider->Enable();
	}
	else
	{
		m_slippi_jukebox_volume_slider->Disable();
	}

#ifndef IS_PLAYBACK
	// If we have a Slippi EXI device loaded, grab it and tell it to reconfigure the Jukebox.
	// Note that this should only execute if `Core` is loaded and running, as otherwise the Expansion
	// Interface is not actually initialized.
	if (Core::IsRunning())
	{
		CEXISlippi *slippiEXIDevice = (CEXISlippi *)ExpansionInterface::FindDevice(TEXIDevices::EXIDEVICE_SLIPPI);

		if (slippiEXIDevice != nullptr && slippiEXIDevice->IsPresent())
		{
			slippiEXIDevice->ConfigureJukebox();
		}
	}
#endif // !IS_PLAYBACK
}

void SlippiNetplayConfigPane::OnJukeboxVolumeUpdate(wxCommandEvent &event)
{
	SConfig::GetInstance().iSlippiJukeboxVolume = event.GetInt();
	m_jukebox_volume_text->SetLabel(wxString::Format("%d %%", event.GetInt()));

#ifndef IS_PLAYBACK
	if (Core::IsRunning())
	{
		CEXISlippi *slippiEXIDevice = (CEXISlippi *)ExpansionInterface::FindDevice(TEXIDevices::EXIDEVICE_SLIPPI);

		if (slippiEXIDevice != nullptr && slippiEXIDevice->IsPresent())
		{
			slippiEXIDevice->SetJukeboxDolphinMusicVolume();
		}
	}
#endif // !IS_PLAYBACK
}

void SlippiNetplayConfigPane::PopulateEnableChatChoiceBox()
{

	for (auto it = quickChatOptions.begin(); it != quickChatOptions.end(); it++)
	{
		m_slippi_enable_quick_chat_choice->Append(StrToWxStr(it->second));
	}

	auto currentChoice = SConfig::GetInstance().m_slippiEnableQuickChat;
	auto currentChoiceStr = quickChatOptions[currentChoice];
	int num = m_slippi_enable_quick_chat_choice->FindString(StrToWxStr(currentChoiceStr));
	m_slippi_enable_quick_chat_choice->SetSelection(num);
}

SlippiPlaybackConfigPane::SlippiPlaybackConfigPane(wxWindow *parent, wxWindowID id)
    : wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	BindEvents();
}

void SlippiPlaybackConfigPane::InitializeGUI()
{
	// Slippi settings
	m_replay_regenerate_checkbox = new wxCheckBox(this, wxID_ANY, _("Regenerate Slippi Replays (off if unsure)"));
	m_replay_regenerate_checkbox->SetToolTip(
	    _("Enable this to regenerate .slp recordings of your games. Does NOT need to be enabled to use slp event monitoring service for powering custom HUDs."));

	m_replay_directory_picker =
	    new wxDirPickerCtrl(this, wxID_ANY, wxEmptyString, _("Slippi Replay Folder:"), wxDefaultPosition, wxDefaultSize,
	                        wxDIRP_USE_TEXTCTRL | wxDIRP_SMALL);
	m_replay_directory_picker->SetToolTip(_("Choose where your regenerated replay files are saved."));

	// Slippi display settings
	m_display_frame_index = new wxCheckBox(this, wxID_ANY, _("Display Frame Index"));
	m_display_frame_index->SetToolTip(
	    _("Displays the Frame Index when viewing replays. On-Screen Display Messages must also be enabled"));

	const int space5 = FromDIP(5);

	wxGridBagSizer *const sSlippiPlaybackSettings = new wxGridBagSizer(space5, space5);
	sSlippiPlaybackSettings->Add(m_display_frame_index, wxGBPosition(0, 0), wxGBSpan(1, 2));
	sSlippiPlaybackSettings->AddGrowableCol(1);

	wxStaticBoxSizer *const sbSlippiPlaybackSettings =
	    new wxStaticBoxSizer(wxVERTICAL, this, _("Playback Display Settings"));
	sbSlippiPlaybackSettings->AddSpacer(space5);
	sbSlippiPlaybackSettings->Add(sSlippiPlaybackSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	sbSlippiPlaybackSettings->AddSpacer(space5);

	wxGridBagSizer *const sSlippiReplaySettings = new wxGridBagSizer(space5, space5);
	sSlippiReplaySettings->Add(m_replay_regenerate_checkbox, wxGBPosition(0, 0), wxGBSpan(1, 2));
	sSlippiReplaySettings->Add(new wxStaticText(this, wxID_ANY, _("Replay folder:")), wxGBPosition(1, 0), wxDefaultSpan,
	                           wxALIGN_CENTER_VERTICAL);
	sSlippiReplaySettings->Add(m_replay_directory_picker, wxGBPosition(1, 1), wxDefaultSpan, wxEXPAND);
	sSlippiReplaySettings->AddGrowableCol(1);

	wxStaticBoxSizer *const sbSlippiReplaySettings =
	    new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Replay Settings"));
	sbSlippiReplaySettings->AddSpacer(space5);
	sbSlippiReplaySettings->Add(sSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	sbSlippiReplaySettings->AddSpacer(space5);

	wxBoxSizer *const main_sizer = new wxBoxSizer(wxVERTICAL);

	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiPlaybackSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);

	SetSizer(main_sizer);
}

void SlippiPlaybackConfigPane::LoadGUIValues()
{
	const SConfig &startup_params = SConfig::GetInstance();

	m_replay_regenerate_checkbox->SetValue(startup_params.m_slippiRegenerateReplays);
	m_replay_directory_picker->SetPath(StrToWxStr(startup_params.m_strSlippiRegenerateReplayDir));
	m_display_frame_index->SetValue(startup_params.m_slippiEnableFrameIndex);
}

void SlippiPlaybackConfigPane::BindEvents()
{
	m_display_frame_index->Bind(wxEVT_CHECKBOX, &SlippiPlaybackConfigPane::OnDisplayFrameIndexToggle, this);

	m_replay_regenerate_checkbox->Bind(wxEVT_CHECKBOX, &SlippiPlaybackConfigPane::OnReplayRegenerateToggle, this);
	m_replay_directory_picker->Bind(wxEVT_DIRPICKER_CHANGED, &SlippiPlaybackConfigPane::OnReplayDirChanged, this);
}

void SlippiPlaybackConfigPane::OnDisplayFrameIndexToggle(wxCommandEvent &event)
{
	bool enableFrameIndex = m_display_frame_index->IsChecked();
	SConfig::GetInstance().m_slippiEnableFrameIndex = enableFrameIndex;
}

void SlippiPlaybackConfigPane::OnReplayRegenerateToggle(wxCommandEvent &event)
{
	bool enableReplays = m_replay_regenerate_checkbox->IsChecked();

	SConfig::GetInstance().m_slippiRegenerateReplays = enableReplays;
}

void SlippiPlaybackConfigPane::OnReplayDirChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().m_strSlippiRegenerateReplayDir = WxStrToStr(m_replay_directory_picker->GetPath());
}
