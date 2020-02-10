#include "SlippiSavestate.h"
#include "Common/MemoryUtil.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/EXI.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/SI.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/HW.h"
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
 // VideoInterface::DoState(p);
	//p.DoMarker("VideoInterface");
	//SerialInterface::DoState(p);
	//p.DoMarker("SerialInterface");
	//ProcessorInterface::DoState(p);
	//p.DoMarker("ProcessorInterface");
	//DSP::DoState(p);
	//p.DoMarker("DSP");
	//DVDInterface::DoState(p);
	//p.DoMarker("DVDInterface");
	//GPFifo::DoState(p);
	//p.DoMarker("GPFifo");
	ExpansionInterface::DoState(p);
	p.DoMarker("ExpansionInterface");
	AudioInterface::DoState(p);
	p.DoMarker("AudioInterface");
}

void SlippiSavestate::Capture()
{
	// First copy memory
	for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
	{
		auto size = it->endAddress - it->startAddress;
		Memory::CopyFromEmu(it->data, it->startAddress, size);
	}

	// Second copy sound
	u8 *ptr = &dolphinSsBackup[0];
	PointerWrap p(&ptr, PointerWrap::MODE_WRITE);
	captureDolphinState(p);
}

void SlippiSavestate::Load(std::vector<PreserveBlock> blocks)
{
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

	// Restore
	for (auto it = blocks.begin(); it != blocks.end(); ++it)
	{
		Memory::CopyToEmu(it->address, &preservationMap[*it][0], it->length);
	}

	// Restore audio
	u8 *ptr = &dolphinSsBackup[0];
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	captureDolphinState(p);
}
