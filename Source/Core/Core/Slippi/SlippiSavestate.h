#pragma once

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include <unordered_map>

class PointerWrap;

class SlippiSavestate
{
  public:
	struct PreserveBlock
	{
		u32 address;
		u32 length;

		bool operator==(const PreserveBlock &p) const { return address == p.address && length == p.length; }
	};

	SlippiSavestate();
	~SlippiSavestate();

	void Capture();
	void Load(std::vector<PreserveBlock> blocks);

  private:
	typedef struct
	{
		u32 startAddress;
		u32 endAddress;
		u8 *data;
	} ssBackupLoc;

	// These are the game locations to back up and restore
	std::vector<ssBackupLoc> backupLocs = {
	    {0x80bd5c40, 0x811AD5A0, NULL}, // Heap
	    {0x80005520, 0x80005940, NULL}, // Data Sections 0 and 1
	    {0x803b7240, 0x804DEC00, NULL}, // Data Sections 2-7 and in between sections including BSS

	    // Vin suggested preserving 804b89e0 (7E00) and 804b09e0 (3000)
	    //{0x803b7240, 0x804b09e0, NULL},
	    //{0x804B39E0, 0x804b89e0, NULL},
	    //{0x804C07E0, 0x804DEC00, NULL},

	    // https://docs.google.com/spreadsheets/d/1IBeM_YPFEzWAyC0SEz5hbFUi7W9pCAx7QRh9hkEZx_w/edit#gid=702784062
	    //{0x8065CC00, 0x1000, NULL}, // Write MemLog Unknown Section while in game (plus lots of padding)

	    {0x804fec00, 0x80BD5C40, NULL}, // Full Unknown Region
	    //{0x811AD5A0, 0x64B520, NULL}, // Unknown Region 2
	};

	struct preserve_hash_fn
	{
		std::size_t operator()(const PreserveBlock &node) const
		{
			return node.address ^ node.length; // TODO: This is probably a bad hash
		}
	};

	std::unordered_map<PreserveBlock, std::vector<u8>, preserve_hash_fn> preservationMap;

	std::vector<u8> dolphinSsBackup;

  void captureDolphinState(PointerWrap &p);
};
