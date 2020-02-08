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
	  u32 address;
	  u32 size;
	  u8 *data;
  } ssBackupLoc;

  // These are the game locations to back up and restore
  std::vector<ssBackupLoc> backupLocs = {
	  {0x80bd5c40, 0x5D7960, NULL}, // Heap
	  {0x80005520, 0x420, NULL},    // Data Sections 0 and 1
	  {0x803b7240, 0x1279C0, NULL}, // Data Sections 2-7 and in between sections including BSS

	  // https://docs.google.com/spreadsheets/d/1IBeM_YPFEzWAyC0SEz5hbFUi7W9pCAx7QRh9hkEZx_w/edit#gid=702784062
	  {0x8065CC00, 0x1000, NULL}, // Write MemLog Unknown Section while in game (plus lots of padding)
  };

  struct preserve_hash_fn
  {
	  std::size_t operator()(const PreserveBlock &node) const
	  {
		  return node.address ^ node.length; // TODO: This is probably a bad hash
	  }
  };

  std::unordered_map<PreserveBlock, std::vector<u8>, preserve_hash_fn> preservationMap;

  std::vector<u8> audioBackup;
};

