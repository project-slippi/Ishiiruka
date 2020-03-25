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
	const u32 FIRST_ALARM_PTR_ADDR = 0x804D7358;
	const u32 READ_ALARM_ADDR = 0x804a74f0;
	const u32 ALARM_DATA_SIZE = 0x28;

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

	    // https://docs.google.com/spreadsheets/d/1IBeM_YPFEzWAyC0SEz5hbFUi7W9pCAx7QRh9hkEZx_w/edit#gid=702784062
	    //{0x8065CC00, 0x8065DC00, NULL}, // Write MemLog Unknown Section while in game (plus lots of padding)

	    {0x804fec00, 0x80BD5C40, NULL}, // Full Unknown Region
	};

  typedef struct
  {
	  u32 address;
	  u32 value;
  } ssBackupStaticToHeapPtr;

  std::vector<ssBackupStaticToHeapPtr> backupPtrLocs = {
	  //{0x80452d08, 0}, // 80030eb0 (CameraInfo_ExecuteScreenRumble)
	  //{0x80458eb4, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x80458ee0, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458ee4, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458ee8, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458eec, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458ef0, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458ef4, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458ef8, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458efc, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458f00, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458f04, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458f08, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458f0c, 0}, // 8005c15c (EfDataIndexer)
	  //{0x80458f60, 0}, // 8005faac (ShieldGFX_StorePointerToStruct)
	  //{0x80458f68, 0}, // 8005faac (ShieldGFX_StorePointerToStruct)
	  //{0x80458f70, 0}, // 8005faac (ShieldGFX_StorePointerToStruct)
	  //{0x80458f78, 0}, // 8005faac (ShieldGFX_StorePointerToStruct)
	  //{0x80458fa4, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x80459280, 0}, // 800773ac (Hitbox_MeleeAttackLogicOnPlayer)
	  //{0x80459284, 0}, // 800773b0 (Hitbox_MeleeAttackLogicOnPlayer)
	  //{0x80459288, 0}, // 800773b4 (Hitbox_MeleeAttackLogicOnPlayer)
	  //{0x804592a8, 0}, // 800773ac (Hitbox_MeleeAttackLogicOnPlayer)
	  //{0x804592ac, 0}, // 800773b0 (Hitbox_MeleeAttackLogicOnPlayer)
	  //{0x804592b0, 0}, // 800773b4 (Hitbox_MeleeAttackLogicOnPlayer)
	  //{0x804595a0, 0}, // 80077158 (Hitbox_MeleeAttackLogicOnPlayer)
	  //{0x804595a4, 0}, // 8007715c (Hitbox_MeleeAttackLogicOnPlayer)
	  //{0x804595a8, 0}, // 80077160 (Hitbox_MeleeAttackLogicOnPlayer)
	  //{0x8049f034, 0}, // 801cae90 (zz_01cae04_)
	  //{0x804a0be4, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804a0c10, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804a0c3c, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804c0884, 0}, // 8037ad28 (HSD_ObjFree)
	  //{0x804c08dc, 0}, // 8037ad28 (HSD_ObjFree)
	  //{0x804c2314, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804c2340, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804c236c, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804c23c4, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804c2584, 0}, // 8037ad28 (HSD_ObjFree)
	  //{0x804ce390, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804ce3bc, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804d0f64, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804d0f94, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804d10b4, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0xcc006014, 0}, // 80336f80 (Read)
	  //{0xcc008000, 0}, // 8033eeec (GXLoadTexObjPreLoaded)

   // // Not in game, maybe not needed?
	  //{0x803f9e14, 0}, // 802ff10c (zz_02fefac_)
	  //{0x804336a4, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x80452c68, 0}, // 800306c0 (InitalizeCamera)
	  //{0x80453130, 0}, // 80031bcc (SetupPlayerSlot)
	  //{0x80453134, 0}, // 80031c60 (SetupPlayerSlot)
	  //{0x80453fc0, 0}, // 80031bcc (SetupPlayerSlot)
	  //{0x80453fc4, 0}, // 80031c60 (SetupPlayerSlot)
	  //{0x80458e88, 0}, // 8005a7f8 (zz_005a728_)
	  //{0x80458e8c, 0}, // 8005a9cc (zz_005a728_)
	  //{0x80458e90, 0}, // 8005a96c (zz_005a728_)
	  //{0x80458e94, 0}, // 8005ab38 (zz_005a728_)
	  //{0x80458e98, 0}, // 8005b310 (zz_005b200_)
	  //{0x80458ea0, 0}, // 8005afec (zz_005ae1c_)
	  //{0x80458fd4, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x80459000, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x8045902c, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x80459058, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x80459084, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x804590b0, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
	  //{0x80459a68, 0}, // 800c0710 (StoreWhispyWindFunction)
	  //{0x804a0fd8, 0}, // 802f39c8 (InitHUD)
	  //{0x804a754c, 0}, // 803370d0 (DVDLowRead)
	  //{0x804c07e8, 0}, // 80361a40 (HSD_SetMaterialColor)
	  //{0x804c25dc, 0}, // 8037ab98 (HSD_ObjAllocAddFree)
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

	std::vector<u8> alarmPtrs;

	u32 origAlarmPtr;

	void getDolphinState(PointerWrap &p);
};
