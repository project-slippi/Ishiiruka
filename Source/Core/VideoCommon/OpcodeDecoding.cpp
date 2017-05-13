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

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/XFMemory.h"

bool g_bRecordFifoData = false;
static bool s_bFifoErrorSeen = false;

namespace OpcodeDecoder
{

template <int count>
__forceinline void ReadU32xn(u32 *bufx16)
{
	g_VideoData.ReadU32xN<count>(bufx16);
}

__forceinline u32 InterpretDisplayList(u32 address, u32 size)
{
	u8* startAddress;

	if (Fifo::UseDeterministicGPUThread())
		startAddress = static_cast<u8*>(Fifo::PopFifoAuxBuffer(size));
	else
		startAddress = static_cast<u8*>(Memory::GetPointer(address));

	u32 cycles = 0;

	// Avoid the crash if Memory::GetPointer failed ..
	if (startAddress != nullptr)
	{
		u8* old_pVideoData = g_VideoData.GetReadPosition();
		u8* old_pVideoDataEnd = g_VideoData.GetEnd();

		g_VideoData.SetReadPosition(startAddress, startAddress + size);

		// temporarily swap dl and non-dl (small "hack" for the stats)
		Statistics::SwapDL();
		OpcodeDecoder::Run<false, false>(g_VideoData, &cycles);
		INCSTAT(stats.thisFrame.numDListsCalled);
		// un-swap
		Statistics::SwapDL();
		// reset to the old pointer
		g_VideoData.SetReadPosition(old_pVideoData, old_pVideoDataEnd);
	}
	return cycles;
}

__forceinline void InterpretDisplayListPreprocess(u32 address, u32 size)
{
	u8* startAddress = Memory::GetPointer(address);

	Fifo::PushFifoAuxBuffer(startAddress, size);

	if (startAddress != nullptr)
	{
		DataReader dlist_reader(startAddress, startAddress + size);
		OpcodeDecoder::Run<true, false>(dlist_reader, nullptr);
	}
}

static void UnknownOpcode(u8 cmd_byte, const void *buffer, bool preprocess)
{
	// TODO(Omega): Maybe dump FIFO to file on this error
	PanicAlertT(
		"GFX FIFO: Unknown Opcode (0x%02x @ %p, preprocessing=%s).\n"
		"This means one of the following:\n"
		"* The emulated GPU got desynced, disabling dual core can help\n"
		"* Command stream corrupted by some spurious memory bug\n"
		"* This really is an unknown opcode (unlikely)\n"
		"* Some other sort of bug\n\n"
		"Further errors will be sent to the Video Backend log and\n"
		"Dolphin will now likely crash or hang. Enjoy.",
		cmd_byte,
		buffer,
		preprocess ? "preprocess=true" : "preprocess=false");

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
			"bFF_GPLinkEnable: %s\n"
			"bFF_HiWatermarkInt: %s\n"
			"bFF_LoWatermarkInt: %s\n"
			, cmd_byte, fifo.CPBase, fifo.CPEnd, fifo.CPHiWatermark, fifo.CPLoWatermark, fifo.CPReadWriteDistance
			, fifo.CPWritePointer, fifo.CPReadPointer, fifo.CPBreakpoint
			, fifo.bFF_GPReadEnable ? "true" : "false"
			, fifo.bFF_BPEnable ? "true" : "false"
			, fifo.bFF_BPInt ? "true" : "false"
			, fifo.bFF_Breakpoint ? "true" : "false"
			, fifo.bFF_GPLinkEnable ? "true" : "false"
			, fifo.bFF_HiWatermarkInt ? "true" : "false"
			, fifo.bFF_LoWatermarkInt ? "true" : "false"
		);
	}
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

void Init()
{
	s_bFifoErrorSeen = false;
}

template <bool is_preprocess, bool sizeCheck>
u8* Run(DataReader& reader, u32* cycles)
{
	u32 totalCycles = 0;
	u8* opcodeStart;
	while (true)
	{
		opcodeStart = reader.GetReadPosition();
		if (!reader.size())
			goto end;

		u8 cmd_byte = reader.Read<u8>();
		size_t distance = reader.size();

		switch (cmd_byte)
		{
		case GX_NOP:
		{
			totalCycles += GX_NOP_CYCLES; // Hm, this means that we scan over nop streams pretty slowly...
		}
		break;
		case GX_UNKNOWN_RESET:
		{
			totalCycles += GX_NOP_CYCLES; // Datel software uses this command
			DEBUG_LOG(VIDEO, "GX Reset?: %08x", cmd_byte);
		}
		break;
		case GX_LOAD_CP_REG:
		{
			if (sizeCheck && distance < GX_LOAD_CP_REG_SIZE)
				goto end;
			totalCycles += GX_LOAD_CP_REG_CYCLES;
			u8 sub_cmd = reader.Read<u8>();
			u32 value = reader.Read<u32>();
			LoadCPReg<is_preprocess>(sub_cmd, value);
			if (!is_preprocess)
				INCSTAT(stats.thisFrame.numCPLoads);
		}
		break;
		case GX_LOAD_XF_REG:
		{
			if (sizeCheck && distance < GX_LOAD_XF_REG_SIZE)
				goto end;
			u32 Cmd2 = reader.Read<u32>();
			distance -= GX_LOAD_XF_REG_SIZE;
			int transfer_size = ((Cmd2 >> 16) & 15) + 1;
			if (sizeCheck && distance < (transfer_size * sizeof(u32)))
				goto end;
			totalCycles += GX_LOAD_XF_REG_BASE_CYCLES + GX_LOAD_XF_REG_TRANSFER_CYCLES * transfer_size;
			if (is_preprocess)
			{
				reader.ReadSkip(transfer_size * sizeof(u32));
			}
			else
			{
				u32 xf_address = Cmd2 & 0xFFFF;
				LoadXFReg(transfer_size, xf_address);
				INCSTAT(stats.thisFrame.numXFLoads);
			}
		}
		break;
		case GX_LOAD_INDX_A: //used for position matrices
		case GX_LOAD_INDX_B: //used for normal matrices
		case GX_LOAD_INDX_C: //used for postmatrices
		case GX_LOAD_INDX_D: //used for lights
		{
			if (sizeCheck && distance < GX_LOAD_INDX_SIZE)
				goto end;
			totalCycles += GX_LOAD_INDX_CYCLES;
			const s32 ref_array = (cmd_byte >> 3) + 8;
			if (is_preprocess)
				PreprocessIndexedXF(reader.Read<u32>(), ref_array);
			else
				LoadIndexedXF(reader.Read<u32>(), ref_array);
		}
		break;
		case GX_CMD_CALL_DL:
		{
			if (sizeCheck && distance < GX_CMD_CALL_DL_SIZE)
				goto end;
			u32 address = reader.Read<u32>();
			u32 count = reader.Read<u32>();
			if (is_preprocess)
				InterpretDisplayListPreprocess(address, count);
			else
				totalCycles += GX_CMD_CALL_DL_BASE_CYCLES + InterpretDisplayList(address, count);
		}
		break;
		case GX_CMD_UNKNOWN_METRICS: // zelda 4 swords calls it and checks the metrics registers after that
		{
			totalCycles += GX_CMD_UNKNOWN_METRICS_CYCLES;
			DEBUG_LOG(VIDEO, "GX 0x44: %08x", cmd_byte);
		}
		break;
		case GX_CMD_INVL_VC: // Invalidate Vertex Cache	
		{
			totalCycles += GX_CMD_INVL_VC_CYCLES;
			DEBUG_LOG(VIDEO, "Invalidate (vertex cache?)");
		}
		break;
		case GX_LOAD_BP_REG:
		{
			if (sizeCheck && distance < GX_LOAD_BP_REG_SIZE)
				goto end;
			totalCycles += GX_LOAD_BP_REG_CYCLES;
			u32 bp_cmd = reader.Read<u32>();
			if (is_preprocess)
			{
				LoadBPRegPreprocess(bp_cmd);
			}
			else
			{
				LoadBPReg(bp_cmd);
				INCSTAT(stats.thisFrame.numBPLoads);
			}
		}
		break;
		// draw primitives 
		default:
			if ((cmd_byte & GX_DRAW_PRIMITIVES) == 0x80)
			{
				// load vertices
				if (sizeCheck && distance < GX_DRAW_PRIMITIVES_SIZE)
					goto end;

				u32 count = reader.Read<u16>();
				distance -= GX_DRAW_PRIMITIVES_SIZE;
				if (count)
				{
					CPState& state = is_preprocess ? g_preprocess_cp_state : g_main_cp_state;
					VertexLoaderParameters parameters;
					parameters.count = count;
					parameters.buf_size = distance;
					parameters.primitive = (cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT;
					u32 vtx_attr_group = cmd_byte & GX_VAT_MASK;
					parameters.vtx_attr_group = vtx_attr_group;
					parameters.needloaderrefresh = (state.attr_dirty & (1u << vtx_attr_group)) != 0;
					parameters.skip_draw = xfmem.viewport.wd == 0.0f
						|| xfmem.viewport.ht == 0.0f
						|| (bpmem.scissorBR.x + 1 - bpmem.scissorTL.x) == 0
						|| (bpmem.scissorBR.y + 1 - bpmem.scissorTL.y) == 0;
					parameters.VtxDesc = &state.vtx_desc;
					parameters.VtxAttr = &state.vtx_attr[vtx_attr_group];
					parameters.source = reader.GetReadPosition();
					state.attr_dirty &= ~(1 << vtx_attr_group);
					u32 readsize = 0;
					if (is_preprocess)
					{
						u32 components = 0;
						VertexLoaderManager::GetVertexSizeAndComponents(parameters, readsize, components);
						readsize *= count;
						if (distance >= readsize)
						{
							totalCycles += GX_NOP_CYCLES + GX_DRAW_PRIMITIVES_CYCLES * parameters.count;
							reader.ReadSkip(readsize);
						}
						else
						{
							goto end;
						}
					}
					else
					{
						u32 writesize = 0;
						if (VertexLoaderManager::ConvertVertices(parameters, readsize, writesize))
						{
							totalCycles += GX_NOP_CYCLES + GX_DRAW_PRIMITIVES_CYCLES * parameters.count;
							reader.ReadSkip(readsize);
							g_vertex_manager->IncCurrentBufferPointer(writesize);
						}
						else
						{
							goto end;
						}
					}
				}
				else
				{
					totalCycles += GX_NOP_CYCLES;
				}
			}
			else
			{
				if (!s_bFifoErrorSeen)
					UnknownOpcode(cmd_byte, opcodeStart, is_preprocess);
				ERROR_LOG(VIDEO, "FIFO: Unknown Opcode(0x%02x @ %p, preprocessing = %s)", cmd_byte, opcodeStart, is_preprocess ? "yes" : "no");
				s_bFifoErrorSeen = true;
				totalCycles += 1;
			}
			break;
		}

		// Display lists get added directly into the FIFO stream
		if (!is_preprocess && g_bRecordFifoData && cmd_byte != GX_CMD_CALL_DL)
		{
			const u8* opcodeEnd;
			opcodeEnd = reader.GetReadPosition();
			FifoRecorder::GetInstance().WriteGPCommand(opcodeStart, u32(opcodeEnd - opcodeStart));
		}
	}
end:
	if (cycles)
	{
		*cycles = totalCycles;
	}
	return opcodeStart;
}

template u8* Run<true, false>(DataReader& reader, u32* cycles);
template u8* Run<false, false>(DataReader& reader, u32* cycles);
template u8* Run<true, true>(DataReader& reader, u32* cycles);
template u8* Run<false, true>(DataReader& reader, u32* cycles);

} // namespace OpcodeDecoder