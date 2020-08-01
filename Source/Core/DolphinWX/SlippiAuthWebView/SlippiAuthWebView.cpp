
#include <wx/debug.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/webview.h>
#include <wx/webviewfshandler.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "DolphinWX/SlippiAuthWebView/SlippiAuthWebView.h"
#include "DolphinWX/WxUtils.h"

SlippiAuthWebView::SlippiAuthWebView(wxWindow* parent, wxWindowID id, const wxString& title,
	const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	Bind(wxEVT_CLOSE_WINDOW, &SlippiAuthWebView::OnClose, this);
	Bind(wxEVT_BUTTON, &SlippiAuthWebView::OnCloseButton, this, wxID_CLOSE);
	Bind(wxEVT_SHOW, &SlippiAuthWebView::OnShow, this);

	wxDialog::SetExtraStyle(GetExtraStyle() & ~wxWS_EX_BLOCK_EVENTS);

	CreateGUIControls();
}

SlippiAuthWebView::~SlippiAuthWebView()
{
}

void SlippiAuthWebView::CreateGUIControls()
{
    std::string url = "https://slippi.gg/online/enable";
    m_browser = wxWebView::New(this, wxID_ANY, url);

    //Connect(m_browser->GetId(), wxEVT_WEBVIEW_NAVIGATED,
    //        wxWebViewEventHandler(SlippiAuthWebView::OnNavigationComplete), NULL, this);
    Connect(m_browser->GetId(), wxEVT_WEBVIEW_TITLE_CHANGED,
            wxWebViewEventHandler(SlippiAuthWebView::OnTitleChanged), NULL, this);

	const int space5 = FromDIP(5);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(m_browser, 1, wxEXPAND, space5);

#ifdef __APPLE__
	main_sizer->SetMinSize(800, 600);
#else
	main_sizer->SetMinSize(FromDIP(400), 0);
#endif

	SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
	SetLayoutAdaptationLevel(wxDIALOG_ADAPTATION_STANDARD_SIZER);
	SetSizerAndFit(main_sizer);
}

void SlippiAuthWebView::OnClose(wxCloseEvent& WXUNUSED(event))
{
	Hide();

	//SConfig::GetInstance().SaveSettings();
}

void SlippiAuthWebView::OnShow(wxShowEvent& event)
{
	if (event.IsShown())
		CenterOnParent();
}

void SlippiAuthWebView::OnCloseButton(wxCommandEvent& WXUNUSED(event))
{
	Close();
}

void SlippiAuthWebView::OnTitleChanged(wxWebViewEvent& evt)
{
    wxString title = evt.GetString();
    INFO_LOG(SLIPPI, "Title: %s",  (const char*)title.mb_str());
}

/*void SlippiAuthWebView::OnNavigationComplete(wxWebViewEvent& evt)
{
    wxString url = evt.GetURL();
    wxString lookup("blob:https://slippi.gg/");
    
    if (url.Find(lookup) == wxNOT_FOUND)
    {
        return;
    }

    INFO_LOG(SLIPPI, "Data blob found; url='%s'",  (const char*)url.mb_str());
    wxString source = m_browser->GetPageSource();
    INFO_LOG(SLIPPI, "Found JSON? %i %s", source.length(), (const char*)source.mb_str());
}*/
