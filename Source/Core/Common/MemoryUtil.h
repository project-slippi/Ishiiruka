// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

namespace Common
{
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

template <typename T, std::size_t Alignment>
class aligned_allocator
{
public:

	// The following will be the same for virtually all allocators.
	typedef T * pointer;
	typedef const T * const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T value_type;
	typedef std::size_t size_type;
	typedef ptrdiff_t difference_type;

	T * address(T& r) const
	{
		return &r;
	}

	const T * address(const T& s) const
	{
		return &s;
	}

	std::size_t max_size() const
	{
		// The following has been carefully written to be independent of
		// the definition of size_t and to avoid signed/unsigned warnings.
		return (static_cast<std::size_t>(0) - static_cast<std::size_t>(1)) / sizeof(T);
	}


	// The following must be the same for all allocators.
	template <typename U>
	struct rebind
	{
		typedef aligned_allocator<U, Alignment> other;
	};

	bool operator!=(const aligned_allocator& other) const
	{
		return !(*this == other);
	}

	void construct(T * const p, const T& t) const
	{
		void * const pv = static_cast<void *>(p);

		new (pv) T(t);
	}

	void destroy(T * const p) const
	{
		p->~T();
	}

	// Returns true if and only if storage allocated from *this
	// can be deallocated from other, and vice versa.
	// Always returns true for stateless allocators.
	bool operator==(const aligned_allocator& other) const
	{
		return true;
	}


	// Default constructor, copy constructor, rebinding constructor, and destructor.
	// Empty for stateless allocators.
	aligned_allocator() { }

	aligned_allocator(const aligned_allocator&) { }

	template <typename U> aligned_allocator(const aligned_allocator<U, Alignment>&) { }

	~aligned_allocator() { }


	// The following will be different for each allocator.
	T * allocate(const std::size_t n) const
	{
		// The return value of allocate(0) is unspecified.
		// Mallocator returns NULL in order to avoid depending
		// on malloc(0)'s implementation-defined behavior
		// (the implementation can define malloc(0) to return NULL,
		// in which case the bad_alloc check below would fire).
		// All allocators can return NULL in this case.
		if (n == 0) {
			return nullptr;
		}

		// All allocators should contain an integer overflow check.
		// The Standardization Committee recommends that std::length_error
		// be thrown in the case of integer overflow.
		if (n > max_size())
		{
			return nullptr;
		}

		// Mallocator wraps malloc().
		void * const pv = AllocateAlignedMemory(n * sizeof(T), Alignment);

		return static_cast<T *>(pv);
	}

	void deallocate(T * const p, const std::size_t n) const
	{
		FreeAlignedMemory(p);
	}


	// The following will be the same for all allocators that ignore hints.
	template <typename U>
	T * allocate(const std::size_t n, const U * /* const hint */) const
	{
		return allocate(n);
	}


	// Allocators are not required to be assignable, so
	// all allocators should have a private unimplemented
	// assignment operator. Note that this will trigger the
	// off-by-default (enabled under /Wall) warning C4626
	// "assignment operator could not be generated because a
	// base class assignment operator is inaccessible" within
	// the STL headers, but that warning is useless.
private:
	aligned_allocator& operator=(const aligned_allocator&);
};
}  // namespace Common

