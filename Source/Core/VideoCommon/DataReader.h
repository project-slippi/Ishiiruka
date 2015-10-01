// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified For Ishiiruka By Tino
#pragma once
#include "Common/Common.h"
#include "Common/CommonFuncs.h"

class DataReader
{
public:
	inline DataReader(const u8 *source) : Rbuffer(source) {}
	inline DataReader() : Rbuffer(nullptr) {}
	__forceinline void ReadSkip(u32 skip)
	{
		Rbuffer += skip;
	}

	template <typename T> __forceinline T Peek(s32 _uOffset)
	{
		auto const result = Common::FromBigEndian(*reinterpret_cast<const T*>(Rbuffer + _uOffset));
		return result;
	}

	template <typename T> __forceinline T Peek()
	{
		auto const result = Common::FromBigEndian(*reinterpret_cast<const T*>(Rbuffer));
		return result;
	}

	template <typename T> __forceinline T PeekUnswapped(s32 _uOffset)
	{
		auto const result = *reinterpret_cast<const T*>(Rbuffer + _uOffset);
		return result;
	}

	template <typename T> __forceinline T PeekUnswapped()
	{
		auto const result = *reinterpret_cast<const T*>(Rbuffer);
		return result;
	}

	template <typename T> __forceinline T Read()
	{
		auto const result = Peek<T>();
		ReadSkip(sizeof(T));
		return result;
	}

	template <typename T> __forceinline T ReadUnswapped()
	{
		auto const result = PeekUnswapped<T>();
		ReadSkip(sizeof(T));
		return result;
	}

	__forceinline const u8* GetReadPosition() const
	{
		return Rbuffer;
	}

	__forceinline void SetReadPosition(const u8 *source)
	{
		Rbuffer = source;
	}

	template<unsigned int N>
	__forceinline void ReadU32xN(u32 *dst)
	{
		const u32* src = (u32*)Rbuffer;
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
		Rbuffer = (const u8*)src;
	}
protected:
	const u8* __restrict Rbuffer;
};

class DataWriter
{
public:
	inline DataWriter(u8 *destination) : Wbuffer(destination) {}
	inline DataWriter() : Wbuffer(nullptr) {}
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
