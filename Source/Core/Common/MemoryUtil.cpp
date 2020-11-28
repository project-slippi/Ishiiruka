// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdlib>
#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/MsgHandler.h"

#ifdef _WIN32
#include <psapi.h>
#include <windows.h>
#include "Common/StringUtil.h"
#else
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#if defined __APPLE__ || defined __FreeBSD__ || defined __OpenBSD__
#include <sys/sysctl.h>
#include <sys/utsname.h>
#else
#include <sys/sysinfo.h>
#endif
#endif

// Valgrind doesn't support MAP_32BIT.
// Uncomment the following line to be able to run Dolphin in Valgrind.
//#undef MAP_32BIT
namespace Common
{

#if !defined(_WIN32) && defined(_M_X86_64) && !defined(MAP_32BIT)
#include <unistd.h>
static uintptr_t RoundPage(uintptr_t addr)
{
	uintptr_t mask = getpagesize() - 1;
	return (addr + mask) & ~mask;
}
#endif

#if defined __APPLE__
// On High Sierra, passing MAP_JIT seems to cause weird issues - the hardened runtime only exists
// from Mojave (10.14) onwards, so there's some clash happening in 10.13. Thus, we just need to branch
// here for that one OS (18 is the kernel for Mojave)... and this should be removed at some point. 
static inline int determine_macos_jit_flag()
{
	static int jit_flag = -1;

	if (jit_flag == -1)
	{
		struct utsname name;
		uname(&name);

		// Kernel version 18 = Mojave
		jit_flag = (atoi(name.release) >= 18) ? MAP_JIT : 0;
	}

	return jit_flag;
}
#endif

// This is purposely not a full wrapper for virtualalloc/mmap, but it
// provides exactly the primitive operations that Dolphin needs.

void* AllocateExecutableMemory(size_t size, bool low)
{
#if defined(_WIN32)
	void* ptr = VirtualAlloc(0, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
	static char* map_hint = nullptr;
#if defined(_M_X86_64) && !defined(MAP_32BIT)
	// This OS has no flag to enforce allocation below the 4 GB boundary,
	// but if we hint that we want a low address it is very likely we will
	// get one.
	// An older version of this code used MAP_FIXED, but that has the side
	// effect of discarding already mapped pages that happen to be in the
	// requested virtual memory range (such as the emulated RAM, sometimes).
	if (low && (!map_hint))
		map_hint = (char*)RoundPage(512 * 1024 * 1024); /* 0.5 GB rounded up to the next page */
#endif

	int flags = MAP_ANON | MAP_PRIVATE;
#ifdef __APPLE__
	flags |= determine_macos_jit_flag();
#endif

	void* ptr = mmap(map_hint, size, PROT_READ | PROT_WRITE | PROT_EXEC, flags
#if defined(_M_X86_64) && defined(MAP_32BIT)
		| (low ? MAP_32BIT : 0)
#endif
		,
		-1, 0);
#endif /* defined(_WIN32) */

#ifdef _WIN32
	if (ptr == nullptr)
	{
#else
	if (ptr == MAP_FAILED)
	{
		ptr = nullptr;
#endif
		PanicAlert("Failed to allocate executable memory. If you are running Dolphin in Valgrind, try "
			"'#undef MAP_32BIT'.");
	}
#if !defined(_WIN32) && defined(_M_X86_64) && !defined(MAP_32BIT)
	else
	{
		if (low)
		{
			map_hint += size;
			map_hint = (char*)RoundPage((uintptr_t)map_hint); /* round up to the next page */
		}
	}
#endif

#if _M_X86_64
	if ((u64)ptr >= 0x80000000 && low == true)
		PanicAlert("Executable memory ended up above 2GB!");
#endif

	return ptr;
	}

void* AllocateMemoryPages(size_t size)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
#else
	void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

	if (ptr == MAP_FAILED)
		ptr = nullptr;
#endif

	if (ptr == nullptr)
		PanicAlert("Failed to allocate raw memory");

	return ptr;
}

void* AllocateAlignedMemory(size_t size, size_t alignment)
{
#ifdef _WIN32
	void* ptr = _aligned_malloc(size, alignment);
#else
	void* ptr = nullptr;
	if (posix_memalign(&ptr, alignment, size) != 0)
		ERROR_LOG(MEMMAP, "Failed to allocate aligned memory");
#endif

	if (ptr == nullptr)
		PanicAlert("Failed to allocate aligned memory");

	return ptr;
}

void FreeMemoryPages(void* ptr, size_t size)
{
	if (ptr)
	{
		bool error_occurred = false;

#ifdef _WIN32
		if (!VirtualFree(ptr, 0, MEM_RELEASE))
			error_occurred = true;
#else
		int retval = munmap(ptr, size);

		if (retval != 0)
			error_occurred = true;
#endif

		if (error_occurred)
			PanicAlert("FreeMemoryPages failed!\n%s", GetLastErrorMsg().c_str());
	}
}

void FreeAlignedMemory(void* ptr)
{
	if (ptr)
	{
#ifdef _WIN32
		_aligned_free(ptr);
#else
		free(ptr);
#endif
	}
}

void ReadProtectMemory(void* ptr, size_t size)
{
	bool error_occurred = false;

#ifdef _WIN32
	DWORD oldValue;
	if (!VirtualProtect(ptr, size, PAGE_NOACCESS, &oldValue))
		error_occurred = true;
#else
	int retval = mprotect(ptr, size, PROT_NONE);

	if (retval != 0)
		error_occurred = true;
#endif

	if (error_occurred)
		PanicAlert("ReadProtectMemory failed!\n%s", GetLastErrorMsg().c_str());
}

void WriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
	bool error_occurred = false;

#ifdef _WIN32
	DWORD oldValue;
	if (!VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READ : PAGE_READONLY, &oldValue))
		error_occurred = true;
#else
	int retval = mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_EXEC) : PROT_READ);

	if (retval != 0)
		error_occurred = true;
#endif

	if (error_occurred)
		PanicAlert("WriteProtectMemory failed!\n%s", GetLastErrorMsg().c_str());
}

void UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
	bool error_occurred = false;

#ifdef _WIN32
	DWORD oldValue;
	if (!VirtualProtect(ptr, size, allowExecute ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE, &oldValue))
		error_occurred = true;
#else
	int retval = mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_WRITE | PROT_EXEC) :
		PROT_WRITE | PROT_READ);

	if (retval != 0)
		error_occurred = true;
#endif

	if (error_occurred)
		PanicAlert("UnWriteProtectMemory failed!\n%s", GetLastErrorMsg().c_str());
}

std::string MemUsage()
{
#ifdef _WIN32
#pragma comment(lib, "psapi")
	DWORD processID = GetCurrentProcessId();
	HANDLE hProcess;
	PROCESS_MEMORY_COUNTERS pmc;
	std::string Ret;

	// Print information about the memory usage of the process.

	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
	if (nullptr == hProcess)
		return "MemUsage Error";

	if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
		Ret = StringFromFormat("%s K", ThousandSeparate(pmc.WorkingSetSize / 1024, 7).c_str());

	CloseHandle(hProcess);
	return Ret;
#else
	return "";
#endif
}

size_t MemPhysical()
{
#ifdef _WIN32
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&memInfo);
	return memInfo.ullTotalPhys;
#elif defined __APPLE__ || defined __FreeBSD__ || defined __OpenBSD__
	int mib[2];
	size_t physical_memory;
	mib[0] = CTL_HW;
#ifdef __APPLE__
	mib[1] = HW_MEMSIZE;
#elif defined __FreeBSD__
	mib[1] = HW_REALMEM;
#elif defined __OpenBSD__
	mib[1] = HW_PHYSMEM;
#endif
	size_t length = sizeof(size_t);
	sysctl(mib, 2, &physical_memory, &length, NULL, 0);
	return physical_memory;
#else
	struct sysinfo memInfo;
	sysinfo(&memInfo);
	return (size_t)memInfo.totalram * memInfo.mem_unit;
#endif
}

}  // namespace Common
