// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <limits>
#include <string>
#include <vector>

#include "Common/IniFile.h"
#include "Common/NonCopyable.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/SI_Device.h"

namespace DiscIO
{
enum class Language;
}

// DSP Backend Types
#define BACKEND_NULLSOUND _trans("No audio output")
#define BACKEND_ALSA "ALSA"
#define BACKEND_AOSOUND "AOSound"
#define BACKEND_COREAUDIO "CoreAudio"
#define BACKEND_CUBEB "Cubeb"
#define BACKEND_EXCLUSIVE_WASAPI "Exclusive WASAPI"
#define BACKEND_SHARED_WASAPI "Shared-mode WASAPI"
#define BACKEND_DIRECTSOUND "DSound"
#define BACKEND_OPENAL "OpenAL"
#define BACKEND_PULSEAUDIO "Pulse"
#define BACKEND_XAUDIO2 "XAudio2"
#define BACKEND_OPENSLES "OpenSLES"

enum GPUDeterminismMode
{
	GPU_DETERMINISM_AUTO,
	GPU_DETERMINISM_NONE,
	// This is currently the only mode.  There will probably be at least
	// one more at some point.
	GPU_DETERMINISM_FAKE_COMPLETION,
};

enum GameType
{
	GAMETYPE_OTHER,
	GAMETYPE_MELEE_NTSC,
	GAMETYPE_MELEE_20XX,
	GAMETYPE_MELEE_UPTM,
	GAMETYPE_MELEE_AKANEIA,
};

enum PollingMethod
{
	POLLING_CONSOLE = 0,
	POLLING_ONSIREAD = 1
};

enum MeleeLagReductionCode
{
	MELEE_LAG_REDUCTION_CODE_UNSET = 0,
	MELEE_LAG_REDUCTION_CODE_NORMAL = 1,
	MELEE_LAG_REDUCTION_CODE_PERFORMANCE = 2
};

struct SConfig : NonCopyable
{
	// Wii Devices
	bool m_WiiSDCard;
	bool m_WiiKeyboard;
	bool m_WiimoteContinuousScanning;
	bool m_WiimoteEnableSpeaker;

	// name of the last used filename
	std::string m_LastFilename;

	GameType m_gameType;

	// ISO folder
	std::vector<std::string> m_ISOFolder;
	bool m_RecursiveISOFolder;

	// enable Slippi Networking output
	bool m_enableSpectator;
	int m_spectator_local_port;

	// Settings
	bool bEnableDebugging = false;
#ifdef USE_GDBSTUB
	int iGDBPort;
#ifndef _WIN32
	std::string gdb_socket;
#endif
#endif
	bool bAutomaticStart = false;
	bool bBootToPause = false;

	int iCPUCore;

	// JIT (shared between JIT and JITIL)
	bool bJITNoBlockCache = false;
	bool bJITNoBlockLinking = false;
	bool bJITOff = false;
	bool bJITLoadStoreOff = false;
	bool bJITLoadStorelXzOff = false;
	bool bJITLoadStorelwzOff = false;
	bool bJITLoadStorelbzxOff = false;
	bool bJITLoadStoreFloatingOff = false;
	bool bJITLoadStorePairedOff = false;
	bool bJITFloatingPointOff = false;
	bool bJITIntegerOff = false;
	bool bJITPairedOff = false;
	bool bJITSystemRegistersOff = false;
	bool bJITBranchOff = false;
	bool bJITILTimeProfiling = false;
	bool bJITILOutputIR = false;

	bool bFastmem;
	bool bFPRF = false;
	bool bAccurateNaNs = false;

	int iTimingVariance = 40; // in milli secounds
	bool bCPUThread = true;
	bool bDSPThread = false;
	bool bDSPHLE = true;
	bool bSyncGPUOnSkipIdleHack = true;
	bool bNTSC = false;
	bool bForceNTSCJ = false;
	bool bHLE_BS2 = true;
	bool bEnableCheats = false;
	bool bBootDefaultISO = false;
	bool bEnableMemcardSdWriting = true;
	bool bAllowAllNetplayVersions = false;
	bool bQoSEnabled = true;
	bool bAdapterWarning = true;

	bool bReduceTimingDispersion = false;

	MeleeLagReductionCode iLagReductionCode = MELEE_LAG_REDUCTION_CODE_UNSET;
	bool bHasShownLagReductionWarning = false;
	bool bMeleeForceWidescreen = false;

	// Slippi
	bool m_slippiSaveReplays = true;
	bool m_slippiEnableQuickChat = true;
	bool m_slippiReplayMonthFolders = false;
	std::string m_strSlippiReplayDir;
	bool m_slippiForceNetplayPort = false;
	int m_slippiNetplayPort;
	bool m_slippiForceLanIp = false;
	std::string m_slippiLanIp = "";
	bool m_meleeUserIniBootstrapped = false;
	bool m_blockingPipes = false;
	bool m_coutEnabled = false;

	// Slippi Playback
	bool m_slippiEnableFrameIndex = false;

	bool bDPL2Decoder = false;
	bool bTimeStretching = false;
	bool bRSHACK = false;
	int iLatency = 14;

	bool bRunCompareServer = false;
	bool bRunCompareClient = false;

	bool bMMU = false;
	bool bDCBZOFF = false;
	int iBBDumpPort = 0;
	bool bFastDiscSpeed = false;
	int iVideoRate = 8;
	bool bHalfAudioRate = false;

	PollingMethod iPollingMethod = POLLING_CONSOLE;

	bool bSyncGPU = false;
	int iSyncGpuMaxDistance;
	int iSyncGpuMinDistance;
	float fSyncGpuOverclock;

	int SelectedLanguage = 0;
	bool bOverrideGCLanguage = false;

	bool bWii = false;

	// Interface settings
	bool bConfirmStop = false;
	bool bHideCursor = false, bAutoHideCursor = false;
	bool bUsePanicHandlers = true;
	bool bOnScreenDisplayMessages = true;
	std::string theme_name;

	// Display settings
	std::string strFullscreenResolution;
	int iRenderWindowXPos = std::numeric_limits<int>::min();
	int iRenderWindowYPos = std::numeric_limits<int>::min();
	int iRenderWindowWidth = -1;
	int iRenderWindowHeight = -1;
	bool bRenderWindowAutoSize = false, bKeepWindowOnTop = false;
	bool bFullscreen = false, bRenderToMain = false;
	bool bProgressive = false, bPAL60 = false;
	bool bDisableScreenSaver = false;

	int iPosX, iPosY, iWidth, iHeight;

	// Analytics settings.
	std::string m_analytics_id;
	bool m_analytics_enabled = false;
	bool m_analytics_permission_asked = false;
	bool m_analytics_will_prompt = false;

	// Bluetooth passthrough mode settings
	bool m_bt_passthrough_enabled = false;
	int m_bt_passthrough_pid = -1;
	int m_bt_passthrough_vid = -1;
	std::string m_bt_passthrough_link_keys;

	// SYSCONF settings
	int m_sensor_bar_position = 0x01;
	int m_sensor_bar_sensitivity = 0x03;
	int m_speaker_volume = 0x58;
	bool m_wiimote_motor = true;
	int m_wii_language = 0x01;
	int m_wii_aspect_ratio = 0x01;
	int m_wii_screensaver = 0x00;

	// Fifo Player related settings
	bool bLoopFifoReplay = true;

	// Custom RTC
	bool bEnableCustomRTC;
	u32 m_customRTCValue;

	enum EBootBS2
	{
		BOOT_DEFAULT,
		BOOT_BS2_JAP,
		BOOT_BS2_USA,
		BOOT_BS2_EUR,
	};

	enum EBootType
	{
		BOOT_ISO,
		BOOT_ELF,
		BOOT_DOL,
		BOOT_WII_NAND,
		BOOT_BS2,
		BOOT_DFF
	};
	EBootType m_BootType;

	std::string m_strVideoBackend;
	std::string m_strSlippiInput;
	std::string m_strOutputDirectory;
	std::string m_strOutputFilenameBase;
	std::string m_strGPUDeterminismMode;

	// set based on the string version
	GPUDeterminismMode m_GPUDeterminismMode;

	// files
	std::string m_strFilename;
	std::string m_strBootROM;
	std::string m_strSRAM;
	std::string m_strDefaultISO;
	std::string m_strDVDRoot;
	std::string m_strApploader;
	std::string m_strGameID;
	std::string m_strName;
	std::string m_strWiiSDCardPath;
	u16 m_revision;

	std::string m_perfDir;

	void LoadDefaults();
	bool AutoSetup(EBootBS2 _BootBS2);
	const std::string &GetGameID() const { return m_strGameID; }
	void CheckMemcardPath(std::string &memcardPath, const std::string &gameRegion, bool isSlotA);
	DiscIO::Language GetCurrentLanguage(bool wii) const;

	u16 GetGameRevision() const;
	std::string GetGameID_Wrapper() const;
	bool GameHasDefaultGameIni() const;
	IniFile LoadDefaultGameIni() const;
	IniFile LoadLocalGameIni() const;
	IniFile LoadGameIni() const;

	static bool GameHasDefaultGameIni(const std::string &id, u16 revision);
	static IniFile LoadDefaultGameIni(const std::string &id, u16 revision);
	static IniFile LoadLocalGameIni(const std::string &id, u16 revision);
	static IniFile LoadGameIni(const std::string &id, u16 revision);

	static std::vector<std::string> GetGameIniFilenames(const std::string &id, u16 revision);

	std::string m_NANDPath;
	std::string m_DumpPath;

	int m_slippiOnlineDelay = 2;

	std::string m_strMemoryCardA;
	std::string m_strMemoryCardB;
	std::string m_strGbaCartA;
	std::string m_strGbaCartB;
	TEXIDevices m_EXIDevice[3];
	SIDevices m_SIDevice[4];
	std::string m_bba_mac;

	// interface language
	std::string m_InterfaceLanguage;
	float m_EmulationSpeed;
	bool m_OCEnable;
	float m_OCFactor;
	// other interface settings
	bool m_InterfaceToolbar;
	bool m_InterfaceStatusbar;
	bool m_InterfaceSeekbar;
	bool m_InterfaceLogWindow;
	bool m_InterfaceLogConfigWindow;
	bool m_InterfaceExtendedFPSInfo;

	bool m_ListDrives;
	bool m_ListWad;
	bool m_ListElfDol;
	bool m_ListWii;
	bool m_ListGC;
	bool m_ListPal;
	bool m_ListUsa;
	bool m_ListJap;
	bool m_ListAustralia;
	bool m_ListFrance;
	bool m_ListGermany;
	bool m_ListItaly;
	bool m_ListKorea;
	bool m_ListNetherlands;
	bool m_ListRussia;
	bool m_ListSpain;
	bool m_ListTaiwan;
	bool m_ListWorld;
	bool m_ListUnknown;
	int m_ListSort;
	int m_ListSort2;

	// Game list column toggles
	bool m_showSystemColumn;
	bool m_showBannerColumn;
	bool m_showMakerColumn;
	bool m_showFileNameColumn;
	bool m_showIDColumn;
	bool m_showRegionColumn;
	bool m_showSizeColumn;
	bool m_showStateColumn;

	// Toggles whether compressed titles show up in blue in the game list
	bool m_ColorCompressed;

	std::string m_WirelessMac;
	bool m_PauseMovie;
	bool m_ShowLag;
	bool m_ShowFrameCount;
	bool m_ShowRTC;
	std::string m_strMovieAuthor;
	unsigned int m_FrameSkip;
	bool m_DumpFrames;
	bool m_DumpFramesSilent;
	bool m_ShowInputDisplay;

	bool m_PauseOnFocusLost;

	bool m_DisableTooltips;

	// DSP settings
	bool m_DSPEnableJIT;
	bool m_DSPCaptureLog;
	bool m_DumpAudio;
	bool m_DumpAudioSilent;
	bool m_IsMuted;
	bool m_DumpUCode;
	int m_Volume;
	std::string sBackend;

	// Input settings
	bool m_BackgroundInput;
	bool m_AdapterRumble[4];
	bool m_AdapterKonga[4];

	// Network settings
	bool m_SSLDumpRead;
	bool m_SSLDumpWrite;
	bool m_SSLVerifyCert;
	bool m_SSLDumpRootCA;
	bool m_SSLDumpPeerCert;

	// Save settings
	void SaveSettings();

	// Load settings
	void LoadSettings();

	void LoadSettingsFromSysconf();
	void SaveSettingsToSysconf();

	// Return the permanent and somewhat globally used instance of this struct
	static SConfig &GetInstance() { return (*m_Instance); }
	static void Init();
	static void Shutdown();

  private:
	SConfig();
	~SConfig();

	void SaveGeneralSettings(IniFile &ini);
	void SaveInterfaceSettings(IniFile &ini);
	void SaveDisplaySettings(IniFile &ini);
	void SaveGameListSettings(IniFile &ini);
	void SaveCoreSettings(IniFile &ini);
	void SaveDSPSettings(IniFile &ini);
	void SaveInputSettings(IniFile &ini);
	void SaveMovieSettings(IniFile &ini);
	void SaveFifoPlayerSettings(IniFile &ini);
	void SaveNetworkSettings(IniFile &ini);
	void SaveAnalyticsSettings(IniFile &ini);
	void SaveBluetoothPassthroughSettings(IniFile &ini);
	void SaveSysconfSettings(IniFile &ini);

	void LoadGeneralSettings(IniFile &ini);
	void LoadInterfaceSettings(IniFile &ini);
	void LoadDisplaySettings(IniFile &ini);
	void LoadGameListSettings(IniFile &ini);
	void LoadCoreSettings(IniFile &ini);
	void LoadDSPSettings(IniFile &ini);
	void LoadInputSettings(IniFile &ini);
	void LoadMovieSettings(IniFile &ini);
	void LoadFifoPlayerSettings(IniFile &ini);
	void LoadNetworkSettings(IniFile &ini);
	void LoadAnalyticsSettings(IniFile &ini);
	void LoadBluetoothPassthroughSettings(IniFile &ini);
	void LoadSysconfSettings(IniFile &ini);

	static SConfig *m_Instance;
};
