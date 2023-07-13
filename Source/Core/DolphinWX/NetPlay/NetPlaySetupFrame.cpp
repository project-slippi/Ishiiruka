// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/gbsizer.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/NetPlay/NetPlayLauncher.h"
#include "DolphinWX/NetPlay/NetPlaySetupFrame.h"
#include "DolphinWX/NetPlay/NetWindow.h"
#include "DolphinWX/WxUtils.h"

namespace
{
wxString GetTraversalLabelText(IniFile::Section& section)
{
	std::string server = NetPlayLaunchConfig::GetTraversalHostFromIniConfig(section);
	std::string port = std::to_string(NetPlayLaunchConfig::GetTraversalPortFromIniConfig(section));
	return wxString::Format(_("Traversal Server: %s"), (server + ":" + port).c_str());
}
}  // Anonymous namespace

NetPlaySetupFrame::NetPlaySetupFrame(wxWindow* const parent, const CGameListCtrl* const game_list)
	: wxFrame(parent, wxID_ANY, _("Dolphin NetPlay Setup")), m_game_list(game_list)
{
	IniFile inifile;
	inifile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	CreateGUI();
	SetIcons(WxUtils::GetDolphinIconBundle());

	{
		std::string temp;
		netplay_section.Get("Nickname", &temp, "Player");
		m_nickname_text->SetValue(StrToWxStr(temp));

		temp.clear();
		netplay_section.Get("HostCode", &temp, "00000000");
		m_connect_hashcode_text->SetValue(StrToWxStr(temp));

		temp.clear();
		netplay_section.Get("Address", &temp, "127.0.0.1");
		m_connect_ip_text->SetValue(StrToWxStr(temp));

		temp.clear();
		netplay_section.Get("ConnectPort", &temp,
			std::to_string(NetPlayHostConfig::DEFAULT_LISTEN_PORT));
		m_connect_port_text->SetValue(StrToWxStr(temp));

		temp.clear();
		netplay_section.Get("HostPort", &temp, std::to_string(NetPlayHostConfig::DEFAULT_LISTEN_PORT));
		m_host_port_text->SetValue(StrToWxStr(temp));

		temp.clear();
		if (netplay_section.Get("SelectedHostGame", &temp, ""))
			m_game_lbox->SetStringSelection(StrToWxStr(temp));

#ifdef USE_UPNP
		bool use_upnp = false;
		netplay_section.Get("UseUPNP", &use_upnp, false);
		m_upnp_chk->SetValue(use_upnp);
#endif

		bool is_spectator = false;
		netplay_section.Get("IsSpectator", &is_spectator, false);
		m_spectator_toggle->SetValue(is_spectator);

		unsigned int listen_port = 0;
		netplay_section.Get("ListenPort", &listen_port, 0);
		m_traversal_listen_port_enabled->SetValue(listen_port != 0);
		m_traversal_listen_port->Enable(m_traversal_listen_port_enabled->IsChecked());
		m_traversal_listen_port->SetValue(listen_port);

		temp.clear();
		netplay_section.Get("TraversalChoice", &temp, "direct");
		if (temp == "traversal")
		{
			m_direct_traversal->Select(TRAVERSAL_CHOICE);
		}
		else
		{
			m_direct_traversal->Select(DIRECT_CHOICE);
		}

		m_traversal_lbl->SetLabelText(GetTraversalLabelText(netplay_section));
	}

	Center();
	Show();

	//  Needs to be done last or it set up the spacing on the page correctly
	wxCommandEvent ev;
	OnDirectTraversalChoice(ev);
}

void NetPlaySetupFrame::CreateGUI()
{
	const int space5 = FromDIP(5);

	wxPanel* const panel = new wxPanel(this);
	panel->Bind(wxEVT_CHAR_HOOK, &NetPlaySetupFrame::OnKeyDown, this);

	// Connection Config
	wxStaticText* const connectiontype_lbl = new wxStaticText(panel, wxID_ANY, _("Connection Type:"));

	m_direct_traversal = new wxChoice(panel, wxID_ANY);
	m_direct_traversal->Bind(wxEVT_CHOICE, &NetPlaySetupFrame::OnDirectTraversalChoice, this);
	m_direct_traversal->Append(_("Direct Connection"));
	m_direct_traversal->Append(_("Traversal Server"));

	m_trav_reset_btn = new wxButton(panel, wxID_ANY, _("Reset Traversal Settings"));
	m_trav_reset_btn->Bind(wxEVT_BUTTON, &NetPlaySetupFrame::OnResetTraversal, this);

	// Nickname
	wxStaticText* const nick_lbl = new wxStaticText(panel, wxID_ANY, _("Nickname:"));

	m_nickname_text = new wxTextCtrl(panel, wxID_ANY, "Player");

	m_traversal_lbl = new wxStaticText(panel, wxID_ANY, "Traversal Server");

	wxButton* const quit_btn = new wxButton(panel, wxID_ANY, _("Quit"));
	quit_btn->Bind(wxEVT_BUTTON, &NetPlaySetupFrame::OnQuit, this);

	wxGridBagSizer* top_sizer = new wxGridBagSizer(space5, space5);
	top_sizer->Add(connectiontype_lbl, wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	top_sizer->Add(WxUtils::GiveMinSizeDIP(m_direct_traversal, wxSize(100, -1)), wxGBPosition(0, 1),
		wxDefaultSpan, wxEXPAND);
	top_sizer->Add(m_trav_reset_btn, wxGBPosition(0, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	top_sizer->Add(nick_lbl, wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	top_sizer->Add(WxUtils::GiveMinSizeDIP(m_nickname_text, wxSize(150, -1)), wxGBPosition(1, 1),
		wxDefaultSpan, wxEXPAND);

	m_notebook = CreateNotebookGUI(panel);
	m_notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &NetPlaySetupFrame::OnTabChanged, this);

	// main sizer
	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
	main_szr->AddSpacer(space5);
	main_szr->Add(top_sizer, 0, wxLEFT | wxRIGHT, space5);
	main_szr->AddSpacer(space5);
	main_szr->Add(m_traversal_lbl, 0, wxLEFT | wxRIGHT, space5);
	main_szr->AddSpacer(space5);
	main_szr->Add(m_notebook, 1, wxLEFT | wxRIGHT | wxEXPAND, space5);
	main_szr->AddSpacer(space5);
	main_szr->Add(quit_btn, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT, space5);
	main_szr->AddSpacer(space5);

	panel->SetSizerAndFit(main_szr);
	main_szr->SetSizeHints(this);
}

wxNotebook* NetPlaySetupFrame::CreateNotebookGUI(wxWindow* parent)
{
	const int space5 = FromDIP(5);

	wxNotebook* const notebook = new wxNotebook(parent, wxID_ANY);
	wxPanel* const connect_tab = new wxPanel(notebook, wxID_ANY);
	notebook->AddPage(connect_tab, _("Connect"));
	wxPanel* const host_tab = new wxPanel(notebook, wxID_ANY);
	notebook->AddPage(host_tab, _("Host"));

	// connect tab
	{
		m_ip_lbl = new wxStaticText(connect_tab, wxID_ANY, _("Host Code :"));
		m_connect_ip_text = new wxTextCtrl(connect_tab, wxID_ANY, "127.0.0.1");
		m_connect_hashcode_text = new wxTextCtrl(connect_tab, wxID_ANY, "00000000");

		// Will be overridden by OnDirectTraversalChoice, but is necessary
		// so that both inputs do not take up space
		m_connect_hashcode_text->Hide();

		m_client_port_lbl = new wxStaticText(connect_tab, wxID_ANY, _("Port :"));
		m_connect_port_text = new wxTextCtrl(connect_tab, wxID_ANY,
			std::to_string(NetPlayHostConfig::DEFAULT_LISTEN_PORT));

		wxButton* const connect_btn = new wxButton(connect_tab, wxID_ANY, _("Connect"));
		connect_btn->Bind(wxEVT_BUTTON, &NetPlaySetupFrame::OnJoin, this);

		wxStaticText* const alert_lbl = new wxStaticText(
			connect_tab, wxID_ANY,
			_("ALERT:\n\n"
				"All players must use the same Dolphin version.\n"
				"All memory cards, SD cards and cheats must be identical between players or disabled.\n"
				"If DSP LLE is used, DSP ROMs must be identical between players.\n"
				"If connecting directly, the host must have the chosen UDP port open/forwarded!\n"
				"\n"
				"Wii Remote support in netplay is experimental and should not be expected to work.\n"));

		wxStaticText *const spectator_info = new wxStaticText(connect_tab, wxID_ANY, 
			_("Enabling Spectator Mode will disable your controller so you can view the game without interfering with the players."));

		m_spectator_toggle = new wxCheckBox(connect_tab, wxID_ANY, _("Spectator Mode"));

		wxBoxSizer* const top_szr = new wxBoxSizer(wxHORIZONTAL);
		top_szr->Add(m_ip_lbl, 0, wxALIGN_CENTER_VERTICAL);
		top_szr->Add(m_connect_ip_text, 3, wxALIGN_CENTER_VERTICAL | wxLEFT, space5);
		top_szr->Add(m_connect_hashcode_text, 3, wxALIGN_CENTER_VERTICAL | wxLEFT, space5);
		top_szr->Add(m_client_port_lbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space5);
		top_szr->Add(m_connect_port_text, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, space5);

		wxBoxSizer* const con_szr = new wxBoxSizer(wxVERTICAL);
		con_szr->AddSpacer(space5);
		con_szr->Add(top_szr, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
		con_szr->AddStretchSpacer(1);
		con_szr->AddSpacer(space5);
		con_szr->Add(alert_lbl, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
		con_szr->AddStretchSpacer(1);
		con_szr->AddSpacer(space5);
		con_szr->Add(spectator_info, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
		con_szr->Add(m_spectator_toggle, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
		con_szr->AddSpacer(space5);
		con_szr->Add(connect_btn, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT, space5);
		con_szr->AddSpacer(space5);

		connect_tab->SetSizerAndFit(con_szr);
	}

	// host tab
	{
		m_host_port_lbl = new wxStaticText(host_tab, wxID_ANY, _("Port :"));
		m_host_port_text =
			new wxTextCtrl(host_tab, wxID_ANY, std::to_string(NetPlayHostConfig::DEFAULT_LISTEN_PORT));

		m_traversal_listen_port_enabled = new wxCheckBox(host_tab, wxID_ANY, _("Force Listen Port: "));
		m_traversal_listen_port = new wxSpinCtrl(host_tab, wxID_ANY, "", wxDefaultPosition,
			wxDefaultSize, wxSP_ARROW_KEYS, 1, 65535);
		m_traversal_listen_port->SetMinSize(WxUtils::GetTextWidgetMinSize(m_traversal_listen_port));

		m_traversal_listen_port_enabled->Bind(wxEVT_CHECKBOX,
			&NetPlaySetupFrame::OnTraversalListenPortChanged, this);
		m_traversal_listen_port->Bind(wxEVT_TEXT, &NetPlaySetupFrame::OnTraversalListenPortChanged,
			this);

		wxButton* const host_btn = new wxButton(host_tab, wxID_ANY, _("Host"));
		host_btn->Bind(wxEVT_BUTTON, &NetPlaySetupFrame::OnHost, this);

		m_game_lbox =
			new wxListBox(host_tab, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
		m_game_lbox->Bind(wxEVT_LISTBOX_DCLICK, &NetPlaySetupFrame::OnHost, this);

		NetPlayDialog::FillWithGameNames(m_game_lbox, *m_game_list);

		wxBoxSizer* const top_szr = new wxBoxSizer(wxHORIZONTAL);
		top_szr->Add(m_host_port_lbl, 0, wxALIGN_CENTER_VERTICAL);
		top_szr->Add(m_host_port_text, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space5);
#ifdef USE_UPNP
		m_upnp_chk = new wxCheckBox(host_tab, wxID_ANY, _("Use port mapping (UPnP/NAT-PMP)"));
		top_szr->Add(m_upnp_chk, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space5);
#endif

		wxBoxSizer* const bottom_szr = new wxBoxSizer(wxHORIZONTAL);
		bottom_szr->Add(m_traversal_listen_port_enabled, 0, wxALIGN_CENTER_VERTICAL);
		bottom_szr->Add(m_traversal_listen_port, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space5);
		bottom_szr->AddStretchSpacer();
		bottom_szr->Add(host_btn, 0, wxLEFT, space5);

		wxBoxSizer* const host_szr = new wxBoxSizer(wxVERTICAL);
		// NOTE: Top row can disappear entirely
		host_szr->Add(top_szr, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, space5);
		host_szr->AddSpacer(space5);
		host_szr->Add(m_game_lbox, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
		host_szr->AddSpacer(space5);
		host_szr->Add(bottom_szr, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
		host_szr->AddSpacer(space5);

		host_tab->SetSizerAndFit(host_szr);
	}

	return notebook;
}

NetPlaySetupFrame::~NetPlaySetupFrame()
{
	IniFile inifile;
	const std::string dolphin_ini = File::GetUserPath(F_DOLPHINCONFIG_IDX);
	inifile.Load(dolphin_ini);
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	std::string travChoice;
	switch (m_direct_traversal->GetSelection())
	{
	case TRAVERSAL_CHOICE:
		travChoice = "traversal";
		break;
	case DIRECT_CHOICE:
		travChoice = "direct";
		break;
	}

	netplay_section.Set("TraversalChoice", travChoice);
	netplay_section.Set("Nickname", WxStrToStr(m_nickname_text->GetValue()));

	if (m_direct_traversal->GetCurrentSelection() == DIRECT_CHOICE)
		netplay_section.Set("Address", WxStrToStr(m_connect_ip_text->GetValue()));
	else
		netplay_section.Set("HostCode", WxStrToStr(m_connect_hashcode_text->GetValue()));

	netplay_section.Set("ConnectPort", WxStrToStr(m_connect_port_text->GetValue()));
	netplay_section.Set("HostPort", WxStrToStr(m_host_port_text->GetValue()));
	netplay_section.Set("ListenPort", m_traversal_listen_port_enabled->IsChecked() ?
		m_traversal_listen_port->GetValue() :
		0);
	netplay_section.Set("IsSpectator", m_spectator_toggle->IsChecked());

#ifdef USE_UPNP
	netplay_section.Set("UseUPNP", m_upnp_chk->GetValue(), false);
#endif

	inifile.Save(dolphin_ini);
	main_frame->g_NetPlaySetupDiag = nullptr;
}

void NetPlaySetupFrame::OnHost(wxCommandEvent&)
{
	DoHost();
}

void NetPlaySetupFrame::DoHost()
{
	if (m_game_lbox->GetSelection() == wxNOT_FOUND)
	{
		WxUtils::ShowErrorDialog(_("You must choose a game!"));
		return;
	}

	IniFile ini_file;
	const std::string dolphin_ini = File::GetUserPath(F_DOLPHINCONFIG_IDX);
	ini_file.Load(dolphin_ini);
	IniFile::Section& netplay_section = *ini_file.GetOrCreateSection("NetPlay");

	NetPlayHostConfig host_config;
	host_config.game_name = WxStrToStr(m_game_lbox->GetStringSelection());
	host_config.use_traversal = m_direct_traversal->GetCurrentSelection() == TRAVERSAL_CHOICE;
	host_config.player_name = WxStrToStr(m_nickname_text->GetValue());
	host_config.game_list_ctrl = m_game_list;
	host_config.SetDialogInfo(netplay_section, m_parent);
#ifdef USE_UPNP
	host_config.forward_port = m_upnp_chk->GetValue();
#endif

	if (host_config.use_traversal)
	{
		host_config.listen_port = static_cast<u16>(
			m_traversal_listen_port_enabled->IsChecked() ? m_traversal_listen_port->GetValue() : 0);
	}
	else
	{
		unsigned long listen_port;
		m_host_port_text->GetValue().ToULong(&listen_port);
		host_config.listen_port = static_cast<u16>(listen_port);
	}

	host_config.traversal_port = NetPlayLaunchConfig::GetTraversalPortFromIniConfig(netplay_section);
	host_config.traversal_host = NetPlayLaunchConfig::GetTraversalHostFromIniConfig(netplay_section);

	netplay_section.Set("SelectedHostGame", host_config.game_name);
	ini_file.Save(dolphin_ini);

	if (NetPlayLauncher::Host(host_config))
	{
		Destroy();
	}
}

void NetPlaySetupFrame::OnJoin(wxCommandEvent&)
{
	DoJoin();
}

void NetPlaySetupFrame::DoJoin()
{
	IniFile inifile;
	inifile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	NetPlayJoinConfig join_config;
	join_config.use_traversal = m_direct_traversal->GetCurrentSelection() == TRAVERSAL_CHOICE;
	join_config.player_name = WxStrToStr(m_nickname_text->GetValue());
	join_config.game_list_ctrl = m_game_list;
	join_config.SetDialogInfo(netplay_section, m_parent);

	unsigned long port = 0;
	m_connect_port_text->GetValue().ToULong(&port);

	join_config.connect_port = static_cast<u16>(port);

	if (join_config.use_traversal)
		join_config.connect_hash_code = WxStrToStr(m_connect_hashcode_text->GetValue());
	else
		join_config.connect_host = WxStrToStr(m_connect_ip_text->GetValue());

	join_config.traversal_port = NetPlayLaunchConfig::GetTraversalPortFromIniConfig(netplay_section);
	join_config.traversal_host = NetPlayLaunchConfig::GetTraversalHostFromIniConfig(netplay_section);

	netplay_section.Get("IsSpectator", &join_config.is_spectator, false);

	if (NetPlayLauncher::Join(join_config))
	{
		Destroy();
	}
}

void NetPlaySetupFrame::OnResetTraversal(wxCommandEvent& event)
{
	IniFile inifile;
	const std::string dolphin_ini = File::GetUserPath(F_DOLPHINCONFIG_IDX);
	inifile.Load(dolphin_ini);
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");
	netplay_section.Delete("TraversalServer");
	netplay_section.Delete("TraversalPort");
	inifile.Save(dolphin_ini);

	m_traversal_lbl->SetLabelText(GetTraversalLabelText(netplay_section));
}

void NetPlaySetupFrame::OnTraversalListenPortChanged(wxCommandEvent& event)
{
	m_traversal_listen_port->Enable(m_traversal_listen_port_enabled->IsChecked());
}

void NetPlaySetupFrame::OnDirectTraversalChoice(wxCommandEvent& event)
{
	int sel = m_direct_traversal->GetSelection();
	IniFile inifile;
	inifile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	if (sel == TRAVERSAL_CHOICE)
	{
		m_traversal_lbl->SetLabelText(m_traversal_string);
		m_trav_reset_btn->Show();
		m_connect_hashcode_text->Show();
		m_connect_ip_text->Hide();
		// Traversal
		// client tab
		{
			m_ip_lbl->SetLabelText("Host Code: ");
			m_client_port_lbl->Hide();
			m_connect_port_text->Hide();
		}

		// server tab
		{
			m_host_port_lbl->Hide();
			m_host_port_text->Hide();
			m_traversal_listen_port->Show();
			m_traversal_listen_port_enabled->Show();
#ifdef USE_UPNP
			m_upnp_chk->Hide();
#endif
		}
	}
	else
	{
		m_traversal_lbl->SetLabel(wxEmptyString);
		m_trav_reset_btn->Hide();
		m_connect_hashcode_text->Hide();
		m_connect_ip_text->Show();
		// Direct
		// Client tab
		{
			m_ip_lbl->SetLabelText("IP Address :");

			std::string address;
			netplay_section.Get("Address", &address, "127.0.0.1");
			m_connect_ip_text->SetLabelText(address);

			m_client_port_lbl->Show();
			m_connect_port_text->Show();
		}

		// Server tab
		m_traversal_listen_port->Hide();
		m_traversal_listen_port_enabled->Hide();
		m_host_port_lbl->Show();
		m_host_port_text->Show();
#ifdef USE_UPNP
		m_upnp_chk->Show();
#endif
	}

	// wxWidgets' layout engine sucks. It only updates when a size event occurs so we
	// have to manually invoke the layout system.
	// Caveat: This only works if the new layout is not substantially different from the
	//   old one because otherwise the minimum sizes assigned by SetSizerAndFit won't make
	//   sense and the layout will break (overlapping widgets). You can't just SetSizeHints
	//   because that will change the current sizes as well as the minimum sizes, it's a mess.
	for (wxWindow* tab : m_notebook->GetChildren())
		tab->Layout();
	// Because this is a wxFrame, not a dialog, everything is inside a wxPanel which
	// is the only direct child of the frame.
	GetChildren()[0]->Layout();

	DispatchFocus();
}

void NetPlaySetupFrame::OnKeyDown(wxKeyEvent& event)
{
	// Let the event propagate
	event.Skip();

	if (event.GetKeyCode() != wxKeyCode::WXK_RETURN)
		return;

	int current_tab = m_notebook->GetSelection();

	switch (current_tab)
	{
	case CONNECT_TAB:
		DoJoin();
		break;
	case HOST_TAB:
		DoHost();
		break;
	}
}

void NetPlaySetupFrame::OnTabChanged(wxCommandEvent& event)
{
	// Propagate event
	event.Skip();

	// Let the base class fiddle with the focus first then correct it afterwards
	CallAfter(&NetPlaySetupFrame::DispatchFocus);
}

void NetPlaySetupFrame::DispatchFocus()
{
	int current_tab = m_notebook->GetSelection();

	switch (current_tab)
	{
	case CONNECT_TAB:
		if (m_direct_traversal->GetCurrentSelection() == TRAVERSAL_CHOICE)
			m_connect_hashcode_text->SetFocus();
		else
			m_connect_ip_text->SetFocus();
		break;

	case HOST_TAB:
		m_game_lbox->SetFocus();
		break;
	}
}

void NetPlaySetupFrame::OnQuit(wxCommandEvent&)
{
	Destroy();
}
