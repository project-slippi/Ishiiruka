#include "DolphinWX/SlippiAuthWebView/SlippiAuthWebView.h"

#include <wx/debug.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/webview.h>

#include <wx/webviewfshandler.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"

#include "Core/Slippi/SlippiUser.h"

#include "DolphinWX/WxUtils.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

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

// On Windows, Edge *may* be available as of Oct 19th 2020. But it also might not.
// So, this is a check to silo some platform-specific logic, and should be called
// before initiating this flow (and opt to use another method of authentication, perhaps).
bool SlippiAuthWebView::IsAvailable()
{
#ifdef _WIN32
    if (!wxWebView::IsBackendAvailable(wxWebViewBackendEdge))
    {
        return false;
    }
#endif

    // macOS will almost certainly have a WebView, as it's built in to the platform.
    // Linux is built explicitly with webkitgtk2 and should have it, but this is admittedly
    // a bit of an assumption...
    return true;
}

void SlippiAuthWebView::CreateGUIControls()
{
    std::string url = "https://slippi.gg/online/enable?isWebview=true";

    // On Windows, we need to explicitly force it to elect to use Edge.
    // The other platforms use WebKit, thankfully... which is a one-liner.
#ifdef _WIN32
    m_browser = wxWebView::New(this, wxID_ANY, url, wxDefaultPosition, wxDefaultSize, wxWebViewBackendEdge);
#else
    m_browser = wxWebView::New(this, wxID_ANY, url);
#endif

    Bind(wxEVT_WEBVIEW_TITLE_CHANGED, &SlippiAuthWebView::OnTitleChanged, this, m_browser->GetId());

	const int space5 = FromDIP(5);
	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(m_browser, 1, wxEXPAND, space5);
	main_sizer->SetMinSize(800, 600);

	SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
	SetLayoutAdaptationLevel(wxDIALOG_ADAPTATION_STANDARD_SIZER);
	SetSizerAndFit(main_sizer);
}

void SlippiAuthWebView::OnClose(wxCloseEvent& WXUNUSED(event))
{
    delete this;
}

void SlippiAuthWebView::OnShow(wxShowEvent& event)
{
	if (event.IsShown())
		CenterOnParent();
}

void SlippiAuthWebView::OnCloseButton(wxCommandEvent& WXUNUSED(event))
{
    delete this;
}

// A cross-platform method of passing data from a webview: simply shove it through the
// document title. Our JSON payload is small enough (and in practice, browsers don't seem to
// limit this) that it works out fine.
void SlippiAuthWebView::OnTitleChanged(wxWebViewEvent& evt)
{
    wxString title = evt.GetString();
    wxString prefix("SlippiUser:");

    // If it's not the first thing, don't grab it.
    if (title.Find(prefix) != 0)
    {
        return;
    }

    int prefix_length = prefix.length();
    wxString userJSON = title.substr(prefix_length, title.length() - prefix_length);
    std::string user = std::string(userJSON.mb_str());

    // INFO_LOG(SLIPPI, "JSON: %s", user.c_str());
	
    // As a sanity check, let's try to parse it before writing it and make sure it's an actual
    // JSON object - i.e, don't write an arbitrary file. ;P
    auto res = json::parse(user, nullptr, false);
	if (res.is_discarded() || !res.is_object())
	{
        ERROR_LOG(SLIPPI, "File is invalid JSON, or not an object.");
		return;
	}

    // Now we can write it and do some cleanup
	std::string userFilePath = File::GetSlippiUserJSONPath();
    File::WriteStringToFile(user, userFilePath);

    // At this point, the background thread in SlippiUser will pick it up and the game should be 
    // logged in. From this point on, we want to go ahead and clean this up - having a browser instance
    // sitting around in memory is no fun when running a game. ;P
    delete this;
}
