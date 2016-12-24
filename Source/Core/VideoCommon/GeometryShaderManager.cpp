// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

static const int LINE_PT_TEX_OFFSETS[8] = {
	0, 16, 8, 4, 2, 1, 1, 1
};

alignas(256) GeometryShaderConstants GeometryShaderManager::constants;
bool GeometryShaderManager::dirty;

bool GeometryShaderManager::s_projection_changed;
bool GeometryShaderManager::s_viewport_changed;
bool GeometryShaderManager::s_line_width_changed;
u32 GeometryShaderManager::s_texcoord_changed;

void GeometryShaderManager::Init()
{
	constants = {};

	// Init any intial constants which aren't zero when bpmem is zero.
	dirty = true;
	s_projection_changed = true;
	s_viewport_changed = true;
	s_line_width_changed = true;
	s_texcoord_changed = 255;
}

void GeometryShaderManager::Shutdown()
{}

void GeometryShaderManager::Dirty()
{
	// This function is called after a savestate is loaded.
	// Any constants that can changed based on settings should be re-calculated
	s_projection_changed = true;

	dirty = true;
}

void GeometryShaderManager::SetConstants()
{
	if (s_projection_changed && g_ActiveConfig.iStereoMode > 0)
	{
		if (xfmem.projection.type == GX_PERSPECTIVE)
		{
			float offset = (g_ActiveConfig.iStereoDepth / 1000.0f) * (g_ActiveConfig.iStereoDepthPercentage / 100.0f);
			constants.stereoparams[0] = g_ActiveConfig.bStereoSwapEyes ? offset : -offset;
			constants.stereoparams[1] = g_ActiveConfig.bStereoSwapEyes ? -offset : offset;
		}
		else
		{
			constants.stereoparams[0] = constants.stereoparams[1] = 0;
		}

		constants.stereoparams[2] = (float)(g_ActiveConfig.iStereoConvergence * (g_ActiveConfig.iStereoConvergencePercentage / 100.0f));

		s_projection_changed = false;
		dirty = true;
	}

	if (s_viewport_changed)
	{
		constants.lineptparams[0] = 2.0f * xfmem.viewport.wd;
		constants.lineptparams[1] = -2.0f * xfmem.viewport.ht;

		s_viewport_changed = false;
		dirty = true;
	}

	if (s_line_width_changed)
	{
		constants.lineptparams[2] = bpmem.lineptwidth.linesize / 6.f;
		constants.lineptparams[3] = bpmem.lineptwidth.pointsize / 6.f;
		constants.texoffset[2] = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.lineoff];
		constants.texoffset[3] = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.pointoff];

		s_line_width_changed = false;
		dirty = true;
	}
	if (s_texcoord_changed)
	{
		for (int texmapid = 0; texmapid < 8; texmapid++)
		{
			int bitmask = 1 << texmapid;
			if (s_texcoord_changed & bitmask)
			{
				TCoordInfo& tc = bpmem.texcoords[texmapid];
				constants.texoffset[0] &= ~bitmask;
				constants.texoffset[0] |= tc.s.line_offset << texmapid;
				constants.texoffset[1] &= ~bitmask;
				constants.texoffset[1] |= tc.s.point_offset << texmapid;
			}
		}
		s_texcoord_changed = 0;
		dirty = true;
	}
}

void GeometryShaderManager::DoState(PointerWrap &p)
{
	p.Do(s_projection_changed);
	p.Do(s_viewport_changed);

	p.Do(constants);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		// Fixup the current state from global GPU state
		// NOTE: This requires that all GPU memory has been loaded already.
		dirty = true;
		s_projection_changed = true;
		s_viewport_changed = true;
		s_line_width_changed = true;
		s_texcoord_changed = 255;
	}
}
