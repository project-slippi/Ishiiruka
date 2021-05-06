// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/SlippiPlaybackConfigPane.h"

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
#include <wx/valtext.h>

SlippiPlaybackConfigPane::SlippiPlaybackConfigPane(wxWindow *parent, wxWindowID id) : wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	BindEvents();
}

void SlippiPlaybackConfigPane::InitializeGUI()
{
	// Slippi Replay settings
	m_display_frame_index = new wxCheckBox(this, wxID_ANY, _("Display Frame index"));
	m_display_frame_index->SetToolTip(
	    _("Enable this to display the Frame Index when viewing replays."));


	const int space5 = FromDIP(5);


	wxGridBagSizer *const sSlippiReplaySettings = new wxGridBagSizer(space5, space5);
	sSlippiReplaySettings->Add(m_display_frame_index, wxGBPosition(0, 0), wxGBSpan(1, 2));
	sSlippiReplaySettings->AddGrowableCol(1);


	wxStaticBoxSizer *const sbSlippiReplaySettings =
	    new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Replay Settings"));
	sbSlippiReplaySettings->AddSpacer(space5);
	sbSlippiReplaySettings->Add(sSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	sbSlippiReplaySettings->AddSpacer(space5);

	wxBoxSizer *const main_sizer = new wxBoxSizer(wxVERTICAL);

	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);

	SetSizer(main_sizer);
}

void SlippiPlaybackConfigPane::LoadGUIValues()
{
	const SConfig &startup_params = SConfig::GetInstance();

	bool enableFrameIndex = startup_params.m_slippiEnableFrameIndex;

	m_display_frame_index->SetValue(enableFrameIndex);
}

void SlippiPlaybackConfigPane::BindEvents()
{
	m_display_frame_index->Bind(wxEVT_CHECKBOX, &SlippiPlaybackConfigPane::OnDisplayFrameIndexToggle, this);
}

void SlippiPlaybackConfigPane::OnDisplayFrameIndexToggle(wxCommandEvent &event)
{
	bool enableFrameIndex = m_display_frame_index->IsChecked();
	SConfig::GetInstance().m_slippiEnableFrameIndex = enableFrameIndex;
}
