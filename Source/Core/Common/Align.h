// This file is under the public domain.

#pragma once

#include <cstddef>
#include <type_traits>

namespace Common
{

template <typename T>
constexpr T AlignUpSizePow2(T value, size_t size)
{
	static_assert(std::is_unsigned<T>(), "T must be an unsigned value.");
	return static_cast<T>((value + size - 1) & ~(size - 1));
}

template <typename T>
constexpr T AlignDownSizePow2(T value, size_t size)
{
	static_assert(std::is_unsigned<T>(), "T must be an unsigned value.");
	return static_cast<T>(value & ~(size-1));
}

template <typename T>
constexpr T AlignUp(T value, size_t size)
{
	static_assert(std::is_unsigned<T>(), "T must be an unsigned value.");
	return static_cast<T>(value + (size - value % size) % size);
}

template <typename T>
constexpr T AlignDown(T value, size_t size)
{
	static_assert(std::is_unsigned<T>(), "T must be an unsigned value.");
	return static_cast<T>(value - value % size);
}

}  // namespace Common