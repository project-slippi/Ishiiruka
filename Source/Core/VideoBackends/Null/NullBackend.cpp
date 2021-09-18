// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Null Backend Documentation

// This backend tries not to do anything in the backend,
// but everything in VideoCommon.

#include "Core/Host.h"

#include "VideoBackends/Null/FramebufferManager.h"
#include "VideoBackends/Null/PerfQuery.h"
#include "VideoBackends/Null/Render.h"
#include "VideoBackends/Null/ShaderCache.h"
#include "VideoBackends/Null/TextureCache.h"
#include "VideoBackends/Null/VertexManager.h"
#include "VideoBackends/Null/VideoBackend.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

namespace Null
{
static void InitBackendInfo()
{
  g_Config.backend_info.APIType = API_NONE;
  g_Config.backend_info.bSupportsExclusiveFullscreen = true;
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsEarlyZ = true;
  g_Config.backend_info.bSupportsPrimitiveRestart = true;
  g_Config.backend_info.bSupportsOversizedViewports = true;
  g_Config.backend_info.bSupportsGeometryShaders = true;
  g_Config.backend_info.bSupports3DVision = false;
  g_Config.backend_info.bSupportsPostProcessing = false;
  g_Config.backend_info.bSupportsPaletteConversion = true;
  g_Config.backend_info.bSupportsClipControl = true;

  // aamodes: We only support 1 sample, so no MSAA
  g_Config.backend_info.AAModes = {1};
}

void VideoBackend::ShowConfig(void* parent)
{
  InitBackendInfo();
  Host_ShowVideoConfig(parent, GetDisplayName(), "gfx_null");
}

bool VideoBackend::Initialize(void* window_handle)
{
  InitializeShared();
  InitBackendInfo();

  // Load Configs
  g_Config.Load(File::GetUserPath(D_CONFIG_IDX) + "GFX.ini");
  g_Config.GameIniLoad();
  g_Config.UpdateProjectionHack();
  g_Config.VerifyValidity();
  UpdateActiveConfig();

  // Do our OSD callbacks
  OSD::DoCallbacks(OSD::CallbackType::Initialization);

  // Initialize VideoCommon
  CommandProcessor::Init();
  PixelEngine::Init();
  BPInit();
  Fifo::Init();
  OpcodeDecoder::Init();
  IndexGenerator::Init();
  VertexShaderManager::Init();
  PixelShaderManager::Init();
  VertexLoaderManager::Init();
  Host_Message(WM_USER_CREATE);

  return true;
}

// This is called after Initialize() from the Core
// Run from the graphics thread
void VideoBackend::Video_Prepare()
{
  g_renderer = std::make_unique<Renderer>();
  g_vertex_manager = std::make_unique<VertexManager>();
  g_perf_query = std::make_unique<PerfQuery>();
  g_framebuffer_manager = std::make_unique<FramebufferManager>();
  g_texture_cache = std::make_unique<TextureCache>();
  VertexShaderCache::s_instance = std::make_unique<VertexShaderCache>();
  GeometryShaderCache::s_instance = std::make_unique<GeometryShaderCache>();
  PixelShaderCache::s_instance = std::make_unique<PixelShaderCache>();
}

void VideoBackend::Shutdown()
{
  // Shutdown VideoCommon
  Fifo::Shutdown();
  VertexLoaderManager::Shutdown();
  VertexShaderManager::Shutdown();
  PixelShaderManager::Shutdown();
  OpcodeDecoder::Shutdown();

  // Do our OSD callbacks
  OSD::DoCallbacks(OSD::CallbackType::Shutdown);
}

void VideoBackend::Video_Cleanup()
{
  PixelShaderCache::s_instance.reset();
  VertexShaderCache::s_instance.reset();
  GeometryShaderCache::s_instance.reset();
  g_texture_cache.reset();
  g_perf_query.reset();
  g_vertex_manager.reset();
  g_framebuffer_manager.reset();
  g_renderer.reset();
}
}
