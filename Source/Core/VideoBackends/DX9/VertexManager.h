// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/MemoryUtil.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexManagerBase.h"

#include "VideoBackends/DX9/VertexShaderCache.h"
#include "VideoBackends/DX9/PixelShaderCache.h"

namespace DX9
{

class VertexManager : public ::VertexManagerBase
{
public:
	VertexManager();
	~VertexManager();

	std::unique_ptr<NativeVertexFormat> CreateNativeVertexFormat(const PortableVertexDeclaration &_vtx_decl) override;

	void GetElements(NativeVertexFormat* format, D3DVERTEXELEMENT9** elems, int* num);

	void CreateDeviceObjects();
	void DestroyDeviceObjects();

	void PrepareShaders(PrimitiveType primitive, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread = true);
protected:
	void ResetBuffer(u32 stride) override;
	u16* GetIndexBuffer() override
	{
		return &LocalIBuffer[0];
	}
private:
	void PrepareDrawBuffers(u32 stride);
	void Draw(u32 stride);
	void SetPLRasterOffsets();
	// temp
	void vFlush(bool useDstAlpha) override;

	u32 m_vertex_buffer_cursor;
	u32 m_vertex_buffer_size;
	u32 m_index_buffer_cursor;
	u32 m_index_buffer_size;
	u32 m_buffers_count;
	u32 m_current_vertex_buffer;
	u32 m_last_stride;
	u32 m_current_index_buffer;
	u32 m_total_index_len;
	u32 m_index_len;
	u32 m_total_num_verts;
	u32 m_num_verts;
	u32 m_primitives;

	LPDIRECT3DVERTEXBUFFER9 *m_vertex_buffers;
	LPDIRECT3DINDEXBUFFER9 *m_index_buffers;

	std::vector<u8, Common::aligned_allocator<u8, 256>> LocalVBuffer;
	std::vector<u16, Common::aligned_allocator<u16, 256>> LocalIBuffer;
};

}