// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// Copyright 2014 Rodolfo Bogado
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the owner nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once
#include <algorithm>
#include <utility>
#include <vector>
#include "Common.h"

typedef std::vector<std::pair<u32, u32>> regionvector;

struct ConstatBuffer
{
private:
	void* m_buffer;
	regionvector m_dirtyRegions;
	//size_t m_size;
	bool m_dirty;
	bool m_dirtyregiondisabled;
	__forceinline void AddDirtyRegion(u32 const_number, u32 size)
	{
		m_dirty = true;
		if (m_dirtyregiondisabled)
		{
			return;
		}
		u32 x = const_number;
		u32 y = const_number + size - 1;		
		for (size_t i = 0; i < m_dirtyRegions.size(); i++)
		{
			std::pair<u32, u32> &currentregion = m_dirtyRegions[i];
			// overlaps
			if (((x <= currentregion.second) && (currentregion.first <= y))
				|| (y + 1 == currentregion.first) || (currentregion.second + 1 == x))
			{
				currentregion.first = std::min(currentregion.first, x);
				currentregion.second = std::max(currentregion.second, y);
				return;
			}
			else if (currentregion.first > x)
			{
				std::swap(x, currentregion.first);
				std::swap(y, currentregion.second);
			}
		}
		m_dirtyRegions.push_back(std::pair<u32, u32>(x, y));
	}
public:
	ConstatBuffer(void* buffer, size_t size) :
		m_buffer(buffer),
		m_dirtyRegions(),
		//m_size(size),
		m_dirty(false),
		m_dirtyregiondisabled(false)
	{
		
	}
	~ConstatBuffer()
	{

	}
	template<typename T>
	__forceinline void SetConstant4(unsigned int const_number, T f1, T f2, T f3, T f4)
	{
		u32 idx = const_number * 4;
		T* buff = &((T*)m_buffer)[idx];
		*buff++ = f1;
		*buff++ = f2;
		*buff++ = f3;
		*buff = f4;
		AddDirtyRegion(const_number, 1);
	}
	template<typename T>
	__forceinline void SetConstant3v(unsigned int const_number, const T *f)
	{
		u32 idx = const_number * 4;
		T* buff = &((T*)m_buffer)[idx];
		*buff++ = *f++;
		*buff++ = *f++;
		*buff++ = *f++;
		*buff = T(0);
		AddDirtyRegion(const_number, 1);
	}
	template<typename T>
	__forceinline void SetConstant4v(unsigned int const_number, const T *f)
	{
		u32 idx = const_number * 4;
		memcpy(&((T*)m_buffer)[idx], f, sizeof(T) * 4);
		AddDirtyRegion(const_number, 1);
	}
	template<typename T>
	__forceinline void SetMultiConstant3v(unsigned int const_number, unsigned int count, const T *f)
	{
		u32 idx = const_number * 4;
		T* buff = &((T*)m_buffer)[idx];
		for (unsigned int i = 0; i < count; i++)
		{
			*buff++ = *f++;
			*buff++ = *f++;
			*buff++ = *f++;
			*buff++ = T(0);
		}
		AddDirtyRegion(const_number, count);
	}
	template<typename T>
	__forceinline void SetMultiConstant4v(unsigned int const_number, unsigned int count, const T *f)
	{
		u32 idx = const_number * 4;
		memcpy(&((T*)m_buffer)[idx], f, sizeof(T) * 4 * count);
		AddDirtyRegion(const_number, count);
	}
	
	template<typename T>
	__forceinline T* GetBufferToUpdate(u32 const_number, u32 count)
	{
		u32 idx = const_number * 4;
		AddDirtyRegion(const_number, count);
		return &((T*)m_buffer)[idx];
	}
	
	template<typename T>
	__forceinline T* GetBuffer(u32 const_number) const
	{
		u32 idx = const_number * 4;
		return &((T*)m_buffer)[idx];
	}

	__forceinline bool IsDirty()
	{
		return m_dirty;
	}

	__forceinline void Clear()
	{
		m_dirty = false;
		if (!m_dirtyregiondisabled)
		{
			m_dirtyRegions.clear();
		}
	}
	
	__forceinline void EnableDirtyRegions()
	{
		m_dirtyregiondisabled = false;
	}

	__forceinline void DisableDirtyRegions()
	{
		m_dirtyregiondisabled = true;
	}

	__forceinline const regionvector& GetRegions()
	{
		return m_dirtyRegions;
	}

};
