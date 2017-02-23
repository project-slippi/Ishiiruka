// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/MemoryUtil.h"
#include "VideoCommon/VertexManagerBase.h"

namespace DX12
{

class D3DStreamBuffer;

class VertexManager final : public VertexManagerBase
{
public:
	VertexManager();
	~VertexManager();

	std::unique_ptr<NativeVertexFormat> CreateNativeVertexFormat(const PortableVertexDeclaration &_vtx_decl) override;
	void CreateDeviceObjects() override;
	void DestroyDeviceObjects() override;
	void PrepareShaders(PrimitiveType primitive,
		u32 components,
		const XFMemory &xfr,
		const BPMemory &bpm,
		bool fromgputhread = true);
	void SetIndexBuffer();

protected:
	void ResetBuffer(u32 stride) override;
	u16* GetIndexBuffer() override;
private:

	void PrepareDrawBuffers(u32 stride);
	void Draw(u32 stride);
	// temp
	void vFlush(bool use_dst_alpha) override;

	u32 m_vertex_draw_offset;
	u32 m_index_draw_offset;

	std::unique_ptr<D3DStreamBuffer> m_stream_buffer = nullptr;

	bool m_stream_buffer_reallocated = true;

	std::vector<u8, Common::aligned_allocator<u8, 256>> m_index_cpu_buffer;
	std::vector<u8, Common::aligned_allocator<u16, 256>> m_vertex_cpu_buffer;

};



}  // namespace
