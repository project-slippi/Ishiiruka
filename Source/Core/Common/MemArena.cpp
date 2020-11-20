// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdlib>
#include <set>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/MemArena.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#ifdef ANDROID
#include <sys/ioctl.h>
#include <linux/ashmem.h>
#endif

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#endif

#ifdef ANDROID
#define ASHMEM_DEVICE "/dev/ashmem"

static int AshmemCreateFileMapping(const char* name, size_t size)
{
	int fd, ret;
	fd = open(ASHMEM_DEVICE, O_RDWR);
	if (fd < 0)
		return fd;

	// We don't really care if we can't set the name, it is optional
	ioctl(fd, ASHMEM_SET_NAME, name);

	ret = ioctl(fd, ASHMEM_SET_SIZE, size);
	if (ret < 0)
	{
		close(fd);
		NOTICE_LOG(MEMMAP, "Ashmem returned error: 0x%08x", ret);
		return ret;
	}
	return fd;
}
#endif

#if defined(__APPLE__)
// This can be used to determine if a process is running under Rosetta 2 on an
// Apple-Silicon-based Mac. Returns 0 if the process is running natively (compiled for
// target), 1 if it's translated (e.g, Rosetta) and -1 for whatever error there could be.
//
// On Intel-based Macs, this should return 0; on Apple-Silicon-based Macs, this should
// return 1.
//
// If Rosetta requires more tweaks this may be better off elsewhere; however, pursuing
// Rosetta performance is probably a losing game and it's better to get it running under
// Rosetta while examining any potential native steps that could be taken.
int processIsRunningUnderRosetta2() {
	int ret = 0;

	size_t size = sizeof(ret);
	if (sysctlbyname("sysctl.proc_translated", &ret, &size, NULL, 0) == -1)
	{
		if (errno == ENOENT)
		{
			return 0;
		}

		return -1;
	}

	return ret;
}
#endif

void MemArena::GrabSHMSegment(size_t size)
{
#ifdef _WIN32
	hMemoryMapping = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)(size), nullptr);
#elif defined(ANDROID)
	fd = AshmemCreateFileMapping("Dolphin-emu", size);
	if (fd < 0)
	{
		NOTICE_LOG(MEMMAP, "Ashmem allocation failed");
		return;
	}
#else
	for (int i = 0; i < 10000; i++)
	{
		std::string file_name = StringFromFormat("/dolphinmem.%d", i);
		fd = shm_open(file_name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd != -1)
		{
			shm_unlink(file_name.c_str());
			break;
		}
		else if (errno != EEXIST)
		{
			ERROR_LOG(MEMMAP, "shm_open failed: %s", strerror(errno));
			return;
		}
	}
	if (ftruncate(fd, size) < 0)
		ERROR_LOG(MEMMAP, "Failed to allocate low memory space");
#endif
}


void MemArena::ReleaseSHMSegment()
{
#ifdef _WIN32
	CloseHandle(hMemoryMapping);
	hMemoryMapping = 0;
#else
	close(fd);
#endif
}


void* MemArena::CreateView(s64 offset, size_t size, void* base)
{
#ifdef _WIN32
	return MapViewOfFileEx(hMemoryMapping, FILE_MAP_ALL_ACCESS, 0, (DWORD)((u64)offset), size, base);
#else
	void* retval = mmap(
		base, size,
		PROT_READ | PROT_WRITE,
		MAP_SHARED | ((base == nullptr) ? 0 : MAP_FIXED),
		fd, offset);

	if (retval == MAP_FAILED)
	{
		NOTICE_LOG(MEMMAP, "mmap failed");
		return nullptr;
	}
	else
	{
		return retval;
	}
#endif
}


void MemArena::ReleaseView(void* view, size_t size)
{
#ifdef _WIN32
	UnmapViewOfFile(view);
#else
	munmap(view, size);
#endif
}


u8* MemArena::FindMemoryBase()
{
	// Running under Rosetta 2 on an Apple-Silicon-based Mac will fail if it
	// goes the normal path, presumably due to the translation process just not
	// grok'ing it.
	//
	// What does seem to work, though, is mapping it the same way that Mainline does
	// from 2017 onwards. Considering the level of plumbing this is, it's blocked off
	// to run _only_ on Apple-Silicon-based Macs that are running the process under
	// a translated environment. Intel-based Macs will still get the "regular" path
	// and should have no changes.
#if defined(__APPLE__)
	if (processIsRunningUnderRosetta2())
	{
		const size_t memory_size = 0x400000000;
		const int flags = MAP_ANON | MAP_PRIVATE;

		void* base = mmap(nullptr, memory_size, PROT_NONE, flags, -1, 0);
		if (base == MAP_FAILED)
		{
			PanicAlert("Failed to map enough memory space: %s", strerror(errno));
			return nullptr;
		}

		munmap(base, memory_size);
		return static_cast<u8*>(base);
	}
#endif

#if _ARCH_64
#ifdef _WIN32
	// 64 bit
	u8* base = (u8*)VirtualAlloc(0, 0x400000000, MEM_RESERVE, PAGE_READWRITE);
	VirtualFree(base, 0, MEM_RELEASE);
	return base;
#else
	// Very precarious - mmap cannot return an error when trying to map already used pages.
	// This makes the Windows approach above unusable on Linux, so we will simply pray...
	return reinterpret_cast<u8*>(0x2300000000ULL);
#endif

#else // 32 bit
#ifdef ANDROID
	// Android 4.3 changed how mmap works.
	// if we map it private and then munmap it, we can't use the base returned.
	// This may be due to changes in them support a full SELinux implementation.
	const int flags = MAP_ANON | MAP_SHARED;
#else
	const int flags = MAP_ANON | MAP_PRIVATE;
#endif
	const u32 MemSize = 0x31000000;
	void* base = mmap(0, MemSize, PROT_NONE, flags, -1, 0);
	if (base == MAP_FAILED)
	{
		PanicAlert("Failed to map 1 GB of memory space: %s", strerror(errno));
		return 0;
	}
	munmap(base, MemSize);
	return static_cast<u8*>(base);
#endif
}


// yeah, this could also be done in like two bitwise ops...
#define SKIP(a_flags, b_flags) \
	if (!(a_flags & MV_WII_ONLY) && (b_flags & MV_WII_ONLY)) \
		continue; \
	if (!(a_flags & MV_FAKE_VMEM) && (b_flags & MV_FAKE_VMEM)) \
		continue; \

static bool Memory_TryBase(u8* base, MemoryView* views, int num_views, u32 flags, MemArena* arena)
{
	// OK, we know where to find free space. Now grab it!
	// We just mimic the popular BAT setup.

	int i;
	for (i = 0; i < num_views; i++)
	{
		MemoryView* view = &views[i];
		void* view_base;
		bool use_sw_mirror;

		SKIP(flags, view->flags);

#if _ARCH_64
		// On 64-bit, we map the same file position multiple times, so we
		// don't need the software fallback for the mirrors.
		view_base = base + view->virtual_address;
		use_sw_mirror = false;
#else
		// On 32-bit, we don't have the actual address space to store all
		// the mirrors, so we just map the fallbacks somewhere in our address
		// space and use the software fallbacks for mirroring.
		view_base = base + (view->virtual_address & 0x3FFFFFFF);
		use_sw_mirror = true;
#endif

		if (use_sw_mirror && (view->flags & MV_MIRROR_PREVIOUS))
		{
			view->view_ptr = views[i - 1].view_ptr;
		}
		else
		{
			view->mapped_ptr = arena->CreateView(view->shm_position, view->size, view_base);
			view->view_ptr = view->mapped_ptr;
		}

		if (!view->view_ptr)
		{
			// Argh! ERROR! Free what we grabbed so far so we can try again.
			MemoryMap_Shutdown(views, i + 1, flags, arena);
			return false;
		}

		if (view->out_ptr)
			*(view->out_ptr) = (u8*)view->view_ptr;
	}

	return true;
}

static u32 MemoryMap_InitializeViews(MemoryView* views, int num_views, u32 flags)
{
	u32 shm_position = 0;
	u32 last_position = 0;

	for (int i = 0; i < num_views; i++)
	{
		// Zero all the pointers to be sure.
		views[i].mapped_ptr = nullptr;

		SKIP(flags, views[i].flags);

		if (views[i].flags & MV_MIRROR_PREVIOUS)
			shm_position = last_position;
		views[i].shm_position = shm_position;
		last_position = shm_position;
		shm_position += views[i].size;
	}

	return shm_position;
}

u8* MemoryMap_Setup(MemoryView* views, int num_views, u32 flags, MemArena* arena)
{
	u32 total_mem = MemoryMap_InitializeViews(views, num_views, flags);

	arena->GrabSHMSegment(total_mem);

	// Now, create views in high memory where there's plenty of space.
	u8* base = MemArena::FindMemoryBase();
	// This really shouldn't fail - in 64-bit, there will always be enough
	// address space.
	if (!Memory_TryBase(base, views, num_views, flags, arena))
	{
		PanicAlert("MemoryMap_Setup: Failed finding a memory base.");
		exit(0);
		return nullptr;
	}

	return base;
}

void MemoryMap_Shutdown(MemoryView* views, int num_views, u32 flags, MemArena* arena)
{
	std::set<void*> freeset;
	for (int i = 0; i < num_views; i++)
	{
		MemoryView* view = &views[i];
		if (view->mapped_ptr && !freeset.count(view->mapped_ptr))
		{
			arena->ReleaseView(view->mapped_ptr, view->size);
			freeset.insert(view->mapped_ptr);
			view->mapped_ptr = nullptr;
		}
	}
}
