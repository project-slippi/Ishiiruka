// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/wx.h>

#include "Common/Logging/LogManager.h"
#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
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
#include "Globals.h"
#include "Common/IniFile.h"
#include "DolphinWX/VideoConfigDiag.h"

#include "VideoBackends/DX11/BoundingBox.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/GeometryShaderCache.h"
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
	return "Direct3D11";
}

std::string VideoBackend::GetConfigName() const
{
   return "gfx_dx11";
}

static std::vector<std::string> GetShaders(const std::string &sub_dir = "")
{
	std::vector<std::string> paths = DoFileSearch({ ".glsl" }, {
		File::GetUserPath(D_SHADERS_IDX) + sub_dir,
		File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir
	});
	std::vector<std::string> result;
	for (std::string path : paths)
	{
	   std::string name;
	   SplitPath(path, nullptr, &name, nullptr);
	   result.push_back(name);
	}
	return result;
}

void InitBackendInfo()
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
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_DXT3] = false;
	g_Config.backend_info.bSupportedFormats[PC_TEX_FMT_DXT5] = true;

	g_Config.backend_info.bSupportsExclusiveFullscreen = true;
	g_Config.backend_info.bSupportsDualSourceBlend = true;
	g_Config.backend_info.bSupportsPixelLighting = true;
	// not worth the effort, less efficient index generation, too much reset ratio over real primitives
	g_Config.backend_info.bSupportsPrimitiveRestart = false;
	g_Config.backend_info.bNeedBlendIndices = false;
	g_Config.backend_info.bSupportsOversizedViewports = false;
	g_Config.backend_info.bSupportsGeometryShaders = true;
	g_Config.backend_info.bSupports3DVision = true;
	g_Config.backend_info.bSupportsPostProcessing = true;
	g_Config.backend_info.bSupportsClipControl = false;
	g_Config.backend_info.bSupportsSSAA = true;
	g_Config.backend_info.bSupportsNormalMaps = true;	IDXGIFactory* factory;
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
			char buf[32];
			std::vector<DXGI_SAMPLE_DESC> modes;
			modes = DX11::D3D::EnumAAModes(ad);
			for (unsigned int i = 0; i < modes.size(); ++i)
			{
				if (i == 0) sprintf_s(buf, 32, _trans("None"));
				else if (modes[i].Quality) sprintf_s(buf, 32, _trans("%d samples (quality level %d)"), modes[i].Count, modes[i].Quality);
				else sprintf_s(buf, 32, _trans("%d samples"), modes[i].Count);
				g_Config.backend_info.AAModes.push_back(buf);
			}

			bool shader_model_5_supported = (DX11::D3D::GetFeatureLevel(ad) >= D3D_FEATURE_LEVEL_11_0);
			// Requires the earlydepthstencil attribute (only available in shader model 5)
			g_Config.backend_info.bSupportsEarlyZ = shader_model_5_supported;
			// Requires full UAV functionality (only available in shader model 5)
			g_Config.backend_info.bSupportsBBox = shader_model_5_supported;
			// Requires the instance attribute (only available in shader model 5)
			g_Config.backend_info.bSupportsGSInstancing = shader_model_5_supported;
		}

		g_Config.backend_info.Adapters.push_back(UTF16ToUTF8(desc.Description));
		ad->Release();
	}

	factory->Release();

	// pp shaders
	g_Config.backend_info.PPShaders = GetShaders("");
	g_Config.backend_info.AnaglyphShaders = GetShaders(ANAGLYPH_DIR DIR_SEP);

	DX11::D3D::UnloadDXGI();
	DX11::D3D::UnloadD3D();
}

void VideoBackend::ShowConfig(void *_hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	if (!s_BackendInitialized)
		InitBackendInfo();
	VideoConfigDiag diag((wxWindow*)_hParent, _trans("Direct3D11"), GetConfigName());
	diag.ShowModal();
#endif
}

bool VideoBackend::Initialize(void *window_handle)
{
	InitializeShared();
	InitBackendInfo();

	frameCount = 0;

	const SConfig& core_params = SConfig::GetInstance();

	g_Config.Load((File::GetUserPath(D_CONFIG_IDX) + GetConfigName() + ".ini").c_str());
	g_Config.GameIniLoad();
	g_Config.UpdateProjectionHack();
	g_Config.VerifyValidity();
	UpdateActiveConfig();

	m_window_handle = window_handle;

	s_BackendInitialized = true;

	return true;
}

void VideoBackend::Video_Prepare()
{
	// internal interfaces
	g_renderer = new Renderer(m_window_handle);
	g_texture_cache = new TextureCache;
	g_vertex_manager = new VertexManager;
	g_perf_query = new PerfQuery;
	VertexShaderCache::Init();
	PixelShaderCache::Init();
	D3D::InitUtils();

	// VideoCommon
	BPInit();
	Fifo_Init();
	IndexGenerator::Init();
	VertexLoaderManager::Init();
	OpcodeDecoder_Init();
	VertexShaderManager::Init();
	GeometryShaderCache::Init();
	PixelShaderManager::Init(true);
	CommandProcessor::Init();
	PixelEngine::Init();
	BBox::Init();
	// Tell the host that the window is ready
	Host_Message(WM_USER_CREATE);
}

void VideoBackend::Shutdown()
{
	s_BackendInitialized = false;

	// TODO: should be in Video_Cleanup
	if (g_renderer)
	{
		// VideoCommon
		Fifo_Shutdown();
		CommandProcessor::Shutdown();
		PixelShaderManager::Shutdown();
		VertexShaderManager::Shutdown();
		OpcodeDecoder_Shutdown();
		VertexLoaderManager::Shutdown();

		// internal interfaces
		D3D::ShutdownUtils();
		PixelShaderCache::Shutdown();
		GeometryShaderCache::Shutdown();
		VertexShaderCache::Shutdown();
		BBox::Shutdown();
		delete g_perf_query;
		delete g_vertex_manager;
		delete g_texture_cache;
		delete g_renderer;
		g_renderer = nullptr;
		g_texture_cache = nullptr;
	}
}

void VideoBackend::Video_Cleanup() {
}

}
