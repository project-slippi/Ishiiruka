// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/MemoryUtil.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexManagerBase.h"

class StreamBuffer;

namespace OGL
{
class GLVertexFormat : public NativeVertexFormat
{
public:
	GLVertexFormat(const PortableVertexDeclaration& vtx_decl);
	~GLVertexFormat();

	void SetupVertexPointers() override;

	GLuint VAO;
};

// Handles the OpenGL details of drawing lots of vertices quickly.
// Other functionality is moving out.
class VertexManager : public VertexManagerBase
{
public:
	VertexManager();
	~VertexManager();
	std::unique_ptr<NativeVertexFormat> CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;
	void CreateDeviceObjects() override;
	void DestroyDeviceObjects() override;
	void PrepareShaders(PrimitiveType primitive, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread) override;
	// NativeVertexFormat use this
	GLuint m_vertex_buffers;
	GLuint m_index_buffers;
	GLuint m_last_vao;

protected:
	void ResetBuffer(u32 stride) override;
	u16* GetIndexBuffer() override;
private:
	void Draw(u32 stride);
	void vFlush(bool useDstAlpha) override;
	void PrepareDrawBuffers(u32 stride);

	// Alternative buffers in CPU memory for primatives we are going to discard.
	std::vector<u8, Common::aligned_allocator<u8, 16>> m_cpu_v_buffer;
	std::vector<u16, Common::aligned_allocator<u16, 16>> m_cpu_i_buffer;
	std::unique_ptr<StreamBuffer> m_vertexBuffer;
	std::unique_ptr<StreamBuffer> m_indexBuffer;
	size_t m_baseVertex;
	size_t m_index_offset;
	u16* m_index_buffer_base;
};
}
