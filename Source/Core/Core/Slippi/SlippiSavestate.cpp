#include "SlippiSavestate.h"
#include "Common/MemoryUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/AudioInterface.h"
#include <vector>

SlippiSavestate::SlippiSavestate()
{
	for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
	{
		it->data = static_cast<u8 *>(Common::AllocateAlignedMemory(it->size, 64));
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

	audioBackup.resize(40);
}

SlippiSavestate::~SlippiSavestate()
{
	for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
	{
		Common::FreeAlignedMemory(it->data);
	}
}

void SlippiSavestate::Capture()
{
	// First copy memory
	for (auto it = backupLocs.begin(); it != backupLocs.end(); ++it)
	{
	  Memory::CopyFromEmu(it->data, it->address, it->size);
	}

	// Second copy sound
	u8 *ptr = &audioBackup[0];
	PointerWrap p(&ptr, PointerWrap::MODE_WRITE);
	AudioInterface::DoState(p);
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
		Memory::CopyToEmu(it->address, it->data, it->size);
	}

	// Restore
	for (auto it = blocks.begin(); it != blocks.end(); ++it)
	{
		Memory::CopyToEmu(it->address, &preservationMap[*it][0], it->length);
	}

	// Restore audio
	u8 *ptr = &audioBackup[0];
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	AudioInterface::DoState(p);
}
