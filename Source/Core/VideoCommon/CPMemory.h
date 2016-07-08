// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

// Vertex array numbers
enum
{
	ARRAY_POSITION = 0,
	ARRAY_NORMAL = 1,
	ARRAY_COLOR = 2,
	ARRAY_COLOR2 = 3,
	ARRAY_TEXCOORD0 = 4,
};

// Vertex components
enum EVTXComponentType
{
	NOT_PRESENT = 0,
	DIRECT = 1,
	INDEX8 = 2,
	INDEX16 = 3,
};

enum EVTXComponentFormat
{
	FORMAT_UBYTE = 0,	// 2 Cmp
	FORMAT_BYTE = 1,	// 3 Cmp
	FORMAT_USHORT = 2,
	FORMAT_SHORT = 3,
	FORMAT_FLOAT = 4,
};

enum EVTXColorFormat
{
	FORMAT_16B_565 = 0,	// NA
	FORMAT_24B_888 = 1,
	FORMAT_32B_888x = 2,
	FORMAT_16B_4444 = 3,
	FORMAT_24B_6666 = 4,
	FORMAT_32B_8888 = 5,
};

enum
{
	VAT_0_FRACBITS = 0x3e0001f0,
	VAT_0_POSBITS = 0x000001ff,
	VAT_0_NRMBITS = 0x80001E00,
	VAT_0_COL0BITS = 0x0001E000,
	VAT_0_COL1BITS = 0x001E0000,
	VAT_0_TEX0BITS = 0x3fE00000,
	VAT_1_FRACBITS = 0x07c3e1f0,
	VAT_1_TEX1BITS = 0x000001ff,
	VAT_1_TEX2BITS = 0x0003fe00,
	VAT_1_TEX3BITS = 0x07fc0000,
	VAT_1_TEX4BITS = 0x78000000,
	VAT_2_FRACBITS = 0xf87c3e1f,
	VAT_2_TEX4BITS = 0x0000001f,
	VAT_2_TEX5BITS = 0x00003fe0,
	VAT_2_TEX6BITS = 0x007fc000,
	VAT_2_TEX7BITS = 0xff800000,
};

#pragma pack(4)
union TVtxDesc
{
	u64 Hex;
	struct
	{
		// 0: not present
		// 1: present
		u64 PosMatIdx : 1;
		u64 Tex0MatIdx : 1;
		u64 Tex1MatIdx : 1;
		u64 Tex2MatIdx : 1;
		u64 Tex3MatIdx : 1;
		u64 Tex4MatIdx : 1;
		u64 Tex5MatIdx : 1;
		u64 Tex6MatIdx : 1;
		u64 Tex7MatIdx : 1;

		// 00: not present 
		// 01: direct 
		// 10: 8 bit index 
		// 11: 16 bit index
		u64 Position : 2;
		u64 Normal : 2;
		u64 Color0 : 2;
		u64 Color1 : 2;
		u64 Tex0Coord : 2;
		u64 Tex1Coord : 2;
		u64 Tex2Coord : 2;
		u64 Tex3Coord : 2;
		u64 Tex4Coord : 2;
		u64 Tex5Coord : 2;
		u64 Tex6Coord : 2;
		u64 Tex7Coord : 2;
		u64 : 31;
	};

	struct
	{
		u32 Hex0, Hex1;
	};

	// Easily index into the Position..Tex7Coord fields.
	u32 GetVertexArrayStatus(int idx) const
	{
		return (Hex >> (9 + idx * 2)) & 0x3;
	}

};

union UVAT_group0
{
	u32 Hex;
	struct
	{
		// 0:8
		u32 PosElements : 1;
		u32 PosFormat : 3;
		u32 PosFrac : 5;
		// 9:12
		u32 NormalElements : 1;
		u32 NormalFormat : 3;
		// 13:16
		u32 Color0Elements : 1;
		u32 Color0Comp : 3;
		// 17:20
		u32 Color1Elements : 1;
		u32 Color1Comp : 3;
		// 21:29
		u32 Tex0CoordElements : 1;
		u32 Tex0CoordFormat : 3;
		u32 Tex0Frac : 5;
		// 30:31
		u32 ByteDequant : 1;
		u32 NormalIndex3 : 1;
	};
};

union UVAT_group1
{
	u32 Hex;
	struct
	{
		// 0:8
		u32 Tex1CoordElements : 1;
		u32 Tex1CoordFormat : 3;
		u32 Tex1Frac : 5;
		// 9:17
		u32 Tex2CoordElements : 1;
		u32 Tex2CoordFormat : 3;
		u32 Tex2Frac : 5;
		// 18:26
		u32 Tex3CoordElements : 1;
		u32 Tex3CoordFormat : 3;
		u32 Tex3Frac : 5;
		// 27:30
		u32 Tex4CoordElements : 1;
		u32 Tex4CoordFormat : 3;
		// 
		u32 : 1;
	};
};

union UVAT_group2
{
	u32 Hex;
	struct
	{
		// 0:4
		u32 Tex4Frac : 5;
		// 5:13
		u32 Tex5CoordElements : 1;
		u32 Tex5CoordFormat : 3;
		u32 Tex5Frac : 5;
		// 14:22
		u32 Tex6CoordElements : 1;
		u32 Tex6CoordFormat : 3;
		u32 Tex6Frac : 5;
		// 23:31
		u32 Tex7CoordElements : 1;
		u32 Tex7CoordFormat : 3;
		u32 Tex7Frac : 5;
	};
};

struct ColorAttr
{
	u8 Elements;
	u8 Comp;
};

struct TexAttr
{
	u8 Elements;
	u8 Format;
	u8 Frac;
};

struct TVtxAttr
{
	u8 PosElements;
	u8 PosFormat;
	u8 PosFrac;
	u8 NormalElements;
	u8 NormalFormat;
	ColorAttr color[2];
	TexAttr texCoord[8];
	bool ByteDequant;
	u8 NormalIndex3;
};

// Matrix indices
union TMatrixIndexA
{
	struct
	{
		u32 PosNormalMtxIdx : 6;
		u32 Tex0MtxIdx : 6;
		u32 Tex1MtxIdx : 6;
		u32 Tex2MtxIdx : 6;
		u32 Tex3MtxIdx : 6;
	};
	struct
	{
		u32 Hex : 30;
		u32 unused : 2;
	};
};

union TMatrixIndexB
{
	struct
	{
		u32 Tex4MtxIdx : 6;
		u32 Tex5MtxIdx : 6;
		u32 Tex6MtxIdx : 6;
		u32 Tex7MtxIdx : 6;
	};
	struct
	{
		u32 Hex : 24;
		u32 unused : 8;
	};
};

#pragma pack()

struct VAT
{
	UVAT_group0 g0;
	UVAT_group1 g1;
	UVAT_group2 g2;
};

class VertexLoaderBase;

// STATE_TO_SAVE
struct CPState final
{
	u32 array_bases[16];
	u32 array_strides[16];
	TMatrixIndexA matrix_index_a;
	TMatrixIndexB matrix_index_b;
	TVtxDesc vtx_desc;
	// Most games only use the first VtxAttr and simply reconfigure it all the time as needed.
	VAT vtx_attr[8];

	// Attributes that actually belong to VertexLoaderManager:
	u32 attr_dirty;
	bool bases_dirty;
	VertexLoaderBase* vertex_loaders[8];
	int last_id;
};

extern CPState g_main_cp_state;
extern CPState g_preprocess_cp_state;
extern u8 *cached_arraybases[16];
class PointerWrap;

// Might move this into its own file later.
template <bool is_preprocess> void LoadCPReg(u32 SubCmd, u32 Value);

// Fills memory with data from CP regs
void FillCPMemoryArray(u32 *memory);

void DoCPState(PointerWrap& p);
void CopyPreprocessCPStateFromMain();