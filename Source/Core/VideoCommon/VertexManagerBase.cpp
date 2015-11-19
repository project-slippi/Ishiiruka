// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/TessellationShaderManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/MainBase.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

VertexManagerBase *g_vertex_manager;
extern NativeVertexFormat *g_nativeVertexFmt;

u8 *VertexManagerBase::s_pCurBufferPointer;
u8 *VertexManagerBase::s_pBaseBufferPointer;
u8 *VertexManagerBase::s_pEndBufferPointer;

bool VertexManagerBase::s_Shader_Refresh_Required = true;
bool VertexManagerBase::s_Zslope_Refresh_Required = true;
Slope VertexManagerBase::s_ZSlope = {0.0f, 0.0f, float(0xFFFFFF)};

PrimitiveType VertexManagerBase::current_primitive_type;

bool VertexManagerBase::IsFlushed;

static const PrimitiveType primitive_from_gx[8] = {
	PRIMITIVE_TRIANGLES, // GX_DRAW_QUADS
	PRIMITIVE_TRIANGLES, // GX_DRAW_QUADS_2
	PRIMITIVE_TRIANGLES, // GX_DRAW_TRIANGLES
	PRIMITIVE_TRIANGLES, // GX_DRAW_TRIANGLE_STRIP
	PRIMITIVE_TRIANGLES, // GX_DRAW_TRIANGLE_FAN
	PRIMITIVE_LINES,     // GX_DRAW_LINES
	PRIMITIVE_LINES,     // GX_DRAW_LINE_STRIP
	PRIMITIVE_POINTS,    // GX_DRAW_POINTS
};

PrimitiveType VertexManagerBase::GetPrimitiveType(int primitive)
{
	return primitive_from_gx[primitive & 7];
}

VertexManagerBase::VertexManagerBase()
{
	IsFlushed = true;
}

VertexManagerBase::~VertexManagerBase()
{
}

u32 VertexManagerBase::GetRemainingSize()
{
	return (u32)(s_pEndBufferPointer - s_pCurBufferPointer);
}

void VertexManagerBase::PrepareForAdditionalData(int primitive, u32 count, u32 stride)
{	
	// The SSE vertex loader can write up to 4 bytes past the end
	u32 const needed_vertex_bytes = count * stride + 4;

	// We can't merge different kinds of primitives, so we have to flush here
	if (current_primitive_type != primitive_from_gx[primitive])
		Flush();
	current_primitive_type = primitive_from_gx[primitive];

	// Check for size in buffer, if the buffer gets full, call Flush()
	if (!IsFlushed && (count > IndexGenerator::GetRemainingIndices() ||
		count > GetRemainingIndices(primitive) || needed_vertex_bytes > GetRemainingSize()))
	{
		Flush();

		if (count > IndexGenerator::GetRemainingIndices())
			ERROR_LOG(VIDEO, "Too little remaining index values. Use 32-bit or reset them on flush.");
		if (count > GetRemainingIndices(primitive))
			ERROR_LOG(VIDEO, "VertexManagerBase: Buffer not large enough for all indices! "
			"Increase MAXIBUFFERSIZE or we need primitive breaking after all.");
		if (needed_vertex_bytes > GetRemainingSize())
			ERROR_LOG(VIDEO, "VertexManagerBase: Buffer not large enough for all vertices! "
			"Increase MAXVBUFFERSIZE or we need primitive breaking after all.");
	}

	// need to alloc new buffer
	if (IsFlushed)
	{
		g_vertex_manager->ResetBuffer(stride);
		IsFlushed = false;
	}
}

u32 VertexManagerBase::GetRemainingIndices(int primitive)
{	
	u32 index_len = MAXIBUFFERSIZE - IndexGenerator::GetIndexLen();

	switch (primitive)
	{
	case GX_DRAW_QUADS:
	case GX_DRAW_QUADS_2:
		return index_len / 6 * 4;
	case GX_DRAW_TRIANGLES:
		return index_len;
	case GX_DRAW_TRIANGLE_STRIP:
		return index_len / 3 + 2;
	case GX_DRAW_TRIANGLE_FAN:
		return index_len / 3 + 2;
	case GX_DRAW_LINES:
		return index_len;
	case GX_DRAW_LINE_STRIP:
		return index_len / 2 + 1;
	case GX_DRAW_POINTS:
		return index_len;
	default:
		return 0;
	}
}

void VertexManagerBase::Flush()
{
	s_Shader_Refresh_Required = true;
	if (IsFlushed)
		return;
	bool useDstAlpha = bpmem.dstalpha.enable &&
		bpmem.blendmode.alphaupdate &&
		bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	// loading a state will invalidate BP, so check for it
	g_video_backend->CheckInvalidState();
	g_vertex_manager->PrepareShaders(current_primitive_type, g_nativeVertexFmt->m_components, xfmem, bpmem, true);

#if defined(_DEBUG) || defined(DEBUGFAST)
	PRIM_LOG("frame%d:\n texgen=%d, numchan=%d, dualtex=%d, ztex=%d, cole=%d, alpe=%d, ze=%d", g_ActiveConfig.iSaveTargetId, xfmem.numTexGen.numTexGens,
		xfmem.numChan.numColorChans, xfmem.dualTexTrans.enabled, bpmem.ztex2.op,
		(int)bpmem.blendmode.colorupdate, (int)bpmem.blendmode.alphaupdate, (int)bpmem.zmode.updateenable);

	for (unsigned int i = 0; i < xfmem.numChan.numColorChans; ++i)
	{
		LitChannel* ch = &xfmem.color[i];
		PRIM_LOG("colchan%d: matsrc=%d, light=0x%x, ambsrc=%d, diffunc=%d, attfunc=%d", i, ch->matsource, ch->GetFullLightMask(), ch->ambsource, ch->diffusefunc, ch->attnfunc);
		ch = &xfmem.alpha[i];
		PRIM_LOG("alpchan%d: matsrc=%d, light=0x%x, ambsrc=%d, diffunc=%d, attfunc=%d", i, ch->matsource, ch->GetFullLightMask(), ch->ambsource, ch->diffusefunc, ch->attnfunc);
	}

	for (unsigned int i = 0; i < xfmem.numTexGen.numTexGens; ++i)
	{
		TexMtxInfo tinfo = xfmem.texMtxInfo[i];
		if (tinfo.texgentype != XF_TEXGEN_EMBOSS_MAP) tinfo.hex &= 0x7ff;
		if (tinfo.texgentype != XF_TEXGEN_REGULAR) tinfo.projection = 0;

		PRIM_LOG("txgen%d: proj=%d, input=%d, gentype=%d, srcrow=%d, embsrc=%d, emblght=%d, postmtx=%d, postnorm=%d",
			i, tinfo.projection, tinfo.inputform, tinfo.texgentype, tinfo.sourcerow, tinfo.embosssourceshift, tinfo.embosslightshift,
			xfmem.postMtxInfo[i].index, xfmem.postMtxInfo[i].normalize);
	}

	PRIM_LOG("pixel: tev=%d, ind=%d, texgen=%d, dstalpha=%d, alphatest=0x%x", (int)bpmem.genMode.numtevstages + 1, (int)bpmem.genMode.numindstages.Value(),
		(int)bpmem.genMode.numtexgens, (u32)bpmem.dstalpha.enable, (bpmem.alpha_test.hex >> 16) & 0xff);
#endif

	u32 usedtextures = 0;
	for (u32 i = 0; i < bpmem.genMode.numtevstages + 1u; ++i)
		if (bpmem.tevorders[i / 2].getEnable(i & 1))
			usedtextures |= 1 << bpmem.tevorders[i / 2].getTexMap(i & 1);

	if (bpmem.genMode.numindstages.Value() > 0)
		for (u32 i = 0; i < bpmem.genMode.numtevstages + 1u; ++i)
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages.Value())
				usedtextures |= 1 << bpmem.tevindref.getTexMap(bpmem.tevind[i].bt);

	TextureCacheBase::UnbindTextures();
	s32 mask = 0;
	for (unsigned int i = 0; i < 8; i++)
	{
		if (usedtextures & (1 << i))
		{
			const TextureCacheBase::TCacheEntryBase* tentry = TextureCacheBase::Load(i);
			if (tentry)
			{
				if (g_ActiveConfig.HiresMaterialMapsEnabled() && tentry->SupportsMaterialMap())
				{
					mask |= 1 << i;
				}
				PixelShaderManager::SetTexDims(i, tentry->native_width, tentry->native_height);
				g_renderer->SetSamplerState(i & 3, i >> 2, tentry->is_custom_tex);
			}
			else
				ERROR_LOG(VIDEO, "error loading texture");
		}
	}
	if (g_ActiveConfig.HiresMaterialMapsEnabled())
	{
		PixelShaderManager::SetFlags(0, ~0, mask);
	}
	TextureCacheBase::BindTextures();

	// set global constants
	VertexShaderManager::SetConstants();
	GeometryShaderManager::SetConstants();
	TessellationShaderManager::SetConstants();
	PixelShaderManager::SetConstants();
	if (current_primitive_type == PRIMITIVE_TRIANGLES)
	{
		const PortableVertexDeclaration &vtx_dcl = g_nativeVertexFmt->GetVertexDeclaration();
		if (bpmem.genMode.zfreeze)
		{
			SetZSlope();
		}
		else if (IndexGenerator::GetIndexLen() >= 3)
		{
			CalculateZSlope(vtx_dcl, g_vertex_manager->GetIndexBuffer() + IndexGenerator::GetIndexLen() - 3);
		}

		// if cull mode is CULL_ALL, ignore triangles and quads
		if (bpmem.genMode.cullmode == GenMode::CULL_ALL)
		{
			IsFlushed = true;
			return;
		}
	}
	if (PerfQueryBase::ShouldEmulate())
		g_perf_query->EnableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
	g_vertex_manager->vFlush(useDstAlpha);
	if (PerfQueryBase::ShouldEmulate())
		g_perf_query->DisableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);

	GFX_DEBUGGER_PAUSE_AT(NEXT_FLUSH, true);

	if (xfmem.numTexGen.numTexGens != bpmem.genMode.numtexgens)
		ERROR_LOG(VIDEO, "xf.numtexgens (%d) does not match bp.numtexgens (%d). Error in command stream.", xfmem.numTexGen.numTexGens, bpmem.genMode.numtexgens.Value());

	IsFlushed = true;
}

void VertexManagerBase::DoState(PointerWrap& p)
{
	g_vertex_manager->vDoState(p);
}

void VertexManagerBase::CalculateZSlope(const PortableVertexDeclaration &vert_decl, const u16* indices)
{
	float out[12];
	float viewOffset[2] = { 
		xfmem.viewport.xOrig - bpmem.scissorOffset.x * 2,
		xfmem.viewport.yOrig - bpmem.scissorOffset.y * 2
	};

	// Lookup vertices of the last rendered triangle and software-transform them
	// This allows us to determine the depth slope, which will be used if zfreeze
	// is enabled in the following flush.
	float *vout = out;
	for (u32 i = 0; i < 3; ++i, vout += 4)
	{
		u8* vtx_ptr = s_pBaseBufferPointer + vert_decl.stride * indices[i];
		VertexShaderManager::TransformToClipSpace(vtx_ptr, vert_decl, vout);
		float w = 1.0f / vout[3];
		// Transform to Screenspace
		vout[0] = vout[0] * w * xfmem.viewport.wd + viewOffset[0];
		vout[1] = vout[1] * w * xfmem.viewport.ht + viewOffset[1];
		vout[2] = vout[2] * w * xfmem.viewport.zRange + xfmem.viewport.farZ;
	}

	float dx31 = out[8] - out[0];
	float dx12 = out[0] - out[4];
	float dy12 = out[1] - out[5];
	float dy31 = out[9] - out[1];
	float c = -dx12 * dy31 - dx31 * -dy12;
	
	if (c == 0)
		return;

	float DF31 = out[10] - out[2];
	float DF21 = out[6] - out[2];
	float a = DF31 * -dy12 - DF21 * dy31;
	float b = dx31 * DF21 + dx12 * DF31;
	s_ZSlope.dfdx = -a / c;
	s_ZSlope.dfdy = -b / c;
	s_ZSlope.f0 = out[2] - (out[0] * s_ZSlope.dfdx + out[1] * s_ZSlope.dfdy);
	s_Zslope_Refresh_Required = true;
}

void VertexManagerBase::SetZSlope()
{
	if (s_Zslope_Refresh_Required)
	{
		PixelShaderManager::SetZSlope(s_ZSlope.dfdx, s_ZSlope.dfdy, s_ZSlope.f0);
		s_Zslope_Refresh_Required = false;
	}
}