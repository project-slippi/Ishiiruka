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

	dirty = true;
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
		if (constants.tessparams[0] != tessmin || constants.tessparams[1] != tessmax || constants.tessparams[2] != rounding)
		{
			constants.tessparams[0] = tessmin;
			constants.tessparams[1] = tessmax;
			constants.tessparams[2] = rounding;
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

void HullDomainShaderManager::DoState(PointerWrap &p)
{
	p.Do(s_projection_changed);
	p.Do(s_viewport_changed);

	p.Do(constants);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		// Fixup the current state from global GPU state
		// NOTE: This requires that all GPU memory has been loaded already.
		Dirty();
	}
}
