// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VertexManagerBase.h"

namespace DX11
{

class VertexManager : public ::VertexManager
{
public:
	VertexManager();
	~VertexManager();

	NativeVertexFormat* CreateNativeVertexFormat();
	void CreateDeviceObjects();
	void DestroyDeviceObjects();
	void PrepareShaders(u32 primitive,
		u32 components,
		const XFMemory &xfr,
		const BPMemory &bpm,
		bool fromgputhread = true);
protected:
	void ResetBuffer(u32 stride) override;
	u16* GetIndexBuffer() override { return m_index_buffer_start; }
private:

	void PrepareDrawBuffers(u32 stride);
	void Draw(u32 stride);
	// temp
	void vFlush(bool useDstAlpha);

	u32 m_vertexDrawOffset;
	u32 m_indexDrawOffset;
	u32 m_currentBuffer;
	u32 m_bufferCursor;
	enum { MAX_BUFFER_COUNT = 2 };
	// TODO: Find sensible values for these two
	const u32 MAX_IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16);
	const u32 MAX_VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE;
	const u32 MAX_BUFFER_SIZE = MAX_IBUFFER_SIZE + MAX_VBUFFER_SIZE;
	D3D::BufferPtr m_buffers[MAX_BUFFER_COUNT];

	std::vector<u8> LocalVBuffer;
	std::vector<u16> LocalIBuffer;
	u16* m_index_buffer_start;
};

}  // namespace