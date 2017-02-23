// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/MemoryUtil.h"
#include "VideoCommon/VertexManagerBase.h"

namespace DX11
{

class VertexManager : public ::VertexManagerBase
{
public:
	VertexManager();
	~VertexManager();

	std::unique_ptr<NativeVertexFormat> CreateNativeVertexFormat(const PortableVertexDeclaration &_vtx_decl) override;
	void CreateDeviceObjects();
	void DestroyDeviceObjects();
	void PrepareShaders(PrimitiveType primitive,
		u32 components,
		const XFMemory &xfr,
		const BPMemory &bpm,
		bool fromgputhread = true);
protected:
	void ResetBuffer(u32 stride) override;
	u16* GetIndexBuffer() override
	{
		return m_index_buffer_start;
	}
private:

	void PrepareDrawBuffers(u32 stride);
	void Draw(u32 stride);
	// temp
	void vFlush(bool useDstAlpha);

	u32 m_vertexDrawOffset;
	u32 m_indexDrawOffset;
	u32 m_currentBuffer;
	u32 m_bufferCursor;
	enum
	{
		MAX_BUFFER_COUNT = 3
	};

	D3D::BufferPtr m_buffers[MAX_BUFFER_COUNT];

	std::vector<u8, Common::aligned_allocator<u8, 256>> LocalVBuffer;
	std::vector<u16, Common::aligned_allocator<u16, 256>> LocalIBuffer;
	u16* m_index_buffer_start;
};

}  // namespace