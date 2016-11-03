// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <utility>

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/FramebufferManager.h"

#include "VideoCommon/VideoCommon.h"

namespace OGL
{

class StreamBuffer
{

public:
	static std::unique_ptr<StreamBuffer> Create(u32 type, u32 size);
	virtual ~StreamBuffer();

	/* This mapping function will return a pair of:
	* - the pointer to the mapped buffer
	* - the offset into the real GPU buffer (always multiple of stride)
	* On mapping, the maximum of size for allocation has to be set.
	* The size really pushed into this fifo only has to be known on Unmapping.
	* Mapping invalidates the current buffer content,
	* so it isn't allowed to access the old content any more.
	*/
	virtual u32 Stream(u32 size, const void* src) = 0;

	u32 Stream(u32 size, u32 stride, const void* src)
	{
		u32 padding = m_iterator % stride;
		if (padding)
		{
			m_iterator += stride - padding;
		}
		return Stream(size, src);
	}
	bool CanStreamWithoutRestart(u32 size, u32 stride = 0)
	{
		return (m_iterator + size + stride) <= m_size;
	}
	const u32 m_buffer;

protected:
	StreamBuffer(u32 type, u32 size, u32 align_size = 16, bool need_cpu_buffer = false);
	void CreateFences();
	void DeleteFences();
	void AllocMemory(u32 size);

	const u32 m_buffertype;
	const u32 m_size;

	u32 m_iterator;
	u32 m_used_iterator;
	u32 m_free_iterator;
	
private:
	static constexpr int SYNC_POINTS = 8;
	int Slot(u32 x) const
	{
		return x >> m_bit_per_slot;
	}
	const int m_bit_per_slot;

	std::array<GLsync, SYNC_POINTS> m_fences{};
};

}
