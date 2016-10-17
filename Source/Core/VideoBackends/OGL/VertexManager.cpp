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

static std::unique_ptr<StreamBuffer> s_vertexBuffer;
static std::unique_ptr<StreamBuffer> s_indexBuffer;
static size_t s_baseVertex;
static size_t s_index_offset;
static u16* s_index_buffer_base;
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
	s_vertexBuffer = StreamBuffer::Create(GL_ARRAY_BUFFER, MAX_VBUFFER_SIZE);
	m_vertex_buffers = s_vertexBuffer->m_buffer;

	s_indexBuffer = StreamBuffer::Create(GL_ELEMENT_ARRAY_BUFFER, MAX_IBUFFER_SIZE);
	m_index_buffers = s_indexBuffer->m_buffer;

	m_last_vao = 0;
}

void VertexManager::DestroyDeviceObjects()
{
	s_vertexBuffer.reset();
	s_indexBuffer.reset();
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
	u32 vertex_data_size = IndexGenerator::GetNumVerts() * stride;
	u32 index_data_size = IndexGenerator::GetIndexLen() * sizeof(u16);
	if (s_vertexBuffer->NeedCPUBuffer())
	{
		auto buffer = s_vertexBuffer->Map(vertex_data_size, stride);
		s_baseVertex = buffer.second / stride;
		memcpy(buffer.first, m_cpu_v_buffer.data(), vertex_data_size);
	}
	if (s_indexBuffer->NeedCPUBuffer())
	{
		auto buffer = s_indexBuffer->Map(index_data_size);
		s_index_offset = buffer.second;
		memcpy(buffer.first, m_cpu_i_buffer.data(), index_data_size);
	}
	s_vertexBuffer->Unmap(vertex_data_size);
	s_indexBuffer->Unmap(index_data_size);

	ADDSTAT(stats.thisFrame.bytesVertexStreamed, vertex_data_size);
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, index_data_size);
}

void VertexManager::ResetBuffer(u32 stride)
{
	if (s_cull_all || s_vertexBuffer->NeedCPUBuffer())
	{
		s_pCurBufferPointer = s_pBaseBufferPointer = m_cpu_v_buffer.data();
		s_pEndBufferPointer = s_pBaseBufferPointer + m_cpu_v_buffer.size();
	}
	else
	{
		auto buffer = s_vertexBuffer->Map(MAXVBUFFERSIZE, stride);
		s_pCurBufferPointer = s_pBaseBufferPointer = buffer.first;
		s_pEndBufferPointer = buffer.first + MAXVBUFFERSIZE;
		s_baseVertex = buffer.second / stride;
	}

	if (s_cull_all || s_indexBuffer->NeedCPUBuffer())
	{
		s_index_buffer_base = m_cpu_i_buffer.data();
		IndexGenerator::Start(m_cpu_i_buffer.data());
	}
	else
	{
		auto buffer = s_indexBuffer->Map(MAXIBUFFERSIZE * sizeof(u16));
		s_index_buffer_base = (u16*)buffer.first;
		IndexGenerator::Start(s_index_buffer_base);
		s_index_offset = buffer.second;
	}
}

void VertexManager::Draw(u32 stride)
{
	u32 index_size = IndexGenerator::GetIndexLen();
	u32 max_index = IndexGenerator::GetNumVerts();
	GLenum primitive_mode = 0;

	switch (current_primitive_type)
	{
	case PRIMITIVE_POINTS:
		primitive_mode = GL_POINTS;
		glDisable(GL_CULL_FACE);
		break;
	case PRIMITIVE_LINES:
		primitive_mode = GL_LINES;
		glDisable(GL_CULL_FACE);
		break;
	case PRIMITIVE_TRIANGLES:
		primitive_mode = GL_TRIANGLES;
		break;
	}

	if (g_ogl_config.bSupportsGLBaseVertex)
	{
		glDrawRangeElementsBaseVertex(primitive_mode, 0, max_index, index_size, GL_UNSIGNED_SHORT, (u8*)nullptr + s_index_offset, (GLint)s_baseVertex);
	}
	else
	{
		glDrawRangeElements(primitive_mode, 0, max_index, index_size, GL_UNSIGNED_SHORT, (u8*)nullptr + s_index_offset);
	}

	INCSTAT(stats.thisFrame.numDrawCalls);

	if (current_primitive_type != PRIMITIVE_TRIANGLES)
		static_cast<Renderer*>(g_renderer.get())->SetGenerationMode();
}

void VertexManager::PrepareShaders(PrimitiveType primitive, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread)
{

}

u16* VertexManager::GetIndexBuffer()
{
	return s_index_buffer_base;
}
void VertexManager::vFlush(bool useDstAlpha)
{
	GLVertexFormat* nativeVertexFmt = (GLVertexFormat*)VertexLoaderManager::GetCurrentVertexFormat();
	u32 stride = nativeVertexFmt->GetVertexStride();
	BBox::Update();
	// Makes sure we can actually do Dual source blending
	bool dualSourcePossible = g_ActiveConfig.backend_info.bSupportsDualSourceBlend;

	// If host supports GL_ARB_blend_func_extended, we can do dst alpha in
	// the same pass as regular rendering.
	if (useDstAlpha && dualSourcePossible)
	{
		ProgramShaderCache::SetShader(PSRM_DUAL_SOURCE_BLEND, VertexLoaderManager::g_current_components, current_primitive_type);
	}
	else
	{
		ProgramShaderCache::SetShader(PSRM_DEFAULT, VertexLoaderManager::g_current_components, current_primitive_type);
	}

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
		ProgramShaderCache::SetShader(PSRM_ALPHA_PASS, VertexLoaderManager::g_current_components, current_primitive_type);

		// only update alpha
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);

		glDisable(GL_BLEND);
		if (logic_op_enabled)
			glDisable(GL_COLOR_LOGIC_OP);

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
