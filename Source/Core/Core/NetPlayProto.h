// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/HW/EXI_Device.h"
#include <array>
#include <vector>

struct NetSettings
{
	bool m_CPUthread;
	int m_CPUcore;
	bool m_EnableCheats;
	int m_SelectedLanguage;
	bool m_OverrideGCLanguage;
	bool m_ProgressiveScan;
	bool m_PAL60;
	bool m_DSPHLE;
	bool m_DSPEnableJIT;
	bool m_WriteToMemcard;
	bool m_OCEnable;
	float m_OCFactor;
	TEXIDevices m_EXIDevice[2];

	MeleeLagReductionCode m_LagReduction;
	bool m_MeleeForceWidescreen;
};

extern NetSettings g_NetPlaySettings;
extern u64 g_netplay_initial_rtc;

struct Rpt : public std::vector<u8>
{
	u16 channel;
};

// messages
enum
{
	NP_MSG_PLAYER_JOIN = 0x10,
	NP_MSG_PLAYER_LEAVE = 0x11,

	NP_MSG_CHAT_MESSAGE = 0x30,

	NP_MSG_REPORT_FRAME_TIME = 0x40,

	NP_MSG_PAD_SPECTATOR = 0x5F,
	NP_MSG_PAD_DATA = 0x60,
	NP_MSG_PAD_MAPPING = 0x61,
	NP_MSG_PAD_BUFFER_MINIMUM = 0x62,
	NP_MSG_PAD_BUFFER_PLAYER = 0x63,

	NP_MSG_WIIMOTE_DATA = 0x70,
	NP_MSG_WIIMOTE_MAPPING = 0x71,

	NP_MSG_SLIPPI_PAD = 0x80,
	NP_MSG_SLIPPI_PAD_ACK = 0x81,
	NP_MSG_SLIPPI_MATCH_SELECTIONS = 0x82,
	NP_MSG_SLIPPI_CONN_SELECTED = 0x83,
	NP_MSG_SLIPPI_CHAT_MESSAGE = 0x84,

	NP_MSG_START_GAME = 0xA0,
	NP_MSG_CHANGE_GAME = 0xA1,
	NP_MSG_STOP_GAME = 0xA2,
	NP_MSG_DISABLE_GAME = 0xA3,
	NP_MSG_GAME_STATUS = 0xA4,

	NP_MSG_TIMEBASE = 0xB0,
	NP_MSG_DESYNC_DETECTED = 0xB1,

	NP_MSG_COMPUTE_MD5 = 0xC0,
	NP_MSG_MD5_PROGRESS = 0xC1,
	NP_MSG_MD5_RESULT = 0xC2,
	NP_MSG_MD5_ABORT = 0xC3,
	NP_MSG_MD5_ERROR = 0xC4,

	NP_MSG_READY = 0xD0,
	NP_MSG_NOT_READY = 0xD1,

	NP_MSG_PING = 0xE0,
	NP_MSG_PONG = 0xE1,
	NP_MSG_PLAYER_PING_DATA = 0xE2,

	NP_MSG_SYNC_GC_SRAM = 0xF0,
};

enum
{
	CON_ERR_SERVER_FULL = 1,
	CON_ERR_GAME_RUNNING = 2,
	CON_ERR_VERSION_MISMATCH = 3
};

using NetWiimote = std::vector<u8>;
using MessageId = u8;
using PlayerId = u8;
using FrameNum = u32;
using PadMapping = s8;
using PadMappingArray = std::array<PadMapping, 4>;

namespace NetPlay
{
bool IsNetPlayRunning();
}
