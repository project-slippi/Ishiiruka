// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Core/Host.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/BPStructs.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/TessellationShaderManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoState.h"

static Common::Flag s_FifoShuttingDown;
volatile u32 s_EFB_PCache_Frame;

static volatile struct
{
	u32 xfbAddr;
	u32 fbWidth;
	u32 fbHeight;
	u32 fbStride;
} s_beginFieldArgs;

void VideoBackendBase::ShowConfig(void* parent_handle)
{
	if (!m_initialized)
	{
		InitBackendInfo();
	}
	Host_ShowVideoConfig(parent_handle, GetDisplayName());
}

void VideoBackendBase::Video_ExitLoop()
{
	Fifo::ExitGpuLoop();
	s_FifoShuttingDown.Set();
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoBackendBase::Video_BeginField(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, u64 ticks)
{
	if (m_initialized && g_ActiveConfig.bUseXFB && g_renderer)
	{
		Fifo::SyncGPU(Fifo::SyncGPUReason::Swap);

		AsyncRequests::Event e;
		e.time = ticks;
		e.type = AsyncRequests::Event::SWAP_EVENT;

		e.swap_event.xfbAddr = xfbAddr;
		e.swap_event.fbWidth = fbWidth;
		e.swap_event.fbStride = fbStride;
		e.swap_event.fbHeight = fbHeight;
		AsyncRequests::GetInstance()->PushEvent(e, false);
	}
}

VideoBackendBase::VideoBackendBase()
{
	// TODO: Make this values configurable
	// Scale aplied to reduce peek cache size
	m_EFB_PCache_Divisor = 3;
	// Lifespam of the cache values in frames
	m_EFB_PCache_Life = 3;
	m_EFB_PCache_Width = EFB_WIDTH >> m_EFB_PCache_Divisor;
	m_EFB_PCache_Height = EFB_HEIGHT >> m_EFB_PCache_Divisor;
	m_EFB_PCache_Size = m_EFB_PCache_Width * m_EFB_PCache_Height;
	m_EFB_PCache = new EFBPeekCacheElement[m_EFB_PCache_Size];
}

VideoBackendBase::~VideoBackendBase()
{
	if (m_EFB_PCache)
	{
		delete[] m_EFB_PCache;
	}
}

u32 VideoBackendBase::Video_AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
	if (!(g_ActiveConfig.bEFBAccessEnable && m_initialized))
	{
		return 0;
	}
	u32 result = InputData;
	u32 efb_p_cache_stride = (y >> m_EFB_PCache_Divisor) * m_EFB_PCache_Width + (x >> m_EFB_PCache_Divisor);
	if (type == EFBAccessType::PokeColor || type == EFBAccessType::PokeZ)
	{
		AsyncRequests::Event e;
		e.type = type == EFBAccessType::PokeColor ? AsyncRequests::Event::EFB_POKE_COLOR : AsyncRequests::Event::EFB_POKE_Z;
		e.time = 0;
		e.efb_poke.data = InputData;
		e.efb_poke.x = x;
		e.efb_poke.y = y;
		AsyncRequests::GetInstance()->PushEvent(e, false);
	}
	else
	{
		if (g_ActiveConfig.bEFBFastAccess)
		{
			if (type == EFBAccessType::PeekColor && m_EFB_PCache[efb_p_cache_stride].ColorFrame > s_EFB_PCache_Frame)
			{
				return m_EFB_PCache[efb_p_cache_stride].ColorValue;
			}
			else if (type == EFBAccessType::PeekZ && m_EFB_PCache[efb_p_cache_stride].DepthFrame > s_EFB_PCache_Frame)
			{
				return m_EFB_PCache[efb_p_cache_stride].DepthValue;
			}
		}
		AsyncRequests::Event e;

		e.type = type == EFBAccessType::PeekColor ? AsyncRequests::Event::EFB_PEEK_COLOR : AsyncRequests::Event::EFB_PEEK_Z;
		e.time = 0;
		e.efb_peek.x = x;
		e.efb_peek.y = y;
		e.efb_peek.data = &result;
		AsyncRequests::GetInstance()->PushEvent(e, true);
	}
	if (g_ActiveConfig.bEFBFastAccess)
	{
		if (type == EFBAccessType::PeekColor || type == EFBAccessType::PokeColor)
		{
			m_EFB_PCache[efb_p_cache_stride].ColorValue = result;
			m_EFB_PCache[efb_p_cache_stride].ColorFrame = s_EFB_PCache_Frame + m_EFB_PCache_Life;
		}
		else
		{
			m_EFB_PCache[efb_p_cache_stride].DepthValue = result;
			m_EFB_PCache[efb_p_cache_stride].DepthFrame = s_EFB_PCache_Frame + m_EFB_PCache_Life;
		}
	}
	return result;
}

u32 VideoBackendBase::Video_GetQueryResult(PerfQueryType type)
{
	if (!g_perf_query->ShouldEmulate())
	{
		return 0;
	}

	Fifo::SyncGPU(Fifo::SyncGPUReason::PerfQuery);

	AsyncRequests::Event e;
	e.time = 0;
	e.type = AsyncRequests::Event::PERF_QUERY;

	if (!g_perf_query->IsFlushed())
		AsyncRequests::GetInstance()->PushEvent(e, true);

	return g_perf_query->GetQueryResult(type);
}
u16 VideoBackendBase::Video_GetBoundingBox(int index)
{
	if (g_ActiveConfig.iBBoxMode == BBoxNone)
		return BoundingBox::coords[index];

	Fifo::SyncGPU(Fifo::SyncGPUReason::BBox);

	AsyncRequests::Event e;
	u16 result;
	e.time = 0;
	e.type = AsyncRequests::Event::BBOX_READ;
	e.bbox.index = index;
	e.bbox.data = &result;
	AsyncRequests::GetInstance()->PushEvent(e, true);
	return result;
}

void VideoBackendBase::InitializeShared()
{
	memset(&g_main_cp_state, 0, sizeof(g_main_cp_state));
	memset(texMem, 0, TMEM_SIZE);
	s_FifoShuttingDown.Clear();
	memset((void*)&s_beginFieldArgs, 0, sizeof(s_beginFieldArgs));
	m_invalid = false;
	memset(m_EFB_PCache, 0, m_EFB_PCache_Size * sizeof(EFBPeekCacheElement));
	s_EFB_PCache_Frame = 1;
	frameCount = 0;
	// Do our OSD callbacks
	OSD::DoCallbacks(OSD::CallbackType::Initialization);

	// do not initialize again for the config window
	m_initialized = true;
	g_Config.Load(File::GetUserPath(D_CONFIG_IDX) + "GFX.ini");
	g_Config.GameIniLoad();
	g_Config.UpdateProjectionHack();
	g_Config.VerifyValidity();

	// Slippi: Always save user's configuration when initializing backend.
	// This is a hack to auto-generate sane defaults for Slippi.
	g_Config.Save(File::GetUserPath(D_CONFIG_IDX) + "GFX.ini");

	UpdateActiveConfig();

	CommandProcessor::Init();
	Fifo::Init();
	OpcodeDecoder::Init();
	PixelEngine::Init();
	BPInit();
	VertexLoaderManager::Init();
	IndexGenerator::Init();
	VertexShaderManager::Init();
	GeometryShaderManager::Init();
	PixelShaderManager::Init(!(g_ActiveConfig.backend_info.APIType & API_D3D9));
	if (!(g_ActiveConfig.backend_info.APIType & API_D3D9))
	{
		PixelShaderManager::DisableDirtyRegions();
		VertexShaderManager::DisableDirtyRegions();
	}
	TessellationShaderManager::Init();

	// Notify the core that the video backend is ready
	Host_Message(WM_USER_CREATE);
}

void VideoBackendBase::ShutdownShared()
{
	// Do our OSD callbacks
	OSD::DoCallbacks(OSD::CallbackType::Shutdown);

	m_initialized = false;

	Fifo::Shutdown();
	GeometryShaderManager::Shutdown();
	TessellationShaderManager::Shutdown();
}

void VideoBackendBase::CleanupShared()
{
	VertexLoaderManager::Shutdown();
}

// Run from the CPU thread
void VideoBackendBase::DoState(PointerWrap& p)
{
	bool software = false;
	p.Do(software);

	if (p.GetMode() == PointerWrap::MODE_READ && software == true)
	{
		// change mode to abort load of incompatible save state.
		p.SetMode(PointerWrap::MODE_VERIFY);
	}

	VideoCommon_DoState(p);
	p.DoMarker("VideoCommon");

	p.Do(s_beginFieldArgs);
	p.DoMarker("VideoBackendBase");

	// Refresh state.
	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		m_invalid = true;
		// Clear all caches that touch RAM
		// (? these don't appear to touch any emulation state that gets saved. moved to on load only.)
		VertexLoaderManager::MarkAllDirty();
	}
}

void VideoBackendBase::CheckInvalidState()
{
	if (m_invalid)
	{
		m_invalid = false;

		BPReload();
		g_texture_cache->Invalidate();
	}
}
