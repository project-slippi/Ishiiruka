#include "SlippiSavestate.h"
#include "Common/CommonFuncs.h"
#include "Common/MemoryUtil.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/EXI.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/HW.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI.h"
#include "Core/HW/VideoInterface.h"
#include <vector>

SlippiSavestate::SlippiSavestate()
{
	for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
	{
		auto size = it->endAddress - it->startAddress;
		it->data = static_cast<u8 *>(Common::AllocateAlignedMemory(size, 64));
	}

	// ssBackupLoc gameMemory;
	// gameMemory.isGame = false;
	// gameMemory.nonGamePtr = &Memory::m_pRAM;
	// gameMemory.size = Memory::RAM_SIZE;
	// gameMemory.data = static_cast<u8 *>(Common::AllocateAlignedMemory(Memory::RAM_SIZE, 64));
	// backupLocs.push_back(gameMemory);

	// ssBackupLoc l1Cache;
	// l1Cache.isGame = false;
	// l1Cache.nonGamePtr = &Memory::m_pL1Cache;
	// l1Cache.size = Memory::L1_CACHE_SIZE;
	// l1Cache.data = static_cast<u8 *>(Common::AllocateAlignedMemory(Memory::L1_CACHE_SIZE, 64));
	// backupLocs.push_back(l1Cache);

	// Second copy sound
	// u8 *ptr = &audioBackup[0];
	// PointerWrap p(&ptr, PointerWrap::MODE_WRITE);
	// AudioInterface::DoState(p);

	u8 *ptr = nullptr;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);

	getDolphinState(p);
	const size_t buffer_size = reinterpret_cast<size_t>(ptr);
	dolphinSsBackup.resize(buffer_size);

	ERROR_LOG(SLIPPI_ONLINE, "Dolphin backup size: %d", buffer_size);

	// Set up alarm stuff
	alarmPtrs.resize(8);
}

SlippiSavestate::~SlippiSavestate()
{
	for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
	{
		Common::FreeAlignedMemory(it->data);
	}
}

void SlippiSavestate::getDolphinState(PointerWrap &p)
{
	// p.DoArray(Memory::m_pRAM, Memory::RAM_SIZE);
	// p.DoMarker("Memory");
	// VideoInterface::DoState(p);
	// p.DoMarker("VideoInterface");
	// SerialInterface::DoState(p);
	// p.DoMarker("SerialInterface");
	// ProcessorInterface::DoState(p);
	// p.DoMarker("ProcessorInterface");
	// DSP::DoState(p);
	// p.DoMarker("DSP");
	// DVDInterface::DoState(p);
	// p.DoMarker("DVDInterface");
	// GPFifo::DoState(p);
	// p.DoMarker("GPFifo");
	ExpansionInterface::DoState(p);
	p.DoMarker("ExpansionInterface");
	// AudioInterface::DoState(p);
	// p.DoMarker("AudioInterface");
}

void SlippiSavestate::Capture()
{
	origAlarmPtr = Memory::Read_U32(FIRST_ALARM_PTR_ADDR);

	// First copy memory
	for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
	{
		auto size = it->endAddress - it->startAddress;
		Memory::CopyFromEmu(it->data, it->startAddress, size);
	}

	// Copy ptr to heap locations
	for (auto it = backupPtrLocs.begin(); it != backupPtrLocs.end(); ++it)
	{
		it->value = Memory::Read_U32(it->address);
	}

	// Second copy dolphin states
	u8 *ptr = &dolphinSsBackup[0];
	PointerWrap p(&ptr, PointerWrap::MODE_WRITE);
	getDolphinState(p);
}

void SlippiSavestate::Load(std::vector<PreserveBlock> blocks)
{
	// Back up alarm stuff
	// Memory::CopyFromEmu(&alarmPtrs[0], FIRST_ALARM_PTR_ADDR, 8);

	// std::unordered_map<u32, std::vector<u8>> alarmData;

	u32 alarmPtr = Memory::Read_U32(FIRST_ALARM_PTR_ADDR);
	if (alarmPtr != origAlarmPtr)
	{
		ERROR_LOG(SLIPPI_ONLINE, "Trying to deal with alarm boundary: %x -> %x", origAlarmPtr, alarmPtr);
	}

	// while (alarmPtr != 0)
	//{
	//	std::vector<u8> iAlarmData;
	//	iAlarmData.resize(ALARM_DATA_SIZE);
	//	Memory::CopyFromEmu(&iAlarmData[0], alarmPtr, ALARM_DATA_SIZE);
	//	alarmData[alarmPtr] = iAlarmData;

	//	alarmPtr = Memory::Read_U32(alarmPtr + 0x14);
	//}

	// static PreserveBlock stackBlock = {0x804DEC00, 0x10000};
	// blocks.push_back(stackBlock);

	// Always back up this alarm anyway... shouldn't be necessary
	// static PreserveBlock readAlarmBlock = {READ_ALARM_ADDR, ALARM_DATA_SIZE};
	// blocks.push_back(readAlarmBlock);

	// static PreserveBlock interruptAlarmBlock = {0x804d7358, 0x34};
	// blocks.push_back(interruptAlarmBlock);

	// static std::vector<PreserveBlock> interruptStuff = {
	//    {0x804BF9D2, 4},
	//    {0x804C3DE4, 20},
	//    {0x804C4560, 44},
	//    {0x804D7760, 36},
	//};

	// for (auto it = interruptStuff.begin(); it != interruptStuff.end(); ++it)
	// {
	//  blocks.push_back(*it);
	// }

	static std::vector<PreserveBlock> soundStuff = {
	    {0x804031A0, 0x24},    // [804031A0 - 804031C4)
	    {0x80407FB4, 0x28},    // [80407FB4 - 80407FDC)
	    {0x80408250, 0xB0},    // [80408250 - 80408300)
	    {0x80433C64, 0x1EE80}, // [80433C64 - 80452AE4)
	    {0x804A8458, 0x238},   // [804A8458 - 804A8690)
	    {0x804A8D78, 0x17A68}, // [804A8D78 - 804C07E0)
	    {0x804C28E0, 0x399C},  // [804C28E0 - 804C627C)
	    {0x804D7474, 0x8},     // [804D7474 - 804D747C)
	    {0x804D74F0, 0x50},    // [804D74F0 - 804D7540)
	    {0x804D7548, 0x4},     // [804D7548 - 804D754C)
	    {0x804D7558, 0x24},    // [804D7558 - 804D757C)
	    {0x804D7580, 0xC},     // [804D7580 - 804D758C)
	    {0x804D759C, 0x4},     // [804D759C - 804D75A0)
	    {0x804D7720, 0x4},     // [804D7720 - 804D7724)
	    {0x804D7744, 0x4},     // [804D7744 - 804D7748)
	    {0x804D774C, 0x8},     // [804D774C - 804D7754)
	    {0x804D7758, 0x8},     // [804D7758 - 804D7760)
	    {0x804D7788, 0x10},    // [804D7788 - 804D7798)
	    {0x804D77C8, 0x4},     // [804D77C8 - 804D77CC)
	    {0x804D77D0, 0x4},     // [804D77D0 - 804D77D4)
	    {0x804D77E0, 0x4},     // [804D77E0 - 804D77E4)
	    {0x804DE358, 0x80},    // [804DE358 - 804DE3D8)
	    {0x804DE800, 0x70},    // [804DE800 - 804DE870)
	};

	for (auto it = soundStuff.begin(); it != soundStuff.end(); ++it)
	{
		blocks.push_back(*it);
	}

	// Back up
	for (auto it = blocks.begin(); it != blocks.end(); ++it)
	{
		if (!preservationMap.count(*it))
		{
			// TODO: Clear preservation map when game ends
			preservationMap[*it] = std::vector<u8>(it->length);
		}

		Memory::CopyFromEmu(&preservationMap[*it][0], it->address, it->length);
	}

	// Restore memory blocks
	for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
	{
		auto size = it->endAddress - it->startAddress;
		Memory::CopyToEmu(it->startAddress, it->data, size);
	}

	// Restore ptr to heap locations
	for (auto it = backupPtrLocs.begin(); it != backupPtrLocs.end(); ++it)
	{
		Memory::Write_U32(it->value, it->address);
	}

	// Restore audio
	u8 *ptr = &dolphinSsBackup[0];
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	getDolphinState(p);

	// Restore
	for (auto it = blocks.begin(); it != blocks.end(); ++it)
	{
		Memory::CopyToEmu(it->address, &preservationMap[*it][0], it->length);
	}

	// Try to turn off any alarms
	// Memory::Write_U32(0, FIRST_ALARM_PTR_ADDR);
	// Memory::Write_U32(0, FIRST_ALARM_PTR_ADDR + 4);
	// Memory::Write_U32(0, READ_ALARM_ADDR);

	// Restore alarm stuff
	// Memory::CopyToEmu(FIRST_ALARM_PTR_ADDR, &alarmPtrs[0], 8);

	// for (auto it = alarmData.begin(); it != alarmData.end(); ++it)
	//{
	//	Memory::CopyToEmu(it->first, &it->second[0], ALARM_DATA_SIZE);
	//}
}
