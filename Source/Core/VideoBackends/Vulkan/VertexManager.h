// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"
#include "VideoCommon/VertexManagerBase.h"

namespace Vulkan
{
class StreamBuffer;

class VertexManager : public VertexManagerBase
{
public:
	VertexManager();
	~VertexManager();

	static VertexManager* GetInstance();

	bool Initialize();

	std::unique_ptr<NativeVertexFormat>
		CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;
	void PrepareShaders(PrimitiveType primitive, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread = true){}
protected:
	void PrepareDrawBuffers(u32 stride);
	void ResetBuffer(u32 stride) override;
	u16* GetIndexBuffer() override;
private:
	void vFlush(bool use_dst_alpha) override;

	std::vector<u8, Common::aligned_allocator<u8, 256>> m_cpu_vertex_buffer;
	std::vector<u16, Common::aligned_allocator<u16, 256>> m_cpu_index_buffer;

	std::unique_ptr<StreamBuffer> m_vertex_stream_buffer;
	std::unique_ptr<StreamBuffer> m_index_stream_buffer;

	u32 m_current_draw_base_vertex = 0;
	u32 m_current_draw_base_index = 0;
};
}
