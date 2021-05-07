// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include <wx/dialog.h>

class wxNotebook;
class wxPanel;

wxDECLARE_EVENT(wxDOLPHIN_CFG_REFRESH_LIST, wxCommandEvent);

class CConfigMain : public wxDialog
{
  public:
	CConfigMain(wxWindow *parent, wxWindowID id = wxID_ANY, const wxString &title = _("Dolphin Configuration"),
	            const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize,
	            long style = wxDEFAULT_DIALOG_STYLE);
	virtual ~CConfigMain();

	void SetSelectedTab(wxWindowID tab_id);

	enum
	{
		ID_NOTEBOOK = 1000,
		ID_GENERALPAGE,
		ID_DISPLAYPAGE,
		ID_AUDIOPAGE,
		ID_SLIPPINETPLAYPAGE,
		ID_SLIPPIPLAYBACKPAGE,
		ID_GAMECUBEPAGE,
		ID_WIIPAGE,
		ID_PATHSPAGE,
		ID_ADVANCEDPAGE,
	};

  private:
	void CreateGUIControls();
	void OnClose(wxCloseEvent &event);
	void OnCloseButton(wxCommandEvent &event);
	void OnShow(wxShowEvent &event);
	void OnSetRefreshGameListOnClose(wxCommandEvent &event);

	wxNotebook *Notebook;

	bool m_refresh_game_list_on_close;
};
