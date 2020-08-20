// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <atomic>
#include <cctype>
#include <cstring>
#include <mutex>
#include <queue>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

#include "AudioCommon/AudioCommon.h"

#include "Common/CPUDetect.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Common/Logging/LogManager.h"
#include "Common/MathUtil.h"
#include "Common/MemoryUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/Analytics.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/DSPEmulator.h"
#include "Core/Host.h"
#include "Core/MemTools.h"
#ifdef USE_MEMORYWATCHER
#include "Core/MemoryWatcher.h"
#endif
#include "Core/Boot/Boot.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/HW/EXI.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/HW.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/Wiimote.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_emu.h"
#include "Core/IPC_HLE/WII_IPC_HLE_WiiMote.h"
#include "Core/IPC_HLE/WII_Socket.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"

#ifdef USE_GDBSTUB
#include "Core/PowerPC/GDBStub.h"
#endif

#include "DiscIO/FileMonitor.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCAdapter.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoBackendBase.h"

// Android and OSX haven't implemented the keyword yet.
#if defined __ANDROID__ || defined __APPLE__
#include <pthread.h>
#else  // Everything besides OSX and Android
#define ThreadLocalStorage thread_local
#endif

namespace Core
{
// TODO: ugly, remove
bool g_aspect_wide;

bool g_want_determinism;

// Declarations and definitions
static Common::Timer s_timer;
static std::atomic<u32> s_drawn_frame;
static std::atomic<u32> s_drawn_video;

// Function forwarding
void Callback_WiimoteInterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size);

// Function declarations
void EmuThread();

static bool s_is_stopping = false;
static bool s_hardware_initialized = false;
static bool s_is_started = false;
static Common::Flag s_is_booting;
static void* s_window_handle = nullptr;
static std::string s_state_filename;
static std::thread s_emu_thread;
static StoppedCallbackFunc s_on_stopped_callback = nullptr;

static std::thread s_cpu_thread;
static bool s_request_refresh_info = false;
static int s_pause_and_lock_depth = 0;
static bool s_is_throttler_temp_disabled = false;

struct HostJob
{
	std::function<void()> job;
	bool run_after_stop;
};
static std::mutex s_host_jobs_lock;
static std::queue<HostJob> s_host_jobs_queue;

#ifdef ThreadLocalStorage
static ThreadLocalStorage bool tls_is_cpu_thread = false;
#else
static pthread_key_t s_tls_is_cpu_key;
static pthread_once_t s_cpu_key_is_init = PTHREAD_ONCE_INIT;
static void InitIsCPUKey()
{
	pthread_key_create(&s_tls_is_cpu_key, nullptr);
}
#endif

bool GetIsThrottlerTempDisabled()
{
	return s_is_throttler_temp_disabled;
}

void SetIsThrottlerTempDisabled(bool disable)
{
	s_is_throttler_temp_disabled = disable;
}

std::string GetStateFileName()
{
	return s_state_filename;
}
void SetStateFileName(const std::string& val)
{
	s_state_filename = val;
}

void FrameUpdateOnCPUThread()
{
	if (NetPlay::IsNetPlayRunning())
		NetPlayClient::SendTimeBase();
}

// Display messages and return values

// Formatted stop message
std::string StopMessage(bool main_thread, const std::string& message)
{
	return StringFromFormat("Stop [%s %i]\t%s\t%s", main_thread ? "Main Thread" : "Video Thread",
		Common::CurrentThreadId(), Common::MemUsage().c_str(), message.c_str());
}

void DisplayMessage(const std::string& message, int time_in_ms)
{
	if (!IsRunning())
		return;

	// Actually displaying non-ASCII could cause things to go pear-shaped
	for (const char& c : message)
	{
		if (!std::isprint(c))
			return;
	}

	OSD::AddMessage(message, time_in_ms);
	Host_UpdateTitle(message);
}

bool IsRunning()
{
	return (GetState() != CORE_UNINITIALIZED || s_hardware_initialized) && !s_is_stopping;
}

bool IsRunningAndStarted()
{
	return s_is_started && !s_is_stopping;
}

bool IsRunningInCurrentThread()
{
	return IsRunning() && IsCPUThread();
}

bool IsCPUThread()
{
#ifdef ThreadLocalStorage
	return tls_is_cpu_thread;
#else
	// Use pthread implementation for Android and Mac
	// Make sure that s_tls_is_cpu_key is initialized
	pthread_once(&s_cpu_key_is_init, InitIsCPUKey);
	return pthread_getspecific(s_tls_is_cpu_key);
#endif
}

bool IsGPUThread()
{
	const SConfig& _CoreParameter = SConfig::GetInstance();
	if (_CoreParameter.bCPUThread)
	{
		return (s_emu_thread.joinable() && (s_emu_thread.get_id() == std::this_thread::get_id()));
	}
	else
	{
		return IsCPUThread();
	}
}

// This is called from the GUI thread. See the booting call schedule in
// BootManager.cpp
bool Init()
{
	const SConfig& _CoreParameter = SConfig::GetInstance();

	if (s_emu_thread.joinable())
	{
		if (IsRunning())
		{
			PanicAlertT("Emu Thread already running");
			return false;
		}

		// The Emu Thread was stopped, synchronize with it.
		s_emu_thread.join();
	}

	// Drain any left over jobs
	HostDispatchJobs();

	Core::UpdateWantDeterminism(/*initial*/ true);

	INFO_LOG(OSREPORT, "Starting core = %s mode", _CoreParameter.bWii ? "Wii" : "GameCube");
	INFO_LOG(OSREPORT, "CPU Thread separate = %s", _CoreParameter.bCPUThread ? "Yes" : "No");

	Host_UpdateMainFrame();  // Disable any menus or buttons at boot

	g_aspect_wide = _CoreParameter.bWii;
	if (g_aspect_wide)
	{
		IniFile gameIni = _CoreParameter.LoadGameIni();
		gameIni.GetOrCreateSection("Wii")->Get("Widescreen", &g_aspect_wide,
			!!SConfig::GetInstance().m_wii_aspect_ratio);
	}

	s_window_handle = Host_GetRenderHandle();

	// Start the emu thread
	s_emu_thread = std::thread(EmuThread);

	return true;
}

// Called from GUI thread
void Stop()  // - Hammertime!
{
	if (GetState() == CORE_STOPPING)
		return;

	const SConfig& _CoreParameter = SConfig::GetInstance();

	s_is_stopping = true;

	// Dump left over jobs
	HostDispatchJobs();

	Fifo::EmulatorState(false);

	INFO_LOG(CONSOLE, "Stop [Main Thread]\t\t---- Shutting down ----");

	// Stop the CPU
	INFO_LOG(CONSOLE, "%s", StopMessage(true, "Stop CPU").c_str());
	CPU::Stop();

	if (_CoreParameter.bCPUThread)
	{
		// Video_EnterLoop() should now exit so that EmuThread()
		// will continue concurrently with the rest of the commands
		// in this function. We no longer rely on Postmessage.
		INFO_LOG(CONSOLE, "%s", StopMessage(true, "Wait for Video Loop to exit ...").c_str());

		g_video_backend->Video_ExitLoop();
	}
#if defined(__LIBUSB__)
	GCAdapter::ResetRumble();
#endif

#ifdef USE_MEMORYWATCHER
	MemoryWatcher::Shutdown();
#endif
}

void DeclareAsCPUThread()
{
#ifdef ThreadLocalStorage
	tls_is_cpu_thread = true;
#else
	// Use pthread implementation for Android and Mac
	// Make sure that s_tls_is_cpu_key is initialized
	pthread_once(&s_cpu_key_is_init, InitIsCPUKey);
	pthread_setspecific(s_tls_is_cpu_key, (void*)true);
#endif
}

void UndeclareAsCPUThread()
{
#ifdef ThreadLocalStorage
	tls_is_cpu_thread = false;
#else
	// Use pthread implementation for Android and Mac
	// Make sure that s_tls_is_cpu_key is initialized
	pthread_once(&s_cpu_key_is_init, InitIsCPUKey);
	pthread_setspecific(s_tls_is_cpu_key, (void*)false);
#endif
}

// For the CPU Thread only.
static void CPUSetInitialExecutionState()
{
	QueueHostJob([] {
		SetState(SConfig::GetInstance().bBootToPause ? CORE_PAUSE : CORE_RUN);
		Host_UpdateMainFrame();
	});
}

// Create the CPU thread, which is a CPU + Video thread in Single Core mode.
static void CpuThread()
{
	DeclareAsCPUThread();

	const SConfig& _CoreParameter = SConfig::GetInstance();

	VideoBackendBase* video_backend = g_video_backend;
	if (_CoreParameter.bCPUThread)
	{
		Common::SetCurrentThreadName("CPU thread");
	}
	else
	{
		Common::SetCurrentThreadName("CPU-GPU thread");
		video_backend->Video_Prepare();
	}

	// This needs to be delayed until after the video backend is ready.
	DolphinAnalytics::Instance()->ReportGameStart();

	if (_CoreParameter.bFastmem)
		EMM::InstallExceptionHandler();  // Let's run under memory watch

	if (!s_state_filename.empty())
	{
		// Needs to PauseAndLock the Core
		// NOTE: EmuThread should have left us in CPU_STEPPING so nothing will happen
		//   until after the job is serviced.
		QueueHostJob([] {
			// Recheck in case Movie cleared it since.
			if (!s_state_filename.empty())
				State::LoadAs(s_state_filename);
		});
	}

	s_is_started = true;
	CPUSetInitialExecutionState();

#ifdef USE_GDBSTUB
#ifndef _WIN32
	if (!_CoreParameter.gdb_socket.empty())
	{
		gdb_init_local(_CoreParameter.gdb_socket.data());
		gdb_break();
	}
	else
#endif
		if (_CoreParameter.iGDBPort > 0)
		{
			gdb_init(_CoreParameter.iGDBPort);
			// break at next instruction (the first instruction)
			gdb_break();
		}
#endif

#ifdef USE_MEMORYWATCHER
	MemoryWatcher::Init();
#endif

	// Enter CPU run loop. When we leave it - we are done.
	CPU::Run();

	s_is_started = false;

	if (!_CoreParameter.bCPUThread)
		video_backend->Video_Cleanup();

	if (_CoreParameter.bFastmem)
		EMM::UninstallExceptionHandler();

	return;
}

static void FifoPlayerThread()
{
	DeclareAsCPUThread();
	const SConfig& _CoreParameter = SConfig::GetInstance();
	VideoBackendBase* video_backend = g_video_backend;
	if (_CoreParameter.bCPUThread)
	{
		Common::SetCurrentThreadName("FIFO player thread");
	}
	else
	{
		video_backend->Video_Prepare();
		Common::SetCurrentThreadName("FIFO-GPU thread");
	}

	// Enter CPU run loop. When we leave it - we are done.
	if (FifoPlayer::GetInstance().Open(_CoreParameter.m_strFilename))
	{
		if (auto cpu_core = FifoPlayer::GetInstance().GetCPUCore())
		{
			PowerPC::InjectExternalCPUCore(cpu_core.get());
			s_is_started = true;

			CPUSetInitialExecutionState();
			CPU::Run();

			s_is_started = false;
			PowerPC::InjectExternalCPUCore(nullptr);
		}
		FifoPlayer::GetInstance().Close();
	}

	// If we did not enter the CPU Run Loop above then run a fake one instead.
	// We need to be IsRunningAndStarted() for DolphinWX to stop us.
	if (CPU::GetState() != CPU::CPU_POWERDOWN)
	{
		s_is_started = true;
		Host_Message(WM_USER_STOP);
		while (CPU::GetState() != CPU::CPU_POWERDOWN)
		{
			if (!_CoreParameter.bCPUThread)
				video_backend->PeekMessages();
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
		s_is_started = false;
	}

	if (!_CoreParameter.bCPUThread)
		video_backend->Video_Cleanup();

	return;
}

// Initialize and create emulation thread
// Call browser: Init():s_emu_thread().
// See the BootManager.cpp file description for a complete call schedule.
void EmuThread()
{
	const SConfig& core_parameter = SConfig::GetInstance();
	s_is_booting.Set();

	Common::SetCurrentThreadName("Emuthread - Starting");
	VideoBackendBase* video_backend = g_video_backend;
	if (SConfig::GetInstance().m_OCEnable)
		DisplayMessage("WARNING: running at non-native CPU clock! Game may not be stable.", 8000);
	DisplayMessage(cpu_info.brand_string, 8000);
	DisplayMessage(cpu_info.Summarize(), 8000);
	DisplayMessage(core_parameter.m_strFilename, 3000);

	// For a time this acts as the CPU thread...
	DeclareAsCPUThread();

	Movie::Init();

	HW::Init();

	if (!video_backend->Initialize(s_window_handle))
	{
		s_is_booting.Clear();
		PanicAlert("Failed to initialize video backend!");
		Host_Message(WM_USER_STOP);
		return;
	}

	OSD::AddMessage("Dolphin " + video_backend->GetName() + " Video Backend.", 5000);

	if (cpu_info.HTT)
		SConfig::GetInstance().bDSPThread = cpu_info.num_cores > 4;
	else
		SConfig::GetInstance().bDSPThread = cpu_info.num_cores > 2;

	if (!DSP::GetDSPEmulator()->Initialize(core_parameter.bWii, core_parameter.bDSPThread))
	{
		s_is_booting.Clear();
		HW::Shutdown();
		video_backend->Shutdown();
		PanicAlert("Failed to initialize DSP emulation!");
		Host_Message(WM_USER_STOP);
		return;
	}

	bool init_controllers = false;
	if (!g_controller_interface.IsInit())
	{
		g_controller_interface.Initialize(s_window_handle);
		Pad::Initialize();
		Keyboard::Initialize();
		init_controllers = true;
	}
	else
	{
		// Update references in case controllers were refreshed
		Pad::LoadConfig();
		Keyboard::LoadConfig();
	}

	// Load and Init Wiimotes - only if we are booting in Wii mode
	if (core_parameter.bWii && !SConfig::GetInstance().m_bt_passthrough_enabled)
	{
		if (init_controllers)
			Wiimote::Initialize(!s_state_filename.empty() ?
				Wiimote::InitializeMode::DO_WAIT_FOR_WIIMOTES :
				Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);
		else
			Wiimote::LoadConfig();

		// Activate Wiimotes which don't have source set to "None"
		for (unsigned int i = 0; i != MAX_BBMOTES; ++i)
			if (g_wiimote_sources[i])
				GetUsbPointer()->AccessWiiMote(i | 0x100)->Activate(true);
	}

	AudioCommon::InitSoundStream(s_window_handle);

	// The hardware is initialized.
	s_hardware_initialized = true;
	s_is_booting.Clear();

	// Set execution state to known values (CPU/FIFO/Audio Paused)
	CPU::Break();

	// Load GCM/DOL/ELF whatever ... we boot with the interpreter core
	PowerPC::SetMode(PowerPC::MODE_INTERPRETER);

	CBoot::BootUp();

	// This adds the SyncGPU handler to CoreTiming, so now CoreTiming::Advance might block.
	Fifo::Prepare();

	// Thread is no longer acting as CPU Thread
	UndeclareAsCPUThread();

	// Setup our core, but can't use dynarec if we are compare server
	if (core_parameter.iCPUCore != PowerPC::CORE_INTERPRETER &&
		(!core_parameter.bRunCompareServer || core_parameter.bRunCompareClient))
	{
		PowerPC::SetMode(PowerPC::MODE_JIT);
	}
	else
	{
		PowerPC::SetMode(PowerPC::MODE_INTERPRETER);
	}

	// Update the window again because all stuff is initialized
	Host_UpdateDisasmDialog();
	Host_UpdateMainFrame();

	// Determine the CPU thread function
	void(*cpuThreadFunc)(void);
	if (core_parameter.m_BootType == SConfig::BOOT_DFF)
		cpuThreadFunc = FifoPlayerThread;
	else
		cpuThreadFunc = CpuThread;

	// ENTER THE VIDEO THREAD LOOP
	if (core_parameter.bCPUThread)
	{
		// This thread, after creating the EmuWindow, spawns a CPU
		// thread, and then takes over and becomes the video thread
		Common::SetCurrentThreadName("Video thread");

		video_backend->Video_Prepare();

		// Spawn the CPU thread
		s_cpu_thread = std::thread(cpuThreadFunc);

		// become the GPU thread
		Fifo::RunGpuLoop();

		// We have now exited the Video Loop
		INFO_LOG(CONSOLE, "%s", StopMessage(false, "Video Loop Ended").c_str());
	}
	else  // SingleCore mode
	{
		// The spawned CPU Thread also does the graphics.
		// The EmuThread is thus an idle thread, which sleeps while
		// waiting for the program to terminate. Without this extra
		// thread, the video backend window hangs in single core mode
		// because no one is pumping messages.
		Common::SetCurrentThreadName("Emuthread - Idle");

		// Spawn the CPU+GPU thread
		s_cpu_thread = std::thread(cpuThreadFunc);

		while (CPU::GetState() != CPU::CPU_POWERDOWN)
		{
			video_backend->PeekMessages();
			Common::SleepCurrentThread(20);
		}
	}

	INFO_LOG(CONSOLE, "%s", StopMessage(true, "Stopping Emu thread ...").c_str());

	// Wait for s_cpu_thread to exit
	INFO_LOG(CONSOLE, "%s", StopMessage(true, "Stopping CPU-GPU thread ...").c_str());

#ifdef USE_GDBSTUB
	INFO_LOG(CONSOLE, "%s", StopMessage(true, "Stopping GDB ...").c_str());
	gdb_deinit();
	INFO_LOG(CONSOLE, "%s", StopMessage(true, "GDB stopped.").c_str());
#endif

	s_cpu_thread.join();

	INFO_LOG(CONSOLE, "%s", StopMessage(true, "CPU thread stopped.").c_str());

	if (core_parameter.bCPUThread)
		video_backend->Video_Cleanup();

	FileMon::Close();

	// Stop audio thread - Actually this does nothing when using HLE
	// emulation, but stops the DSP Interpreter when using LLE emulation.
	DSP::GetDSPEmulator()->DSP_StopSoundStream();

	// We must set up this flag before executing HW::Shutdown()
	s_hardware_initialized = false;
	INFO_LOG(CONSOLE, "%s", StopMessage(false, "Shutting down HW").c_str());
	HW::Shutdown();
	INFO_LOG(CONSOLE, "%s", StopMessage(false, "HW shutdown").c_str());

	if (init_controllers)
	{
		Wiimote::Shutdown();
		Keyboard::Shutdown();
		Pad::Shutdown();
		g_controller_interface.Shutdown();
		init_controllers = false;
	}

	video_backend->Shutdown();
	AudioCommon::ShutdownSoundStream();

	INFO_LOG(CONSOLE, "%s", StopMessage(true, "Main Emu thread stopped").c_str());

	// Clear on screen messages that haven't expired
	OSD::ClearMessages();

	BootManager::RestoreConfig();

	INFO_LOG(CONSOLE, "Stop [Video Thread]\t\t---- Shutdown complete ----");
	Movie::Shutdown();
	PatchEngine::Shutdown();

	s_is_stopping = false;

	if (s_on_stopped_callback)
		s_on_stopped_callback();
}

// Set or get the running state

void SetState(EState state)
{
	// State cannot be controlled until the CPU Thread is operational
	if (!IsRunningAndStarted())
		return;

	switch (state)
	{
	case CORE_PAUSE:
		// NOTE: GetState() will return CORE_PAUSE immediately, even before anything has
		//   stopped (including the CPU).
		CPU::EnableStepping(true);  // Break
		Wiimote::Pause();
#if defined(__LIBUSB__)
		GCAdapter::ResetRumble();
#endif
		break;
	case CORE_RUN:
		CPU::EnableStepping(false);
		Wiimote::Resume();
		break;
	default:
		PanicAlert("Invalid state");
		break;
	}
}

EState GetState()
{
	if (s_is_stopping)
		return CORE_STOPPING;

	if (s_hardware_initialized)
	{
		if (CPU::IsStepping())
			return CORE_PAUSE;

		return CORE_RUN;
	}

	return CORE_UNINITIALIZED;
}

static std::string GenerateScreenshotFolderPath()
{
	const std::string& gameId = SConfig::GetInstance().GetGameID();
	std::string path = File::GetUserPath(D_SCREENSHOTS_IDX) + gameId + DIR_SEP_CHR;

	if (!File::CreateFullPath(path))
	{
		// fallback to old-style screenshots, without folder.
		path = File::GetUserPath(D_SCREENSHOTS_IDX);
	}

	return path;
}

static std::string GenerateScreenshotName()
{
	std::string path = GenerateScreenshotFolderPath();

	// append gameId, path only contains the folder here.
	path += SConfig::GetInstance().GetGameID();

	std::string name;
	for (int i = 1; File::Exists(name = StringFromFormat("%s-%d.png", path.c_str(), i)); ++i)
	{
		// TODO?
	}

	return name;
}

void SaveScreenShot()
{
	const bool bPaused = (GetState() == CORE_PAUSE);

	SetState(CORE_PAUSE);

	g_renderer->SaveScreenshot(GenerateScreenshotName(), false);

	if (!bPaused)
		SetState(CORE_RUN);
}

void SaveScreenShot(const std::string& name)
{
	const bool bPaused = (GetState() == CORE_PAUSE);

	SetState(CORE_PAUSE);

	std::string filePath = GenerateScreenshotFolderPath() + name + ".png";

	g_renderer->SaveScreenshot(filePath, false);

	if (!bPaused)
		SetState(CORE_RUN);
}

void RequestRefreshInfo()
{
	s_request_refresh_info = true;
}

bool PauseAndLock(bool do_lock, bool unpause_on_unlock)
{
	// WARNING: PauseAndLock is not fully threadsafe so is only valid on the Host Thread
	if (!IsRunning())
		return true;

	// let's support recursive locking to simplify things on the caller's side,
	// and let's do it at this outer level in case the individual systems don't support it.
	if (do_lock ? s_pause_and_lock_depth++ : --s_pause_and_lock_depth)
		return true;

	bool was_unpaused = true;
	if (do_lock)
	{
		// first pause the CPU
		// This acquires a wrapper mutex and converts the current thread into
		// a temporary replacement CPU Thread.
		was_unpaused = CPU::PauseAndLock(true);
	}

	ExpansionInterface::PauseAndLock(do_lock, false);

	// audio has to come after CPU, because CPU thread can wait for audio thread (m_throttle).
	DSP::GetDSPEmulator()->PauseAndLock(do_lock, false);

	// video has to come after CPU, because CPU thread can wait for video thread
	// (s_efbAccessRequested).
	Fifo::PauseAndLock(do_lock, false);

#if defined(__LIBUSB__)
	GCAdapter::ResetRumble();
#endif

	// CPU is unlocked last because CPU::PauseAndLock contains the synchronization
	// mechanism that prevents CPU::Break from racing.
	if (!do_lock)
	{
		// The CPU is responsible for managing the Audio and FIFO state so we use its
		// mechanism to unpause them. If we unpaused the systems above when releasing
		// the locks then they could call CPU::Break which would require detecting it
		// and re-pausing with CPU::EnableStepping.
		was_unpaused = CPU::PauseAndLock(false, unpause_on_unlock, true);
	}

	return was_unpaused;
}

void RunAsCPUThread(std::function<void()> function)
{
	const bool is_cpu_thread = IsCPUThread();
	bool was_unpaused = false;
	if (!is_cpu_thread)
		was_unpaused = PauseAndLock(true, true);

	function();

	if (!is_cpu_thread)
		PauseAndLock(false, was_unpaused);
}

// Display FPS info
// This should only be called from VI
void VideoThrottle()
{
	// Update info per second
	u32 ElapseTime = (u32)s_timer.GetTimeDifference();
	if ((ElapseTime >= 1000 && s_drawn_video.load() > 0) || s_request_refresh_info)
	{
		UpdateTitle();

		// Reset counter
		s_timer.Update();
		s_drawn_frame.store(0);
		s_drawn_video.store(0);
	}

	s_drawn_video++;
	bool update_ss_speed = true;
	if (SConfig::GetInstance().iVideoRate > 8)
	{
		update_ss_speed = (s_drawn_video % (SConfig::GetInstance().iVideoRate >> 3)) == 0;
	}
	// Update the audio timestretcher with the current speed
	if (g_sound_stream && update_ss_speed)
	{
		float Speed = (float)(s_drawn_video.load() * 1000.0 / (VideoInterface::GetTargetRefreshRate() * ElapseTime));
		g_sound_stream->GetMixer()->UpdateSpeed((float)Speed);
	}
}

// Executed from GPU thread
// reports if a frame should be skipped or not
// depending on the emulation speed set
bool ShouldSkipFrame(int skipped)
{
	u32 TargetFPS = VideoInterface::GetTargetRefreshRate();
	if (SConfig::GetInstance().m_EmulationSpeed > 0.0f)
		TargetFPS = u32(TargetFPS * SConfig::GetInstance().m_EmulationSpeed);
	const u32 frames = s_drawn_frame.load();
	const bool fps_slow = !(s_timer.GetTimeDifference() < (frames + skipped) * 1000 / TargetFPS);

	return fps_slow;
}

// --- Callbacks for backends / engine ---

// Should be called from GPU thread when a frame is drawn
void Callback_VideoCopiedToXFB(bool video_update)
{
	if (video_update)
		s_drawn_frame++;

	Movie::FrameUpdate();
}

void UpdateTitle()
{
	u32 ElapseTime = (u32)s_timer.GetTimeDifference();
	s_request_refresh_info = false;
	SConfig& _CoreParameter = SConfig::GetInstance();

	if (ElapseTime == 0)
		ElapseTime = 1;

	float FPS = (float)(s_drawn_frame.load() * 1000.0 / ElapseTime);
	float VPS = (float)(s_drawn_video.load() * 1000.0 / ElapseTime);
	float Speed = (float)(s_drawn_video.load() * (100 * 1000.0) /
		(VideoInterface::GetTargetRefreshRate() * ElapseTime));

	// Settings are shown the same for both extended and summary info
	std::string SSettings = StringFromFormat(
		"%s %s | %s | %s", PowerPC::GetCPUName(), _CoreParameter.bCPUThread ? "DC" : "SC",
		g_video_backend->GetDisplayName().c_str(), _CoreParameter.bDSPHLE ? "HLE" : "LLE");

	std::string SFPS;

	if (Movie::IsPlayingInput())
		SFPS = StringFromFormat("Input: %u/%u - VI: %u - FPS: %.0f - VPS: %.0f - %.0f%%",
		(u32)Movie::GetCurrentInputCount(), (u32)Movie::GetTotalInputCount(),
			(u32)Movie::GetCurrentFrame(), FPS, VPS, Speed);
	else if (Movie::IsRecordingInput())
		SFPS = StringFromFormat("Input: %u - VI: %u - FPS: %.0f - VPS: %.0f - %.0f%%",
		(u32)Movie::GetCurrentInputCount(), (u32)Movie::GetCurrentFrame(), FPS,
			VPS, Speed);
	else
	{
		SFPS = StringFromFormat("FPS: %.0f - VPS: %.0f - %.0f%%", FPS, VPS, Speed);
		if (SConfig::GetInstance().m_InterfaceExtendedFPSInfo)
		{
			// Use extended or summary information. The summary information does not print the ticks data,
			// that's more of a debugging interest, it can always be optional of course if someone is
			// interested.
			static u64 ticks = 0;
			static u64 idleTicks = 0;
			u64 newTicks = CoreTiming::GetTicks();
			u64 newIdleTicks = CoreTiming::GetIdleTicks();

			u64 diff = (newTicks - ticks) / 1000000;
			u64 idleDiff = (newIdleTicks - idleTicks) / 1000000;

			ticks = newTicks;
			idleTicks = newIdleTicks;

			float TicksPercentage =
				(float)diff / (float)(SystemTimers::GetTicksPerSecond() / 1000000) * 100;

			SFPS += StringFromFormat(" | CPU: ~%i MHz [Real: %i + IdleSkip: %i] / %i MHz (~%3.0f%%)",
				(int)(diff), (int)(diff - idleDiff), (int)(idleDiff),
				SystemTimers::GetTicksPerSecond() / 1000000, TicksPercentage);
		}
	}
	// This is our final "frame counter" string
	std::string SMessage = StringFromFormat("%s | %s", SSettings.c_str(), SFPS.c_str());
	Host_UpdateTitle(SMessage);
}

void Shutdown()
{
	// During shutdown DXGI expects us to handle some messages on the UI thread.
	// Therefore we can't immediately block and wait for the emu thread to shut
	// down, so we join the emu thread as late as possible when the UI has already
	// shut down.
	// For more info read "DirectX Graphics Infrastructure (DXGI): Best Practices"
	// on MSDN.
	if (s_emu_thread.joinable())
		s_emu_thread.join();

	// Make sure there's nothing left over in case we're about to exit.
	HostDispatchJobs();
}

void SetOnStoppedCallback(StoppedCallbackFunc callback)
{
	s_on_stopped_callback = callback;
}

void UpdateWantDeterminism(bool initial)
{
	// For now, this value is not itself configurable.  Instead, individual
	// settings that depend on it, such as GPU determinism mode. should have
	// override options for testing,
	bool new_want_determinism = Movie::IsMovieActive() || NetPlay::IsNetPlayRunning();
	if (new_want_determinism != g_want_determinism || initial)
	{
		NOTICE_LOG(COMMON, "Want determinism <- %s", new_want_determinism ? "true" : "false");

		bool was_unpaused = Core::PauseAndLock(true);

		g_want_determinism = new_want_determinism;
		WiiSockMan::GetInstance().UpdateWantDeterminism(new_want_determinism);
		Fifo::UpdateWantDeterminism(new_want_determinism);
		// We need to clear the cache because some parts of the JIT depend on want_determinism, e.g. use
		// of FMA.
		JitInterface::ClearCache();
		Common::InitializeWiiRoot(g_want_determinism);

		Core::PauseAndLock(false, was_unpaused);
	}
}

void QueueHostJob(std::function<void()> job, bool run_during_stop)
{
	if (!job)
		return;

	bool send_message = false;
	{
		std::lock_guard<std::mutex> guard(s_host_jobs_lock);
		send_message = s_host_jobs_queue.empty();
		s_host_jobs_queue.emplace(HostJob{ std::move(job), run_during_stop });
	}
	// If the the queue was empty then kick the Host to come and get this job.
	if (send_message)
		Host_Message(WM_USER_JOB_DISPATCH);
}

void HostDispatchJobs()
{
	// WARNING: This should only run on the Host Thread.
	// NOTE: This function is potentially re-entrant. If a job calls
	//   Core::Stop for instance then we'll enter this a second time.
	std::unique_lock<std::mutex> guard(s_host_jobs_lock);
	while (!s_host_jobs_queue.empty())
	{
		HostJob job = std::move(s_host_jobs_queue.front());
		s_host_jobs_queue.pop();

		// NOTE: Memory ordering is important. The booting flag needs to be
		//   checked first because the state transition is:
		//   CORE_UNINITIALIZED: s_is_booting -> s_hardware_initialized
		//   We need to check variables in the same order as the state
		//   transition, otherwise we race and get transient failures.
		if (!job.run_after_stop && !s_is_booting.IsSet() && !IsRunning())
			continue;

		guard.unlock();
		job.job();
		guard.lock();
	}
}

}  // Core
