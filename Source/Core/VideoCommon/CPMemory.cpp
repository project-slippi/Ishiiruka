// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#include "Core/HW/Memmap.h"
#include "Common/Common.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/VertexShaderManager.h"

// CP state
u8 *cached_arraybases[16];

CPState g_main_cp_state;

void DoCPState(PointerWrap& p)
{
	p.DoArray(g_main_cp_state.array_bases);
	p.DoArray(g_main_cp_state.array_strides);
	p.Do(g_main_cp_state.matrix_index_a);
	p.Do(g_main_cp_state.matrix_index_b);
	p.Do(g_main_cp_state.vtx_desc.Hex);
	p.DoArray(g_main_cp_state.vtx_attr);
	p.DoMarker("CP Memory");
	if (p.mode == PointerWrap::MODE_READ)
	{
		g_main_cp_state.bases_dirty = true;
	}
}

void MarkAllAttrDirty()
{
	g_main_cp_state.attr_dirty = 0xff;
	g_main_cp_state.bases_dirty = true;
}

void LoadCPReg(u32 sub_cmd, u32 value)
{
	switch (sub_cmd & 0xF0)
	{
	case 0x30:
		VertexShaderManager::SetTexMatrixChangedA(value);
		break;

	case 0x40:
		VertexShaderManager::SetTexMatrixChangedB(value);
		break;

	case 0x50:
		g_main_cp_state.vtx_desc.Hex &= ~0x1FFFF;  // keep the Upper bits
		g_main_cp_state.vtx_desc.Hex |= value;
		g_main_cp_state.attr_dirty = 0xFF;
		g_main_cp_state.bases_dirty = true;
		break;

	case 0x60:
		g_main_cp_state.vtx_desc.Hex &= 0x1FFFF;  // keep the lower 17Bits
		g_main_cp_state.vtx_desc.Hex |= (u64)value << 17;
		g_main_cp_state.attr_dirty = 0xFF;
		g_main_cp_state.bases_dirty = true;
		break;

	case 0x70:
		_assert_((sub_cmd & 0x0F) < 8);
		g_main_cp_state.vtx_attr[sub_cmd & 7].g0.Hex = value;
		g_main_cp_state.attr_dirty |= 1 << (sub_cmd & 7);
		break;

	case 0x80:
		_assert_((sub_cmd & 0x0F) < 8);
		g_main_cp_state.vtx_attr[sub_cmd & 7].g1.Hex = value;
		g_main_cp_state.attr_dirty |= 1 << (sub_cmd & 7);
		break;

	case 0x90:
		_assert_((sub_cmd & 0x0F) < 8);
		g_main_cp_state.vtx_attr[sub_cmd & 7].g2.Hex = value;
		g_main_cp_state.attr_dirty |= 1 << (sub_cmd & 7);
		break;

		// Pointers to vertex arrays in GC RAM
	case 0xA0:
		g_main_cp_state.array_bases[sub_cmd & 0xF] = value;
		g_main_cp_state.bases_dirty = true;
		break;

	case 0xB0:
		g_main_cp_state.array_strides[sub_cmd & 0xF] = value & 0xFF;
		break;
	}
}

void FillCPMemoryArray(u32 *memory)
{
	memory[0x30] = g_main_cp_state.matrix_index_a.Hex;
	memory[0x40] = g_main_cp_state.matrix_index_b.Hex;
	memory[0x50] = (u32)g_main_cp_state.vtx_desc.Hex;
	memory[0x60] = (u32)(g_main_cp_state.vtx_desc.Hex >> 17);

	for (int i = 0; i < 8; ++i)
	{
		memory[0x70 + i] = g_main_cp_state.vtx_attr[i].g0.Hex;
		memory[0x80 + i] = g_main_cp_state.vtx_attr[i].g1.Hex;
		memory[0x90 + i] = g_main_cp_state.vtx_attr[i].g2.Hex;
	}

	for (int i = 0; i < 16; ++i)
	{
		memory[0xA0 + i] = g_main_cp_state.array_bases[i];
		memory[0xB0 + i] = g_main_cp_state.array_strides[i];
	}
}