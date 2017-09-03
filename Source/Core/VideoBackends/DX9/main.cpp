// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#include <memory>

#include "Common/Atomic.h"
#include "Common/Common.h"
#include "Common/IniFile.h"
#include "Common/Logging/LogManager.h"
#include "Common/Thread.h"

#if defined(HAVE_WX) && HAVE_WX
#include "DolphinWX/VideoConfigDiag.h"
#include "DolphinWX/Debugger/DebuggerPanel.h"
#endif // HAVE_WX

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"

#include "VideoBackends/DX9/D3DTexture.h"
#include "VideoBackends/DX9/D3DUtil.h"
#include "VideoBackends/DX9/FramebufferManager.h"
#include "VideoBackends/DX9/PerfQuery.h"
#include "VideoBackends/DX9/PixelShaderCache.h"
#include "VideoBackends/DX9/Render.h"
#include "VideoBackends/DX9/TextureCache.h"
#include "VideoBackends/DX9/VertexManager.h"
#include "VideoBackends/DX9/VertexShaderCache.h"
#include "VideoBackends/DX9/VideoBackend.h"

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
#include "VideoCommon/VideoState.h"
#include "VideoCommon/VideoConfig.h"

namespace DX9
{

unsigned int VideoBackend::PeekMessages()
{
	MSG msg;
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return FALSE;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return TRUE;
}

std::string VideoBackend::GetName() const
{
	return "DX9";
}

std::string VideoBackend::GetDisplayName() const
{
	return "Direct3D 9";
}

void VideoBackend::InitBackendInfo()
{
	DX9::D3D::Init();
	D3DCAPS9 device_caps = DX9::D3D::GetCaps();
	const int shaderModel = ((device_caps.PixelShaderVersion >> 8) & 0xFF);
	const int maxConstants = (shaderModel < 3) ? 32 : ((shaderModel < 4) ? 224 : 65536);
	g_Config.backend_info.APIType = shaderModel < 3 ? API_D3D9_SM20 : API_D3D9_SM30;
	g_Config.backend_info.MaxTextureSize = static_cast<u32>(device_caps.MaxTextureWidth);
	g_Config.backend_info.bSupportsExclusiveFullscreen = false;
	g_Config.backend_info.bSupportsSeparateAlphaFunction = (device_caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) == D3DPMISCCAPS_SEPARATEALPHABLEND;
	// Dual source blend disabled by default until a proper method to test for support is found	
	g_Config.backend_info.bSupportsScaling = true;
	g_Config.backend_info.bSupportsDualSourceBlend = false;
	g_Config.backend_info.bSupportsPixelLighting = C_PENVCONST_END <= maxConstants;
	g_Config.backend_info.bSupportsNormalMaps = false;
	g_Config.backend_info.bSupportsEarlyZ = true;
	g_Config.backend_info.bNeedBlendIndices = true;
	g_Config.backend_info.bSupportsOversizedViewports = false;
	g_Config.backend_info.bSupportsBBox = false;
	g_Config.backend_info.bSupportsGeometryShaders = false;
	g_Config.backend_info.bSupports3DVision = false;
	g_Config.backend_info.bSupportsPostProcessing = false;
	g_Config.backend_info.bSupportsClipControl = true;
	g_Config.backend_info.bSupportsSSAA = false;
	g_Config.backend_info.bSupportsTessellation = false;
	g_Config.backend_info.bSupportsGPUTextureDecoding = false;
	g_Config.backend_info.bSupportsComputeTextureEncoding = false;
	g_Config.backend_info.bSupportsDepthClamp = false;
	g_Config.backend_info.bSupportsMultithreading = false;
	g_Config.backend_info.bSupportsValidationLayer = false;
	g_Config.backend_info.bSupportsReversedDepthRange = false;
	g_Config.backend_info.bSupportsInternalResolutionFrameDumps = false;
	g_Config.backend_info.bSupportsAsyncShaderCompilation = true;
	// adapters
	g_Config.backend_info.Adapters.clear();
	for (int i = 0; i < DX9::D3D::GetNumAdapters(); ++i)
		g_Config.backend_info.Adapters.push_back(DX9::D3D::GetAdapter(i).ident.Description);

	// aamodes
	g_Config.backend_info.AAModes.clear();
	if (g_Config.iAdapter < DX9::D3D::GetNumAdapters())
	{
		const DX9::D3D::Adapter &adapter = DX9::D3D::GetAdapter(g_Config.iAdapter);

		for (int i = 0; i < (int)adapter.aa_levels.size(); ++i)
			g_Config.backend_info.AAModes.push_back(i + 1);
	}

	DX9::D3D::Shutdown();
	UpdateActiveConfig();
}

bool VideoBackend::Initialize(void *window_handle)
{
	if (window_handle == nullptr)
		return false;
	InitBackendInfo();
	InitializeShared();
	// as only some driver/hardware configurations support dual source blending only enable it if is 
	// configured by user
	g_Config.backend_info.bSupportsDualSourceBlend = g_Config.bForceDualSourceBlend;
	UpdateActiveConfig();
	m_window_handle = window_handle;
	if (FAILED(DX9::D3D::Init()))
	{
		MessageBox(GetActiveWindow(), _T("Unable to initialize Direct3D. Please make sure that you have the latest version of DirectX 9.0c correctly installed."), _T("Fatal Error"), MB_ICONERROR | MB_OK);
		return false;
	}
	return true;
}

void VideoBackend::Video_Prepare()
{
	// internal interfaces
	g_texture_cache = std::make_unique<TextureCache>();
	g_vertex_manager = std::make_unique<VertexManager>();
	g_perf_query = std::make_unique<PerfQuery>();
	g_renderer = std::make_unique<Renderer>(m_window_handle);
	g_renderer->Init();
	UpdateActiveConfig();
}

void VideoBackend::Shutdown()
{
	// internal interfaces
	PixelShaderCache::Shutdown();
	VertexShaderCache::Shutdown();
	g_renderer.reset();
	g_perf_query.reset();
	g_vertex_manager.reset();
	g_texture_cache.reset();
	D3D::Shutdown();
	ShutdownShared();
}

void VideoBackend::Video_Cleanup()
{
	CleanupShared();
}

}
