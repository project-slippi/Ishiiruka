// This file is under the public domain.

#pragma once

#include "CommonTypes.h"

// Helper functions:

#ifdef _WIN32

#include <intrin.h>

template <typename T>
constexpr int CountSetBits(T v)
{
	// from https://graphics.stanford.edu/~seander/bithacks.html
	// GCC has this built in, but MSVC's intrinsic will only emit the actual
	// POPCNT instruction, which we're not depending on
	v = v - ((v >> 1) & (T)~(T)0 / 3);
	v = (v & (T)~(T)0 / 15 * 3) + ((v >> 2) & (T)~(T)0 / 15 * 3);
	v = (v + (v >> 4)) & (T)~(T)0 / 255 * 15;
	return (T)(v * ((T)~(T)0 / 255)) >> (sizeof(T) - 1) * 8;
}
inline int LeastSignificantSetBit(u8 val)
{
	unsigned long index;
	_BitScanForward(&index, val);
	return (int)index;
}
inline int LeastSignificantSetBit(u16 val)
{
	unsigned long index;
	_BitScanForward(&index, val);
	return (int)index;
}
inline int LeastSignificantSetBit(u32 val)
{
	unsigned long index;
	_BitScanForward(&index, val);
	return (int)index;
}
inline int LeastSignificantSetBit(u64 val)
{
	unsigned long index;
	_BitScanForward64(&index, val);
	return (int)index;
}
#else
constexpr int CountSetBits(u8 val)
{
	return __builtin_popcount(val);
}
constexpr int CountSetBits(u16 val)
{
	return __builtin_popcount(val);
}
constexpr int CountSetBits(u32 val)
{
	return __builtin_popcount(val);
}
constexpr int CountSetBits(u64 val)
{
	return __builtin_popcountll(val);
}
inline int LeastSignificantSetBit(u8 val)
{
	return __builtin_ctz(val);
}
inline int LeastSignificantSetBit(u16 val)
{
	return __builtin_ctz(val);
}
inline int LeastSignificantSetBit(u32 val)
{
	return __builtin_ctz(val);
}
inline int LeastSignificantSetBit(u64 val)
{
	return __builtin_ctzll(val);
}
#endif