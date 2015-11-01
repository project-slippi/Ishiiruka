// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cfloat>
#include <cmath>

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/HullDomainShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/VertexShaderManager.h"
const float U24_NORM_COEF = 1.0f / 16777216.0f;

HullDomainShaderConstants HullDomainShaderManager::constants;
bool HullDomainShaderManager::dirty;

static bool s_projection_changed;
static bool s_viewport_changed;
static u8 s_nTexDimsChanged;
static u8 s_nIndTexScaleChanged;
static int s_nIndTexMtxChanged;
static u32 lastTexDims[8]; // width | height << 16 | wrap_s << 28 | wrap_t << 30

void HullDomainShaderManager::Init()
{
	memset(&constants, 0, sizeof(constants));

	// Init any intial constants which aren't zero when bpmem is zero.
	SetViewportChanged();
	SetProjectionChanged();

	dirty = true;
}

void HullDomainShaderManager::Shutdown()
{
}

void HullDomainShaderManager::Dirty()
{
	// This function is called after a savestate is loaded.
	// Any constants that can changed based on settings should be re-calculated
	s_projection_changed = true;
	s_nTexDimsChanged = 0xFF;
	s_nIndTexScaleChanged = 0xFF;
	s_nIndTexMtxChanged = 15;
	dirty = true;
}

void HullDomainShaderManager::SetHDSTextureDims(int texid)
{
	// texdims.xy are reciprocals of the real texture dimensions
	// texdims.zw are the scaled dimensions
	TCoordInfo& tc = bpmem.texcoords[texid];
	constants.texdims[texid][0] = 1.0f / (float)(lastTexDims[texid] & 0xffff);
	constants.texdims[texid][1] = 1.0f / (float)((lastTexDims[texid] >> 16) & 0xfff);
	constants.texdims[texid][2] = (float)(tc.s.scale_minus_1 + 1);
	constants.texdims[texid][3] = (float)(tc.t.scale_minus_1 + 1);
}

void HullDomainShaderManager::SetConstants()
{
	if (s_projection_changed)
	{
		s_projection_changed = false;
		if (g_ActiveConfig.TessellationEnabled())
		{
			memcpy(constants.projection, VertexShaderManager::GetBuffer(), 16 * sizeof(float));
			dirty = true;
		}
	}

	if (s_nTexDimsChanged)
	{
		for (int i = 0; i < 8; ++i)
		{
			if ((s_nTexDimsChanged & (1 << i)))
			{
				SetHDSTextureDims(i);
				s_nTexDimsChanged &= ~(1 << i);
			}
		}
	}

	// indirect incoming texture scales
	if (s_nIndTexScaleChanged)
	{
		// set as two sets of vec4s, each containing S and T of two ind stages.
		if (s_nIndTexScaleChanged & 0x03)
		{
			int4& current = constants.indtexscale[0];
			current[0] = bpmem.texscale[0].ss0;
			current[1] = bpmem.texscale[0].ts0;
			current[2] = bpmem.texscale[0].ss1;
			current[3] = bpmem.texscale[0].ts1;
		}

		if (s_nIndTexScaleChanged & 0x0c)
		{
			int4& current = constants.indtexscale[1];
			current[0] = bpmem.texscale[1].ss0;
			current[1] = bpmem.texscale[1].ts0;
			current[2] = bpmem.texscale[1].ss1;
			current[3] = bpmem.texscale[1].ts1;
		}
		s_nIndTexScaleChanged = 0;
	}

	if (s_nIndTexMtxChanged)
	{
		for (int i = 0; i < 3; ++i)
		{
			if (s_nIndTexMtxChanged & (1 << i))
			{
				int scale = ((u32)bpmem.indmtx[i].col0.s0 << 0) |
					((u32)bpmem.indmtx[i].col1.s1 << 2) |
					((u32)bpmem.indmtx[i].col2.s2 << 4);
				scale = 17 - scale;
				int4& current = constants.indtexmtx[2 * i];
				current[0] = bpmem.indmtx[i].col0.ma;
				current[1] = bpmem.indmtx[i].col1.mc;
				current[2] = bpmem.indmtx[i].col2.me;
				current[3] = scale;
				s_nIndTexMtxChanged &= ~(1 << i);
			}
		}
	}

	if (s_viewport_changed)
	{
		s_viewport_changed = false;
		if (g_ActiveConfig.TessellationEnabled())
		{
			const float pixel_center_correction = 0.5f - 7.0f / 12.0f;
			const float pixel_size_x = 2.f / Renderer::EFBToScaledXf(2.f * xfmem.viewport.wd);
			const float pixel_size_y = 2.f / Renderer::EFBToScaledXf(2.f * xfmem.viewport.ht);
			float nearz = xfmem.viewport.farZ - xfmem.viewport.zRange;
			float farz = xfmem.viewport.farZ;
			const bool nonStandartViewport = (g_ActiveConfig.backend_info.APIType != API_OPENGL)
				&& (nearz < 0.f || farz > 16777216.0f || nearz >= 16777216.0f || farz <= 0.f);
			float rangez = 1.0f;
			if (nonStandartViewport)
			{
				farz *= U24_NORM_COEF;
				rangez = xfmem.viewport.zRange * U24_NORM_COEF;
			}
			else
			{
				farz = 1.0f;
			}
			constants.depthparams[0] = farz;
			constants.depthparams[1] = rangez;
			constants.depthparams[2] = pixel_center_correction * pixel_size_x;
			constants.depthparams[3] = pixel_center_correction * pixel_size_y;
		}
		dirty = true;
	}
	if (g_ActiveConfig.TessellationEnabled())
	{
		float tessmin = float(g_ActiveConfig.iTessellationMin);
		float tessmax = float(g_ActiveConfig.iTessellationMax);
		float rounding = float(g_ActiveConfig.iTessellationRoundingIntensity) * 0.01f;
		float displacement = float(g_ActiveConfig.iTessellationDisplacementIntensity) * 0.01f;
		if (constants.tessparams[0] != tessmin
			|| constants.tessparams[1] != tessmax
			|| constants.tessparams[2] != rounding
			|| constants.tessparams[3] != displacement)
		{
			constants.tessparams[0] = tessmin;
			constants.tessparams[1] = tessmax;
			constants.tessparams[2] = rounding;
			constants.tessparams[3] = displacement;
			dirty = true;
		}
	}
}

void HullDomainShaderManager::SetViewportChanged()
{
	s_viewport_changed = true;
}

void HullDomainShaderManager::SetProjectionChanged()
{
	s_projection_changed = true;
}

void HullDomainShaderManager::SetTexDims(int texmapid, u32 width, u32 height)
{
	u32 wh = width | (height << 16);
	if (lastTexDims[texmapid] != wh)
	{
		lastTexDims[texmapid] = wh;
		s_nTexDimsChanged |= 1 << texmapid;
	}
}

void HullDomainShaderManager::SetIndMatrixChanged(int matrixidx)
{
	s_nIndTexMtxChanged |= 1 << matrixidx;
}

void HullDomainShaderManager::SetTexCoordChanged(u8 texmapid)
{
	s_nTexDimsChanged |= 1 << texmapid;
}

void HullDomainShaderManager::SetIndTexScaleChanged(bool high)
{
	s_nIndTexScaleChanged |= high ? 0x0c : 0x03;
}

void HullDomainShaderManager::SetFlags(int index, int mask, int value)
{
	int newflag = (constants.flags[index] & (~mask)) | (value & mask);
	if (newflag != constants.flags[index])
	{
		dirty = true;
		constants.flags[index] = newflag;
	}
}

void HullDomainShaderManager::DoState(PointerWrap &p)
{
	p.Do(s_projection_changed);
	p.Do(s_viewport_changed);
	p.Do(lastTexDims);
	p.Do(constants);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		// Fixup the current state from global GPU state
		// NOTE: This requires that all GPU memory has been loaded already.
		Dirty();
	}
}
