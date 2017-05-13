// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#include <cmath>
#include <sstream>
#include <float.h>

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"

alignas(256) float VertexShaderManager::vsconstants[VertexShaderManager::ConstantBufferSize];
ConstatBuffer VertexShaderManager::m_buffer(VertexShaderManager::vsconstants, VertexShaderManager::ConstantBufferSize);

alignas(256) float g_fProjectionMatrix[16];

// track changes
bool VertexShaderManager::bProjectionChanged;
bool VertexShaderManager::bViewportChanged;
int VertexShaderManager::s_materials_changed;

static bool s_tex_matrices_changed[2];
static int s_transform_matrices_changed[2]; // min,max
static int s_normal_matrices_changed[2]; // min,max
static int nPostTransformMatricesChanged[2]; // min,max
static int s_lights_changed[2]; // min,max
static int s_lights_phong[8];

static Matrix44 s_viewportCorrection;
static Matrix33 s_viewRotationMatrix;
static Matrix33 s_viewInvRotationMatrix;
static float s_fViewTranslationVector[3];
static float s_fViewRotation[2];

const float U8_NORM_COEF = 1.0f / 255.0f;
const float U10_NORM_COEF = 1.0f / 1023.0f;
const float U24_NORM_COEF = 1.0f / 16777216.0f;

struct ProjectionHack
{
	float sign;
	float value;
	ProjectionHack()
	{}
	ProjectionHack(float new_sign, float new_value)
		: sign(new_sign), value(new_value)
	{}
};

namespace
{
// Control Variables
static ProjectionHack g_ProjHack1;
static ProjectionHack g_ProjHack2;
} // Namespace

static float PHackValue(std::string sValue)
{
	float f = 0;
	bool fp = false;
	const char *cStr = sValue.c_str();
	char *c = new char[strlen(cStr) + 1];
	std::istringstream sTof("");

	for (unsigned int i = 0; i <= strlen(cStr); ++i)
	{
		if (i == 20)
		{
			c[i] = '\0';
			break;
		}

		c[i] = (cStr[i] == ',') ? '.' : *(cStr + i);
		if (c[i] == '.')
			fp = true;
	}

	cStr = c;
	sTof.str(cStr);
	sTof >> f;

	if (!fp)
		f /= 0xF4240;

	delete[] c;
	return f;
}

void UpdateProjectionHack(int iPhackvalue[], std::string sPhackvalue[])
{
	float fhackvalue1 = 0, fhackvalue2 = 0;
	float fhacksign1 = 1.0, fhacksign2 = 1.0;
	bool bProjHack3 = false;
	const char *sTemp[2];

	if (iPhackvalue[0] == 1)
	{
		NOTICE_LOG(VIDEO, "\t\t--- Orthographic Projection Hack ON ---");

		fhacksign1 *= (iPhackvalue[1] == 1) ? -1.0f : fhacksign1;
		sTemp[0] = (iPhackvalue[1] == 1) ? " * (-1)" : "";
		fhacksign2 *= (iPhackvalue[2] == 1) ? -1.0f : fhacksign2;
		sTemp[1] = (iPhackvalue[2] == 1) ? " * (-1)" : "";

		fhackvalue1 = PHackValue(sPhackvalue[0]);
		NOTICE_LOG(VIDEO, "- zNear Correction = (%f + zNear)%s", fhackvalue1, sTemp[0]);

		fhackvalue2 = PHackValue(sPhackvalue[1]);
		NOTICE_LOG(VIDEO, "- zFar Correction =  (%f + zFar)%s", fhackvalue2, sTemp[1]);

		sTemp[0] = "DISABLED";
		bProjHack3 = (iPhackvalue[3] == 1) ? true : bProjHack3;
		if (bProjHack3)
			sTemp[0] = "ENABLED";
		NOTICE_LOG(VIDEO, "- Extra Parameter: %s", sTemp[0]);
	}

	// Set the projections hacks
	g_ProjHack1 = ProjectionHack(fhacksign1, fhackvalue1);
	g_ProjHack2 = ProjectionHack(fhacksign2, fhackvalue2);
}

// Viewport correction :
// In D3D, the viewport rectangle must fit within the render target.
// Say you want a viewport at (ix, iy) with size (iw, ih),
// but your viewport must be clamped at (ax, ay) with size (aw, ah).
// Just multiply the projection matrix with the following to get the same
// effect:
// [   (iw/aw)         0     0    ((iw - 2*(ax-ix)) / aw - 1)   ]
// [         0   (ih/ah)     0   ((-ih + 2*(ay-iy)) / ah + 1)   ]
// [         0         0     1                              0   ]
// [         0         0     0                              1   ]
static void ViewportCorrectionMatrix(Matrix44& result)
{
	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;

	// TODO: ceil, floor or just cast to int?
	// TODO: Directly use the floats instead of rounding them?
	float intendedX = xfmem.viewport.xOrig - xfmem.viewport.wd - scissorXOff;
	float intendedY = xfmem.viewport.yOrig + xfmem.viewport.ht - scissorYOff;
	float intendedWd = 2.0f * xfmem.viewport.wd;
	float intendedHt = -2.0f * xfmem.viewport.ht;

	if (intendedWd < 0.f)
	{
		intendedX += intendedWd;
		intendedWd = -intendedWd;
	}
	if (intendedHt < 0.f)
	{
		intendedY += intendedHt;
		intendedHt = -intendedHt;
	}

	// fit to EFB size
	float X = (intendedX >= 0.f) ? intendedX : 0.f;
	float Y = (intendedY >= 0.f) ? intendedY : 0.f;
	float Wd = (X + intendedWd <= EFB_WIDTH) ? intendedWd : (EFB_WIDTH - X);
	float Ht = (Y + intendedHt <= EFB_HEIGHT) ? intendedHt : (EFB_HEIGHT - Y);

	Matrix44::LoadIdentity(result);
	if (Wd == 0 || Ht == 0)
		return;

	result.data[4 * 0 + 0] = intendedWd / Wd;
	result.data[4 * 0 + 3] = (intendedWd - 2.f * (X - intendedX)) / Wd - 1.f;
	result.data[4 * 1 + 1] = intendedHt / Ht;
	result.data[4 * 1 + 3] = (-intendedHt + 2.f * (Y - intendedY)) / Ht + 1.f;
}

void VertexShaderManager::Init()
{
	Dirty();
	m_buffer.Clear();
	memset(&xfmem, 0, sizeof(xfmem));
	ResetView();

	// TODO: should these go inside ResetView()?
	Matrix44::LoadIdentity(s_viewportCorrection);
	memset(g_fProjectionMatrix, 0, sizeof(g_fProjectionMatrix));
	for (int i = 0; i < 4; ++i)
		g_fProjectionMatrix[i * 5] = 1.0f;
}

const float* VertexShaderManager::GetBuffer()
{
	return vsconstants;
}

float* VertexShaderManager::GetBufferToUpdate(u32 const_number, u32 size)
{
	return m_buffer.GetBufferToUpdate<float>(const_number, size);
}

const regionvector &VertexShaderManager::GetDirtyRegions()
{
	return m_buffer.GetRegions();
}

void VertexShaderManager::EnableDirtyRegions()
{
	m_buffer.EnableDirtyRegions();
}
void VertexShaderManager::DisableDirtyRegions()
{
	m_buffer.DisableDirtyRegions();
}

void VertexShaderManager::Dirty()
{
	s_transform_matrices_changed[0] = 0;
	s_transform_matrices_changed[1] = 256;

	s_normal_matrices_changed[0] = 0;
	s_normal_matrices_changed[1] = 96;

	nPostTransformMatricesChanged[0] = 0;
	nPostTransformMatricesChanged[1] = 256;

	s_lights_changed[0] = 0;
	s_lights_changed[1] = 0x80;

	s_tex_matrices_changed[0] = true;
	s_tex_matrices_changed[1] = true;

	bProjectionChanged = true;

	s_materials_changed = 15;
	memset(s_lights_phong, 0, sizeof(s_lights_phong));
}

// Syncs the shader constant buffers with xfmem
// TODO: A cleaner way to control the matrices without making a mess in the parameters field
void VertexShaderManager::SetConstants()
{
	if (g_ActiveConfig.iRimBase != s_lights_phong[0]
		|| g_ActiveConfig.iRimPower != s_lights_phong[1]
		|| g_ActiveConfig.iRimIntesity != s_lights_phong[2]
		|| g_ActiveConfig.iSpecularMultiplier != s_lights_phong[3])
	{
		s_lights_phong[0] = g_ActiveConfig.iRimBase;
		s_lights_phong[1] = g_ActiveConfig.iRimPower;
		s_lights_phong[2] = g_ActiveConfig.iRimIntesity;
		s_lights_phong[3] = g_ActiveConfig.iSpecularMultiplier;
		m_buffer.SetConstant4(C_PHONG
			, float(g_ActiveConfig.iRimBase)
			, 1.0f + U8_NORM_COEF * g_ActiveConfig.iRimPower * 7.0f
			, U8_NORM_COEF * g_ActiveConfig.iRimIntesity
			, U8_NORM_COEF * g_ActiveConfig.iSpecularMultiplier);
	}
	if (g_ActiveConfig.iSimBumpStrength != s_lights_phong[4]
		|| g_ActiveConfig.iSimBumpThreshold != s_lights_phong[5]
		|| g_ActiveConfig.iSimBumpDetailBlend != s_lights_phong[6]
		|| g_ActiveConfig.iSimBumpDetailFrequency != s_lights_phong[7])
	{
		s_lights_phong[4] = g_ActiveConfig.iSimBumpStrength;
		s_lights_phong[5] = g_ActiveConfig.iSimBumpThreshold;
		s_lights_phong[6] = g_ActiveConfig.iSimBumpDetailBlend;
		s_lights_phong[7] = g_ActiveConfig.iSimBumpDetailFrequency;
		float bump_strenght = U10_NORM_COEF * g_ActiveConfig.iSimBumpStrength;
		m_buffer.SetConstant4(C_PHONG + 1
			, bump_strenght * bump_strenght
			, U8_NORM_COEF * g_ActiveConfig.iSimBumpThreshold * 16.0f
			, U8_NORM_COEF * g_ActiveConfig.iSimBumpDetailBlend
			, float(g_ActiveConfig.iSimBumpDetailFrequency));
	}
	if (s_transform_matrices_changed[0] >= 0)
	{
		int startn = s_transform_matrices_changed[0] / 4;
		int endn = (s_transform_matrices_changed[1] + 3) / 4;
		const float* pstart = &xfmem.posMatrices[startn * 4];
		m_buffer.SetMultiConstant4v(C_TRANSFORMMATRICES + startn, endn - startn, pstart);
		s_transform_matrices_changed[0] = s_transform_matrices_changed[1] = -1;
	}

	if (s_normal_matrices_changed[0] >= 0)
	{
		int startn = s_normal_matrices_changed[0] / 3;
		int endn = (s_normal_matrices_changed[1] + 2) / 3;
		const float* pnstart = &xfmem.normalMatrices[3 * startn];
		m_buffer.SetMultiConstant3v(C_NORMALMATRICES + startn, endn - startn, pnstart);
		s_normal_matrices_changed[0] = s_normal_matrices_changed[1] = -1;
	}

	if (nPostTransformMatricesChanged[0] >= 0)
	{
		int startn = nPostTransformMatricesChanged[0] / 4;
		int endn = (nPostTransformMatricesChanged[1] + 3) / 4;
		const float* pstart = &xfmem.postMatrices[startn * 4];
		m_buffer.SetMultiConstant4v(C_POSTTRANSFORMMATRICES + startn, endn - startn, pstart);
		nPostTransformMatricesChanged[0] = nPostTransformMatricesChanged[1] = -1;
	}

	if (s_lights_changed[0] >= 0)
	{
		// lights don't have a 1 to 1 mapping, the color component needs to be converted to 4 floats
		int istart = s_lights_changed[0] / 0x10;
		int iend = (s_lights_changed[1] + 15) / 0x10;

		for (int i = istart; i < iend; ++i)
		{
			const Light& light = xfmem.lights[i];
			// xfmem.light.color is packed as abgr in u8[4], so we have to swap the order
			m_buffer.SetConstant4<float>(C_LIGHTS + 5 * i,
				float(light.color[3]),
				float(light.color[2]),
				float(light.color[1]),
				float(light.color[0]));
			m_buffer.SetConstant3v(C_LIGHTS + 5 * i + 1, light.cosatt);
			if (fabs(light.distatt[0]) < 0.00001f &&
				fabs(light.distatt[1]) < 0.00001f &&
				fabs(light.distatt[2]) < 0.00001f)
			{
				// dist attenuation, make sure not equal to 0!!!
				m_buffer.SetConstant4(C_LIGHTS + 5 * i + 2, 0.00001f, light.distatt[1], light.distatt[2], 0.0f);
			}
			else
			{
				m_buffer.SetConstant3v(C_LIGHTS + 5 * i + 2, light.distatt);
			}
			m_buffer.SetConstant3v(C_LIGHTS + 5 * i + 3, light.dpos);
			double norm = double(light.ddir[0]) * double(light.ddir[0]) +
				double(light.ddir[1]) * double(light.ddir[1]) +
				double(light.ddir[2]) * double(light.ddir[2]);
			norm = 1.0 / sqrt(norm);
			float norm_float = static_cast<float>(norm);
			m_buffer.SetConstant4(C_LIGHTS + 5 * i + 4, light.ddir[0] * norm_float, light.ddir[1] * norm_float, light.ddir[2] * norm_float, 0.0f);
		}

		s_lights_changed[0] = s_lights_changed[1] = -1;
	}

	if (s_materials_changed)
	{
		for (int i = 0; i < 2; ++i)
		{
			if (s_materials_changed & (1 << i))
			{
				u32 data = xfmem.ambColor[i];
				m_buffer.SetConstant4<float>(C_MATERIALS + i,
					float((data >> 24) & 0xFF),
					float((data >> 16) & 0xFF),
					float((data >> 8) & 0xFF),
					float(data & 0xFF));
			}
		}

		for (int i = 0; i < 2; ++i)
		{
			if (s_materials_changed & (1 << (i + 2)))
			{
				u32 data = xfmem.matColor[i];
				m_buffer.SetConstant4<float>(C_MATERIALS + i + 2,
					float((data >> 24) & 0xFF),
					float((data >> 16) & 0xFF),
					float((data >> 8) & 0xFF),
					float(data & 0xFF));
			}
		}

		s_materials_changed = 0;
	}

	if (s_tex_matrices_changed[0])
	{
		s_tex_matrices_changed[0] = false;
		m_buffer.SetMultiConstant4v(C_TEXMATRICES + 3 * 0, 3, xfmem.posMatrices + g_main_cp_state.matrix_index_a.Tex0MtxIdx * 4);
		m_buffer.SetMultiConstant4v(C_TEXMATRICES + 3 * 1, 3, xfmem.posMatrices + g_main_cp_state.matrix_index_a.Tex1MtxIdx * 4);
		m_buffer.SetMultiConstant4v(C_TEXMATRICES + 3 * 2, 3, xfmem.posMatrices + g_main_cp_state.matrix_index_a.Tex2MtxIdx * 4);
		m_buffer.SetMultiConstant4v(C_TEXMATRICES + 3 * 3, 3, xfmem.posMatrices + g_main_cp_state.matrix_index_a.Tex3MtxIdx * 4);
	}

	if (s_tex_matrices_changed[1])
	{
		s_tex_matrices_changed[1] = false;
		m_buffer.SetMultiConstant4v(C_TEXMATRICES + 3 * 0 + 12, 3, xfmem.posMatrices + g_main_cp_state.matrix_index_b.Tex4MtxIdx * 4);
		m_buffer.SetMultiConstant4v(C_TEXMATRICES + 3 * 1 + 12, 3, xfmem.posMatrices + g_main_cp_state.matrix_index_b.Tex5MtxIdx * 4);
		m_buffer.SetMultiConstant4v(C_TEXMATRICES + 3 * 2 + 12, 3, xfmem.posMatrices + g_main_cp_state.matrix_index_b.Tex6MtxIdx * 4);
		m_buffer.SetMultiConstant4v(C_TEXMATRICES + 3 * 3 + 12, 3, xfmem.posMatrices + g_main_cp_state.matrix_index_b.Tex7MtxIdx * 4);
	}

	if (bViewportChanged)
	{
		bViewportChanged = false;
		// The console GPU places the pixel center at 7/12 unless antialiasing
		// is enabled, while D3D11 and OpenGL place it at 0.5, D3D9 at 0.0. See the comment
		// in VertexShaderGen.cpp for details.
		// NOTE: If we ever emulate antialiasing, the sample locations set by
		// BP registers 0x01-0x04 need to be considered here.
		const float pixel_center_correction = ((g_ActiveConfig.backend_info.APIType & API_D3D9) ? 0.0f : 0.5f) - 7.0f / 12.0f;
		const bool bUseVertexRounding =
			g_ActiveConfig.bVertexRounding && g_ActiveConfig.iEFBScale != SCALE_1X;
		float viewport_width = 2.f * xfmem.viewport.wd;
		float viewport_height = 2.f * xfmem.viewport.ht;
		if (!bUseVertexRounding)
		{
			viewport_width = g_renderer->EFBToScaledXf(viewport_width);
			viewport_height = g_renderer->EFBToScaledXf(viewport_height);
		}
		const float pixel_size_x = 2.f / viewport_width;
		const float pixel_size_y = 2.f / viewport_height;
		float rangez = xfmem.viewport.zRange;
		float farz = xfmem.viewport.farZ;
		const bool vertex_depth = g_ActiveConfig.backend_info.bSupportsDepthClamp &&
			((fabs(rangez) > 16777215.0f || fabs(farz) > 16777215.0f) ||
			(rangez < 0.0f && !g_ActiveConfig.backend_info.bSupportsReversedDepthRange));

		if (g_ActiveConfig.backend_info.APIType & API_D3D9)
		{
			if (rangez >= 0.0f)
			{
				rangez = 16777215.0f;
				farz = 16777215.0f;
			}
			rangez = rangez / 16777215.0f;
			farz = 1.0f - (farz / 16777215.0f);
		}
		else if (!vertex_depth)
		{
			rangez = 1.0f;
			farz = 0.0f;
		}
		else if (g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
		{
			if (rangez < 0.0f)
			{
				farz = farz / 16777215.0f;
			}
			else
			{
				farz = 1.0f - (farz / 16777215.0f);
			}
			rangez = fabs(rangez) / 16777215.0f;
		}
		else
		{
			rangez = rangez / 16777215.0f;
			farz = 1.0f - (farz / 16777215.0f);
		}

		m_buffer.SetConstant4(C_DEPTHPARAMS,
			farz,
			rangez,
			pixel_center_correction * pixel_size_x,
			pixel_center_correction * pixel_size_y);
		m_buffer.SetConstant4(C_VIEWPARAMS,
			viewport_width * 0.5f,
			viewport_height * 0.5f,
			pixel_size_x,
			pixel_size_y);
		// This is so implementation-dependent that we can't have it here.
		g_renderer->SetViewport();

		// Update projection if the viewport isn't 1:1 useable
		if (!g_ActiveConfig.backend_info.bSupportsOversizedViewports)
		{
			ViewportCorrectionMatrix(s_viewportCorrection);
			bProjectionChanged = true;
		}
	}

	if (bProjectionChanged)
	{
		if (g_ActiveConfig.backend_info.bSupportsPostProcessing && g_renderer->GetPostProcessor())
		{
			g_renderer->GetPostProcessor()->OnProjectionLoaded(xfmem.projection.type);
		}
		bProjectionChanged = false;

		float *rawProjection = xfmem.projection.rawProjection;

		switch (xfmem.projection.type)
		{
		case GX_PERSPECTIVE:

			g_fProjectionMatrix[0] = rawProjection[0] * g_ActiveConfig.fAspectRatioHackW;
			g_fProjectionMatrix[1] = 0.0f;
			g_fProjectionMatrix[2] = rawProjection[1] * g_ActiveConfig.fAspectRatioHackW;
			g_fProjectionMatrix[3] = 0.0f;

			g_fProjectionMatrix[4] = 0.0f;
			g_fProjectionMatrix[5] = rawProjection[2] * g_ActiveConfig.fAspectRatioHackH;
			g_fProjectionMatrix[6] = rawProjection[3] * g_ActiveConfig.fAspectRatioHackH;
			g_fProjectionMatrix[7] = 0.0f;

			g_fProjectionMatrix[8] = 0.0f;
			g_fProjectionMatrix[9] = 0.0f;
			g_fProjectionMatrix[10] = rawProjection[4];

			g_fProjectionMatrix[11] = rawProjection[5];

			g_fProjectionMatrix[12] = 0.0f;
			g_fProjectionMatrix[13] = 0.0f;
			// Hack to fix depth clipping precision issues (such as Sonic Adventure UI)
			g_fProjectionMatrix[14] = g_ActiveConfig.backend_info.APIType & API_D3D9 ? (-(1.0f + FLT_EPSILON)) : -1.0f;
			g_fProjectionMatrix[15] = 0.0f;

			SETSTAT_FT(stats.gproj_0, g_fProjectionMatrix[0]);
			SETSTAT_FT(stats.gproj_1, g_fProjectionMatrix[1]);
			SETSTAT_FT(stats.gproj_2, g_fProjectionMatrix[2]);
			SETSTAT_FT(stats.gproj_3, g_fProjectionMatrix[3]);
			SETSTAT_FT(stats.gproj_4, g_fProjectionMatrix[4]);
			SETSTAT_FT(stats.gproj_5, g_fProjectionMatrix[5]);
			SETSTAT_FT(stats.gproj_6, g_fProjectionMatrix[6]);
			SETSTAT_FT(stats.gproj_7, g_fProjectionMatrix[7]);
			SETSTAT_FT(stats.gproj_8, g_fProjectionMatrix[8]);
			SETSTAT_FT(stats.gproj_9, g_fProjectionMatrix[9]);
			SETSTAT_FT(stats.gproj_10, g_fProjectionMatrix[10]);
			SETSTAT_FT(stats.gproj_11, g_fProjectionMatrix[11]);
			SETSTAT_FT(stats.gproj_12, g_fProjectionMatrix[12]);
			SETSTAT_FT(stats.gproj_13, g_fProjectionMatrix[13]);
			SETSTAT_FT(stats.gproj_14, g_fProjectionMatrix[14]);
			SETSTAT_FT(stats.gproj_15, g_fProjectionMatrix[15]);
			break;

		case GX_ORTHOGRAPHIC:

			g_fProjectionMatrix[0] = rawProjection[0];
			g_fProjectionMatrix[1] = 0.0f;
			g_fProjectionMatrix[2] = 0.0f;
			g_fProjectionMatrix[3] = rawProjection[1];

			g_fProjectionMatrix[4] = 0.0f;
			g_fProjectionMatrix[5] = rawProjection[2];
			g_fProjectionMatrix[6] = 0.0f;
			g_fProjectionMatrix[7] = rawProjection[3];

			g_fProjectionMatrix[8] = 0.0f;
			g_fProjectionMatrix[9] = 0.0f;
			g_fProjectionMatrix[10] = (g_ProjHack1.value + rawProjection[4]) * ((g_ProjHack1.sign == 0) ? 1.0f : g_ProjHack1.sign);
			g_fProjectionMatrix[11] = (g_ProjHack2.value + rawProjection[5]) * ((g_ProjHack2.sign == 0) ? 1.0f : g_ProjHack2.sign);

			g_fProjectionMatrix[12] = 0.0f;
			g_fProjectionMatrix[13] = 0.0f;

			g_fProjectionMatrix[14] = 0.0f;

			// Hack to fix depth clipping precision issues (such as Sonic Unleashed UI)
			g_fProjectionMatrix[15] = (g_ActiveConfig.backend_info.APIType & API_D3D9) ? 1.0f + FLT_EPSILON : 1.0f;

			SETSTAT_FT(stats.g2proj_0, g_fProjectionMatrix[0]);
			SETSTAT_FT(stats.g2proj_1, g_fProjectionMatrix[1]);
			SETSTAT_FT(stats.g2proj_2, g_fProjectionMatrix[2]);
			SETSTAT_FT(stats.g2proj_3, g_fProjectionMatrix[3]);
			SETSTAT_FT(stats.g2proj_4, g_fProjectionMatrix[4]);
			SETSTAT_FT(stats.g2proj_5, g_fProjectionMatrix[5]);
			SETSTAT_FT(stats.g2proj_6, g_fProjectionMatrix[6]);
			SETSTAT_FT(stats.g2proj_7, g_fProjectionMatrix[7]);
			SETSTAT_FT(stats.g2proj_8, g_fProjectionMatrix[8]);
			SETSTAT_FT(stats.g2proj_9, g_fProjectionMatrix[9]);
			SETSTAT_FT(stats.g2proj_10, g_fProjectionMatrix[10]);
			SETSTAT_FT(stats.g2proj_11, g_fProjectionMatrix[11]);
			SETSTAT_FT(stats.g2proj_12, g_fProjectionMatrix[12]);
			SETSTAT_FT(stats.g2proj_13, g_fProjectionMatrix[13]);
			SETSTAT_FT(stats.g2proj_14, g_fProjectionMatrix[14]);
			SETSTAT_FT(stats.g2proj_15, g_fProjectionMatrix[15]);
			SETSTAT_FT(stats.proj_0, rawProjection[0]);
			SETSTAT_FT(stats.proj_1, rawProjection[1]);
			SETSTAT_FT(stats.proj_2, rawProjection[2]);
			SETSTAT_FT(stats.proj_3, rawProjection[3]);
			SETSTAT_FT(stats.proj_4, rawProjection[4]);
			SETSTAT_FT(stats.proj_5, rawProjection[5]);
			break;

		default:
			ERROR_LOG(VIDEO, "Unknown projection type: %d", xfmem.projection.type);
		}

		PRIM_LOG("Projection: %f %f %f %f %f %f\n", rawProjection[0], rawProjection[1], rawProjection[2], rawProjection[3], rawProjection[4], rawProjection[5]);

		Matrix44 projMtx;
		Matrix44 correctedMtx;
		Matrix44::Set(projMtx, g_fProjectionMatrix);

		if ((g_ActiveConfig.bFreeLook || g_ActiveConfig.iStereoMode) && xfmem.projection.type == GX_PERSPECTIVE)
		{
			Matrix44 mtxA;
			Matrix44 mtxB;
			Matrix44 viewMtx;

			Matrix44::Translate(mtxA, s_fViewTranslationVector);
			Matrix44::LoadMatrix33(mtxB, s_viewRotationMatrix);
			Matrix44::Multiply(mtxB, mtxA, viewMtx); // view = rotation x translation
			Matrix44::Multiply(projMtx, viewMtx, mtxA); // mtxA = projection x view
			Matrix44::Multiply(s_viewportCorrection, mtxA, correctedMtx); // correctedMtx = viewportCorrection x mtxA			
		}
		else
		{
			Matrix44::Multiply(s_viewportCorrection, projMtx, correctedMtx);  // correctedMtx = viewportCorrection x projMtx			
		}
		if (xfmem.viewport.wd < 0.0f)
		{
			correctedMtx.data[0] *= -1.0f;
			correctedMtx.data[1] *= -1.0f;
			correctedMtx.data[2] *= -1.0f;
			correctedMtx.data[3] *= -1.0f;
		}
		if (xfmem.viewport.ht > 0.0f)
		{
			correctedMtx.data[4] *= -1.0f;
			correctedMtx.data[5] *= -1.0f;
			correctedMtx.data[6] *= -1.0f;
			correctedMtx.data[7] *= -1.0f;
		}
		m_buffer.SetMultiConstant4v(C_PROJECTION, 4, correctedMtx.data);
	}
}

void VertexShaderManager::InvalidateXFRange(u32 start, u32 end)
{
	const TMatrixIndexA& ma = g_main_cp_state.matrix_index_a;
	const TMatrixIndexB& mb = g_main_cp_state.matrix_index_b;

	if ((start >= ma.Tex0MtxIdx * 4u && start < ma.Tex0MtxIdx * 4u + 12u) ||
		(start >= ma.Tex1MtxIdx * 4u && start < ma.Tex1MtxIdx * 4u + 12u) ||
		(start >= ma.Tex2MtxIdx * 4u && start < ma.Tex2MtxIdx * 4u + 12u) ||
		(start >= ma.Tex3MtxIdx * 4u && start < ma.Tex3MtxIdx * 4u + 12u))
	{
		s_tex_matrices_changed[0] = true;
	}

	if ((start >= mb.Tex4MtxIdx * 4u && start < mb.Tex4MtxIdx * 4u + 12u) ||
		(start >= mb.Tex5MtxIdx * 4u && start < mb.Tex5MtxIdx * 4u + 12u) ||
		(start >= mb.Tex6MtxIdx * 4u && start < mb.Tex6MtxIdx * 4u + 12u) ||
		(start >= mb.Tex7MtxIdx * 4u && start < mb.Tex7MtxIdx * 4u + 12u))
	{
		s_tex_matrices_changed[1] = true;
	}

	if (start < XFMEM_POSMATRICES_END)
	{
		if (s_transform_matrices_changed[0] == -1)
		{
			s_transform_matrices_changed[0] = start;
			s_transform_matrices_changed[1] = end > XFMEM_POSMATRICES_END ? XFMEM_POSMATRICES_END : end;
		}
		else
		{
			if (s_transform_matrices_changed[0] > static_cast<int>(start)) s_transform_matrices_changed[0] = start;
			if (s_transform_matrices_changed[1] < static_cast<int>(end)) s_transform_matrices_changed[1] = end > XFMEM_POSMATRICES_END ? XFMEM_POSMATRICES_END : end;
		}
	}

	if (start < XFMEM_NORMALMATRICES_END && end > XFMEM_NORMALMATRICES)
	{
		int _start = start < XFMEM_NORMALMATRICES ? 0 : start - XFMEM_NORMALMATRICES;
		int _end = end < XFMEM_NORMALMATRICES_END ? end - XFMEM_NORMALMATRICES : XFMEM_NORMALMATRICES_END - XFMEM_NORMALMATRICES;

		if (s_normal_matrices_changed[0] == -1)
		{
			s_normal_matrices_changed[0] = _start;
			s_normal_matrices_changed[1] = _end;
		}
		else
		{
			if (s_normal_matrices_changed[0] > _start) s_normal_matrices_changed[0] = _start;
			if (s_normal_matrices_changed[1] < _end) s_normal_matrices_changed[1] = _end;
		}
	}

	if (start < XFMEM_POSTMATRICES_END && end > XFMEM_POSTMATRICES)
	{
		int _start = start < XFMEM_POSTMATRICES ? XFMEM_POSTMATRICES : start - XFMEM_POSTMATRICES;
		int _end = end < XFMEM_POSTMATRICES_END ? end - XFMEM_POSTMATRICES : XFMEM_POSTMATRICES_END - XFMEM_POSTMATRICES;

		if (nPostTransformMatricesChanged[0] == -1)
		{
			nPostTransformMatricesChanged[0] = _start;
			nPostTransformMatricesChanged[1] = _end;
		}
		else
		{
			if (nPostTransformMatricesChanged[0] > _start) nPostTransformMatricesChanged[0] = _start;
			if (nPostTransformMatricesChanged[1] < _end) nPostTransformMatricesChanged[1] = _end;
		}
	}

	if (start < XFMEM_LIGHTS_END && end > XFMEM_LIGHTS)
	{
		int _start = start < XFMEM_LIGHTS ? XFMEM_LIGHTS : start - XFMEM_LIGHTS;
		int _end = end < XFMEM_LIGHTS_END ? end - XFMEM_LIGHTS : XFMEM_LIGHTS_END - XFMEM_LIGHTS;

		if (s_lights_changed[0] == -1)
		{
			s_lights_changed[0] = _start;
			s_lights_changed[1] = _end;
		}
		else
		{
			if (s_lights_changed[0] > _start) s_lights_changed[0] = _start;
			if (s_lights_changed[1] < _end)   s_lights_changed[1] = _end;
		}
	}
}

void VertexShaderManager::SetTexMatrixChangedA(u32 Value)
{
	if (g_main_cp_state.matrix_index_a.Hex != Value)
	{
		g_vertex_manager->Flush();
		s_tex_matrices_changed[0] = true;
		g_main_cp_state.matrix_index_a.Hex = Value;
	}
}

void VertexShaderManager::SetTexMatrixChangedB(u32 Value)
{
	if (g_main_cp_state.matrix_index_b.Hex != Value)
	{
		g_vertex_manager->Flush();
		s_tex_matrices_changed[1] = true;
		g_main_cp_state.matrix_index_b.Hex = Value;
	}
}

void VertexShaderManager::TranslateView(float x, float y, float z)
{
	float result[3];
	float vector[3] = { x, z, y };

	Matrix33::Multiply(s_viewInvRotationMatrix, vector, result);

	for (int i = 0; i < 3; i++)
		s_fViewTranslationVector[i] += result[i];

	bProjectionChanged = true;
}

void VertexShaderManager::RotateView(float x, float y)
{
	s_fViewRotation[0] += x;
	s_fViewRotation[1] += y;

	Matrix33 mx;
	Matrix33 my;
	Matrix33::RotateX(mx, s_fViewRotation[1]);
	Matrix33::RotateY(my, s_fViewRotation[0]);
	Matrix33::Multiply(mx, my, s_viewRotationMatrix);

	// reverse rotation
	Matrix33::RotateX(mx, -s_fViewRotation[1]);
	Matrix33::RotateY(my, -s_fViewRotation[0]);
	Matrix33::Multiply(my, mx, s_viewInvRotationMatrix);

	bProjectionChanged = true;
}

void VertexShaderManager::ResetView()
{
	memset(s_fViewTranslationVector, 0, sizeof(s_fViewTranslationVector));
	Matrix33::LoadIdentity(s_viewRotationMatrix);
	Matrix33::LoadIdentity(s_viewInvRotationMatrix);
	s_fViewRotation[0] = s_fViewRotation[1] = 0.0f;

	bProjectionChanged = true;
}

void VertexShaderManager::TransformToClipSpace(const u8* data, const PortableVertexDeclaration &vtx_dcl, float *out)
{
	// First 3 floats
	const float* possrc = (const float*)(data + vtx_dcl.position.offset);
	float pos[3];
	pos[0] = possrc[0];
	pos[1] = possrc[1];
	pos[2] = vtx_dcl.position.components == 3 ? possrc[2] : 0;

	const int mtx_idx = *((const u32*)(data + vtx_dcl.posmtx.offset));
	const float *world_matrix = xfmem.posMatrices + (mtx_idx & 0xFF) * 4;
	const float *proj_matrix = &g_fProjectionMatrix[0];
	float t[3];
	t[0] = pos[0] * world_matrix[0] + pos[1] * world_matrix[1] + pos[2] * world_matrix[2] + world_matrix[3];
	t[1] = pos[0] * world_matrix[4] + pos[1] * world_matrix[5] + pos[2] * world_matrix[6] + world_matrix[7];
	t[2] = pos[0] * world_matrix[8] + pos[1] * world_matrix[9] + pos[2] * world_matrix[10] + world_matrix[11];
	// TODO: this requires g_fProjectionMatrix to be up to date, which is not really a good design decision.
	out[0] = t[0] * proj_matrix[0] + t[1] * proj_matrix[1] + t[2] * proj_matrix[2] + proj_matrix[3];
	out[1] = t[0] * proj_matrix[4] + t[1] * proj_matrix[5] + t[2] * proj_matrix[6] + proj_matrix[7];
	out[2] = t[0] * proj_matrix[8] + t[1] * proj_matrix[9] + t[2] * proj_matrix[10] + proj_matrix[11];
	out[3] = t[0] * proj_matrix[12] + t[1] * proj_matrix[13] + t[2] * proj_matrix[14] + proj_matrix[15];
}


void VertexShaderManager::DoState(PointerWrap &p)
{
	p.Do(g_fProjectionMatrix);
	p.Do(s_viewportCorrection);
	p.Do(s_viewRotationMatrix);
	p.Do(s_viewInvRotationMatrix);
	p.Do(s_fViewTranslationVector);
	p.Do(s_fViewRotation);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		Dirty();
	}
}
