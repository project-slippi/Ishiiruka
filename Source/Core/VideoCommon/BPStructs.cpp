// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <cstring>
#include <string>

#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigManager.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BPStructs.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

using namespace BPFunctions;

static u32 mapTexAddress;
static bool mapTexFound;
static int numWrites;

extern bool g_bSkipCurrentFrame;

static const float s_gammaLUT[] =
{
	1.0f,
	1.7f,
	2.2f,
	1.0f
};

void BPInit()
{
	memset(&bpmem, 0, sizeof(bpmem));
	bpmem.bpMask = 0xFFFFFF;

	mapTexAddress = 0;
	numWrites = 0;
	mapTexFound = false;
}

void BPWritten(const BPCmd& bp)
{
	/*
	----------------------------------------------------------------------------------------------------------------
	Purpose: Writes to the BP registers
	Called: At the end of every: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg
	How It Works: First the pipeline is flushed then update the bpmem with the new value.
	Some of the BP cases have to call certain functions while others just update the bpmem.
	some bp cases check the changes variable, because they might not have to be updated all the time
	NOTE: it seems not all bp cases like checking changes, so calling if (bp.changes == 0 ? false : true)
	had to be ditched and the games seem to work fine with out it.
	NOTE2: Yet Another Gamecube Documentation calls them Bypass Raster State Registers but possibly completely wrong
	NOTE3: This controls the register groups: RAS1/2, SU, TF, TEV, C/Z, PEC
	TODO: Turn into function table. The (future) DisplayList (DL) jit can then call the functions directly,
	getting rid of dynamic dispatch. Unfortunately, few games use DLs properly - most\
	just stuff geometry in them and don't put state changes there
	----------------------------------------------------------------------------------------------------------------
	*/

	// check for invalid state, else unneeded configuration are built
	g_video_backend->CheckInvalidState();

	// Debugging only, this lets you skip a bp update
	//static int times = 0;
	//static bool enable = false;

	//switch (bp.address)
	//{
	//case BPMEM_CONSTANTALPHA:
	//	{
	//	if (times-- == 0 && enable)
	//		return;
	//	else
	//		break;
	//	}
	//default: break;
	//}

	// FIXME: Hangs load-state, but should fix graphic-heavy games state loading
	//std::lock_guard<std::mutex> lk(s_bpCritical);

	if (((s32*)&bpmem)[bp.address] == bp.newvalue)
	{
		if (!(bp.address == BPMEM_TRIGGER_EFB_COPY
			|| bp.address == BPMEM_CLEARBBOX1
			|| bp.address == BPMEM_CLEARBBOX2
			|| bp.address == BPMEM_SETDRAWDONE
			|| bp.address == BPMEM_PE_TOKEN_ID
			|| bp.address == BPMEM_PE_TOKEN_INT_ID
			|| bp.address == BPMEM_LOADTLUT0
			|| bp.address == BPMEM_LOADTLUT1
			|| bp.address == BPMEM_TEXINVALIDATE
			|| bp.address == BPMEM_PRELOAD_MODE
			|| bp.address == BPMEM_CLEAR_PIXEL_PERF))
		{
			return;
		}
	}

	FlushPipeline();

	((u32*)&bpmem)[bp.address] = bp.newvalue;

	switch (bp.address)
	{
	case BPMEM_GENMODE: // Set the Generation Mode
		PRIM_LOG("genmode: texgen=%d, col=%d, multisampling=%d, tev=%d, cullmode=%d, ind=%d, zfeeze=%d",
			(u32)bpmem.genMode.numtexgens, (u32)bpmem.genMode.numcolchans,
			(u32)bpmem.genMode.multisampling, (u32)bpmem.genMode.numtevstages + 1, (u32)bpmem.genMode.cullmode,
			(u32)bpmem.genMode.numindstages.Value(), (u32)bpmem.genMode.zfreeze);

		// Only call SetGenerationMode when cull mode changes.
		if (bp.changes & 0xC000)
			SetGenerationMode();
		return;
	case BPMEM_IND_MTXA: // Index Matrix Changed
	case BPMEM_IND_MTXB:
	case BPMEM_IND_MTXC:
	case BPMEM_IND_MTXA + 3:
	case BPMEM_IND_MTXB + 3:
	case BPMEM_IND_MTXC + 3:
	case BPMEM_IND_MTXA + 6:
	case BPMEM_IND_MTXB + 6:
	case BPMEM_IND_MTXC + 6:
		if (bp.changes)
		{
			PixelShaderManager::SetIndMatrixChanged((bp.address - BPMEM_IND_MTXA) / 3);
		}
		return;
	case BPMEM_RAS1_SS0: // Index Texture Coordinate Scale 0
		if (bp.changes)
		{
			PixelShaderManager::SetIndTexScaleChanged(false);
		}
		return;
	case BPMEM_RAS1_SS1: // Index Texture Coordinate Scale 1
		if (bp.changes)
		{
			PixelShaderManager::SetIndTexScaleChanged(true);
		}
		return;
		// ----------------
		// Scissor Control
		// ----------------
	case BPMEM_SCISSORTL: // Scissor Rectable Top, Left
	case BPMEM_SCISSORBR: // Scissor Rectable Bottom, Right
	case BPMEM_SCISSOROFFSET: // Scissor Offset
		SetScissor();
		return;
	case BPMEM_LINEPTWIDTH: // Line Width
		SetLineWidth();
		return;
	case BPMEM_ZMODE: // Depth Control
		PRIM_LOG("zmode: test=%d, func=%d, upd=%d", (int)bpmem.zmode.testenable,
			(int)bpmem.zmode.func, (int)bpmem.zmode.updateenable);
		SetDepthMode();
		return;
	case BPMEM_BLENDMODE: // Blending Control
		if (bp.changes & 0xFFFF)
		{
			PRIM_LOG("blendmode: en=%d, open=%d, colupd=%d, alphaupd=%d, dst=%d, src=%d, sub=%d, mode=%d",
				(int)bpmem.blendmode.blendenable, (int)bpmem.blendmode.logicopenable, (int)bpmem.blendmode.colorupdate,
				(int)bpmem.blendmode.alphaupdate, (int)bpmem.blendmode.dstfactor, (int)bpmem.blendmode.srcfactor,
				(int)bpmem.blendmode.subtract, (int)bpmem.blendmode.logicmode);

			// Set LogicOp Blending Mode
			if (bp.changes & 0xF002) // logicopenable | logicmode
				SetLogicOpMode();

			// Set Blending Mode
			if (bp.changes & 0xFF1) // blendenable | alphaupdate | dstfactor | srcfactor | subtract
				SetBlendMode();

			// Set Color Mask
			if (bp.changes & 0x18) // colorupdate | alphaupdate
				SetColorMask();
		}
		return;
	case BPMEM_CONSTANTALPHA: // Set Destination Alpha
		PRIM_LOG("constalpha: alp=%d, en=%d", bpmem.dstalpha.alpha, bpmem.dstalpha.enable);
		if (bp.changes & 0xFF)
			PixelShaderManager::SetDestAlpha();
		if (bp.changes & 0x100)
			SetBlendMode();
		return;

		// This is called when the game is done drawing the new frame (eg: like in DX: Begin(); Draw(); End();)
		// Triggers an interrupt on the PPC side so that the game knows when the GPU has finished drawing.
		// Tokens are similar.
	case BPMEM_SETDRAWDONE:
		switch (bp.newvalue & 0xFF)
		{
		case 0x02:
			if (!Fifo::UseDeterministicGPUThread())
				PixelEngine::SetFinish(); // may generate interrupt
			DEBUG_LOG(VIDEO, "GXSetDrawDone SetPEFinish (value: 0x%02X)", (bp.newvalue & 0xFFFF));
			return;

		default:
			WARN_LOG(VIDEO, "GXSetDrawDone ??? (value 0x%02X)", (bp.newvalue & 0xFFFF));
			return;
		}
		return;
	case BPMEM_PE_TOKEN_ID: // Pixel Engine Token ID
		if (!Fifo::UseDeterministicGPUThread())
			PixelEngine::SetToken(static_cast<u16>(bp.newvalue & 0xFFFF), false);
		DEBUG_LOG(VIDEO, "SetPEToken 0x%04x", (bp.newvalue & 0xFFFF));
		return;
	case BPMEM_PE_TOKEN_INT_ID: // Pixel Engine Interrupt Token ID
		if (!Fifo::UseDeterministicGPUThread())
			PixelEngine::SetToken(static_cast<u16>(bp.newvalue & 0xFFFF), true);
		DEBUG_LOG(VIDEO, "SetPEToken + INT 0x%04x", (bp.newvalue & 0xFFFF));
		return;

		// ------------------------
		// EFB copy command. This copies a rectangle from the EFB to either RAM in a texture format or to XFB as YUYV.
		// It can also optionally clear the EFB while copying from it. To emulate this, we of course copy first and clear afterwards.
	case BPMEM_TRIGGER_EFB_COPY: // Copy EFB Region or Render to the XFB or Clear the screen.
	{
		// The bottom right is within the rectangle
		// The values in bpmem.copyTexSrcXY and bpmem.copyTexSrcWH are updated in case 0x49 and 0x4a in this function

		u32 destAddr = bpmem.copyTexDest << 5;
		u32 destStride = bpmem.copyMipMapStrideChannels << 5;

		EFBRectangle srcRect;
		srcRect.left = (int)bpmem.copyTexSrcXY.x;
		srcRect.top = (int)bpmem.copyTexSrcXY.y;

		// Here Width+1 like Height, otherwise some textures are corrupted already since the native resolution.
		// TODO: What's the behavior of out of bound access?
		srcRect.right = (int)(bpmem.copyTexSrcXY.x + bpmem.copyTexSrcWH.x + 1);
		srcRect.bottom = (int)(bpmem.copyTexSrcXY.y + bpmem.copyTexSrcWH.y + 1);
		UPE_Copy PE_copy = bpmem.triggerEFBCopy;

		// Check if we are to copy from the EFB or draw to the XFB
		if (PE_copy.copy_to_xfb == 0)
		{
			// bpmem.zcontrol.pixel_format to PEControl::Z24 is when the game wants to copy from ZBuffer (Zbuffer uses 24-bit Format)
			bool is_depth_copy = bpmem.zcontrol.pixel_format == PEControl::Z24;
			g_texture_cache->CopyRenderTargetToTexture(destAddr, PE_copy.tp_realFormat(), destStride,
				is_depth_copy, srcRect,
				!!PE_copy.intensity_fmt, !!PE_copy.half_scale);
		}
		else
		{
			// We should be able to get away with deactivating the current bbox tracking
			// here. Not sure if there's a better spot to put this.
			// the number of lines copied is determined by the y scale * source efb height

			BoundingBox::active = false;

			float yScale;
			if (PE_copy.scale_invert)
				yScale = 256.0f / (float)bpmem.dispcopyyscale;
			else
				yScale = (float)bpmem.dispcopyyscale / 256.0f;

			float num_xfb_lines = 1.0f + bpmem.copyTexSrcWH.y * yScale;

			u32 height = static_cast<u32>(num_xfb_lines);
			if (height > MAX_XFB_HEIGHT)
			{
				INFO_LOG(VIDEO, "Tried to scale EFB to too many XFB lines: %d (%f)",
					height, num_xfb_lines);
				height = MAX_XFB_HEIGHT;
			}

			DEBUG_LOG(VIDEO, "RenderToXFB: destAddr: %08x | srcRect {%d %d %d %d} | fbWidth: %u | fbStride: %u | fbHeight: %u",
				destAddr, srcRect.left, srcRect.top, srcRect.right, srcRect.bottom, bpmem.copyTexSrcWH.x + 1, destStride, height);
			g_renderer->RenderToXFB(destAddr, srcRect, destStride, height, s_gammaLUT[PE_copy.gamma]);
		}

		// Clear the rectangular region after copying it.
		if (PE_copy.clear)
		{
			ClearScreen(srcRect);
		}

		return;
	}
	case BPMEM_LOADTLUT0: // This one updates bpmem.tlutXferSrc, no need to do anything here.
		return;
	case BPMEM_LOADTLUT1: // Load a Texture Look Up Table
	{
		u32 tlutTMemAddr = (bp.newvalue & 0x3FF) << 9;
		u32 tlutXferCount = (bp.newvalue & 0x1FFC00) >> 5;
		u32 addr = bpmem.tmem_config.tlut_src << 5;

		// The GameCube ignores the upper bits of this address. Some games (WW, MKDD) set them.
		if (!SConfig::GetInstance().bWii)
			addr = addr & 0x01FFFFFF;

		Memory::CopyFromEmu(texMem + tlutTMemAddr, addr, tlutXferCount);

		if (g_bRecordFifoData)
			FifoRecorder::GetInstance().UseMemory(addr, tlutXferCount, MemoryUpdate::TMEM);

		return;
	}
	case BPMEM_FOGRANGE: // Fog Settings Control
	case BPMEM_FOGRANGE + 1:
	case BPMEM_FOGRANGE + 2:
	case BPMEM_FOGRANGE + 3:
	case BPMEM_FOGRANGE + 4:
	case BPMEM_FOGRANGE + 5:
		if (bp.changes)
			PixelShaderManager::SetFogRangeAdjustChanged();
		return;
	case BPMEM_FOGPARAM0:
	case BPMEM_FOGBMAGNITUDE:
	case BPMEM_FOGBEXPONENT:
	case BPMEM_FOGPARAM3:
		if (bp.changes)
			PixelShaderManager::SetFogParamChanged();
		return;
	case BPMEM_FOGCOLOR: // Fog Color
		if (bp.changes)
			PixelShaderManager::SetFogColorChanged();
		return;
	case BPMEM_ALPHACOMPARE: // Compare Alpha Values
		PRIM_LOG("alphacmp: ref0=%d, ref1=%d, comp0=%d, comp1=%d, logic=%d",
			(int)bpmem.alpha_test.ref0, (int)bpmem.alpha_test.ref1,
			(int)bpmem.alpha_test.comp0, (int)bpmem.alpha_test.comp1,
			(int)bpmem.alpha_test.logic);
		if (bp.changes & 0xFFFF)
			PixelShaderManager::SetAlpha();
		if (bp.changes)
			g_renderer->SetColorMask();
		return;
	case BPMEM_BIAS: // BIAS
		PRIM_LOG("ztex bias=0x%x", bpmem.ztex1.bias);
		if (bp.changes)
			PixelShaderManager::SetZTextureBias();
		return;
	case BPMEM_ZTEX2: // Z Texture type
	{
		if (bp.changes & 3)
			PixelShaderManager::SetZTextureTypeChanged();
#if defined(_DEBUG) || defined(DEBUGFAST)
		const char* pzop[] = { "DISABLE", "ADD", "REPLACE", "?" };
		const char* pztype[] = { "Z8", "Z16", "Z24", "?" };
		PRIM_LOG("ztex op=%s, type=%s", pzop[bpmem.ztex2.op], pztype[bpmem.ztex2.type]);
#endif
	}
	return;
	// ----------------------------------
	// Display Copy Filtering Control - GX_SetCopyFilter(u8 aa,u8 sample_pattern[12][2],u8 vf,u8 vfilter[7])
	// Fields: Destination, Frame2Field, Gamma, Source
	// ----------------------------------
	case BPMEM_DISPLAYCOPYFILTER:   // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_DISPLAYCOPYFILTER + 1: // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_DISPLAYCOPYFILTER + 2: // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_DISPLAYCOPYFILTER + 3: // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_COPYFILTER0:	        // if (vf) { use vfilter } else { use 595000 }
	case BPMEM_COPYFILTER1:	        // if (vf) { use vfilter } else { use 000015 }
		return;
		// -----------------------------------
		// Interlacing Control
		// -----------------------------------
	case BPMEM_FIELDMASK: // GX_SetFieldMask(u8 even_mask,u8 odd_mask)
	case BPMEM_FIELDMODE: // GX_SetFieldMode(u8 field_mode,u8 half_aspect_ratio)
		// TODO
		return;
		// ----------------------------------------
		// Unimportant regs (Clock, Perf, ...)
		// ----------------------------------------
	case BPMEM_BUSCLOCK0:      // TB Bus Clock ?
	case BPMEM_BUSCLOCK1:      // TB Bus Clock ?
	case BPMEM_PERF0_TRI:      // Perf: Triangles
	case BPMEM_PERF0_QUAD:     // Perf: Quads
	case BPMEM_PERF1:          // Perf: Some Clock, Texels, TX, TC
		return;
		// ----------------
		// EFB Copy config
		// ----------------
	case BPMEM_EFB_TL:   // EFB Source Rect. Top, Left
	case BPMEM_EFB_BR:   // EFB Source Rect. Bottom, Right (w, h - 1)
	case BPMEM_EFB_ADDR: // EFB Target Address
		return;
		// --------------
		// Clear Config
		// --------------
	case BPMEM_CLEAR_AR: // Alpha and Red Components
	case BPMEM_CLEAR_GB: // Green and Blue Components
	case BPMEM_CLEAR_Z:  // Z Components (24-bit Zbuffer)
		return;
		// -------------------------
		// Bounding Box Control
		// -------------------------
	case BPMEM_CLEARBBOX1:
	case BPMEM_CLEARBBOX2:
		{
			u8 offset = bp.address & 2;
			BoundingBox::active = true;

			BoundingBox::coords[offset] = bp.newvalue & 0x3ff;
			BoundingBox::coords[offset + 1] = bp.newvalue >> 10;

			if (g_ActiveConfig.backend_info.bSupportsBBox && g_ActiveConfig.iBBoxMode == BBoxGPU)
			{
				g_renderer->BBoxWrite(offset, bp.newvalue & 0x3ff);
				g_renderer->BBoxWrite(offset + 1, bp.newvalue >> 10);
			}
		}
		return;
	case BPMEM_TEXINVALIDATE:
		// TODO: Needs some restructuring in TextureCacheBase.
		return;

	case BPMEM_ZCOMPARE:      // Set the Z-Compare and EFB pixel format
		OnPixelFormatChange();
		if (bp.changes & 7)
		{
			SetBlendMode(); // dual source could be activated by changing to PIXELFMT_RGBA6_Z24
			g_renderer->SetColorMask(); // alpha writing needs to be disabled if the new pixel format doesn't have an alpha channel
		}
		return;

	case BPMEM_MIPMAP_STRIDE: // MipMap Stride Channel
	case BPMEM_COPYYSCALE:    // Display Copy Y Scale

		/* 24 RID
		* 21 BC3 - Ind. Tex Stage 3 NTexCoord
		* 18 BI3 - Ind. Tex Stage 3 NTexMap
		* 15 BC2 - Ind. Tex Stage 2 NTexCoord
		* 12 BI2 - Ind. Tex Stage 2 NTexMap
		* 9 BC1 - Ind. Tex Stage 1 NTexCoord
		* 6 BI1 - Ind. Tex Stage 1 NTexMap
		* 3 BC0 - Ind. Tex Stage 0 NTexCoord
		* 0 BI0 - Ind. Tex Stage 0 NTexMap */
	case BPMEM_IREF:

	case BPMEM_TEV_KSEL:   // Texture Environment Swap Mode Table 0
	case BPMEM_TEV_KSEL + 1: // Texture Environment Swap Mode Table 1
	case BPMEM_TEV_KSEL + 2: // Texture Environment Swap Mode Table 2
	case BPMEM_TEV_KSEL + 3: // Texture Environment Swap Mode Table 3
	case BPMEM_TEV_KSEL + 4: // Texture Environment Swap Mode Table 4
	case BPMEM_TEV_KSEL + 5: // Texture Environment Swap Mode Table 5
	case BPMEM_TEV_KSEL + 6: // Texture Environment Swap Mode Table 6
	case BPMEM_TEV_KSEL + 7: // Texture Environment Swap Mode Table 7

		/* This Register can be used to limit to which bits of BP registers is
		* actually written to. The mask is only valid for the next BP write,
		* and will reset itself afterwards. It's handled as a special case in
		* LoadBPReg. */
	case BPMEM_BP_MASK:

	case BPMEM_IND_IMASK: // Index Mask ?
	case BPMEM_REVBITS: // Always set to 0x0F when GX_InitRevBits() is called.
		return;

	case BPMEM_CLEAR_PIXEL_PERF:
		// GXClearPixMetric writes 0xAAA here, Sunshine alternates this register between values 0x000 and 0xAAA
		if (PerfQueryBase::ShouldEmulate())
			g_perf_query->ResetQuery();
		return;

	case BPMEM_PRELOAD_ADDR:
	case BPMEM_PRELOAD_TMEMEVEN:
	case BPMEM_PRELOAD_TMEMODD: // Used when PRELOAD_MODE is set
		return;

	case BPMEM_PRELOAD_MODE: // Set to 0 when GX_TexModeSync() is called.
									 // if this is different from 0, manual TMEM management is used (GX_PreloadEntireTexture).
		if (bp.newvalue != 0)
		{
			// TODO: Not quite sure if this is completely correct (likely not)
			// NOTE: libogc's implementation of GX_PreloadEntireTexture seems flawed, so it's not necessarily a good reference for RE'ing this feature.

			BPS_TmemConfig& tmem_cfg = bpmem.tmem_config;
			u32 src_addr = tmem_cfg.preload_addr << 5; // TODO: Should we add mask here on GC?
			u32 bytes_read = 0;
			u32 tmem_addr_even = tmem_cfg.preload_tmem_even * TMEM_LINE_SIZE;

			if (tmem_cfg.preload_tile_info.type != 3)
			{
				bytes_read = tmem_cfg.preload_tile_info.count * TMEM_LINE_SIZE;
				if (tmem_addr_even + bytes_read > TMEM_SIZE)
					bytes_read = TMEM_SIZE - tmem_addr_even;

				Memory::CopyFromEmu(texMem + tmem_addr_even, src_addr, bytes_read);
			}
			else // RGBA8 tiles (and CI14, but that might just be stupid libogc!)
			{
				u8* src_ptr = Memory::GetPointer(src_addr);

				// AR and GB tiles are stored in separate TMEM banks => can't use a single memcpy for everything
				u32 tmem_addr_odd = tmem_cfg.preload_tmem_odd * TMEM_LINE_SIZE;

				for (u32 i = 0; i < tmem_cfg.preload_tile_info.count; ++i)
				{
					if (tmem_addr_even + TMEM_LINE_SIZE > TMEM_SIZE || tmem_addr_odd + TMEM_LINE_SIZE > TMEM_SIZE)
						break;

					memcpy(texMem + tmem_addr_even, src_ptr + bytes_read, TMEM_LINE_SIZE);
					memcpy(texMem + tmem_addr_odd, src_ptr + bytes_read + TMEM_LINE_SIZE, TMEM_LINE_SIZE);
					tmem_addr_even += TMEM_LINE_SIZE;
					tmem_addr_odd += TMEM_LINE_SIZE;
					bytes_read += TMEM_LINE_SIZE * 2;
				}
			}

			if (g_bRecordFifoData)
				FifoRecorder::GetInstance().UseMemory(src_addr, bytes_read, MemoryUpdate::TMEM);
		}
		return;

		// ---------------------------------------------------
		// Set the TEV Color
		// ---------------------------------------------------
		//
		// NOTE: Each of these registers actually maps to two variables internally.
		//       There's a bit that specifies which one is currently written to.
		//
		// NOTE: Some games write only to the RA register (or only to the BG register).
		//       We may not assume that the unwritten register holds a valid value, hence
		//       both component pairs need to be loaded individually.
	case BPMEM_TEV_COLOR_RA:
	case BPMEM_TEV_COLOR_RA + 2:
	case BPMEM_TEV_COLOR_RA + 4:
	case BPMEM_TEV_COLOR_RA + 6:
	{
		int num = (bp.address >> 1) & 0x3;
		if (bpmem.tevregs[num].type_ra)
		{
			PixelShaderManager::SetTevKonstColor(num, 0, (s32)bpmem.tevregs[num].red);
			PixelShaderManager::SetTevKonstColor(num, 3, (s32)bpmem.tevregs[num].alpha);
		}
		else
		{
			PixelShaderManager::SetTevColor(num, 0, (s32)bpmem.tevregs[num].red);
			PixelShaderManager::SetTevColor(num, 3, (s32)bpmem.tevregs[num].alpha);
		}
		return;
	}

	case BPMEM_TEV_COLOR_BG:
	case BPMEM_TEV_COLOR_BG + 2:
	case BPMEM_TEV_COLOR_BG + 4:
	case BPMEM_TEV_COLOR_BG + 6:
	{
		int num = (bp.address >> 1) & 0x3;
		if (bpmem.tevregs[num].type_bg)
		{
			PixelShaderManager::SetTevKonstColor(num, 1, (s32)bpmem.tevregs[num].green);
			PixelShaderManager::SetTevKonstColor(num, 2, (s32)bpmem.tevregs[num].blue);
		}
		else
		{
			PixelShaderManager::SetTevColor(num, 1, (s32)bpmem.tevregs[num].green);
			PixelShaderManager::SetTevColor(num, 2, (s32)bpmem.tevregs[num].blue);
		}
		return;
	}

	default:
		break;
	}

	switch (bp.address & 0xFC)  // Texture sampler filter
	{
		// -------------------------
		// Texture Environment Order
		// -------------------------
	case BPMEM_TREF:
	case BPMEM_TREF + 4:
		return;
		// ----------------------
		// Set wrap size
		// ----------------------
	case BPMEM_SU_SSIZE:
	case BPMEM_SU_SSIZE + 4:
	case BPMEM_SU_SSIZE + 8:
	case BPMEM_SU_SSIZE + 12:
		if (bp.changes)
		{
			PixelShaderManager::SetTexCoordChanged((bp.address - BPMEM_SU_SSIZE) >> 1);
			GeometryShaderManager::SetTexCoordChanged((bp.address - BPMEM_SU_SSIZE) >> 1);
		}
		return;
		// ------------------------
		// BPMEM_TX_SETMODE0 - (Texture lookup and filtering mode) LOD/BIAS Clamp, MaxAnsio, LODBIAS, DiagLoad, Min Filter, Mag Filter, Wrap T, S
		// BPMEM_TX_SETMODE1 - (LOD Stuff) - Max LOD, Min LOD
		// ------------------------
	case BPMEM_TX_SETMODE0: // (0x90 for linear)
	case BPMEM_TX_SETMODE0_4:
		return;

	case BPMEM_TX_SETMODE1:
	case BPMEM_TX_SETMODE1_4:
		return;
		// --------------------------------------------
		// BPMEM_TX_SETIMAGE0 - Texture width, height, format
		// BPMEM_TX_SETIMAGE1 - even LOD address in TMEM - Image Type, Cache Height, Cache Width, TMEM Offset
		// BPMEM_TX_SETIMAGE2 - odd LOD address in TMEM - Cache Height, Cache Width, TMEM Offset
		// BPMEM_TX_SETIMAGE3 - Address of Texture in main memory
		// --------------------------------------------
	case BPMEM_TX_SETIMAGE0:
	case BPMEM_TX_SETIMAGE0_4:
	case BPMEM_TX_SETIMAGE1:
	case BPMEM_TX_SETIMAGE1_4:
	case BPMEM_TX_SETIMAGE2:
	case BPMEM_TX_SETIMAGE2_4:
	case BPMEM_TX_SETIMAGE3:
	case BPMEM_TX_SETIMAGE3_4:
		return;
		// -------------------------------
		// Set a TLUT
		// BPMEM_TX_SETTLUT - Format, TMEM Offset (offset of TLUT from start of TMEM high bank > > 5)
		// -------------------------------
	case BPMEM_TX_SETTLUT:
	case BPMEM_TX_SETTLUT_4:
		return;

	default:
		break;
	}

	switch (bp.address & 0xF0)
	{
		// --------------
		// Indirect Tev
		// --------------
	case BPMEM_IND_CMD:
		return;
		// --------------------------------------------------
		// Set Color/Alpha of a Tev
		// BPMEM_TEV_COLOR_ENV - Dest, Shift, Clamp, Sub, Bias, Sel A, Sel B, Sel C, Sel D
		// BPMEM_TEV_ALPHA_ENV - Dest, Shift, Clamp, Sub, Bias, Sel A, Sel B, Sel C, Sel D, T Swap, R Swap
		// --------------------------------------------------
	case BPMEM_TEV_COLOR_ENV:    // Texture Environment 1	
	case BPMEM_TEV_COLOR_ENV + 16: // Texture Environment 9	
		return;
	default:
		break;
	}

	WARN_LOG(VIDEO, "Unknown BP opcode: address = 0x%08x value = 0x%08x", bp.address, bp.newvalue);
}

// Called when loading a saved state.
void BPReload()
{
	// restore anything that goes straight to the renderer.
	// let's not risk actually replaying any writes.
	// note that PixelShaderManager is already covered since it has its own DoState.
	SetGenerationMode();
	SetScissor();
	SetLineWidth();
	SetDepthMode();
	SetLogicOpMode();
	SetBlendMode();
	SetColorMask();
	OnPixelFormatChange();
	{
		BPCmd bp = { BPMEM_FIELDMASK, 0xFFFFFF, static_cast<int>(((u32*)&bpmem)[BPMEM_FIELDMASK]) };
		SetInterlacingMode(bp);
	}
	{
		BPCmd bp = { BPMEM_FIELDMODE, 0xFFFFFF, static_cast<int>(((u32*)&bpmem)[BPMEM_FIELDMODE]) };
		SetInterlacingMode(bp);
	}
}
