// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cfloat>
#include <cmath>

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/TessellationShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/VertexShaderManager.h"


TessellationShaderConstants TessellationShaderManager::constants;
bool TessellationShaderManager::dirty;

void TessellationShaderManager::Init()
{
	memset(&constants, 0, sizeof(constants));
	dirty = true;
}

void TessellationShaderManager::Shutdown()
{
}

void TessellationShaderManager::Dirty()
{
	// This function is called after a savestate is loaded.
	// Any constants that can changed based on settings should be re-calculated
	dirty = true;
}

void TessellationShaderManager::SetConstants()
{
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

void TessellationShaderManager::DoState(PointerWrap &p)
{
	p.Do(constants);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		// Fixup the current state from global GPU state
		// NOTE: This requires that all GPU memory has been loaded already.
		Dirty();
	}
}
