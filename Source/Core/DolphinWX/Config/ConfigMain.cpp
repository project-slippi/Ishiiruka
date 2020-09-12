// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/ConfigMain.h"

#include <wx/debug.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/NetPlayProto.h"
#include "DolphinWX/Config/AdvancedConfigPane.h"
#include "DolphinWX/Config/AudioConfigPane.h"
#include "DolphinWX/Config/SlippiConfigPane.h"
#include "DolphinWX/Config/GameCubeConfigPane.h"
#include "DolphinWX/Config/GeneralConfigPane.h"
#include "DolphinWX/Config/InterfaceConfigPane.h"
#include "DolphinWX/Config/PathConfigPane.h"
#include "DolphinWX/Config/WiiConfigPane.h"
#include "DolphinWX/GameListCtrl.h"
#include "DolphinWX/WxUtils.h"

// Sent by child panes to signify that the game list should
// be updated when this modal dialog closes.
wxDEFINE_EVENT(wxDOLPHIN_CFG_REFRESH_LIST, wxCommandEvent);

CConfigMain::CConfigMain(wxWindow* parent, wxWindowID id, const wxString& title,
	const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	// Control refreshing of the ISOs list
	m_refresh_game_list_on_close = false;

	Bind(wxEVT_CLOSE_WINDOW, &CConfigMain::OnClose, this);
	Bind(wxEVT_BUTTON, &CConfigMain::OnCloseButton, this, wxID_CLOSE);
	Bind(wxEVT_SHOW, &CConfigMain::OnShow, this);
	Bind(wxDOLPHIN_CFG_REFRESH_LIST, &CConfigMain::OnSetRefreshGameListOnClose, this);

	wxDialog::SetExtraStyle(GetExtraStyle() & ~wxWS_EX_BLOCK_EVENTS);

	CreateGUIControls();
}

CConfigMain::~CConfigMain()
{
}

void CConfigMain::SetSelectedTab(wxWindowID tab_id)
{
	switch (tab_id)
	{
	case ID_GENERALPAGE:
	case ID_DISPLAYPAGE:
	case ID_AUDIOPAGE:
	case ID_GAMECUBEPAGE:
	case ID_WIIPAGE:
	case ID_PATHSPAGE:
	case ID_ADVANCEDPAGE:
		Notebook->SetSelection(Notebook->FindPage(Notebook->FindWindowById(tab_id)));
		break;

	default:
		wxASSERT_MSG(false, wxString::Format("Invalid tab page ID specified (%d)", tab_id));
		break;
	}
}

void CConfigMain::CreateGUIControls()
{
	// Create the notebook and pages
	Notebook = new wxNotebook(this, ID_NOTEBOOK);
	wxPanel* const general_pane = new GeneralConfigPane(Notebook, ID_GENERALPAGE);
	wxPanel* const interface_pane = new InterfaceConfigPane(Notebook, ID_DISPLAYPAGE);
	wxPanel* const audio_pane = new AudioConfigPane(Notebook, ID_AUDIOPAGE);
#ifndef IS_PLAYBACK
	wxPanel *const slippi_pane = new SlippiConfigPane(Notebook, ID_SLIPPIPAGE);
#endif
	wxPanel* const gamecube_pane = new GameCubeConfigPane(Notebook, ID_GAMECUBEPAGE);
	wxPanel* const wii_pane = new WiiConfigPane(Notebook, ID_WIIPAGE);
	wxPanel* const path_pane = new PathConfigPane(Notebook, ID_PATHSPAGE);
	wxPanel* const advanced_pane = new AdvancedConfigPane(Notebook, ID_ADVANCEDPAGE);

	Notebook->AddPage(general_pane, _("General"));
	Notebook->AddPage(interface_pane, _("Interface"));
	Notebook->AddPage(audio_pane, _("Audio"));
#ifndef IS_PLAYBACK
	Notebook->AddPage(slippi_pane, _("Slippi"));
#endif
	Notebook->AddPage(gamecube_pane, _("GameCube"));
	Notebook->AddPage(wii_pane, _("Wii"));
	Notebook->AddPage(path_pane, _("Paths"));
	Notebook->AddPage(advanced_pane, _("Advanced"));

	const int space5 = FromDIP(5);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(Notebook, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(CreateButtonSizer(wxCLOSE), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);

#ifdef __APPLE__
	main_sizer->SetMinSize(550, 0);
#else
	main_sizer->SetMinSize(FromDIP(400), 0);
#endif

	SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
	SetLayoutAdaptationLevel(wxDIALOG_ADAPTATION_STANDARD_SIZER);
	SetSizerAndFit(main_sizer);
}

void CConfigMain::OnClose(wxCloseEvent& WXUNUSED(event))
{
	Hide();

	SConfig::GetInstance().SaveSettings();

	if (m_refresh_game_list_on_close)
		AddPendingEvent(wxCommandEvent{ DOLPHIN_EVT_RELOAD_GAMELIST });
}

void CConfigMain::OnShow(wxShowEvent& event)
{
	if (event.IsShown())
		CenterOnParent();
}

void CConfigMain::OnCloseButton(wxCommandEvent& WXUNUSED(event))
{
	Close();
}

void CConfigMain::OnSetRefreshGameListOnClose(wxCommandEvent& WXUNUSED(event))
{
	m_refresh_game_list_on_close = true;
}
