// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>
#include <cstring>
#include <locale>
#include <mutex>
#include <string>
#include <utility>

#include <wx/app.h>
#include <wx/buffer.h>
#include <wx/cmdline.h>
#include <wx/evtloop.h>
#include <wx/image.h>
#include <wx/imagpng.h>
#include <wx/intl.h>
#include <wx/language.h>
#include <wx/msgdlg.h>
#include <wx/thread.h>
#include <wx/timer.h>
#include <wx/tooltip.h>
#include <wx/utils.h>
#include <wx/window.h>

#include "Common/CPUDetect.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/LogManager.h"
#include "Common/Thread.h"

#include "UICommon/UICommon.h"

#include "Core/Analytics.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/Movie.h"
#include "Core/Slippi/SlippiSpectate.h"

#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"

#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/JitWindow.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/NetPlay/NetWindow.h"
#include "DolphinWX/SoftwareVideoConfigDialog.h"
#include "DolphinWX/VideoConfigDiag.h"
#include "DolphinWX/WxUtils.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoBackendBase.h"

#if defined HAVE_X11 && HAVE_X11
#include <X11/Xlib.h>
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#endif

#ifdef _WIN32

// Applications exporting this symbol with this value will be automatically
// directed to the high-performance GPU on Nvidia Optimus systems with
// up-to-date drivers
//
__declspec(dllexport) DWORD NvOptimusEnablement = 1;

// Applications exporting this symbol with this value will be automatically
// directed to the high-performance GPU on AMD PowerXpress systems with
// up-to-date drivers
//
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

#endif
// ------------
//  Main window

IMPLEMENT_APP(DolphinApp)

bool wxMsgAlert(const char *, const char *, bool, int);
std::string wxStringTranslator(const char *);

CFrame *main_frame = nullptr;

void UICommon::LowerRenderWindow()
{
	main_frame->LowerRenderWindow();
}

void UICommon::RaiseRenderWindow()
{
	main_frame->RaiseRenderWindow();
}

static std::mutex s_init_mutex;

bool DolphinApp::Initialize(int &c, wxChar **v)
{
#if defined HAVE_X11 && HAVE_X11
	XInitThreads();
#endif
	return wxApp::Initialize(c, v);
}

// The 'main program' equivalent that creates the main window and return the main frame

bool DolphinApp::OnInit()
{
	std::lock_guard<std::mutex> lk(s_init_mutex);
	if (!wxApp::OnInit())
		return false;
	if (m_show_version)
	{
		std::cout << scm_slippi_semver_str << std::endl;
		return false;
	}
	wxLog::SetLogLevel(0);
	Bind(wxEVT_QUERY_END_SESSION, &DolphinApp::OnEndSession, this);
	Bind(wxEVT_END_SESSION, &DolphinApp::OnEndSession, this);
	Bind(wxEVT_IDLE, &DolphinApp::OnIdle, this);
	Bind(wxEVT_ACTIVATE_APP, &DolphinApp::OnActivate, this);

	// Register message box and translation handlers
	RegisterMsgAlertHandler(&wxMsgAlert);
	RegisterStringTranslator(&wxStringTranslator);

#if wxUSE_ON_FATAL_EXCEPTION
	wxHandleFatalExceptions(true);
#endif

	UICommon::SetUserDirectory(m_user_path.ToStdString());
	UICommon::CreateDirectories();
	InitLanguageSupport(); // The language setting is loaded from the user directory
	UICommon::Init();

	if (m_select_video_backend && !m_video_backend_name.empty())
		SConfig::GetInstance().m_strVideoBackend = WxStrToStr(m_video_backend_name);

#ifdef IS_PLAYBACK
	// Fallback to a default config file path if the user fails to provide one
	if (m_select_slippi_input && !m_slippi_input_name.empty())
		SConfig::GetInstance().m_strSlippiInput = WxStrToStr(m_slippi_input_name);
	else
		SConfig::GetInstance().m_strSlippiInput = "Slippi/playback.txt";

	if (m_hide_seekbar) // Hide seekbar if necessary by cmd line (mostly for external recording applications)
		SConfig::GetInstance().m_CLIHideSeekbar = true;

	if (m_enable_cout) // Enable cout if necessary by cmd line (mostly for external recording applications)
		SConfig::GetInstance().m_coutEnabled = true;
#endif

	if (m_select_output_directory && !m_output_directory.empty())
	{
		std::string output_directory = WxStrToStr(m_output_directory);
		if (output_directory.back() != '/' && output_directory.back() != '\\')
			output_directory = output_directory + "/";
		SConfig::GetInstance().m_strOutputDirectory = output_directory;
	}

	if (m_select_output_filename_base && !m_output_filename_base.empty())
		SConfig::GetInstance().m_strOutputFilenameBase = WxStrToStr(m_output_filename_base);

	if (m_select_audio_emulation)
		SConfig::GetInstance().bDSPHLE = (m_audio_emulation_name.Upper() == "HLE");

	VideoBackendBase::ActivateBackend(SConfig::GetInstance().m_strVideoBackend);

	DolphinAnalytics::Instance()->ReportDolphinStart("wx");

	wxToolTip::Enable(!SConfig::GetInstance().m_DisableTooltips);

	// Enable the PNG image handler for screenshots
	wxImage::AddHandler(new wxPNGHandler);

#ifdef __APPLE__
	// Here we check if the app is running in a quarantined state. A quarantined flag is
	// applied by macOS GateKeeper if the app is unsigned, downloaded from the internet, or
	// some other flags that can't possibly be listed here.
	//
	// If we detect that it's running quarantined, we tell the user to explicitly move it to the
	// Applications folder, otherwise the app will fail in subtle fails due to be mounted as a translocated
	// binary and being "read-only".
	typedef Boolean (*SecTranslocateIsTranslocatedURL)(CFURLRef path, bool *isTranslocated, CFErrorRef *error);
	typedef CFURLRef (*SecTranslocateCreateOriginalPathForURL)(CFURLRef translocatedPath, CFErrorRef * error);

	void *security_framework = dlopen("/System/Library/Frameworks/Security.framework/Security", RTLD_NOW);

	if (security_framework)
	{
		SecTranslocateIsTranslocatedURL SecTranslocateIsTranslocatedURL_func =
		    (SecTranslocateIsTranslocatedURL)dlsym(security_framework, "SecTranslocateIsTranslocatedURL");
		SecTranslocateCreateOriginalPathForURL SecTranslocateCreateOriginalPathForURL_func =
		    (SecTranslocateCreateOriginalPathForURL)dlsym(security_framework, "SecTranslocateCreateOriginalPathForURL");

		if (SecTranslocateIsTranslocatedURL_func && SecTranslocateCreateOriginalPathForURL_func)
		{
			CFStringRef path =
			    CFStringCreateWithCString(NULL, File::GetBundleDirectory().c_str(), kCFStringEncodingUTF8);
			CFURLRef url = CFURLCreateWithFileSystemPath(NULL, path, kCFURLPOSIXPathStyle, 0);
			CFURLRef translocated_original = SecTranslocateCreateOriginalPathForURL_func(url, nullptr);

			bool is_translocated = false;
			SecTranslocateIsTranslocatedURL_func(url, &is_translocated, nullptr);

			if (is_translocated)
			{
				// https://stackoverflow.com/questions/28860033/convert-from-cfurlref-or-cfstringref-to-stdstring
				CFIndex bufferSize = CFStringGetLength(CFURLGetString(translocated_original)) +
				                     1; // The +1 is for having space for the string to be NUL terminated
				char buffer[bufferSize];

				// CFStringGetCString is documented to return a false if the buffer is too small
				// (which shouldn't happen in this example) or if the conversion generally fails
				if (CFStringGetCString(CFURLGetString(translocated_original), buffer, bufferSize,
				                       kCFStringEncodingUTF8))
				{
					std::string cppString(buffer);
					cppString.erase(0, std::string("file://").size());

					if (system(("xattr -r -d com.apple.quarantine \"" + cppString + "\"").c_str()) == EXIT_SUCCESS)
					{
						system(("\"" + cppString + "/Contents/MacOS/Dolphin\" &disown").c_str());
						exit(EXIT_SUCCESS);
					}
				}

				wxMessageBox("This app is quarantined! Move it to your Applications folder and reopen it.\nAsk in the "
				             "Discord (#mac-support) for further help.",
				             "Slippi is Quarantined.", wxOK | wxCENTRE | wxICON_WARNING);
				exit(EXIT_SUCCESS);
			}
		}

		dlclose(security_framework);
	}

	// Here, we check to see if the user is running the app from the mounted installer (DMG) volume. If so,
	// we guide them to make sure the app is installed and running correctly. Running from the DMG volume exhibits
	// similar characteristics to running the app as a quarantined application re: read-only filesystem issues.
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	CFURLRef bundleURL = CFBundleCopyBundleURL(mainBundle);
	CFStringRef url;

	if (CFURLCopyResourcePropertyForKey(bundleURL, kCFURLVolumeNameKey, &url, NULL))
	{
		// If you look at this and wonder why we can't just call CFStringGetCStringPtr, the
		// reason is that it can technically return NULL - and actually does, in this case...
		// but on Mojave.
		//
		// Go figure.
		//
		// If we can't determine the volume name, then we'll just silently move on and deal
		// with it as a support request I guess.
		CFIndex maxSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(url), kCFStringEncodingUTF8);
		char volume_name[maxSize + 1];
		if (CFStringGetCString(url, volume_name, maxSize + 1, kCFStringEncodingUTF8))
		{
			// fprintf(stderr, "Volume: %s\n", volume_name);

			if (strcmp(volume_name, "Slippi Dolphin Installer") == 0)
			{
				wxMessageBox(
				    "Slippi needs to be in your Applications folder to run properly, but you're trying to "
				    "run it from the Installer. Make sure you've dragged the app to the Applications folder, and "
				    "then start the app from there.",
				    "Slippi must be in Applications.", wxOK | wxCENTRE | wxICON_WARNING);
				exit(EXIT_SUCCESS);
			}
		}

		CFRelease(url);
	}

	CFRelease(bundleURL);
	CFRelease(mainBundle);
#endif

	// We have to copy the size and position out of SConfig now because CFrame's OnMove
	// handler will corrupt them during window creation (various APIs like SetMenuBar cause
	// event dispatch including WM_MOVE/WM_SIZE)
	wxRect window_geometry(SConfig::GetInstance().iPosX, SConfig::GetInstance().iPosY, SConfig::GetInstance().iWidth,
	                       SConfig::GetInstance().iHeight);

	main_frame = new CFrame(nullptr, wxID_ANY, StrToWxStr(scm_rev_str), window_geometry, m_use_debugger, m_batch_mode,
	                        m_use_logger);
	SetTopWindow(main_frame);

	AfterInit();

	// Init the spectator server
	SlippiSpectateServer *init = SlippiSpectateServer::getInstance();

	return true;
}

void DolphinApp::OnInitCmdLine(wxCmdLineParser &parser)
{
	static const wxCmdLineEntryDesc desc[] = {
	    {wxCMD_LINE_SWITCH, "h", "help", "Show this help message", wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP},
	    {wxCMD_LINE_SWITCH, nullptr, "version", "Show the current app version", wxCMD_LINE_VAL_NONE,
	     wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_SWITCH, "d", "debugger", "Opens the debugger", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_SWITCH, "l", "logger", "Opens the logger", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_OPTION, "e", "exec", "Loads the specified file (ELF, DOL, GCM, ISO, TGC, WBFS, CISO, GCZ, WAD)",
	     wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_SWITCH, "b", "batch", "Exit Dolphin with emulator", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_OPTION, "c", "confirm", "Set Confirm on Stop", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_OPTION, "v", "video_backend", "Specify a video backend", wxCMD_LINE_VAL_STRING,
	     wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_OPTION, nullptr, "output-directory", "Directory to place audio and video dump files",
	     wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_OPTION, "o", "output-filename-base", "Base of filenames for audio and video dump files",
	     wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_OPTION, "a", "audio_emulation", "Low level (LLE) or high level (HLE) audio", wxCMD_LINE_VAL_STRING,
	     wxCMD_LINE_PARAM_OPTIONAL},
#ifdef IS_PLAYBACK
	    {wxCMD_LINE_OPTION, "i", "slippi-input", "Path to Slippi replay config file (default: Slippi/playback.txt)",
	     wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_SWITCH, nullptr, "hide-seekbar", "Hide seekbar during playback", wxCMD_LINE_VAL_NONE,
	     wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_SWITCH, nullptr, "cout", "Enable cout during playback", wxCMD_LINE_VAL_NONE,
	     wxCMD_LINE_PARAM_OPTIONAL},
#endif
	    {wxCMD_LINE_OPTION, "m", "movie", "Play a movie file", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_OPTION, "u", "user", "User folder path", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
	    {wxCMD_LINE_NONE, nullptr, nullptr, nullptr, wxCMD_LINE_VAL_NONE, 0}};

	parser.SetDesc(desc);
}

int DolphinApp::FilterEvent(wxEvent &event)
{
	wxKeyEvent &kev = reinterpret_cast<wxKeyEvent &>(event);

	if (main_frame && main_frame->RendererHasFocus())
	{
		if (event.GetEventType() == wxEVT_CHAR)
		{
			if (kev.GetKeyCode() == WXK_BACK)
			{
				if (OSD::Chat::current_msg.size() > 0)
					OSD::Chat::current_msg.pop_back();
			}
			else
			{
				std::string result = wxString(kev.GetUnicodeKey()).ToStdString();
				std::string filtered;

				for (char c : result)
				{
					if (std::isalnum(c, std::locale::classic()) || std::ispunct(c, std::locale::classic()) || c == ' ')
						filtered += c;
				}

				OSD::Chat::current_msg += filtered;
			}
		}
	}

	return -1;
}

bool DolphinApp::OnCmdLineParsed(wxCmdLineParser &parser)
{
	if (argc == 2 && File::Exists(argv[1].ToUTF8().data()))
	{
		m_load_file = true;
		m_file_to_load = argv[1];
	}
	else if (parser.Parse() != 0)
	{
		return false;
	}

	if (!m_load_file)
		m_load_file = parser.Found("exec", &m_file_to_load);

	m_use_debugger = parser.Found("debugger");
	m_use_logger = parser.Found("logger");
	m_show_version = parser.Found("version");
	m_batch_mode = parser.Found("batch");
	m_confirm_stop = parser.Found("confirm", &m_confirm_setting);
	m_select_video_backend = parser.Found("video_backend", &m_video_backend_name);
	m_select_audio_emulation = parser.Found("audio_emulation", &m_audio_emulation_name);
#ifdef IS_PLAYBACK
	m_select_slippi_input = parser.Found("slippi-input", &m_slippi_input_name);
	m_hide_seekbar = parser.Found("hide-seekbar");
	m_enable_cout = parser.Found("cout");
#endif
	m_select_output_directory = parser.Found("output-directory", &m_output_directory);
	m_select_output_filename_base = parser.Found("output-filename-base", &m_output_filename_base);
	m_play_movie = parser.Found("movie", &m_movie_file);
	parser.Found("user", &m_user_path);

	return true;
}

#ifdef __APPLE__
void DolphinApp::MacOpenFile(const wxString &fileName)
{
	m_file_to_load = fileName;
	m_load_file = true;
	main_frame->BootGame(WxStrToStr(m_file_to_load));
}
#endif

void DolphinApp::AfterInit()
{
	// always raise the frame so that it is in the foreground on launch
	main_frame->Raise();

	if (!m_batch_mode)
		main_frame->UpdateGameList();

	if (SConfig::GetInstance().m_analytics_will_prompt && !SConfig::GetInstance().m_analytics_permission_asked)
	{
		int answer = wxMessageBox(_("If authorized, Dolphin can collect data on its performance, "
		                            "feature usage, and configuration, as well as data on your system's "
		                            "hardware and operating system.\n\n"
		                            "No private data is ever collected. This data helps us understand "
		                            "how people and emulated games use Dolphin and prioritize our "
		                            "efforts. It also helps us identify rare configurations that are "
		                            "causing bugs, performance and stability issues.\n"
		                            "This authorization can be revoked at any time through Dolphin's "
		                            "settings.\n\n"
		                            "Do you authorize Dolphin to report this information to Dolphin's "
		                            "developers?"),
		                          _("Usage statistics reporting"), wxYES_NO, main_frame);

		SConfig::GetInstance().m_analytics_permission_asked = true;
		SConfig::GetInstance().m_analytics_enabled = (answer == wxYES);
		SConfig::GetInstance().SaveSettings();

		DolphinAnalytics::Instance()->ReloadConfig();
	}

#ifdef _WIN32
	// delete the VC notice file since the user has successfully started Dolphin.
	std::string vc_notice_path = File::GetExeDirectory() + DIR_SEP + "FIX-VCRUNTIME140-ERROR.txt";
	File::Delete(vc_notice_path);
#endif

	if (m_confirm_stop)
	{
		if (m_confirm_setting.Upper() == "TRUE")
			SConfig::GetInstance().bConfirmStop = true;
		else if (m_confirm_setting.Upper() == "FALSE")
			SConfig::GetInstance().bConfirmStop = false;
	}

	if (m_play_movie && !m_movie_file.empty())
	{
		if (Movie::PlayInput(WxStrToStr(m_movie_file)))
		{
			if (m_load_file && !m_file_to_load.empty())
			{
				main_frame->BootGame(WxStrToStr(m_file_to_load));
				main_frame->RaiseRenderWindow();
			}
			else if (SConfig::GetInstance().bBootDefaultISO && !SConfig::GetInstance().m_strDefaultISO.empty())
			{
				main_frame->BootGame(WxStrToStr(SConfig::GetInstance().m_strDefaultISO));
				main_frame->RaiseRenderWindow();
			}
			else
			{
				main_frame->BootGame("");
			}
		}
	}
	// First check if we have an exec command line.
	else if (m_load_file && !m_file_to_load.empty())
	{
		main_frame->BootGame(WxStrToStr(m_file_to_load));
		main_frame->RaiseRenderWindow();
	}
	else if (SConfig::GetInstance().bBootDefaultISO && !SConfig::GetInstance().m_strDefaultISO.empty())
	{
		main_frame->BootGame(WxStrToStr(SConfig::GetInstance().m_strDefaultISO));
		main_frame->RaiseRenderWindow();
	}
	// If we have selected Automatic Start, start the default ISO,
	// or if no default ISO exists, start the last loaded ISO
	else if (main_frame->g_pCodeWindow)
	{
		if (main_frame->g_pCodeWindow->AutomaticStart())
		{
			main_frame->BootGame("");
		}
	}
}

void DolphinApp::OnActivate(wxActivateEvent &ev)
{
	m_is_active = ev.GetActive();
}

void DolphinApp::InitLanguageSupport()
{
	std::string language_code;
	{
		IniFile ini;
		ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
		ini.GetOrCreateSection("Interface")->Get("LanguageCode", &language_code, "");
	}
	int language = wxLANGUAGE_UNKNOWN;
	if (language_code.empty())
	{
		language = wxLANGUAGE_DEFAULT;
	}
	else
	{
		const wxLanguageInfo *language_info = wxLocale::FindLanguageInfo(StrToWxStr(language_code));
		if (language_info)
			language = language_info->Language;
	}

	// Load language if possible, fall back to system default otherwise
	if (wxLocale::IsAvailable(language))
	{
		m_locale.reset(new wxLocale(language));

		// Specify where dolphins *.gmo files are located on each operating system
#ifdef __WXMSW__
		m_locale->AddCatalogLookupPathPrefix(StrToWxStr(File::GetExeDirectory() + DIR_SEP "Languages"));
#elif defined(__WXGTK__)
		m_locale->AddCatalogLookupPathPrefix(StrToWxStr(DATA_DIR "../locale"));
#elif defined(__WXOSX__)
		m_locale->AddCatalogLookupPathPrefix(StrToWxStr(File::GetBundleDirectory() + "Contents/Resources"));
#endif

		m_locale->AddCatalog("dolphin-emu");

		if (!m_locale->IsOk())
		{
			m_locale.reset(new wxLocale(wxLANGUAGE_DEFAULT));
		}
	}
	else
	{
		m_locale.reset(new wxLocale(wxLANGUAGE_DEFAULT));
	}
}

void DolphinApp::OnEndSession(wxCloseEvent &event)
{
	// Close if we've received wxEVT_END_SESSION (ignore wxEVT_QUERY_END_SESSION)
	if (!event.CanVeto())
	{
		main_frame->Close(true);
	}
}

int DolphinApp::OnExit()
{
	Core::Shutdown();
	UICommon::Shutdown();

	return wxApp::OnExit();
}

void DolphinApp::OnFatalException()
{
	WiimoteReal::Shutdown();
}

void DolphinApp::OnIdle(wxIdleEvent &ev)
{
	ev.Skip();
	Core::HostDispatchJobs();
}

// ------------
// Talk to GUI

bool wxMsgAlert(const char *caption, const char *text, bool yes_no, int /*Style*/)
{
#ifdef __WXGTK__
	if (wxIsMainThread())
	{
#endif
		NetPlayDialog *&npd = NetPlayDialog::GetInstance();
		if (npd != nullptr && npd->IsShown())
		{
			npd->AppendChat("/!\\ " + std::string{text}, false);
			return true;
		}
		return wxYES ==
		       wxMessageBox(StrToWxStr(text), StrToWxStr(caption), (yes_no) ? wxYES_NO : wxOK, wxWindow::FindFocus());
#ifdef __WXGTK__
	}
	else
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_PANIC);
		event.SetString(StrToWxStr(caption) + ":" + StrToWxStr(text));
		event.SetInt(yes_no);
		main_frame->GetEventHandler()->AddPendingEvent(event);
		main_frame->panic_event.Wait();
		return main_frame->bPanicResult;
	}
#endif
}

std::string wxStringTranslator(const char *text)
{
	return WxStrToStr(wxGetTranslation(wxString::FromUTF8(text)));
}

// Accessor for the main window class
CFrame *DolphinApp::GetCFrame()
{
	return main_frame;
}

void Host_Message(int Id)
{
	if (Id == WM_USER_JOB_DISPATCH)
	{
		// Trigger a wxEVT_IDLE
		wxWakeUpIdle();
		return;
	}
	wxCommandEvent event(wxEVT_HOST_COMMAND, Id);
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

void *Host_GetRenderHandle()
{
	return main_frame->GetRenderHandle();
}

// OK, this thread boundary is DANGEROUS on Linux
// wxPostEvent / wxAddPendingEvent is the solution.
void Host_NotifyMapLoaded()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_NOTIFY_MAP_LOADED);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}

void Host_UpdateDisasmDialog()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATE_DISASM_DIALOG);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}

void Host_UpdateMainFrame()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATE_GUI);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}

void Host_UpdateTitle(const std::string &title)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATE_TITLE);
	event.SetString(StrToWxStr(title));
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

void Host_RequestRenderWindowSize(int width, int height)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_WINDOW_SIZE_REQUEST);
	event.SetClientData(new std::pair<int, int>(width, height));
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

void Host_SetStartupDebuggingParameters()
{
	SConfig &StartUp = SConfig::GetInstance();
	if (main_frame->g_pCodeWindow)
	{
		StartUp.bBootToPause = main_frame->g_pCodeWindow->BootToPause();
		StartUp.bAutomaticStart = main_frame->g_pCodeWindow->AutomaticStart();
		StartUp.bJITNoBlockCache = main_frame->g_pCodeWindow->JITNoBlockCache();
		StartUp.bJITNoBlockLinking = main_frame->g_pCodeWindow->JITNoBlockLinking();
	}
	else
	{
		StartUp.bBootToPause = false;
	}
	StartUp.bEnableDebugging = main_frame->g_pCodeWindow ? true : false; // RUNNING_DEBUG
}

void Host_SetWiiMoteConnectionState(int _State)
{
	static int currentState = -1;
	if (_State == currentState)
		return;
	currentState = _State;

	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATE_STATUS_BAR);

	switch (_State)
	{
	case 0:
		event.SetString(_("Not connected"));
		break;
	case 1:
		event.SetString(_("Connecting..."));
		break;
	case 2:
		event.SetString(_("Wii Remote Connected"));
		break;
	}
	// Update field 1 or 2
	event.SetInt(1);

	NOTICE_LOG(WIIMOTE, "%s", static_cast<const char *>(event.GetString().c_str()));

	main_frame->GetEventHandler()->AddPendingEvent(event);
}

bool Host_UIHasFocus()
{
	return wxGetApp().IsActiveThreadsafe();
}

bool Host_RendererHasFocus()
{
	return main_frame->RendererHasFocus();
}

bool Host_RendererIsFullscreen()
{
	return main_frame->RendererIsFullscreen();
}

void Host_ConnectWiimote(int wm_idx, bool connect)
{
	std::lock_guard<std::mutex> lk(s_init_mutex);
	if (connect)
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_FORCE_CONNECT_WIIMOTE1 + wm_idx);
		main_frame->GetEventHandler()->AddPendingEvent(event);
	}
	else
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_FORCE_DISCONNECT_WIIMOTE1 + wm_idx);
		main_frame->GetEventHandler()->AddPendingEvent(event);
	}
}

void Host_ShowVideoConfig(void *parent, const std::string &backend_name)
{
	if (backend_name == "Software Renderer")
	{
		SoftwareVideoConfigDialog diag((wxWindow *)parent, backend_name);
		diag.ShowModal();
	}
	else
	{
		VideoConfigDiag diag((wxWindow *)parent, backend_name);
		diag.ShowModal();
	}
}

void Host_YieldToUI()
{
	wxGetApp().GetMainLoop()->YieldFor(wxEVT_CATEGORY_UI);
}
