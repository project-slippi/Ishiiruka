// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

//DL facts:
//	Ikaruga uses (nearly) NO display lists!
//  Zelda WW uses TONS of display lists
//  Zelda TP uses almost 100% display lists except menus (we like this!)
//  Super Mario Galaxy has nearly all geometry and more than half of the state in DLs (great!)

// Note that it IS NOT GENERALLY POSSIBLE to precompile display lists! You can compile them as they are
// while interpreting them, and hope that the vertex format doesn't change, though, if you do it right
// when they are called. The reason is that the vertex format affects the sizes of the vertices.
#include "Common/CPUDetect.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OpcodeDecoding.h"
#ifdef _WIN32
#include "VideoCommon/OpenCL.h"
#endif
#include "VideoCommon/OpenCL/OCLTextureDecoder.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

bool g_bRecordFifoData = false;
static bool s_bFifoErrorSeen = false;

template <int count>
void ReadU32xn(u32 *bufx16)
{
	g_VideoData.ReadU32xN<count>(bufx16);
}

static u32 InterpretDisplayList(u32 address, u32 size);
static void UnknownOpcode(u8 cmd_byte, const void *buffer);

template<bool sizeCheck>
inline u32 Decode(const u8* end)
{
	const u8 *opcodeStart = g_VideoData.GetReadPosition();
	if (opcodeStart == end)
		return 0;

	u8 cmd_byte = g_VideoData.Read<u8>();
	size_t distance = (size_t)(end - g_VideoData.GetReadPosition());
	u32 cycles;

	switch (cmd_byte)
	{
	case GX_NOP:
	{
		cycles = GX_NOP_CYCLES; // Hm, this means that we scan over nop streams pretty slowly...
	}
	break;
	case GX_UNKNOWN_RESET:
	{
		cycles = GX_NOP_CYCLES; // Datel software uses this command
		DEBUG_LOG(VIDEO, "GX Reset?: %08x", cmd_byte);		
	}
	break;
	case GX_LOAD_CP_REG:
	{
		if (sizeCheck && distance < GX_LOAD_CP_REG_SIZE)
			return 0;
		cycles = GX_LOAD_CP_REG_CYCLES;
		u8 sub_cmd = g_VideoData.Read<u8>();
		u32 value = g_VideoData.Read<u32>();
		LoadCPReg(sub_cmd, value);
		INCSTAT(stats.thisFrame.numCPLoads);
	}
	break;
	case GX_LOAD_XF_REG:
	{
		if (sizeCheck && distance < GX_LOAD_XF_REG_SIZE)
			return 0;
		u32 Cmd2 = g_VideoData.Read<u32>();
		distance -= GX_LOAD_XF_REG_SIZE;
		int transfer_size = ((Cmd2 >> 16) & 15) + 1;
		if (sizeCheck && (distance < (transfer_size * sizeof(u32))))
			return 0;
		cycles = GX_LOAD_XF_REG_BASE_CYCLES + GX_LOAD_XF_REG_TRANSFER_CYCLES * transfer_size;
		u32 xf_address = Cmd2 & 0xFFFF;
		LoadXFReg(transfer_size, xf_address);

		INCSTAT(stats.thisFrame.numXFLoads);
	}
	break;
	case GX_LOAD_INDX_A: //used for position matrices
	{	
		if (sizeCheck && distance < GX_LOAD_INDX_A_SIZE)
			return 0;
		cycles = GX_LOAD_INDX_A_CYCLES;
		LoadIndexedXF(g_VideoData.Read<u32>(), 0xC);
	}
	break;
	case GX_LOAD_INDX_B: //used for normal matrices
	{
		if (sizeCheck && distance < GX_LOAD_INDX_B_SIZE)
			return 0;
		cycles = GX_LOAD_INDX_B_CYCLES;
		LoadIndexedXF(g_VideoData.Read<u32>(), 0xD);
	}
	break;
	case GX_LOAD_INDX_C: //used for postmatrices
	{
		if (sizeCheck && distance < GX_LOAD_INDX_C_SIZE)
			return 0;
		cycles = GX_LOAD_INDX_C_CYCLES;
		LoadIndexedXF(g_VideoData.Read<u32>(), 0xE);
	}
	break;
	case GX_LOAD_INDX_D: //used for lights
	{
		if (sizeCheck && distance < GX_LOAD_INDX_D_SIZE)
			return 0;
		cycles = GX_LOAD_INDX_D_CYCLES;
		LoadIndexedXF(g_VideoData.Read<u32>(), 0xF);
	}
	break;
	case GX_CMD_CALL_DL:
	{
		if (sizeCheck && distance < GX_CMD_CALL_DL_SIZE)
			return 0;
		u32 address = g_VideoData.Read<u32>();
		u32 count = g_VideoData.Read<u32>();
		cycles = GX_CMD_CALL_DL_BASE_CYCLES + InterpretDisplayList(address, count);
	}
	break;
	case GX_CMD_UNKNOWN_METRICS: // zelda 4 swords calls it and checks the metrics registers after that
	{	
		cycles = GX_CMD_UNKNOWN_METRICS_CYCLES;
		DEBUG_LOG(VIDEO, "GX 0x44: %08x", cmd_byte);
	}
	break;
	case GX_CMD_INVL_VC: // Invalidate Vertex Cache	
	{
		cycles = GX_CMD_INVL_VC_CYCLES;
		DEBUG_LOG(VIDEO, "Invalidate (vertex cache?)");
	}
	break;
	case GX_LOAD_BP_REG:
	{
		if (sizeCheck && distance < GX_LOAD_BP_REG_SIZE)
			return 0;
		cycles = GX_LOAD_BP_REG_CYCLES;
		u32 bp_cmd = g_VideoData.Read<u32>();
		LoadBPReg(bp_cmd);
		INCSTAT(stats.thisFrame.numBPLoads);
	}
	break;
		// draw primitives 
	default:
		if ((cmd_byte & GX_DRAW_PRIMITIVES) == 0x80)
		{
			// load vertices
			if (sizeCheck && distance < GX_DRAW_PRIMITIVES_SIZE)
				return 0;
			
			u32 count = g_VideoData.Read<u16>();
			distance -= GX_DRAW_PRIMITIVES_SIZE;
			if (count)
			{
				VertexLoaderParameters parameters;
				parameters.count = count;				
				parameters.buf_size = distance;
				parameters.primitive = (cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT;
				parameters.vtx_attr_group = cmd_byte & GX_VAT_MASK;
				parameters.needloaderrefresh = (g_main_cp_state.attr_dirty & (1 << parameters.vtx_attr_group)) != 0;
				parameters.skip_draw = g_bSkipCurrentFrame;
				parameters.VtxDesc = &g_main_cp_state.vtx_desc;
				parameters.VtxAttr = &g_main_cp_state.vtx_attr[parameters.vtx_attr_group];
				parameters.source = g_VideoData.GetReadPosition();
				g_main_cp_state.attr_dirty &= ~(1 << parameters.vtx_attr_group);
				u32 readsize = 0;
				u32 writesize = 0;
				if (VertexLoaderManager::ConvertVertices(parameters, readsize, writesize))
				{
					cycles = GX_NOP_CYCLES + GX_DRAW_PRIMITIVES_CYCLES * parameters.count;
					g_VideoData.ReadSkip(readsize);
					VertexManagerBase::s_pCurBufferPointer += writesize;
				}
				else
				{
					return 0;
				}
			}
			else
			{
				cycles = GX_NOP_CYCLES;
			}
		}
		else
		{
			if (!s_bFifoErrorSeen)
				UnknownOpcode(cmd_byte, opcodeStart);
			ERROR_LOG(VIDEO, "FIFO: Unknown Opcode(0x%02x @ %p)", cmd_byte, opcodeStart);
			s_bFifoErrorSeen = true;
			g_VideoData.SetReadPosition(end);
			cycles = 1;
		}
		break;
	}

	// Display lists get added directly into the FIFO stream
	if (g_bRecordFifoData && cmd_byte != GX_CMD_CALL_DL)
		FifoRecorder::GetInstance().WriteGPCommand(opcodeStart, u32(g_VideoData.GetReadPosition() - opcodeStart));

	return cycles;
}


DataReadU32xNfunc DataReadU32xFuncs[16] = {
	ReadU32xn<1>,
	ReadU32xn<2>,
	ReadU32xn<3>,
	ReadU32xn<4>,
	ReadU32xn<5>,
	ReadU32xn<6>,
	ReadU32xn<7>,
	ReadU32xn<8>,
	ReadU32xn<9>,
	ReadU32xn<10>,
	ReadU32xn<11>,
	ReadU32xn<12>,
	ReadU32xn<13>,
	ReadU32xn<14>,
	ReadU32xn<15>,
	ReadU32xn<16>
};

static u32 InterpretDisplayList(u32 address, u32 size)
{
	const u8* old_pVideoData = g_VideoData.GetReadPosition();
	const u8* startAddress = Memory::GetPointer(address);
	
	u32 cycles = 0;
	
	// Avoid the crash if Memory::GetPointer failed ..
	if (startAddress != nullptr)
	{
		g_VideoData.SetReadPosition(startAddress);

		// temporarily swap dl and non-dl (small "hack" for the stats)
		Statistics::SwapDL();
		const u8 *end = startAddress + size;		
		while (g_VideoData.GetReadPosition() < end)
		{
			cycles += Decode<false>(end);			
		}		
		INCSTAT(stats.thisFrame.numDListsCalled);
		// un-swap
		Statistics::SwapDL();
	}

	// reset to the old pointer
	g_VideoData.SetReadPosition(old_pVideoData);

	return cycles;
}

static void UnknownOpcode(u8 cmd_byte, const void *buffer)
{
	// TODO(Omega): Maybe dump FIFO to file on this error
	PanicAlert(
		"GFX FIFO: Unknown Opcode (0x%02x @ %p).\n"
		"This means one of the following:\n"
		"* The emulated GPU got desynced, disabling dual core can help\n"
		"* Command stream corrupted by some spurious memory bug\n"
		"* This really is an unknown opcode (unlikely)\n"
		"* Some other sort of bug\n\n"
		"Further errors will be sent to the Video Backend log and\n"
		"Dolphin will now likely crash or hang. Enjoy.",
		cmd_byte,
		buffer);
	{
		SCPFifoStruct &fifo = CommandProcessor::fifo;

		PanicAlert(
			"Illegal command %02x\n"
			"CPBase: 0x%08x\n"
			"CPEnd: 0x%08x\n"
			"CPHiWatermark: 0x%08x\n"
			"CPLoWatermark: 0x%08x\n"
			"CPReadWriteDistance: 0x%08x\n"
			"CPWritePointer: 0x%08x\n"
			"CPReadPointer: 0x%08x\n"
			"CPBreakpoint: 0x%08x\n"
			"bFF_GPReadEnable: %s\n"
			"bFF_BPEnable: %s\n"
			"bFF_BPInt: %s\n"
			"bFF_Breakpoint: %s\n"
			, cmd_byte, fifo.CPBase, fifo.CPEnd, fifo.CPHiWatermark, fifo.CPLoWatermark, fifo.CPReadWriteDistance
			, fifo.CPWritePointer, fifo.CPReadPointer, fifo.CPBreakpoint, fifo.bFF_GPReadEnable ? "true" : "false"
			, fifo.bFF_BPEnable ? "true" : "false", fifo.bFF_BPInt ? "true" : "false"
			, fifo.bFF_Breakpoint ? "true" : "false");
	}
}

void OpcodeDecoder_Init()
{
	s_bFifoErrorSeen = false;
	g_VideoData.SetReadPosition(GetVideoBufferStartPtr());
#ifdef _WIN32
	if (g_Config.bEnableOpenCL)
	{
		OpenCL::Initialize();
		TexDecoder_OpenCL_Initialize();
	}
#endif
}

void ResetStates()
{
	memset(&bpmem, 0, sizeof(bpmem));
	bpmem.bpMask = 0xFFFFFF;

	memset(g_main_cp_state.array_bases, 0, sizeof(g_main_cp_state.array_bases));
	memset(g_main_cp_state.array_strides, 0, sizeof(g_main_cp_state.array_strides));
	memset(&g_main_cp_state.matrix_index_a, 0, sizeof(g_main_cp_state.matrix_index_a));
	memset(&g_main_cp_state.matrix_index_b, 0, sizeof(g_main_cp_state.matrix_index_b));
	memset(&g_main_cp_state.vtx_desc, 0, sizeof(g_main_cp_state.vtx_desc));
	memset(g_main_cp_state.vtx_attr, 0, sizeof(g_main_cp_state.vtx_attr));
}

void OpcodeDecoder_Shutdown()
{
#ifdef _WIN32
	if (g_Config.bEnableOpenCL)
	{
		TexDecoder_OpenCL_Shutdown();
		OpenCL::Destroy();
	}
#endif
}

u32 OpcodeDecoder_Run(const u8* end)
{
	u32 totalCycles = 0;
	while (true)
	{
		const u8* old = g_VideoData.GetReadPosition();
		u32 cycles = Decode<true>(end);
		if (cycles == 0)
		{
			g_VideoData.SetReadPosition(old);
			break;
		}
		totalCycles += cycles;
	}
	return totalCycles;
}
