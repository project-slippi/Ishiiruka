// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <wx/wx.h>

#include "Common/Logging/LogManager.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/TessellationShaderManager.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "Core/Core.h"
#include "Core/Host.h"

#include "DolphinWX/Debugger/DebuggerPanel.h"
#include "VideoCommon/IndexGenerator.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "DolphinWX/VideoConfigDiag.h"

#include "VideoBackends/DX11/BoundingBox.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/GeometryShaderCache.h"
#include "VideoBackends/DX11/HullDomainShaderCache.h"
#include "VideoBackends/DX11/PerfQuery.h"
#include "VideoBackends/DX11/PixelShaderCache.h"
#include "VideoBackends/DX11/Render.h"
#include "VideoBackends/DX11/TextureCache.h"
#include "VideoBackends/DX11/VertexManager.h"
#include "VideoBackends/DX11/VertexShaderCache.h"

#include "VideoBackends/DX11/VideoBackend.h"
#include "Core/ConfigManager.h"

namespace DX11
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
	return "DX11";
}

std::string VideoBackend::GetDisplayName() const
{
	return "Direct3D 11";
}

void VideoBackend::InitBackendInfo()
{
	HRESULT hr = DX11::D3D::LoadDXGI();
	if (SUCCEEDED(hr)) hr = DX11::D3D::LoadD3D();
	if (FAILED(hr))
	{
		DX11::D3D::UnloadDXGI();
		return;
	}

	g_Config.backend_info.APIType = API_D3D11;
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_BGRA32] = false;
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_RGBA32] = true;
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_I4_AS_I8] = false;
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_IA4_AS_IA8] = false;
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_I8] = false;
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_IA8] = false;
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_RGB565] = false;
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_DXT1] = true;
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_DXT3] = true;
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_DXT5] = true;

	g_Config.backend_info.bSupportsScaling = false;
	g_Config.backend_info.bSupportsExclusiveFullscreen = true;
	g_Config.backend_info.bSupportsDualSourceBlend = true;
	g_Config.backend_info.bSupportsPixelLighting = true;
	g_Config.backend_info.bNeedBlendIndices = false;
	g_Config.backend_info.bSupportsOversizedViewports = false;
	g_Config.backend_info.bSupportsGeometryShaders = true;
	g_Config.backend_info.bSupports3DVision = true;
	g_Config.backend_info.bSupportsPostProcessing = true;
	g_Config.backend_info.bSupportsClipControl = true;
	g_Config.backend_info.bSupportsNormalMaps = true;
	g_Config.backend_info.bSupportsDepthClamp = true;
	g_Config.backend_info.bSupportsMultithreading = false;
	g_Config.backend_info.bSupportsValidationLayer = false;
	g_Config.backend_info.bSupportsReversedDepthRange = true;
	g_Config.backend_info.bSupportsInternalResolutionFrameDumps = true;
	g_Config.backend_info.bSupportsAsyncShaderCompilation = true;
	IDXGIFactory* factory;
	IDXGIAdapter* ad;
	hr = DX11::PCreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	if (FAILED(hr))
		PanicAlert("Failed to create IDXGIFactory object");

	// adapters
	g_Config.backend_info.Adapters.clear();
	g_Config.backend_info.AAModes.clear();
	while (factory->EnumAdapters((UINT)g_Config.backend_info.Adapters.size(), &ad) != DXGI_ERROR_NOT_FOUND)
	{
		const size_t adapter_index = g_Config.backend_info.Adapters.size();

		DXGI_ADAPTER_DESC desc;
		ad->GetDesc(&desc);

		// TODO: These don't get updated on adapter change, yet
		if (adapter_index == g_Config.iAdapter)
		{
			std::string samples;
			std::vector<DXGI_SAMPLE_DESC> modes = DX11::D3D::EnumAAModes(ad);
			// First iteration will be 1. This equals no AA.
			for (unsigned int i = 0; i < modes.size(); ++i)
			{
				g_Config.backend_info.AAModes.push_back(modes[i].Count);
			}

			bool shader_model_5_supported = (DX11::D3D::GetFeatureLevel(ad) >= D3D_FEATURE_LEVEL_11_0);
			// Requires the earlydepthstencil attribute (only available in shader model 5)
			g_Config.backend_info.bSupportsEarlyZ = shader_model_5_supported;
			// Requires full UAV functionality (only available in shader model 5)
			g_Config.backend_info.bSupportsBBox = shader_model_5_supported;
			// Requires the instance attribute (only available in shader model 5)
			g_Config.backend_info.bSupportsGSInstancing = shader_model_5_supported;
			g_Config.backend_info.bSupportsTessellation = shader_model_5_supported;
			g_Config.backend_info.bSupportsSSAA = shader_model_5_supported;
			g_Config.backend_info.bSupportsGPUTextureDecoding = shader_model_5_supported;
			g_Config.backend_info.bSupportsComputeTextureEncoding = shader_model_5_supported;
			g_Config.backend_info.MaxTextureSize = DX11::D3D::GetMaxTextureSize(DX11::D3D::GetFeatureLevel(ad));
		}

		g_Config.backend_info.Adapters.push_back(UTF16ToUTF8(desc.Description));
		ad->Release();
	}

	factory->Release();

	DX11::D3D::UnloadDXGI();
	DX11::D3D::UnloadD3D();
}

bool VideoBackend::Initialize(void *window_handle)
{
	if (window_handle == nullptr)
		return false;
	InitBackendInfo();
	InitializeShared();
	m_window_handle = window_handle;
	return true;
}

void VideoBackend::Video_Prepare()
{
	// internal interfaces
	g_renderer = std::make_unique<Renderer>(m_window_handle);
	g_renderer->Init();
	g_texture_cache = std::make_unique<TextureCache>();
	g_vertex_manager = std::make_unique<VertexManager>();
	g_perf_query = std::make_unique<PerfQuery>();
	VertexShaderCache::Init();
	PixelShaderCache::Init();
	GeometryShaderCache::Init();
	HullDomainShaderCache::Init();
	D3D::InitUtils();
	BBox::Init();
}

void VideoBackend::Shutdown()
{
	// internal interfaces
	D3D::ShutdownUtils();
	PixelShaderCache::Shutdown();
	GeometryShaderCache::Shutdown();
	HullDomainShaderCache::Shutdown();
	VertexShaderCache::Shutdown();
	BBox::Shutdown();

	g_perf_query.reset();
	g_vertex_manager.reset();
	g_texture_cache.reset();
	g_renderer.reset();

	ShutdownShared();
}

void VideoBackend::Video_Cleanup()
{
	CleanupShared();
}

}
