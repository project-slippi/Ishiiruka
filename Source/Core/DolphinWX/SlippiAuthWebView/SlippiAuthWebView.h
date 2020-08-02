#pragma once

#include <wx/dialog.h>

#include "Common/CommonTypes.h"

class wxWebView;
class wxWebViewEvent;

class SlippiAuthWebView : public wxDialog
{
public:
	SlippiAuthWebView(wxWindow* parent, wxWindowID id = wxID_ANY,
		const wxString& title = _("Sign In to Slippi"),
		const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
		long style = wxDEFAULT_DIALOG_STYLE);
	virtual ~SlippiAuthWebView();

    void OnTitleChanged(wxWebViewEvent& evt);

private:
	void CreateGUIControls();
	void OnClose(wxCloseEvent& event);
	void OnCloseButton(wxCommandEvent& event);
	void OnShow(wxShowEvent& event);

    wxWebView* m_browser;
};
