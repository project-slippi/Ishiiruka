// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/GL/GLExtensions/GLExtensions.h"
#include "Common/StringUtil.h"

#include "VideoBackends/OGL/BoundingBox.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/StreamBuffer.h"
#include "VideoBackends/OGL/VertexManager.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
// This are the initially requested size for the buffers expressed in bytes
const u32 MAX_IBUFFER_SIZE = 2 * 1024 * 1024;
const u32 MAX_VBUFFER_SIZE = 32 * 1024 * 1024;


VertexManager::VertexManager() : m_cpu_v_buffer(MAXVBUFFERSIZE), m_cpu_i_buffer(MAXIBUFFERSIZE)
{
	CreateDeviceObjects();
}

VertexManager::~VertexManager()
{
	DestroyDeviceObjects();
}

void VertexManager::CreateDeviceObjects()
{
	m_vertexBuffer = StreamBuffer::Create(GL_ARRAY_BUFFER, MAX_VBUFFER_SIZE);
	m_vertex_buffers = m_vertexBuffer->m_buffer;

	m_indexBuffer = StreamBuffer::Create(GL_ELEMENT_ARRAY_BUFFER, MAX_IBUFFER_SIZE);
	m_index_buffers = m_indexBuffer->m_buffer;

	m_last_vao = 0;
}

void VertexManager::DestroyDeviceObjects()
{
	m_vertexBuffer.reset();
	m_indexBuffer.reset();
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
	u32 vertex_data_size = IndexGenerator::GetNumVerts() * stride;
	u32 index_data_size = IndexGenerator::GetIndexLen() * sizeof(u16);
	m_baseVertex = m_vertexBuffer->Stream(vertex_data_size, stride, m_cpu_v_buffer.data()) / stride;
	m_index_offset = m_indexBuffer->Stream(index_data_size, m_cpu_i_buffer.data());
	ADDSTAT(stats.thisFrame.bytesVertexStreamed, vertex_data_size);
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, index_data_size);
}

void VertexManager::ResetBuffer(u32 stride)
{
	m_pCurBufferPointer = m_pBaseBufferPointer = m_cpu_v_buffer.data();
	m_pEndBufferPointer = m_pBaseBufferPointer + m_cpu_v_buffer.size();
	m_index_buffer_base = m_cpu_i_buffer.data();
	IndexGenerator::Start(m_cpu_i_buffer.data());
}

void VertexManager::Draw(u32 stride)
{
	u32 index_size = IndexGenerator::GetIndexLen();
	u32 max_index = IndexGenerator::GetNumVerts();
	GLenum primitive_mode = 0;
	static const GLenum modes[3] = {
		GL_POINTS,
		GL_LINES,
		GL_TRIANGLES
	};
	primitive_mode = modes[m_current_primitive_type];
	bool cull_changed = primitive_mode != GL_TRIANGLES && bpmem.genMode.cullmode > 0;
	if (cull_changed)
	{
		glDisable(GL_CULL_FACE);
	}

	if (g_ogl_config.bSupportsGLBaseVertex)
	{
		glDrawRangeElementsBaseVertex(primitive_mode, 0, max_index, index_size, GL_UNSIGNED_SHORT, (u8*)nullptr + m_index_offset, (GLint)m_baseVertex);
	}
	else
	{
		glDrawRangeElements(primitive_mode, 0, max_index, index_size, GL_UNSIGNED_SHORT, (u8*)nullptr + m_index_offset);
	}

	INCSTAT(stats.thisFrame.numDrawCalls);

	if (cull_changed)
		static_cast<Renderer*>(g_renderer.get())->SetGenerationMode();
}

void VertexManager::PrepareShaders(PrimitiveType primitive, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread)
{
	const bool useDstAlpha = bpm.dstalpha.enable && bpm.blendmode.alphaupdate &&
		bpm.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	// Makes sure we can actually do Dual source blending
	bool dualSourcePossible = g_ActiveConfig.backend_info.bSupportsDualSourceBlend;
	// If host supports GL_ARB_blend_func_extended, we can do dst alpha in
	// the same pass as regular rendering.
	if (useDstAlpha && dualSourcePossible)
	{
		ProgramShaderCache::SetShader(PSRM_DUAL_SOURCE_BLEND, VertexLoaderManager::g_current_components, m_current_primitive_type);
	}
	else
	{
		if (useDstAlpha)
		{
			ProgramShaderCache::SetShader(PSRM_ALPHA_PASS, VertexLoaderManager::g_current_components, m_current_primitive_type);
		}
		ProgramShaderCache::SetShader(PSRM_DEFAULT, VertexLoaderManager::g_current_components, m_current_primitive_type);
	}
}

u16* VertexManager::GetIndexBuffer()
{
	return m_index_buffer_base;
}
void VertexManager::vFlush(bool useDstAlpha)
{
	GLVertexFormat* nativeVertexFmt = (GLVertexFormat*)VertexLoaderManager::GetCurrentVertexFormat();
	u32 stride = nativeVertexFmt->GetVertexStride();
	BBox::Update();
	// Makes sure we can actually do Dual source blending
	bool dualSourcePossible = g_ActiveConfig.backend_info.bSupportsDualSourceBlend;

	// upload global constants
	ProgramShaderCache::UploadConstants();

	// setup the pointers
	nativeVertexFmt->SetupVertexPointers();
	if (m_last_vao != nativeVertexFmt->VAO)
	{
		glBindVertexArray(nativeVertexFmt->VAO);
		m_last_vao = nativeVertexFmt->VAO;
	}
	PrepareDrawBuffers(stride);
	// If host supports GL_ARB_blend_func_extended, we can do dst alpha in
	// the same pass as regular rendering.
	OGL::SHADER* active_shader = nullptr;
	if (useDstAlpha && dualSourcePossible)
	{
		active_shader = ProgramShaderCache::SetShader(PSRM_DUAL_SOURCE_BLEND, VertexLoaderManager::g_current_components, m_current_primitive_type);
	}
	else
	{
		active_shader = ProgramShaderCache::SetShader(PSRM_DEFAULT, VertexLoaderManager::g_current_components, m_current_primitive_type);
	}
	active_shader->Bind();
	g_renderer->ApplyState(false);
	Draw(stride);
	// If the GPU does not support dual-source blending, we can approximate the effect by drawing
	// the object a second time, with the write mask set to alpha only using a shader that outputs
	// the destination/constant alpha value (which would normally be SRC_COLOR.a).
	//
	// This is also used when logic ops and destination alpha is enabled, since we can't enable
	// blending and logic ops concurrently.
	const bool logic_op_enabled = bpmem.blendmode.logicopenable && bpmem.blendmode.logicmode != BlendMode::LogicOp::COPY && !bpmem.blendmode.blendenable;
	// run through vertex groups again to set alpha
	if (useDstAlpha && (!dualSourcePossible || logic_op_enabled))
	{
		active_shader = ProgramShaderCache::SetShader(PSRM_ALPHA_PASS, VertexLoaderManager::g_current_components, m_current_primitive_type);

		// only update alpha
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);

		glDisable(GL_BLEND);
		if (logic_op_enabled)
			glDisable(GL_COLOR_LOGIC_OP);

		active_shader->Bind();
		Draw(stride);

		// restore color mask
		g_renderer->SetColorMask();

		if (bpmem.blendmode.blendenable || bpmem.blendmode.subtract)
			glEnable(GL_BLEND);
		if (logic_op_enabled)
			glEnable(GL_COLOR_LOGIC_OP);
	}
	g_Config.iSaveTargetId++;

	ClearEFBCache();
}

}  // namespace
