// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <mutex>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/HW/Memmap.h"
#include "Core/NetPlayProto.h"
#include "Core/PowerPC/PowerPC.h"

#include "VideoCommon/OnScreenDisplay.h"

#include <iostream>

namespace Gecko
{

static const u32 INSTALLER_BASE_ADDRESS = 0x80001800;
static const u32 INSTALLER_END_ADDRESS = 0x80003000;

// return true if a code exists
bool GeckoCode::Exist(u32 address, u32 data) const
{
	for (const GeckoCode::Code& code : codes)
	{
		if (code.address == address && code.data == data)
			return true;
	}

	return false;
}

// return true if the code is identical
bool GeckoCode::Compare(const GeckoCode& compare) const
{
	if (codes.size() != compare.codes.size())
		return false;

	unsigned int exist = 0;

	for (const GeckoCode::Code& code : codes)
	{
		if (compare.Exist(code.address, code.data))
			exist++;
	}

	return exist == codes.size();
}

static bool initialization_failed = false;
static bool code_handler_installed = false;
// the currently active codes
static std::vector<GeckoCode> active_codes;
static std::mutex active_codes_lock;

static bool IsEnabledMeleeCode(const GeckoCode& code)
{
    if(SConfig::GetInstance().bMeleeForceWidescreen && code.name == "Widescreen 16:9")
        return true;
        
    if(NetPlay::IsNetPlayRunning() && SConfig::GetInstance().iLagReductionCode != MELEE_LAG_REDUCTION_CODE_UNSET)
    {
        if(SConfig::GetInstance().iLagReductionCode == MELEE_LAG_REDUCTION_CODE_NORMAL)
            return code.name.find("Normal Lag Reduction") != std::string::npos;

        if(SConfig::GetInstance().iLagReductionCode == MELEE_LAG_REDUCTION_CODE_PERFORMANCE)
            return code.name.find("Performance Lag Reduction") != std::string::npos;
    }

    return false;
}

static bool IsDisabledMeleeCode(const GeckoCode& code)
{
    if(NetPlay::IsNetPlayRunning() && SConfig::GetInstance().iLagReductionCode != MELEE_LAG_REDUCTION_CODE_UNSET)
    {
        if(SConfig::GetInstance().iLagReductionCode == MELEE_LAG_REDUCTION_CODE_NORMAL)
            return code.name.find("Performance Lag Reduction") != std::string::npos;

        if(SConfig::GetInstance().iLagReductionCode == MELEE_LAG_REDUCTION_CODE_PERFORMANCE)
            return code.name.find("Normal Lag Reduction") != std::string::npos;
    }

    return false;
}

void SetActiveCodes(const std::vector<GeckoCode>& gcodes)
{
	std::lock_guard<std::mutex> lk(active_codes_lock);

	active_codes.clear();

	// add enabled codes
	for (const GeckoCode& gecko_code : gcodes)
	{        
		if ((gecko_code.enabled && !IsDisabledMeleeCode(gecko_code)) 
			|| IsEnabledMeleeCode(gecko_code))
		{
			active_codes.push_back(gecko_code);
		}
	}

	initialization_failed = false;
	code_handler_installed = false;
}

static bool InstallCodeHandler()
{
	if (initialization_failed)
		return false;

	std::string data;
	std::string _rCodeHandlerFilename = File::GetSysDirectory() + GECKO_CODE_HANDLER;
	if (!File::ReadFileToString(_rCodeHandlerFilename, data))
	{
		NOTICE_LOG(ACTIONREPLAY, "Could not enable cheats because codehandler.bin was missing.");
		return false;
	}

	u8 mmioAddr = 0xCC;

	if (SConfig::GetInstance().bWii)
	{
		mmioAddr = 0xCD;
	}

	// Install code handler
	for (size_t i = 0, e = data.length(); i < e; ++i)
		PowerPC::HostWrite_U8(data[i], (u32)(INSTALLER_BASE_ADDRESS + i));

	// Patch the code handler to the system starting up
	for (unsigned int h = 0; h < data.length(); h += 4)
	{
		// Patch MMIO address
		if (PowerPC::HostRead_U32(INSTALLER_BASE_ADDRESS + h) == (0x3f000000u | ((mmioAddr ^ 1) << 8)))
		{
			NOTICE_LOG(ACTIONREPLAY, "Patching MMIO access at %08x", INSTALLER_BASE_ADDRESS + h);
			PowerPC::HostWrite_U32(0x3f000000u | mmioAddr << 8, INSTALLER_BASE_ADDRESS + h);
		}
	}

	u32 codelist_base_address = INSTALLER_BASE_ADDRESS + (u32)data.length() - 8;
	u32 codelist_end_address = INSTALLER_END_ADDRESS;

	// Write a magic value to 'gameid' (codehandleronly does not actually read this).
	PowerPC::HostWrite_U32(0xd01f1bad, INSTALLER_BASE_ADDRESS);

	if(SConfig::GetInstance().m_gameType == GAMETYPE_MELEE_NTSC)
	{
		// Here we are replacing a line in the codehandler with a blr.
		// The reason for this is that this is the section of the codehandler
		// that attempts to read/write commands for the USB Gecko. These calls
		// were sometimes interfering with the Slippi EXI calls and causing
		// the game to loop infinitely in EXISync.
		PowerPC::HostWrite_U32(0x4E800020, 0x80001D6C);

		// Write GCT loader into memory which will eventually load the real GCT into the heap
		std::string bootloaderData;
		std::string _bootloaderFilename = File::GetSysDirectory() + GCT_BOOTLOADER;
		if (!File::ReadFileToString(_bootloaderFilename, bootloaderData))
		{
			OSD::AddMessage("bootloader.gct not found in Sys folder.", 30000, 0xFFFF0000);
			ERROR_LOG(ACTIONREPLAY, "Could not enable cheats because bootloader.gct was missing.");
			initialization_failed = true;
			return false;
		}

		if (bootloaderData.length() > codelist_end_address - codelist_base_address)
		{
			OSD::AddMessage("Gecko bootloader too large.", 30000, 0xFFFF0000);
			ERROR_LOG(SLIPPI, "Gecko bootloader too large");
			initialization_failed = true;
			return false;
		}

		// Install bootloader gct
		for (size_t i = 0, e = bootloaderData.length(); i < e; ++i)
			PowerPC::HostWrite_U8(bootloaderData[i], (u32)(codelist_base_address + i));
	}
	else
	{
		// Create GCT in memory
		PowerPC::HostWrite_U32(0x00d0c0de, codelist_base_address);
		PowerPC::HostWrite_U32(0x00d0c0de, codelist_base_address + 4);

		std::lock_guard<std::mutex> lk(active_codes_lock);

		int i = 0;
		// First check if we have enough space for all the codes
		for (const GeckoCode &active_code : active_codes)
		{
			if ((active_code.enabled && !IsDisabledMeleeCode(active_code)) || IsEnabledMeleeCode(active_code))
			{
				for (const GeckoCode::Code &code : active_code.codes)
				{
					i += 8;
				}
			}
		}

		auto available = codelist_end_address - (codelist_base_address + 24);
		INFO_LOG(ACTIONREPLAY, "Code usage: %d/%d", i, available);

		// Write error message if not enough space
		if ((codelist_base_address + 24 + i) >= codelist_end_address)
		{
			auto s = StringFromFormat("Ran out of memory applying gecko codes (%d/%d).", i, available);
			OSD::AddMessage(s, 30000, 0xFFFF0000);
			OSD::AddMessage("Codes were not applied, try disabling some codes.", 30000, 0xFFFF0000);

			ERROR_LOG(SLIPPI, "Ran out of memory applying gecko codes");
			initialization_failed = true;
			return false;
		}

		i = 0;
		for (const GeckoCode &active_code : active_codes)
		{
			if ((active_code.enabled && !IsDisabledMeleeCode(active_code)) || IsEnabledMeleeCode(active_code))
			{
				for (const GeckoCode::Code &code : active_code.codes)
				{
					PowerPC::HostWrite_U32(code.address, codelist_base_address + 8 + i);
					PowerPC::HostWrite_U32(code.data, codelist_base_address + 12 + i);
					i += 8;
				}
			}
		}

		PowerPC::HostWrite_U32(0xff000000, codelist_base_address + 8 + i);
		PowerPC::HostWrite_U32(0x00000000, codelist_base_address + 12 + i);
	}

	// Turn on codes
	PowerPC::HostWrite_U8(1, INSTALLER_BASE_ADDRESS + 7);

	// Invalidate the icache and any asm codes
	for (unsigned int j = 0; j < (INSTALLER_END_ADDRESS - INSTALLER_BASE_ADDRESS); j += 32)
	{
		PowerPC::ppcState.iCache.Invalidate(INSTALLER_BASE_ADDRESS + j);
	}
	for (unsigned int k = codelist_base_address; k < codelist_end_address; k += 32)
	{
		PowerPC::ppcState.iCache.Invalidate(k);
	}
	return true;
}

void RunCodeHandler()
{
	if (SConfig::GetInstance().bEnableCheats && active_codes.size() > 0)
	{
		if (!code_handler_installed || PowerPC::HostRead_U32(INSTALLER_BASE_ADDRESS) - 0xd01f1bad > 5)
			code_handler_installed = InstallCodeHandler();

		if (!code_handler_installed)
		{
			// A warning was already issued.
			return;
		}

		// Removed this condition and changed a few things with the codehandler run process. This should now only get called once
		// at the earliest possible moment during boot. It should no longer get called on a timer.
		// if (PC == LR)
		//{
		u32 oldPC = PC;
		u32 oldLR = LR;

		// Backup all registers
		// std::vector<u32> oldRegisters(32);
		// for (int i = 0; i < 32; i++)
		// oldRegisters[i] = GPR(i);

		PowerPC::CoreMode oldMode = PowerPC::GetMode();

		PC = INSTALLER_BASE_ADDRESS + 0xA8;
		LR = 0;

		// Execute the code handler in interpreter mode to track when it exits
		PowerPC::SetMode(PowerPC::MODE_INTERPRETER);

		while (PC != 0)
		{
			// if ((PC >= 0x80001f54) && (PC <= 0x80001f94))
			//{
			//	INFO_LOG(ACTIONREPLAY, "pc=%08x r3=%08x r4=%08x r15=%08x", PC, GPR(3), GPR(4), GPR(15));
			//}
			PowerPC::SingleStep();
		}

		PowerPC::SetMode(oldMode);

		//// Restore registers
		//   for (int i = 0; i < 32; i++)
		//    PowerPC::ppcState.gpr[i] = oldRegisters[i];

		PC = oldPC;
		LR = oldLR;
		//}
	}
}

u32 GetGctLength()
{
	std::lock_guard<std::mutex> lk(active_codes_lock);

	int i = 0;

	for (const GeckoCode &active_code : active_codes)
	{
		if ((active_code.enabled && !IsDisabledMeleeCode(active_code)) || IsEnabledMeleeCode(active_code))
		{
			for (const GeckoCode::Code &code : active_code.codes)
			{
				i += 8;
			}
		}
	}

	return i + 0x10; // 0x10 is the fixed size of the header and terminator
}

std::vector<u8> uint32ToVector(u32 num)
{
	u8 byte0 = num >> 24;
	u8 byte1 = (num & 0xFF0000) >> 16;
	u8 byte2 = (num & 0xFF00) >> 8;
	u8 byte3 = num & 0xFF;

	return std::vector<u8>({byte0, byte1, byte2, byte3});
}

void appendWordToBuffer(std::vector<u8> *buf, u32 word)
{
	auto wordVector = uint32ToVector(word);
	buf->insert(buf->end(), wordVector.begin(), wordVector.end());
}

std::vector<u8> GenerateGct()
{
	std::vector<u8> res;

	// Write header
	appendWordToBuffer(&res, 0x00d0c0de);
	appendWordToBuffer(&res, 0x00d0c0de);

	std::lock_guard<std::mutex> lk(active_codes_lock);

	int i = 0;

	// Write codes
	for (const GeckoCode &active_code : active_codes)
	{
		if ((active_code.enabled && !IsDisabledMeleeCode(active_code)) || IsEnabledMeleeCode(active_code))
		{
			for (const GeckoCode::Code &code : active_code.codes)
			{
				appendWordToBuffer(&res, code.address);
				appendWordToBuffer(&res, code.data);
			}
		}
	}

	// Write footer
	appendWordToBuffer(&res, 0xff000000);
	appendWordToBuffer(&res, 0x00000000);

	return res;
}

} // namespace Gecko
