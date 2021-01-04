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
#include <wx/stattext.h>
#include <wx/spinctrl.h>

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI.h"
#include "Core/HW/GCMemcard.h"
#include "Core/HW/GCPad.h"
#include "Core/NetPlayProto.h"
#include "DolphinWX/Config/ConfigMain.h"
#include "DolphinWX/Input/MicButtonConfigDiag.h"
#include "DolphinWX/WxEventUtils.h"
#include "DolphinWX/WxUtils.h"

SlippiConfigPane::SlippiConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	BindEvents();
}

void SlippiConfigPane::InitializeGUI()
{

#ifndef IS_PLAYBACK
	// Slippi settings
	m_replay_enable_checkbox = new wxCheckBox(this, wxID_ANY, _("Save Slippi Replays"));
	m_replay_enable_checkbox->SetToolTip(
		_("Enable this to make Slippi automatically save .slp recordings of your games."));

	m_replay_month_folders_checkbox =
		new wxCheckBox(this, wxID_ANY, _("Save Replays to Monthly Subfolders"));
	m_replay_month_folders_checkbox->SetToolTip(
		_("Enable this to save your replays into subfolders by month (YYYY-MM)."));

	m_replay_directory_picker = new wxDirPickerCtrl(this, wxID_ANY, wxEmptyString,
		_("Slippi Replay Folder:"), wxDefaultPosition, wxDefaultSize,
		wxDIRP_USE_TEXTCTRL | wxDIRP_SMALL);
	m_replay_directory_picker->SetToolTip(
		_("Choose where your Slippi replay files are saved."));

	// Online settings
	m_slippi_delay_frames_txt = new wxStaticText(this, wxID_ANY, _("Delay Frames:"));
	m_slippi_delay_frames_ctrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(50, -1));
	m_slippi_delay_frames_ctrl->SetToolTip(_(
		"Leave this at 2 unless consistently playing on 120+ ping. "
		"Increasing this can cause unplayable input delay, and lowering it can cause visual artifacts/lag."));
	m_slippi_delay_frames_ctrl->SetRange(1, 9);

	m_slippi_enable_quick_chat = new wxCheckBox(this, wxID_ANY, _("Enable Quick Chat"));
	m_slippi_enable_quick_chat->SetToolTip(
		_("Enable this to send and receive Quick Chat Messages when online."));


#endif
	const int space5 = FromDIP(5);
	const int space10 = FromDIP(10);

#ifndef IS_PLAYBACK
	wxGridBagSizer* const sSlippiReplaySettings = new wxGridBagSizer(space5, space5);
	sSlippiReplaySettings->Add(m_replay_enable_checkbox, wxGBPosition(0, 0), wxGBSpan(1, 2));
	sSlippiReplaySettings->Add(m_replay_month_folders_checkbox, wxGBPosition(1, 0), wxGBSpan(1, 2),
		wxRESERVE_SPACE_EVEN_IF_HIDDEN);
	sSlippiReplaySettings->Add(new wxStaticText(this, wxID_ANY, _("Replay folder:")), wxGBPosition(2, 0),
		wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	sSlippiReplaySettings->Add(m_replay_directory_picker, wxGBPosition(2, 1), wxDefaultSpan, wxEXPAND);
	sSlippiReplaySettings->AddGrowableCol(1);

	wxStaticBoxSizer* const sbSlippiReplaySettings =
		new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Replay Settings"));
	sbSlippiReplaySettings->AddSpacer(space5);
	sbSlippiReplaySettings->Add(sSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	sbSlippiReplaySettings->AddSpacer(space5);

	wxGridBagSizer* const sSlippiOnlineSettings = new wxGridBagSizer(space10, space5);
	sSlippiOnlineSettings->Add(m_slippi_delay_frames_txt, wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	sSlippiOnlineSettings->Add(m_slippi_delay_frames_ctrl, wxGBPosition(0, 1), wxDefaultSpan, wxALIGN_LEFT);
	sSlippiOnlineSettings->Add(m_slippi_enable_quick_chat, wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_LEFT);

	wxStaticBoxSizer* const sbSlippiOnlineSettings =
		new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Online Settings"));
	sbSlippiOnlineSettings->AddSpacer(space5);
	sbSlippiOnlineSettings->Add(sSlippiOnlineSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	sbSlippiOnlineSettings->AddSpacer(space5);
#endif
	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
#ifndef IS_PLAYBACK
	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiOnlineSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);
#endif
	SetSizer(main_sizer);
}

void SlippiConfigPane::LoadGUIValues()
{
	const SConfig& startup_params = SConfig::GetInstance();

#if HAVE_PORTAUDIO
#endif


#ifndef IS_PLAYBACK
	bool enableReplays = startup_params.m_slippiSaveReplays;

	m_replay_enable_checkbox->SetValue(enableReplays);
	m_replay_month_folders_checkbox->SetValue(startup_params.m_slippiReplayMonthFolders);
	m_replay_directory_picker->SetPath(StrToWxStr(startup_params.m_strSlippiReplayDir));

	if (!enableReplays) {
		m_replay_month_folders_checkbox->Hide();
	}

	m_slippi_delay_frames_ctrl->SetValue(startup_params.m_slippiOnlineDelay);
	m_slippi_enable_quick_chat->SetValue(startup_params.m_slippiEnableQuickChat);
#endif
}

void SlippiConfigPane::BindEvents()
{
#ifndef IS_PLAYBACK
	m_replay_enable_checkbox->Bind(wxEVT_CHECKBOX, &SlippiConfigPane::OnReplaySavingToggle, this);

	m_replay_month_folders_checkbox->Bind(wxEVT_CHECKBOX, &SlippiConfigPane::OnReplayMonthFoldersToggle,
		this);

	m_replay_directory_picker->Bind(wxEVT_DIRPICKER_CHANGED, &SlippiConfigPane::OnReplayDirChanged, this);

	m_slippi_delay_frames_ctrl->Bind(wxEVT_SPINCTRL, &SlippiConfigPane::OnDelayFramesChanged, this);
	m_slippi_enable_quick_chat->Bind(wxEVT_CHECKBOX, &SlippiConfigPane::OnQuickChatToggle, this);
#endif
}

void SlippiConfigPane::OnQuickChatToggle(wxCommandEvent& event)
{
	bool enableQuickChat = m_slippi_enable_quick_chat->IsChecked();
	SConfig::GetInstance().m_slippiEnableQuickChat = enableQuickChat;
}

void SlippiConfigPane::OnReplaySavingToggle(wxCommandEvent& event)
{
	bool enableReplays = m_replay_enable_checkbox->IsChecked();

	SConfig::GetInstance().m_slippiSaveReplays = enableReplays;

	if (enableReplays) {
		m_replay_month_folders_checkbox->Show();
	} else {
		m_replay_month_folders_checkbox->SetValue(false);
		m_replay_month_folders_checkbox->Hide();
		SConfig::GetInstance().m_slippiReplayMonthFolders = false;
	}
}

void SlippiConfigPane::OnReplayMonthFoldersToggle(wxCommandEvent& event)
{
	SConfig::GetInstance().m_slippiReplayMonthFolders =
		m_replay_enable_checkbox->IsChecked() &&
		m_replay_month_folders_checkbox->IsChecked();
}

void SlippiConfigPane::OnReplayDirChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_strSlippiReplayDir =
		WxStrToStr(m_replay_directory_picker->GetPath());
}

void SlippiConfigPane::OnDelayFramesChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().m_slippiOnlineDelay = m_slippi_delay_frames_ctrl->GetValue();
}
