// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "VideoCommon/PerfQueryBase.h"

namespace MMIO {
class Mapping;
}
class PointerWrap;

typedef struct _EFBPeekCacheElement
{
	u32 ColorValue;
	u32 DepthValue;
	u32 ColorFrame;
	u32 DepthFrame;
}EFBPeekCacheElement;

enum FieldType
{
	Odd = 0,
	Even = 1,
};

enum EFBAccessType
{
	PeekZ = 0,
	PokeZ,
	PeekColor,
	PokeColor
};

struct SCPFifoStruct
{
	// fifo registers
	volatile u32 CPBase;
	volatile u32 CPEnd;
	u32 CPHiWatermark;
	u32 CPLoWatermark;
	volatile u32 CPReadWriteDistance;
	volatile u32 CPWritePointer;
	volatile u32 CPReadPointer;
	volatile u32 CPBreakpoint;
	volatile u32 SafeCPReadPointer;	

	volatile u32 bFF_GPLinkEnable;
	volatile u32 bFF_GPReadEnable;
	volatile u32 bFF_BPEnable;
	volatile u32 bFF_BPInt;
	volatile u32 bFF_Breakpoint;

	volatile u32 CPCmdIdle;
	volatile u32 CPReadIdle;

	volatile u32 bFF_LoWatermarkInt;
	volatile u32 bFF_HiWatermarkInt;

	volatile u32 bFF_LoWatermark;
	volatile u32 bFF_HiWatermark;

	// for GP watchdog hack	
	volatile u32 isGpuReadingData;
};

class VideoBackendBase
{
public:
	VideoBackendBase();
	virtual ~VideoBackendBase();

	virtual unsigned int PeekMessages() = 0;

	virtual bool Initialize(void* window_handle) = 0;
	virtual void Shutdown() = 0;

	virtual std::string GetName() const = 0;
	virtual std::string GetDisplayName() const
	{
		return GetName();
	}
	virtual void InitBackendInfo() = 0;

	void ShowConfig(void*);

	virtual void Video_Prepare() = 0;
	void Video_ExitLoop();
	virtual void Video_Cleanup() = 0; // called from gl/d3d thread

	void Video_BeginField(u32, u32, u32, u32, u64);

	u32 Video_AccessEFB(EFBAccessType, u32, u32, u32);
	u32 Video_GetQueryResult(PerfQueryType type);
	u16 Video_GetBoundingBox(int index);

	static void PopulateList();
	static void ClearList();
	static void ActivateBackend(const std::string& name);

	// the implementation needs not do synchronization logic, because calls to it are surrounded by PauseAndLock now
	void DoState(PointerWrap &p);

	void CheckInvalidState();

protected:
	void InitializeShared();
	void ShutdownShared();
	void CleanupShared();

	bool m_initialized = false;
	bool m_invalid = false;
	u32 m_EFB_PCache_Width;
	u32 m_EFB_PCache_Height;
	u32 m_EFB_PCache_Size;
	u32 m_EFB_PCache_Divisor;
	u32 m_EFB_PCache_Life;
	EFBPeekCacheElement* m_EFB_PCache;
};

extern std::vector<std::unique_ptr<VideoBackendBase>> g_available_video_backends;
extern VideoBackendBase* g_video_backend;