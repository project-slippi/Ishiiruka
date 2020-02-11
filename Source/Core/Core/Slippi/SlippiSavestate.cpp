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

	captureDolphinState(p);
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

void SlippiSavestate::captureDolphinState(PointerWrap &p)
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
	AudioInterface::DoState(p);
	p.DoMarker("AudioInterface");
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

	// Second copy dolphin states
	u8 *ptr = &dolphinSsBackup[0];
	PointerWrap p(&ptr, PointerWrap::MODE_WRITE);
	captureDolphinState(p);
}

void SlippiSavestate::Load(std::vector<PreserveBlock> blocks)
{
	// Back up alarm stuff
	Memory::CopyFromEmu(&alarmPtrs[0], FIRST_ALARM_PTR_ADDR, 8);

	std::unordered_map<u32, std::vector<u8>> alarmData;

	u32 alarmPtr = Memory::Read_U32(FIRST_ALARM_PTR_ADDR);
	if (alarmPtr != origAlarmPtr)
	{
		ERROR_LOG(SLIPPI_ONLINE, "Trying to deal with alarm boundary: %8x -> %8x", origAlarmPtr, alarmPtr);
	}

	while (alarmPtr != 0)
	{
		std::vector<u8> iAlarmData;
		iAlarmData.resize(ALARM_DATA_SIZE);
		Memory::CopyFromEmu(&iAlarmData[0], alarmPtr, ALARM_DATA_SIZE);
		alarmData[alarmPtr] = iAlarmData;

		alarmPtr = Memory::Read_U32(alarmPtr + 0x14);
	}

	// static PreserveBlock stackBlock = {0x804DEC00, 0x10000};
	// blocks.push_back(stackBlock);

	// Always back up this alarm anyway... shouldn't be necessary
	static PreserveBlock readAlarmBlock = {READ_ALARM_ADDR, ALARM_DATA_SIZE};
	blocks.push_back(readAlarmBlock);

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

	for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
	{
		auto size = it->endAddress - it->startAddress;
		Memory::CopyToEmu(it->startAddress, it->data, size);
	}

	// Restore audio
	u8 *ptr = &dolphinSsBackup[0];
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	captureDolphinState(p);

	// Restore
	for (auto it = blocks.begin(); it != blocks.end(); ++it)
	{
		Memory::CopyToEmu(it->address, &preservationMap[*it][0], it->length);
	}

	// Restore alarm stuff
	Memory::CopyToEmu(FIRST_ALARM_PTR_ADDR, &alarmPtrs[0], 8);

	for (auto it = alarmData.begin(); it != alarmData.end(); ++it)
	{
		Memory::CopyToEmu(it->first, &it->second[0], ALARM_DATA_SIZE);
	}
}
