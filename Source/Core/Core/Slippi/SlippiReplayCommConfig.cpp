// Copyright 2026 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "SlippiReplayComm.h"

#include "Common/Logging/LogManager.h"
#include "Core/ConfigManager.h"

SlippiReplayComm::SlippiReplayComm() : SlippiReplayComm(SConfig::GetInstance().m_strSlippiInput)
{
	INFO_LOG(EXPANSIONINTERFACE, "SlippiReplayComm: Using playback config path: %s",
	         SConfig::GetInstance().m_strSlippiInput.c_str());
}
