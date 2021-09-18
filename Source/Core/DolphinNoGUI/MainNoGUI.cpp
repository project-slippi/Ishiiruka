// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <getopt.h>
#include <signal.h>
#include <string>
#include <thread>
#include <unistd.h>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"

#include "Core/Analytics.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_stm.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_emu.h"
#include "Core/IPC_HLE/WII_IPC_HLE_WiiMote.h"
#include "Core/State.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoBackendBase.h"

static bool rendererHasFocus = true;
static bool rendererIsFullscreen = false;
static Common::Flag s_running{ true };
static Common::Flag s_shutdown_requested{ false };
static Common::Flag s_tried_graceful_shutdown{ false };

static void signal_handler(int)
{
	const char message[] = "A signal was received. A second signal will force Dolphin to stop.\n";
	if (write(STDERR_FILENO, message, sizeof(message)) < 0)
	{
	}
	s_shutdown_requested.Set();
}

namespace ProcessorInterface
{
void PowerButton_Tap();
}

class Platform
{
public:
	virtual void Init() {}
	virtual void SetTitle(const std::string& title) {}
	virtual void MainLoop()
	{
		while (s_running.IsSet())
		{
			Core::HostDispatchJobs();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
	virtual void Shutdown() {}
	virtual ~Platform() {}
};

static Platform* platform;

void Host_NotifyMapLoaded()
{
}
void Host_RefreshDSPDebuggerWindow()
{
}

static Common::Event updateMainFrameEvent;
void Host_Message(int Id)
{
	if (Id == WM_USER_STOP)
	{
		s_running.Clear();
		updateMainFrameEvent.Set();
	}
}

static void* s_window_handle = nullptr;
void* Host_GetRenderHandle()
{
	return s_window_handle;
}

void Host_UpdateTitle(const std::string& title)
{
	platform->SetTitle(title);
}

void Host_UpdateDisasmDialog()
{
}

void Host_UpdateMainFrame()
{
	updateMainFrameEvent.Set();
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

void Host_SetStartupDebuggingParameters()
{
	SConfig& StartUp = SConfig::GetInstance();
	StartUp.bEnableDebugging = false;
	StartUp.bBootToPause = false;
}

bool Host_UIHasFocus()
{
	return false;
}

bool Host_RendererHasFocus()
{
	return rendererHasFocus;
}

bool Host_RendererIsFullscreen()
{
	return rendererIsFullscreen;
}

void Host_ConnectWiimote(int wm_idx, bool connect)
{
	if (Core::IsRunning() && SConfig::GetInstance().bWii &&
		!SConfig::GetInstance().m_bt_passthrough_enabled)
	{
		Core::QueueHostJob([=] {
			bool was_unpaused = Core::PauseAndLock(true);
			GetUsbPointer()->AccessWiiMote(wm_idx | 0x100)->Activate(connect);
			Host_UpdateMainFrame();
			Core::PauseAndLock(false, was_unpaused);
		});
	}
}

void Host_SetWiiMoteConnectionState(int _State)
{
}

void Host_ShowVideoConfig(void*, const std::string&)
{
}

void Host_YieldToUI()
{
}

#if HAVE_X11
#include <X11/keysym.h>
#include "DolphinWX/X11Utils.h"

class PlatformX11 : public Platform
{
	Display* dpy;
	Window win;
	Cursor blankCursor = None;
#if defined(HAVE_XRANDR) && HAVE_XRANDR
	X11Utils::XRRConfiguration* XRRConfig;
#endif

	void Init() override
	{
		XInitThreads();
		dpy = XOpenDisplay(nullptr);
		if (!dpy)
		{
			PanicAlert("No X11 display found");
			exit(1);
		}

		win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), SConfig::GetInstance().iRenderWindowXPos,
			SConfig::GetInstance().iRenderWindowYPos,
			SConfig::GetInstance().iRenderWindowWidth,
			SConfig::GetInstance().iRenderWindowHeight, 0, 0, BlackPixel(dpy, 0));
		XSelectInput(dpy, win, StructureNotifyMask | KeyPressMask | FocusChangeMask);
		Atom wmProtocols[1];
		wmProtocols[0] = XInternAtom(dpy, "WM_DELETE_WINDOW", True);
		XSetWMProtocols(dpy, win, wmProtocols, 1);
		XMapRaised(dpy, win);
		XFlush(dpy);
		s_window_handle = (void*)win;

		if (SConfig::GetInstance().bDisableScreenSaver)
			X11Utils::InhibitScreensaver(dpy, win, true);

#if defined(HAVE_XRANDR) && HAVE_XRANDR
		XRRConfig = new X11Utils::XRRConfiguration(dpy, win);
#endif

		if (SConfig::GetInstance().bHideCursor)
		{
			// make a blank cursor
			Pixmap Blank;
			XColor DummyColor;
			char ZeroData[1] = { 0 };
			Blank = XCreateBitmapFromData(dpy, win, ZeroData, 1, 1);
			blankCursor = XCreatePixmapCursor(dpy, Blank, Blank, &DummyColor, &DummyColor, 0, 0);
			XFreePixmap(dpy, Blank);
			XDefineCursor(dpy, win, blankCursor);
		}
	}

	void SetTitle(const std::string& string) override { XStoreName(dpy, win, string.c_str()); }
	void MainLoop() override
	{
		bool fullscreen = SConfig::GetInstance().bFullscreen;
		int last_window_width = SConfig::GetInstance().iRenderWindowWidth;
		int last_window_height = SConfig::GetInstance().iRenderWindowHeight;
		if (fullscreen)
		{
			rendererIsFullscreen = X11Utils::ToggleFullscreen(dpy, win);
#if defined(HAVE_XRANDR) && HAVE_XRANDR
			XRRConfig->ToggleDisplayMode(True);
#endif
		}

		// The actual loop
		while (s_running.IsSet())
		{
			if (s_shutdown_requested.TestAndClear())
			{
				const auto& stm = WII_IPC_HLE_Interface::GetDeviceByName("/dev/stm/eventhook");
				if (!s_tried_graceful_shutdown.IsSet() && stm &&
					std::static_pointer_cast<CWII_IPC_HLE_Device_stm_eventhook>(stm)->HasHookInstalled())
				{
					ProcessorInterface::PowerButton_Tap();
					s_tried_graceful_shutdown.Set();
				}
				else
				{
					s_running.Clear();
				}
			}

			XEvent event;
			KeySym key;
			for (int num_events = XPending(dpy); num_events > 0; num_events--)
			{
				XNextEvent(dpy, &event);
				switch (event.type)
				{
				case KeyPress:
					key = XLookupKeysym((XKeyEvent*)&event, 0);
					if (key == XK_Escape)
					{
						if (Core::GetState() == Core::CORE_RUN)
						{
							if (SConfig::GetInstance().bHideCursor)
								XUndefineCursor(dpy, win);
							Core::SetState(Core::CORE_PAUSE);
						}
						else
						{
							if (SConfig::GetInstance().bHideCursor)
								XDefineCursor(dpy, win, blankCursor);
							Core::SetState(Core::CORE_RUN);
						}
					}
					else if ((key == XK_Return) && (event.xkey.state & Mod1Mask))
					{
						fullscreen = !fullscreen;
						X11Utils::ToggleFullscreen(dpy, win);
#if defined(HAVE_XRANDR) && HAVE_XRANDR
						XRRConfig->ToggleDisplayMode(fullscreen);
#endif
					}
					else if (key >= XK_F1 && key <= XK_F8)
					{
						int slot_number = key - XK_F1 + 1;
						if (event.xkey.state & ShiftMask)
							State::Save(slot_number);
						else
							State::Load(slot_number);
					}
					else if (key == XK_F9)
						Core::SaveScreenShot();
					else if (key == XK_F11)
						State::LoadLastSaved();
					else if (key == XK_F12)
					{
						if (event.xkey.state & ShiftMask)
							State::UndoLoadState();
						else
							State::UndoSaveState();
					}
					break;
				case FocusIn:
					rendererHasFocus = true;
					if (SConfig::GetInstance().bHideCursor && Core::GetState() != Core::CORE_PAUSE)
						XDefineCursor(dpy, win, blankCursor);
					break;
				case FocusOut:
					rendererHasFocus = false;
					if (SConfig::GetInstance().bHideCursor)
						XUndefineCursor(dpy, win);
					break;
				case ClientMessage:
					if ((unsigned long)event.xclient.data.l[0] == XInternAtom(dpy, "WM_DELETE_WINDOW", False))
						s_shutdown_requested.Set();
					break;
				case ConfigureNotify:
				{
					if (last_window_width != event.xconfigure.width ||
						last_window_height != event.xconfigure.height)
					{
						last_window_width = event.xconfigure.width;
						last_window_height = event.xconfigure.height;

						// We call Renderer::ChangeSurface here to indicate the size has changed,
						// but pass the same window handle. This is needed for the Vulkan backend,
						// otherwise it cannot tell that the window has been resized on some drivers.
						if (g_renderer)
							g_renderer->ChangeSurface(s_window_handle);
					}
				}
				break;
				}
			}
			if (!fullscreen)
			{
				Window winDummy;
				unsigned int borderDummy, depthDummy;
				XGetGeometry(dpy, win, &winDummy, &SConfig::GetInstance().iRenderWindowXPos,
					&SConfig::GetInstance().iRenderWindowYPos,
					(unsigned int*)&SConfig::GetInstance().iRenderWindowWidth,
					(unsigned int*)&SConfig::GetInstance().iRenderWindowHeight, &borderDummy,
					&depthDummy);
				rendererIsFullscreen = false;
			}
			Core::HostDispatchJobs();
			usleep(100000);
		}
	}

	void Shutdown() override
	{
#if defined(HAVE_XRANDR) && HAVE_XRANDR
		delete XRRConfig;
#endif

		if (SConfig::GetInstance().bHideCursor)
			XFreeCursor(dpy, blankCursor);

		XCloseDisplay(dpy);
	}
};
#endif

static Platform* GetPlatform()
{
#if defined(USE_EGL) && defined(USE_HEADLESS)
	return new Platform();
#elif HAVE_X11
	return new PlatformX11();
#endif
	return nullptr;
}

int main(int argc, char* argv[])
{
	int ch, help = 0;
	struct option longopts[] = { { "exec", no_argument, nullptr, 'e' },
	{ "help", no_argument, nullptr, 'h' },
	{ "version", no_argument, nullptr, 'v' },
	{ nullptr, 0, nullptr, 0 } };

	while ((ch = getopt_long(argc, argv, "eh?v", longopts, 0)) != -1)
	{
		switch (ch)
		{
		case 'e':
			break;
		case 'h':
		case '?':
			help = 1;
			break;
		case 'v':
			fprintf(stderr, "%s\n", scm_rev_str.c_str());
			return 1;
		}
	}

	if (help == 1 || argc == optind)
	{
		fprintf(stderr, "%s\n\n", scm_rev_str.c_str());
		fprintf(stderr, "A multi-platform GameCube/Wii emulator\n\n");
		fprintf(stderr, "Usage: %s [-e <file>] [-h] [-v]\n", argv[0]);
		fprintf(stderr, "  -e, --exec     Load the specified file\n");
		fprintf(stderr, "  -h, --help     Show this help message\n");
		fprintf(stderr, "  -v, --version  Print version and exit\n");
		return 1;
	}

	platform = GetPlatform();
	if (!platform)
	{
		fprintf(stderr, "No platform found\n");
		return 1;
	}

	UICommon::SetUserDirectory("");  // Auto-detect user folder
	UICommon::Init();

	Core::SetOnStoppedCallback([]() { s_running.Clear(); });
	platform->Init();

	// Shut down cleanly on SIGINT and SIGTERM
	struct sigaction sa;
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);

	DolphinAnalytics::Instance()->ReportDolphinStart("nogui");

	if (!BootManager::BootCore(argv[optind]))
	{
		fprintf(stderr, "Could not boot %s\n", argv[optind]);
		return 1;
	}

	while (!Core::IsRunning() && s_running.IsSet())
	{
		Core::HostDispatchJobs();
		updateMainFrameEvent.Wait();
	}

	if (s_running.IsSet())
		platform->MainLoop();
	Core::Stop();

	Core::Shutdown();
	platform->Shutdown();
	UICommon::Shutdown();

	delete platform;

	return 0;
}
