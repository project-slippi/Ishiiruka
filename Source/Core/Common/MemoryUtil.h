// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

void* AllocateExecutableMemory(size_t size, bool low = true);
void* AllocateMemoryPages(size_t size);
void FreeMemoryPages(void* ptr, size_t size);
void* AllocateAlignedMemory(size_t size, size_t alignment);
void FreeAlignedMemory(void* ptr);
void ReadProtectMemory(void* ptr, size_t size);
void WriteProtectMemory(void* ptr, size_t size, bool executable = false);
void UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute = false);
std::string MemUsage();
size_t MemPhysical();

void GuardMemoryMake(void* ptr, size_t size);
void GuardMemoryUnmake(void* ptr, size_t size);

inline int GetPageSize()
{
	return 4096;
}

template <typename T>
class SimpleBuf
{
public:
	SimpleBuf() : m_buf(0), m_size(0)
	{}

	SimpleBuf(size_t s) : m_buf(0)
	{
		resize(s);
	}

	~SimpleBuf()
	{
		if (m_buf != 0)
		{
			FreeMemoryPages(m_buf, m_size * sizeof(T));
		}
	}

	inline T &operator[](size_t index)
	{
		return m_buf[index];
	}

	// Doesn't preserve contents.
	void resize(size_t s)
	{
		if (m_size < s)
		{
			if (m_buf != 0)
			{
				FreeMemoryPages(m_buf, m_size * sizeof(T));
			}
			m_buf = (T *)AllocateMemoryPages(s * sizeof(T));
			m_size = s;
		}
	}

	T *data()
	{
		return m_buf;
	}

	size_t size() const
	{
		return m_size;
	}

private:
	T *m_buf;
	size_t m_size;
};

