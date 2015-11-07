// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/HW/Memmap.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"
#include "Common/CPUDetect.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/OpcodeDecodingSC.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/VertexLoader_Normal.h"
#include "VideoCommon/VertexLoader_TextCoord.h"
#include "VideoCommon/VertexLoader_Position.h"
#include "VideoCommon/VertexManagerBase.h"


DataReader g_VideoDataSC;
bool shaderGenDirty = false;

template <int count>
void ReadU32xnSC(u32 *bufx16)
{
	g_VideoDataSC.ReadU32xN<count>(bufx16);
}

DataReadU32xNfunc DataReadU32xFuncsSC[16] = {
	ReadU32xnSC<1>,
	ReadU32xnSC<2>,
	ReadU32xnSC<3>,
	ReadU32xnSC<4>,
	ReadU32xnSC<5>,
	ReadU32xnSC<6>,
	ReadU32xnSC<7>,
	ReadU32xnSC<8>,
	ReadU32xnSC<9>,
	ReadU32xnSC<10>,
	ReadU32xnSC<11>,
	ReadU32xnSC<12>,
	ReadU32xnSC<13>,
	ReadU32xnSC<14>,
	ReadU32xnSC<15>,
	ReadU32xnSC<16>
};


extern u8* GetVideoBufferStartPtrSC();
extern u8* GetVideoBufferEndPtrSC();

static void DecodeSemiNopSC();

// Bp Register
BPMemory bpmemSC;

// XF Register
XFMemory xfmemSC;

// CP Register
TVtxDesc g_VtxDescSC;
VAT g_VtxAttrSC[8];
int s_vtxattr_dirty;
u32 arraybasesSC[16];
u32 arraystridesSC[16];

void LoadBPRegSC(u32 value0)
{
	//handle the mask register
	int opcode = value0 >> 24;
	int oldval = ((u32*)&bpmemSC)[opcode];
	int newval = (oldval & ~bpmemSC.bpMask) | (value0 & bpmemSC.bpMask);	
	//reset the mask register
	if (opcode != 0xFE)
		bpmemSC.bpMask = 0xFFFFFF;

	((u32*)&bpmemSC)[opcode] = newval;
}

void LoadXFRegSC(u32 transferSize, u32 baseAddress)
{
	// do not allow writes past registers
	if (baseAddress + transferSize > 0x1058)
	{
		if (baseAddress >= 0x1058)
			transferSize = 0;
		else
			transferSize = 0x1058 - baseAddress;
	}

	// write to XF mem
	if (baseAddress < 0x1000 && transferSize > 0)
	{
		u32 end = baseAddress + transferSize;

		u32 xfMemBase = baseAddress;
		u32 xfMemTransferSize = transferSize;

		if (end >= 0x1000)
		{
			xfMemTransferSize = 0x1000 - baseAddress;

			baseAddress = 0x1000;
			transferSize = end - 0x1000;
		}
		else
		{
			transferSize = 0;
		}

		DataReadU32xFuncsSC[xfMemTransferSize - 1](&((u32*)&xfmemSC)[xfMemBase]);
	}

	// write to XF regs
	if (transferSize > 0)
	{
		DataReadU32xFuncsSC[transferSize - 1](&((u32*)&xfmemSC)[baseAddress]);
	}
}

void LoadIndexedXFSC(u32 val, int refarray)
{
	int index = val >> 16;
	int address = val & 0xFFF; // check mask
	int size = ((val >> 12) & 0xF) + 1;
	//load stuff from array to address in xf mem

	u32* currData = ((u32*)&xfmemSC) + address;
	u32* newData = (u32*)Memory::GetPointer(arraybasesSC[refarray] + arraystridesSC[refarray] * index);
	for (int i = 0; i < size; ++i)
		currData[i] = Common::swap32(newData[i]);
}

void LoadCPRegSC(u32 sub_cmd, u32 value)
{
	switch (sub_cmd & 0xF0)
	{
	case 0x30:
	case 0x40:
		break;
	case 0x50:
		g_VtxDescSC.Hex &= ~0x1FFFF;  // keep the Upper bits
		g_VtxDescSC.Hex |= value;
		s_vtxattr_dirty = 0xFF;
		break;

	case 0x60:
		g_VtxDescSC.Hex &= 0x1FFFF;  // keep the lower 17Bits
		g_VtxDescSC.Hex |= (u64)value << 17;
		s_vtxattr_dirty = 0xFF;
		break;

	case 0x70:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttrSC[sub_cmd & 7].g0.Hex = value;
		s_vtxattr_dirty |= 1 << (sub_cmd & 7);
		break;

	case 0x80:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttrSC[sub_cmd & 7].g1.Hex = value;
		s_vtxattr_dirty |= 1 << (sub_cmd & 7);
		break;

	case 0x90:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttrSC[sub_cmd & 7].g2.Hex = value;
		s_vtxattr_dirty |= 1 << (sub_cmd & 7);
		break;

		// Pointers to vertex arrays in GC RAM
	case 0xA0:
		arraybasesSC[sub_cmd & 0xF] = value;
		//cached_arraybasesSC[sub_cmd & 0xF] = Memory::GetPointer(value);
		break;

	case 0xB0:
		arraystridesSC[sub_cmd & 0xF] = value & 0xFF;
		break;
	}
}

static void InterpretDisplayList(u32 address, u32 size);

template<bool sizeCheck>
inline bool DecodeSC(const u8* end)
{
	const u8 *opcodeStart = g_VideoDataSC.GetReadPosition();
	if (opcodeStart == end)
		return false;

	u8 cmd_byte = g_VideoDataSC.Read<u8>();
	size_t distance = (size_t)(end - g_VideoDataSC.GetReadPosition());

	switch (cmd_byte)
	{
	case GX_NOP:
	case GX_CMD_UNKNOWN_METRICS: // zelda 4 swords calls it and checks the metrics registers after that
	case GX_CMD_INVL_VC: // Invalidate Vertex Cache	
		break;
	case GX_LOAD_CP_REG: //0x08
	{
		if (sizeCheck && distance < GX_LOAD_CP_REG_SIZE)
			return false;
		u8 sub_cmd = g_VideoDataSC.Read<u8>();
		u32 value = g_VideoDataSC.Read<u32>();
		LoadCPRegSC(sub_cmd, value); 
		shaderGenDirty = true;
	}
		break;
	case GX_LOAD_XF_REG:
	{
		if (sizeCheck && distance < GX_LOAD_XF_REG_SIZE)
			return false;
		u32 Cmd2 = g_VideoDataSC.Read<u32>();
		distance -= GX_LOAD_XF_REG_SIZE;
		int transfer_size = ((Cmd2 >> 16) & 15) + 1;
		if (sizeCheck && distance < (transfer_size * sizeof(u32)))
			return false;
		u32 xf_address = Cmd2 & 0xFFFF;
		LoadXFRegSC(transfer_size, xf_address);
		shaderGenDirty = true;
	}
		break;
	case GX_LOAD_INDX_A: //used for position matrices
	{	
		if (sizeCheck && distance < GX_LOAD_INDX_A_SIZE)
			return false;
		LoadIndexedXFSC(g_VideoDataSC.Read<u32>(), 0xC);
		shaderGenDirty = true;
	}
		break;
	case GX_LOAD_INDX_B: //used for normal matrices
	{
		if (sizeCheck && distance < GX_LOAD_INDX_B_SIZE)
			return false;
		LoadIndexedXFSC(g_VideoDataSC.Read<u32>(), 0xD);
		shaderGenDirty = true;
	}
		break;
	case GX_LOAD_INDX_C: //used for postmatrices
	{
		if (sizeCheck && distance < GX_LOAD_INDX_C_SIZE)
			return false;
		LoadIndexedXFSC(g_VideoDataSC.Read<u32>(), 0xE);
		shaderGenDirty = true;
	}
		break;
	case GX_LOAD_INDX_D: //used for lights
	{
		if (sizeCheck && distance < GX_LOAD_INDX_D_SIZE)
			return false;
		LoadIndexedXFSC(g_VideoDataSC.Read<u32>(), 0xF);
		shaderGenDirty = true;
	}
		break;
	case GX_CMD_CALL_DL:
	{
		if (sizeCheck && distance < GX_CMD_CALL_DL_SIZE)
			return false;
		u32 address = g_VideoDataSC.Read<u32>();
		u32 count = g_VideoDataSC.Read<u32>();
		InterpretDisplayList(address, count);
	}
		break;	
	case GX_LOAD_BP_REG: //0x61
	{
		if (sizeCheck && distance < GX_LOAD_BP_REG_SIZE)
			return false;
		u32 bp_cmd = g_VideoDataSC.Read<u32>();
		LoadBPRegSC(bp_cmd);
		shaderGenDirty = true;
	}
		break;
		// draw primitives 
	default:
		if ((cmd_byte & GX_DRAW_PRIMITIVES) == 0x80)
		{
			if (sizeCheck && distance < GX_DRAW_PRIMITIVES_SIZE)
				return false;
			distance -= GX_DRAW_PRIMITIVES_SIZE;
			u16 numVertices = g_VideoDataSC.Read<u16>();
			u32 vertexSize = 0;
			u32 components = 0;
			VertexLoaderParameters parameters;
			parameters.vtx_attr_group = cmd_byte & GX_VAT_MASK;
			parameters.needloaderrefresh = (s_vtxattr_dirty & (1 << parameters.vtx_attr_group)) != 0;
			parameters.VtxDesc = &g_VtxDescSC;
			parameters.VtxAttr = &g_VtxAttrSC[parameters.vtx_attr_group];
			VertexLoaderManager::GetVertexSizeAndComponents(parameters, vertexSize, components);
			s_vtxattr_dirty &= ~(1 << parameters.vtx_attr_group);
			vertexSize *= numVertices;
			if (shaderGenDirty)
			{
				g_vertex_manager->PrepareShaders(VertexManagerBase::GetPrimitiveType((cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT),
					components, 
					xfmemSC, 
					bpmemSC, 
					false);
				shaderGenDirty = false;
			}
			if (sizeCheck &&  distance < vertexSize)
				return false;
			g_VideoDataSC.ReadSkip(vertexSize);
		}
		else
		{
			g_VideoDataSC.SetReadPosition(end);
			PanicAlert("Unknown Opcode on predictive fifo.");			
		}
		break;
	}
	return true;
}


static void InterpretDisplayList(u32 address, u32 size)
{
	const u8* old_pVideoData = g_VideoDataSC.GetReadPosition();
	const u8* startAddress = Memory::GetPointer(address);

	// Avoid the crash if Memory::GetPointer failed ..
	if (startAddress != nullptr)
	{
		g_VideoDataSC.SetReadPosition(startAddress);
		const u8 *end = startAddress + size;
		while (g_VideoDataSC.GetReadPosition() < end)
		{
			DecodeSC<false>(end);
		}
	}
	// reset to the old pointer
	g_VideoDataSC.SetReadPosition(old_pVideoData);
}

void OpcodeDecoderSC_Init()
{
	g_VideoDataSC.SetReadPosition(GetVideoBufferStartPtrSC());

	//bpmem Init
	memset(&bpmemSC, 0, sizeof(bpmemSC));
	bpmemSC.bpMask = 0xFFFFFF;

	memset(arraybasesSC, 0, sizeof(g_main_cp_state.array_bases));
	memset(arraystridesSC, 0, sizeof(g_main_cp_state.array_strides));
	//memset(&g_main_cp_state.matrix_index_a, 0, sizeof(g_main_cp_state.matrix_index_a));
	//memset(&g_main_cp_state.matrix_index_b, 0, sizeof(g_main_cp_state.matrix_index_b));
	memset(&g_VtxDescSC, 0, sizeof(g_VtxDescSC));
	memset(g_VtxAttrSC, 0, sizeof(g_VtxAttrSC));
	s_vtxattr_dirty = 0xFF;
	shaderGenDirty = true;

	VertexLoader_Normal::Init();
	VertexLoader_Position::Init();
	VertexLoader_TextCoord::Init();
}


void OpcodeDecoderSC_Shutdown()
{

}


void OpcodeDecoderSC_Run(const u8* end)
{
	while (true)
	{
		const u8* old = g_VideoDataSC.GetReadPosition();
		if (!DecodeSC<true>(end))
		{
			g_VideoDataSC.SetReadPosition(old);
			break;
		}
	}
}


