// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified For Ishiiruka By Tino
#pragma once
#include <cstring>
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"

class DataReader
{
public:
	__forceinline DataReader()
		: Rbuffer(nullptr), end(nullptr)
	{}

	__forceinline DataReader(u8* src, u8* _end)
		: Rbuffer(src), end(_end)
	{}

	__forceinline const u8* GetPointer() const
	{
		return Rbuffer;
	}

	__forceinline u8* operator=(u8* src)
	{
		Rbuffer = src;
		return src;
	}

	__forceinline size_t size() const
	{
		return end - Rbuffer;
	}
	__forceinline void ReadSkip(u32 skip)
	{
		Rbuffer += skip;
	}

	template <typename T> __forceinline T Peek(s32 _uOffset) const
	{
		auto const result = Common::FromBigEndian(*reinterpret_cast<const T*>(Rbuffer + _uOffset));
		return result;
	}

	template <typename T> __forceinline T Peek() const
	{
		auto const result = Common::FromBigEndian(*reinterpret_cast<const T*>(Rbuffer));
		return result;
	}

	template <typename T> __forceinline T PeekUnswapped(s32 _uOffset) const
	{
		auto const result = *reinterpret_cast<const T*>(Rbuffer + _uOffset);
		return result;
	}

	template <typename T> __forceinline T PeekUnswapped() const
	{
		auto const result = *reinterpret_cast<const T*>(Rbuffer);
		return result;
	}

	template <typename T, bool swap = true> __forceinline T Read()
	{
		auto const result = PeekUnswapped<T>();
		ReadSkip(sizeof(T));
		if (swap)
		{
			return Common::FromBigEndian(result);
		}
		return result;
	}

	template <typename T> __forceinline T ReadUnswapped()
	{
		auto const result = PeekUnswapped<T>();
		ReadSkip(sizeof(T));
		return result;
	}

	__forceinline u8* GetReadPosition() const
	{
		return Rbuffer;
	}

	__forceinline u8* GetEnd() const
	{
		return end;
	}

	__forceinline void SetReadPosition(u8 *source, u8 *e)
	{
		Rbuffer = source;
		end = e;
	}

	__forceinline void SetReadPosition(u8 *source)
	{
		Rbuffer = source;
	}

	template<unsigned int N>
	__forceinline void ReadU32xN(u32 *dst)
	{
		u32* src = (u32*)Rbuffer;
		if (N >= 1) *dst++ = Common::swap32(*src++);
		if (N >= 2) *dst++ = Common::swap32(*src++);
		if (N >= 3) *dst++ = Common::swap32(*src++);
		if (N >= 4) *dst++ = Common::swap32(*src++);
		if (N >= 5) *dst++ = Common::swap32(*src++);
		if (N >= 6) *dst++ = Common::swap32(*src++);
		if (N >= 7) *dst++ = Common::swap32(*src++);
		if (N >= 8) *dst++ = Common::swap32(*src++);
		if (N >= 9) *dst++ = Common::swap32(*src++);
		if (N >= 10) *dst++ = Common::swap32(*src++);
		if (N >= 11) *dst++ = Common::swap32(*src++);
		if (N >= 12) *dst++ = Common::swap32(*src++);
		if (N >= 13) *dst++ = Common::swap32(*src++);
		if (N >= 14) *dst++ = Common::swap32(*src++);
		if (N >= 15) *dst++ = Common::swap32(*src++);
		if (N >= 16) *dst++ = Common::swap32(*src++);
		Rbuffer = (u8*)src;
	}
protected:
	u8* __restrict Rbuffer;
	u8* end;
};

class DataWriter
{
public:
	__forceinline DataWriter(u8 *destination) : Wbuffer(destination)
	{}
	__forceinline DataWriter() : Wbuffer(nullptr)
	{}
	__forceinline void WriteSkip(u32 skip)
	{
		Wbuffer += skip;
	}

	template <typename T> __forceinline void Write(T data)
	{
		*((T*)Wbuffer) = data;
		WriteSkip(sizeof(T));
	}

	__forceinline u8* GetWritePosition() const
	{
		return Wbuffer;
	}

	__forceinline void SetWritePosition(u8 *destination)
	{
		Wbuffer = destination;
	}
protected:
	u8* __restrict Wbuffer;
};

extern DataReader g_VideoData;
