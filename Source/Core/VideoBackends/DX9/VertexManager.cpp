// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/FileUtil.h"

#include "VideoBackends/DX9/D3DBase.h"
#include "VideoBackends/DX9/PixelShaderCache.h"
#include "VideoBackends/DX9/TextureCache.h"
#include "VideoBackends/DX9/VertexManager.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFStructs.h"

// internal state for loading vertices
namespace DX9
{
// This are the initially requeted size for the buffers expresed in elements
const u32 MAX_IBUFFER_SIZE = VertexManagerBase::MAXIBUFFERSIZE * sizeof(u16) * 8;
const u32 MAX_VBUFFER_SIZE = VertexManagerBase::MAXVBUFFERSIZE;
const u32 MAX_VBUFFER_COUNT = 2;
// Register count for line/point offset storage
const u32 LINE_PT_OFFSETS_PARAMS_LEN = C_VENVCONST_END - C_PLOFFSETPARAMS;

#define PLO_ZERO 0

#define PLO_TEX_MASK_POINT_0_3 1
#define PLO_TEX_MASK_POINT_4_7 2
#define PLO_TEX_MASK_LINE_0_3 3
#define PLO_TEX_MASK_LINE_4_7 4

#define PLO_TEX_POINT_X 5
#define PLO_TEX_POINT_Y 6
#define PLO_TEX_POINT_XY 7

#define PLO_TEX_LINE 8

#define PLO_POS_POINT_LEFT_TOP 5
#define PLO_POS_POINT_LEFT_BOTTOM 6
#define PLO_POS_POINT_RIGHT_TOP 7
#define PLO_POS_POINT_RIGHT_BOTTOM 8

#define PLO_POS_LINE_NEGATIVE_X 9
#define PLO_POS_LINE_NEGATIVE_Y 10
#define PLO_POS_LINE_POSITIVE_X 11
#define PLO_POS_LINE_POSITIVE_Y 12


typedef struct Float_2
{
	float x, y;
}Float_2;

typedef struct Float_4
{
	Float_4(float px, float py, float pz, float pw)
	{
		x = px;
		y = py;
		z = pz;
		w = pw;
	}
	Float_4()
	{
		x = 0.f;
		y = 0.f;
		z = 0.f;
		w = 0.f;
	}
	float x, y, z, w;
}Float_4;

typedef struct U8_4
{
	u8 x, y, z, w;
}U8_4;

static const float LINE_PT_TEX_OFFSETS[8] = {
	0.f,
	0.0625f,
	0.125f,
	0.25f,
	0.5f,
	1.f,
	1.f,
	1.f
};

inline void DumpBadShaders()
{
#if defined(_DEBUG) || defined(DEBUGFAST)
	// TODO: Reimplement!
	/*	std::string error_shaders;
	error_shaders.append(VertexShaderCache::GetCurrentShaderCode());
	error_shaders.append(PixelShaderCache::GetCurrentShaderCode());
	char filename[512] = "bad_shader_combo_0.txt";
	int which = 0;
	while (File::Exists(filename))
	{
	which++;
	sprintf(filename, "bad_shader_combo_%i.txt", which);
	}
	File::WriteStringToFile(true, error_shaders, filename);
	PanicAlert("DrawIndexedPrimitiveUP failed. Shaders written to %s", filename);*/
#endif
}

#define CHECK(hr, Message, ...) if (FAILED(hr)) { PanicAlert(__FUNCTION__ "Failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); return;}

void VertexManager::CreateDeviceObjects()
{
	HRESULT hr = S_OK;
	m_buffers_count = 0;
	m_vertex_buffers = nullptr;
	m_index_buffers = nullptr;
	D3DCAPS9 DeviceCaps = D3D::GetCaps();
	u32 devicevMaxBufferSize = DeviceCaps.MaxPrimitiveCount * 3 * DeviceCaps.MaxStreamStride;
	//Calculate Device Dependant size
	m_vertex_buffer_size = (MAX_VBUFFER_SIZE > devicevMaxBufferSize) ? devicevMaxBufferSize : MAX_VBUFFER_SIZE;
	m_index_buffer_size = (MAX_IBUFFER_SIZE > DeviceCaps.MaxVertexIndex) ? DeviceCaps.MaxVertexIndex : MAX_IBUFFER_SIZE;
	//if device caps are not enough for Vbuffer then fail
	if (m_index_buffer_size < MAXIBUFFERSIZE || m_vertex_buffer_size < MAXVBUFFERSIZE) return;

	m_vertex_buffers = new LPDIRECT3DVERTEXBUFFER9[MAX_VBUFFER_COUNT];
	m_index_buffers = new LPDIRECT3DINDEXBUFFER9[MAX_VBUFFER_COUNT];

	bool Fail = false;
	for (m_current_vertex_buffer = 0; m_current_vertex_buffer < MAX_VBUFFER_COUNT; m_current_vertex_buffer++)
	{
		m_vertex_buffers[m_current_vertex_buffer] = nullptr;
		m_index_buffers[m_current_vertex_buffer] = nullptr;
	}
	for (m_current_vertex_buffer = 0; m_current_vertex_buffer < MAX_VBUFFER_COUNT; m_current_vertex_buffer++)
	{
		hr = D3D::dev->CreateVertexBuffer(m_vertex_buffer_size, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &m_vertex_buffers[m_current_vertex_buffer], nullptr);
		CHECK(hr, "Create vertex buffer ", m_current_vertex_buffer);
		hr = D3D::dev->CreateIndexBuffer(m_index_buffer_size * sizeof(u16), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m_index_buffers[m_current_vertex_buffer], nullptr);
	}
	m_buffers_count = m_current_vertex_buffer;
	m_current_vertex_buffer = 0;
	m_current_index_buffer = 0;
	m_index_buffer_cursor = m_index_buffer_size;
	m_vertex_buffer_cursor = m_vertex_buffer_size;
	m_last_stride = 0;
	VertexShaderManager::EnableDirtyRegions();
	PixelShaderManager::EnableDirtyRegions();
}

void VertexManager::DestroyDeviceObjects()
{
	D3D::SetStreamSource(0, nullptr, 0, 0);
	D3D::SetIndices(nullptr);
	for (int i = 0; i < MAX_VBUFFER_COUNT; i++)
	{
		if (m_vertex_buffers)
		{
			if (m_vertex_buffers[i])
			{
				m_vertex_buffers[i]->Release();
				m_vertex_buffers[i] = nullptr;
			}
		}
		if (m_index_buffers)
		{
			if (m_index_buffers[i])
			{
				m_index_buffers[i]->Release();
				m_index_buffers[i] = nullptr;
			}
		}
	}
	if (m_vertex_buffers)
		delete[] m_vertex_buffers;
	if (m_index_buffers)
		delete[] m_index_buffers;
	m_vertex_buffers = nullptr;
	m_index_buffers = nullptr;
}

VertexManager::VertexManager()
{
	LocalVBuffer.resize(MAXVBUFFERSIZE);

	m_pCurBufferPointer = m_pBaseBufferPointer = &LocalVBuffer[0];
	m_pEndBufferPointer = m_pBaseBufferPointer + LocalVBuffer.size();

	LocalIBuffer.resize(MAXIBUFFERSIZE);
}

VertexManager::~VertexManager()
{

}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
	u8* p_vertices_base;
	u8* p_vertices;
	u16* p_indices;
	u16* indices = GetIndexBuffer();
	u32 total_data_size = m_total_num_verts * stride;
	u32 data_size = m_num_verts * stride;
	u16 current_index = m_num_verts;
	if (m_index_len)
	{
		DWORD LockMode = D3DLOCK_NOOVERWRITE;
		m_vertex_buffer_cursor--;
		m_vertex_buffer_cursor = m_vertex_buffer_cursor - (m_vertex_buffer_cursor % stride) + stride;
		if (m_vertex_buffer_cursor > m_vertex_buffer_size - total_data_size)
		{
			LockMode = D3DLOCK_DISCARD;
			m_vertex_buffer_cursor = 0;
			m_current_vertex_buffer = (m_current_vertex_buffer + 1) % m_buffers_count;
		}
		if (FAILED(m_vertex_buffers[m_current_vertex_buffer]->Lock(m_vertex_buffer_cursor, total_data_size, (VOID**)(&p_vertices_base), LockMode)))
		{
			DestroyDeviceObjects();
			return;
		}
		LockMode = D3DLOCK_NOOVERWRITE;
		if (m_index_buffer_cursor > m_index_buffer_size - m_total_index_len)
		{
			LockMode = D3DLOCK_DISCARD;
			m_index_buffer_cursor = 0;
			m_current_index_buffer = (m_current_index_buffer + 1) % m_buffers_count;
		}
		if (FAILED(m_index_buffers[m_current_index_buffer]->Lock(m_index_buffer_cursor * sizeof(u16), m_total_index_len * sizeof(u16), (VOID**)(&p_indices), LockMode)))
		{
			DestroyDeviceObjects();
			return;
		}
		memcpy(p_vertices_base, m_pBaseBufferPointer, data_size);
		p_vertices = p_vertices_base + data_size;
		if (m_current_primitive_type == PRIMITIVE_TRIANGLES)
		{
			memcpy(p_indices, indices, m_index_len * sizeof(u16));
		}
		else if (m_current_primitive_type == PRIMITIVE_LINES)
		{
			for (u32 i = 0; i < (m_index_len - 1); i += 2)
			{
				// Get Line Indices
				u16 first_index = indices[i];
				u16 second_index = indices[i + 1];
				// Get the position in the stream o f the first vertex
				u32 currentstride = first_index * stride;
				// Get The first vertex Position data
				Float_2* base_vertex_0 = (Float_2*)(m_pBaseBufferPointer + currentstride);
				// Get The blendindices data
				U8_4* blendindices_vertex_0 = (U8_4*)(p_vertices_base + currentstride + stride - sizeof(U8_4));
				// Get The first vertex Position data
				currentstride = second_index * stride;
				Float_2* base_vertex_1 = (Float_2*)(m_pBaseBufferPointer + currentstride);
				U8_4* blendindices_vertex_1 = (U8_4*)(p_vertices_base + currentstride + stride - sizeof(U8_4));

				// Calculate line orientation
				// mostly a hack because we are in object space but is better than nothing
				float dx = base_vertex_1->x - base_vertex_0->x;
				float dy = base_vertex_1->y - base_vertex_0->y;
				bool horizontal = fabs(dx) > fabs(dy);
				bool positive = horizontal ? dx > 0 : dy > 0;

				// setup offset index acording to line orientation
				u8 idx0 = horizontal ?
					(positive ? PLO_POS_LINE_NEGATIVE_Y : PLO_POS_LINE_POSITIVE_Y) :
					(positive ? PLO_POS_LINE_POSITIVE_X : PLO_POS_LINE_NEGATIVE_X);
				u8 idx1 = horizontal ?
					(positive ? PLO_POS_LINE_POSITIVE_Y : PLO_POS_LINE_NEGATIVE_Y) :
					(positive ? PLO_POS_LINE_NEGATIVE_X : PLO_POS_LINE_POSITIVE_X);


				memcpy(p_vertices, base_vertex_0, stride);
				p_vertices += stride;
				U8_4* blendindices_vertex_2 = (U8_4*)(p_vertices - sizeof(U8_4));
				memcpy(p_vertices, base_vertex_1, stride);
				p_vertices += stride;
				U8_4* blendindices_vertex_3 = (U8_4*)(p_vertices - sizeof(U8_4));

				// Setup Blend Indices
				blendindices_vertex_0->y = PLO_TEX_MASK_LINE_0_3;
				blendindices_vertex_0->z = idx0;
				blendindices_vertex_0->w = PLO_ZERO;

				blendindices_vertex_1->y = PLO_TEX_MASK_LINE_0_3;
				blendindices_vertex_1->z = idx0;
				blendindices_vertex_1->w = PLO_ZERO;

				blendindices_vertex_2->y = PLO_TEX_MASK_LINE_0_3;
				blendindices_vertex_2->z = idx1;
				blendindices_vertex_2->w = PLO_TEX_LINE;

				blendindices_vertex_3->y = PLO_TEX_MASK_LINE_0_3;
				blendindices_vertex_3->z = idx1;
				blendindices_vertex_3->w = PLO_TEX_LINE;


				// Setup new triangle indices
				*p_indices = first_index;
				p_indices++;

				*p_indices = current_index;
				current_index++;
				p_indices++;

				*p_indices = current_index;
				p_indices++;

				*p_indices = current_index;
				current_index++;
				p_indices++;

				*p_indices = second_index;
				p_indices++;

				*p_indices = first_index;
				p_indices++;
			}
		}
		else if (m_current_primitive_type == PRIMITIVE_POINTS)
		{
			for (u32 i = 0; i < m_index_len; i++)
			{
				// Get point indes
				u16 pointindex = indices[i];
				// Calculate stream Position
				int currentstride = pointindex * stride;
				// Get data Pointer for vertex replication
				u8* base_vertex = m_pBaseBufferPointer + currentstride;
				U8_4* blendindices_vertex_0 = (U8_4*)(p_vertices_base + currentstride + stride - sizeof(U8_4));

				// Generate Extra vertices
				memcpy(p_vertices, base_vertex, stride);
				p_vertices += stride;
				U8_4* blendindices_vertex_1 = (U8_4*)(p_vertices - sizeof(U8_4));
				memcpy(p_vertices, base_vertex, stride);
				p_vertices += stride;
				U8_4* blendindices_vertex_2 = (U8_4*)(p_vertices - sizeof(U8_4));
				memcpy(p_vertices, base_vertex, stride);
				p_vertices += stride;
				U8_4* blendindices_vertex_3 = (U8_4*)(p_vertices - sizeof(U8_4));

				// Setup Blen Indices
				blendindices_vertex_0->y = PLO_TEX_MASK_POINT_0_3;
				blendindices_vertex_0->z = PLO_POS_POINT_LEFT_TOP;
				blendindices_vertex_0->w = PLO_ZERO;

				blendindices_vertex_1->y = PLO_TEX_MASK_POINT_0_3;
				blendindices_vertex_1->z = PLO_POS_POINT_LEFT_BOTTOM;
				blendindices_vertex_1->w = PLO_TEX_POINT_X;

				blendindices_vertex_2->y = PLO_TEX_MASK_POINT_0_3;
				blendindices_vertex_2->z = PLO_POS_POINT_RIGHT_TOP;
				blendindices_vertex_2->w = PLO_TEX_POINT_Y;

				blendindices_vertex_3->y = PLO_TEX_MASK_POINT_0_3;
				blendindices_vertex_3->z = PLO_POS_POINT_RIGHT_BOTTOM;
				blendindices_vertex_3->w = PLO_TEX_POINT_XY;

				// Setup new triangle indices
				*p_indices = pointindex; // Left Top
				p_indices++;

				*p_indices = current_index; // Left Bottom
				current_index++;
				p_indices++;

				*p_indices = current_index; // Right Top
				p_indices++;

				*p_indices = current_index; // Right Top
				p_indices++;

				*p_indices = current_index - 1; // Left Bottom
				p_indices++;
				current_index++;
				*p_indices = current_index; // Right Bottom
				p_indices++;
				current_index++;
			}
		}
		m_vertex_buffers[m_current_vertex_buffer]->Unlock();
		m_index_buffers[m_current_index_buffer]->Unlock();
	}
	if (m_last_stride != stride || m_vertex_buffer_cursor == 0)
	{
		m_last_stride = stride;
		D3D::SetStreamSource(0, m_vertex_buffers[m_current_vertex_buffer], 0, m_last_stride);
	}
	if (m_index_buffer_cursor == 0)
	{
		D3D::SetIndices(m_index_buffers[m_current_index_buffer]);
	}

	ADDSTAT(stats.thisFrame.bytesVertexStreamed, total_data_size);
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, m_total_index_len);
}

void VertexManager::Draw(u32 stride)
{
	int StartIndex = m_index_buffer_cursor;
	int basevertex = m_vertex_buffer_cursor / stride;
	if (FAILED(D3D::dev->DrawIndexedPrimitive(
		D3DPT_TRIANGLELIST,
		basevertex,
		0,
		m_total_num_verts,
		StartIndex,
		m_primitives)))
	{
		DumpBadShaders();
	}
	INCSTAT(stats.thisFrame.numDrawCalls);
}


void VertexManager::SetPLRasterOffsets()
{
	// calculate base values for the offset parameters
	float vpWidth = 2.0f * xfmem.viewport.wd;
	float vpHeight = -2.0f * xfmem.viewport.ht;
	vpWidth = vpWidth > 0 ? vpWidth : 640.f;
	vpHeight = vpHeight > 0 ? vpHeight : 480.f;
	vpWidth = 1.0f / vpWidth;
	vpHeight = 1.0f / vpHeight;
	float psize = float(bpmem.lineptwidth.pointsize) / 6.0f;
	float psizex = psize * vpWidth;
	float psizey = psize * vpHeight;
	float ptxoff = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.pointoff];
	float lsize = float(bpmem.lineptwidth.linesize) / 6.0f;
	float lsizex = lsize * vpWidth;
	float lsizey = lsize * vpHeight;
	float ltxoff = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.lineoff];
	float* offsets_pointer = VertexShaderManager::GetBufferToUpdate(C_PLOFFSETPARAMS, 13);
	Float_4* line_pt_params = (Float_4*)offsets_pointer;
	// Fill texture offset Mask
	for (int i = 0; i < 8; i++)
	{
		offsets_pointer[i + 4] = bpmem.texcoords[i].s.point_offset ? 1.0f : 0.0f;
		offsets_pointer[i + 12] = bpmem.texcoords[i].s.line_offset ? 1.0f : 0.0f;
	}
	line_pt_params[PLO_POS_POINT_LEFT_TOP] = Float_4(-psizex, psizey, ptxoff, 0.f);
	line_pt_params[PLO_POS_POINT_LEFT_BOTTOM] = Float_4(-psizex, -psizey, 0.f, ptxoff);
	line_pt_params[PLO_POS_POINT_RIGHT_TOP] = Float_4(psizex, psizey, ptxoff, ptxoff);
	line_pt_params[PLO_POS_POINT_RIGHT_BOTTOM] = Float_4(psizex, -psizey, ltxoff, 0.f);
	line_pt_params[PLO_POS_LINE_NEGATIVE_X].x = -lsizex;
	line_pt_params[PLO_POS_LINE_NEGATIVE_Y].y = -lsizey;
	line_pt_params[PLO_POS_LINE_POSITIVE_X].x = lsizex;
	line_pt_params[PLO_POS_LINE_POSITIVE_Y].y = lsizey;
}

void DX9::VertexManager::PrepareShaders(PrimitiveType primitive, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread)
{
	const bool useDstAlpha = bpm.dstalpha.enable && bpm.blendmode.alphaupdate &&
		bpm.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	const bool useDualSource = useDstAlpha && g_ActiveConfig.backend_info.bSupportsDualSourceBlend;
	const bool forced_early_z = bpm.UseEarlyDepthTest() && bpm.zmode.updateenable && bpm.alpha_test.TestResult() == AlphaTest::UNDETERMINED && !g_ActiveConfig.bFastDepthCalc;
	g_ActiveConfig.backend_info.bSupportsEarlyZ = !g_ActiveConfig.bFastDepthCalc;
	PIXEL_SHADER_RENDER_MODE render_mode = forced_early_z ? PSRM_DEPTH_ONLY : (useDualSource ? PSRM_DUAL_SOURCE_BLEND : PSRM_DEFAULT);
	VertexShaderCache::PrepareShader(components, xfr, bpm, ongputhread);
	PixelShaderCache::PrepareShader(render_mode, components, xfr, bpm, ongputhread);
	if (forced_early_z)
	{
		PixelShaderCache::PrepareShader(useDualSource ? PSRM_DUAL_SOURCE_BLEND : PSRM_DEFAULT, components, xfr, bpm, ongputhread);
	}
	if (useDstAlpha && !useDualSource)
	{
		PixelShaderCache::PrepareShader(PSRM_ALPHA_PASS, components, xfr, bpm, ongputhread);
	}
}

void VertexManager::vFlush(bool useDstAlpha)
{
	// initialize all values for the current flush
	m_index_len = IndexGenerator::GetIndexLen();
	m_num_verts = IndexGenerator::GetNumVerts();
	m_total_num_verts = m_num_verts;
	m_total_index_len = m_index_len;
	NativeVertexFormat* current_vertex_format = VertexLoaderManager::GetCurrentVertexFormat();
	const u32 stride = current_vertex_format->GetVertexStride();
	switch (m_current_primitive_type)
	{
	case PRIMITIVE_POINTS:
		// We need point emulation so setup values to allow point to triangle translation
		// 3 extra vertices will be generated for each point
		m_total_num_verts += m_index_len * 3;
		// 6 indices for each point
		m_total_index_len *= 6;
		break;
	case PRIMITIVE_LINES:
		// We need line emulation so transform the values to allow lines to triangle translation
		// we will generate 2 extra vertex for each line (the same amount of original indices)
		m_total_num_verts += m_index_len;
		// for each line, 6 indices will be generated ( 3 times the amount of original indices )
		m_total_index_len *= 3;
		break;
	default:
		break;
	}
	m_primitives = m_total_index_len / 3;
	// set global constants

	const bool useDualSource = useDstAlpha && g_ActiveConfig.backend_info.bSupportsDualSourceBlend;
	const bool forced_early_z = bpmem.UseEarlyDepthTest() && bpmem.zmode.updateenable && bpmem.alpha_test.TestResult() == AlphaTest::UNDETERMINED && !g_ActiveConfig.bFastDepthCalc;
	PIXEL_SHADER_RENDER_MODE render_mode = forced_early_z ? PSRM_DEPTH_ONLY : (useDualSource ? PSRM_DUAL_SOURCE_BLEND : PSRM_DEFAULT);
	if (!VertexShaderCache::TestShader())
	{
		goto shader_fail;
	}
	if (!PixelShaderCache::SetShader(render_mode))
	{
		goto shader_fail;
	}
	g_renderer->ApplyState(false);
	if (m_current_primitive_type != PRIMITIVE_TRIANGLES)
	{
		// if we use emulation setup the offsets for the vertex shaders
		SetPLRasterOffsets();
		// Disable Culling
		D3D::ChangeRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	}
	if (VertexShaderManager::IsDirty())
	{
		const regionvector & regions = VertexShaderManager::GetDirtyRegions();
		const float* buffer = VertexShaderManager::GetBuffer();
		for (size_t i = 0; i < regions.size(); i++)
		{
			const std::pair<u32, u32> &region = regions[i];
			DX9::D3D::dev->SetVertexShaderConstantF(region.first, &buffer[region.first * 4], region.second - region.first + 1);
		}
		VertexShaderManager::Clear();
	}
	if (PixelShaderManager::IsDirty())
	{
		const regionvector & regions = PixelShaderManager::GetDirtyRegions();
		const float* buffer = PixelShaderManager::GetBuffer();
		for (size_t i = 0; i < regions.size(); i++)
		{
			const std::pair<u32, u32> &region = regions[i];
			DX9::D3D::dev->SetPixelShaderConstantF(region.first, &buffer[region.first * 4], region.second - region.first + 1);
		}
		PixelShaderManager::Clear();
	}
	PrepareDrawBuffers(stride);
	if (forced_early_z)
	{
		D3D::ChangeRenderState(D3DRS_COLORWRITEENABLE, 0);
	}
	current_vertex_format->SetupVertexPointers();
	Draw(stride);
	if (forced_early_z)
	{
		D3D::RefreshRenderState(D3DRS_COLORWRITEENABLE);
		D3D::ChangeRenderState(D3DRS_ZWRITEENABLE, FALSE);
		D3D::ChangeRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
		render_mode = useDualSource ? PSRM_DUAL_SOURCE_BLEND : PSRM_DEFAULT;
		if (!PixelShaderCache::SetShader(render_mode))
		{
			goto shader_fail;
		}
		Draw(stride);
		D3D::RefreshRenderState(D3DRS_ZWRITEENABLE);
		D3D::RefreshRenderState(D3DRS_ZFUNC);
	}

	if (useDstAlpha && !useDualSource)
	{
		if (!PixelShaderCache::SetShader(PSRM_ALPHA_PASS))
		{
			goto shader_fail;
		}
		// update alpha only
		g_renderer->ApplyState(true);
		Draw(stride);
		g_renderer->RestoreState();
	}
	if (m_current_primitive_type != PRIMITIVE_TRIANGLES)
	{
		D3D::RefreshRenderState(D3DRS_CULLMODE);
	}
shader_fail:
	m_index_buffer_cursor += m_total_index_len;
	m_vertex_buffer_cursor += (m_total_num_verts)* stride;
}

void VertexManager::ResetBuffer(u32 stride)
{
	m_pCurBufferPointer = m_pBaseBufferPointer;
	IndexGenerator::Start(GetIndexBuffer());
}

}
