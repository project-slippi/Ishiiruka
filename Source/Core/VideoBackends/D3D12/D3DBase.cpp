// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

static const unsigned int SWAP_CHAIN_BUFFER_COUNT = 4;

namespace DX12
{

// dxgi.dll exports
static HINSTANCE s_dxgi_dll = nullptr;
static int s_dxgi_dll_ref = 0;
CREATEDXGIFACTORY create_dxgi_factory = nullptr;

// d3d12.dll exports
static HINSTANCE s_d3d12_dll = nullptr;
static int s_d3d12_dll_ref = 0;
D3D12CREATEDEVICE d3d12_create_device = nullptr;
D3D12SERIALIZEROOTSIGNATURE d3d12_serialize_root_signature = nullptr;
D3D12GETDEBUGINTERFACE d3d12_get_debug_interface = nullptr;

namespace D3D
{

// Begin extern'd variables.
ID3D12Device* device;

ComPtr<ID3D12CommandQueue> command_queue;
std::unique_ptr<D3DCommandListManager> command_list_mgr;
ID3D12GraphicsCommandList* current_command_list = nullptr;
size_t root_signature_index = 0;
std::array<ComPtr<ID3D12RootSignature>, 3> root_signatures;

D3D12_CPU_DESCRIPTOR_HANDLE null_srv_cpu = {};
D3D12_CPU_DESCRIPTOR_HANDLE null_srv_cpu_shadow = {};

unsigned int resource_descriptor_size = 0;
unsigned int sampler_descriptor_size = 0;
std::unique_ptr<D3DDescriptorHeapManager> gpu_descriptor_heap_mgr;
std::unique_ptr<D3DSamplerHeapManager> sampler_descriptor_heap_mgr;
std::unique_ptr<D3DDescriptorHeapManager> dsv_descriptor_heap_mgr;
std::unique_ptr<D3DDescriptorHeapManager> rtv_descriptor_heap_mgr;
std::array<ID3D12DescriptorHeap*, 2> gpu_descriptor_heaps;

HWND hWnd;
// End extern'd variables.

static ComPtr<IDXGISwapChain> s_swap_chain;
static unsigned int s_monitor_refresh_rate = 0;

static LARGE_INTEGER s_qpc_frequency;

static ComPtr<ID3D12DebugDevice> s_debug_device;

static D3D_FEATURE_LEVEL s_feat_level;
static bool s_logic_op_supported = false;
static D3DTexture2D* s_backbuf[SWAP_CHAIN_BUFFER_COUNT];
static unsigned int s_current_back_buf = 0;
static unsigned int s_xres = 0;
static unsigned int s_yres = 0;
static bool s_frame_in_progress = false;

static std::vector<DXGI_SAMPLE_DESC> s_aa_modes; // supported AA modes of the current adapter

D3D_FEATURE_LEVEL GetFeatureLevel()
{
	return s_feat_level;
}

bool GetLogicOpSupported()
{
	return s_logic_op_supported;
}

HRESULT LoadDXGI()
{
	if (s_dxgi_dll_ref++ > 0)
		return S_OK;

	if (s_dxgi_dll)
		return S_OK;

	s_dxgi_dll = LoadLibraryA("dxgi.dll");
	if (!s_dxgi_dll)
	{
		MessageBoxA(nullptr, "Failed to load dxgi.dll", "Critical error", MB_OK | MB_ICONERROR);
		--s_dxgi_dll_ref;
		return E_FAIL;
	}
	create_dxgi_factory = (CREATEDXGIFACTORY)GetProcAddress(s_dxgi_dll, "CreateDXGIFactory");

	if (create_dxgi_factory == nullptr)
		MessageBoxA(nullptr, "GetProcAddress failed for CreateDXGIFactory!", "Critical error", MB_OK | MB_ICONERROR);

	return S_OK;
}

HRESULT LoadD3D()
{
	if (s_d3d12_dll_ref++ > 0)
		return S_OK;

	s_d3d12_dll = LoadLibraryA("d3d12.dll");
	if (!s_d3d12_dll)
	{
		MessageBoxA(nullptr, "Failed to load d3d12.dll", "Critical error", MB_OK | MB_ICONERROR);
		--s_d3d12_dll_ref;
		return E_FAIL;
	}

	d3d12_create_device = (D3D12CREATEDEVICE)GetProcAddress(s_d3d12_dll, "D3D12CreateDevice");
	if (d3d12_create_device == nullptr)
	{
		MessageBoxA(nullptr, "GetProcAddress failed for D3D12CreateDevice!", "Critical error", MB_OK | MB_ICONERROR);
		return E_FAIL;
	}

	d3d12_serialize_root_signature = (D3D12SERIALIZEROOTSIGNATURE)GetProcAddress(s_d3d12_dll, "D3D12SerializeRootSignature");
	if (d3d12_serialize_root_signature == nullptr)
	{
		MessageBoxA(nullptr, "GetProcAddress failed for D3D12SerializeRootSignature!", "Critical error", MB_OK | MB_ICONERROR);
		return E_FAIL;
	}

	d3d12_get_debug_interface = (D3D12GETDEBUGINTERFACE)GetProcAddress(s_d3d12_dll, "D3D12GetDebugInterface");
	if (d3d12_get_debug_interface == nullptr)
	{
		MessageBoxA(nullptr, "GetProcAddress failed for D3D12GetDebugInterface!", "Critical error", MB_OK | MB_ICONERROR);
		return E_FAIL;
	}

	return S_OK;
}

void UnloadDXGI()
{
	if (!s_dxgi_dll_ref)
		return;

	if (--s_dxgi_dll_ref != 0)
		return;

	if (s_dxgi_dll)
		FreeLibrary(s_dxgi_dll);

	s_dxgi_dll = nullptr;
	create_dxgi_factory = nullptr;
}

void UnloadD3D()
{
	if (!s_d3d12_dll_ref)
		return;

	if (--s_d3d12_dll_ref != 0)
		return;

	if (s_d3d12_dll)
		FreeLibrary(s_d3d12_dll);

	s_d3d12_dll = nullptr;
	d3d12_create_device = nullptr;
	d3d12_serialize_root_signature = nullptr;
}

std::vector<DXGI_SAMPLE_DESC> EnumAAModes(ID3D12Device* device)
{
	std::vector<DXGI_SAMPLE_DESC> aa_modes;

	for (int samples = 0; samples < D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT; ++samples)
	{
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS multisample_quality_levels = {};
		multisample_quality_levels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		multisample_quality_levels.SampleCount = samples;

		device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &multisample_quality_levels, sizeof(multisample_quality_levels));

		DXGI_SAMPLE_DESC desc;
		desc.Count = samples;
		desc.Quality = 0;

		if (multisample_quality_levels.NumQualityLevels > 0)
			aa_modes.push_back(desc);
	}

	return aa_modes;
}

HRESULT Create(HWND wnd)
{
	hWnd = wnd;
	HRESULT hr;

	RECT client;
	GetClientRect(hWnd, &client);
	s_xres = client.right - client.left;
	s_yres = client.bottom - client.top;

	hr = LoadDXGI();

	if (FAILED(hr))
		return hr;

	hr = LoadD3D();

	if (FAILED(hr))
	{
		UnloadDXGI();
		return hr;
	}

	ComPtr<IDXGIFactory> factory;
	ComPtr<IDXGIAdapter> adapter;

	hr = create_dxgi_factory(__uuidof(IDXGIFactory), (void**)factory.GetAddressOf());
	if (FAILED(hr))
		MessageBox(wnd, _T("Failed to create IDXGIFactory object"), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);

	hr = factory->EnumAdapters(g_ActiveConfig.iAdapter, adapter.GetAddressOf());
	if (FAILED(hr))
	{
		// try using the first one
		hr = factory->EnumAdapters(0, adapter.GetAddressOf());
		if (FAILED(hr))
			MessageBox(wnd, _T("Failed to enumerate adapters"), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);
	}

	DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
	swap_chain_desc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = wnd;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.Windowed = true;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swap_chain_desc.Flags = 0;

	swap_chain_desc.BufferDesc.Width = s_xres;
	swap_chain_desc.BufferDesc.Height = s_yres;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;


	// Enabling the debug layer will fail if the Graphics Tools feature is not installed.
	if (SUCCEEDED(hr) && g_ActiveConfig.bEnableValidationLayer)
	{
		ID3D12Debug* debug_controller;
		hr = d3d12_get_debug_interface(IID_PPV_ARGS(&debug_controller));
		if (SUCCEEDED(hr))
		{
			debug_controller->EnableDebugLayer();
			debug_controller->Release();
		}
		else
		{
			MessageBox(wnd, _T("WARNING: Failed to enable D3D12 debug layer, please ensure the Graphics Tools feature is installed."), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);
		}
	}



	if (SUCCEEDED(hr))
	{
		s_feat_level = D3D_FEATURE_LEVEL_11_0;
		s_logic_op_supported = false;
		hr = d3d12_create_device(adapter.Get(), s_feat_level, IID_PPV_ARGS(&device));
		if (SUCCEEDED(hr))
		{
			D3D_FEATURE_LEVEL levels[] = {
				D3D_FEATURE_LEVEL_12_1,
				D3D_FEATURE_LEVEL_12_0,
				D3D_FEATURE_LEVEL_11_1,
				D3D_FEATURE_LEVEL_11_0
			};
			D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels =
			{
				_countof(levels), levels, D3D_FEATURE_LEVEL_11_0
			};
			HRESULT hres = device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, static_cast<void*>(&featLevels), sizeof(featLevels));
			if (SUCCEEDED(hres))
			{
				s_feat_level = featLevels.MaxSupportedFeatureLevel;
				D3D12_FEATURE_DATA_FORMAT_SUPPORT Support =
				{
					DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE
				};

				if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
					(Support.Support2 & D3D12_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP) != 0)
				{
					s_logic_op_supported = true;
				}
			}
		}
		if (FAILED(hr))
			MessageBox(wnd, _T("Failed to initialize Direct3D.\nMake sure your video card supports Direct3D 12 and your drivers are up-to-date."), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);
	}

	if (SUCCEEDED(hr))
	{
		// get supported AA modes
		s_aa_modes = EnumAAModes(device);

		if (std::find_if(s_aa_modes.begin(), s_aa_modes.end(),
			[](const DXGI_SAMPLE_DESC& desc)
		{
			return desc.Count == g_Config.iMultisamples;
		}
		) == s_aa_modes.end())
		{
			g_Config.iMultisamples = 1;
			UpdateActiveConfig();
		}
	}

	if (SUCCEEDED(hr))
	{
		D3D12_COMMAND_QUEUE_DESC command_queue_desc = {
			D3D12_COMMAND_LIST_TYPE_DIRECT, // D3D12_COMMAND_LIST_TYPE Type;
			0,                              // INT Priority;
			D3D12_COMMAND_QUEUE_FLAG_NONE,  // D3D12_COMMAND_QUEUE_FLAG Flags;
			0                               // UINT NodeMask;
		};

		CheckHR(device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(command_queue.ReleaseAndGetAddressOf())));

		CheckHR(factory->CreateSwapChain(command_queue.Get(), &swap_chain_desc, s_swap_chain.ReleaseAndGetAddressOf()));

		s_current_back_buf = 0;
	}

	if (SUCCEEDED(hr))
	{
		// Query the monitor refresh rate, to ensure proper Present throttling behavior.
		DEVMODE dev_mode;
		memset(&dev_mode, 0, sizeof(DEVMODE));
		dev_mode.dmSize = sizeof(DEVMODE);
		dev_mode.dmDriverExtra = 0;

		if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dev_mode) == 0)
		{
			// If EnumDisplaySettings fails, assume monitor refresh rate of 60 Hz.
			s_monitor_refresh_rate = 60;
		}
		else
		{
			s_monitor_refresh_rate = dev_mode.dmDisplayFrequency;
		}
	}

	if (FAILED(hr))
	{
		MessageBox(wnd, _T("Failed to initialize Direct3D.\nMake sure your video card supports Direct3D 12 and your drivers are up-to-date."), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);
		s_swap_chain.Reset();
		SAFE_RELEASE(device);
		adapter.Reset();
		factory.Reset();
		UnloadD3D();
		UnloadDXGI();
		return E_FAIL;
	}

	ComPtr<ID3D12InfoQueue> info_queue;
	if (SUCCEEDED(device->QueryInterface(info_queue.ReleaseAndGetAddressOf())))
	{
		CheckHR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
		if (g_ActiveConfig.bEnableValidationLayer)
		{
			CheckHR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
		}
		D3D12_INFO_QUEUE_FILTER filter = {};
		D3D12_MESSAGE_ID id_list[] = {
			D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_DEPTHSTENCILVIEW_NOT_SET, // Benign.
			D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET, // Benign.
			D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_TYPE_MISMATCH, // Benign.
			D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED // Benign.
		};
		filter.DenyList.NumIDs = ARRAYSIZE(id_list);
		filter.DenyList.pIDList = id_list;
		info_queue->PushStorageFilter(&filter);

		info_queue.Reset();

		// Used at Close time to report live objects.
		CheckHR(device->QueryInterface(s_debug_device.ReleaseAndGetAddressOf()));
	}

	// prevent DXGI from responding to Alt+Enter, unfortunately DXGI_MWA_NO_ALT_ENTER
	// does not work so we disable all monitoring of window messages. However this
	// may make it more difficult for DXGI to handle display mode changes.
	hr = factory->MakeWindowAssociation(wnd, DXGI_MWA_NO_WINDOW_CHANGES);
	if (FAILED(hr))
		MessageBox(wnd, _T("Failed to associate the window"), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);

	CreateRootSignatures();

	command_list_mgr = std::make_unique<D3DCommandListManager>(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		device,
		command_queue.Get()
		);

	CreateDescriptorHeaps();

	command_list_mgr->GetCommandList(&current_command_list);
	command_list_mgr->SetInitialCommandListState();

	for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
	{
		ComPtr<ID3D12Resource> buf;
		hr = s_swap_chain->GetBuffer(i, IID_PPV_ARGS(buf.ReleaseAndGetAddressOf()));

		CHECK(SUCCEEDED(hr), "Retrieve back buffer texture");

		s_backbuf[i] = new D3DTexture2D(buf.Get(),
			TEXTURE_BIND_FLAG_RENDER_TARGET,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			false,
			D3D12_RESOURCE_STATE_PRESENT // Swap Chain back buffers start out in D3D12_RESOURCE_STATE_PRESENT.
		);

		SetDebugObjectName12(s_backbuf[i]->GetTex(), "backbuffer texture");
	}

	s_backbuf[s_current_back_buf]->TransitionToResourceState(current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	current_command_list->OMSetRenderTargets(1, &s_backbuf[s_current_back_buf]->GetRTV(), FALSE, nullptr);

	QueryPerformanceFrequency(&s_qpc_frequency);

	// Render the device name.
	DXGI_ADAPTER_DESC adapter_desc;
	CheckHR(adapter->GetDesc(&adapter_desc));
	OSD::AddMessage(
		StringFromFormat("Using D3D Adapter: %s.", UTF16ToUTF8(adapter_desc.Description).c_str()));

	StateCache::CheckDiskCacheState(adapter.Get());
	factory.Reset();
	adapter.Reset();

	return S_OK;
}

static void HandleSamplerHeapRestart(void* obj)
{
	if (obj)
	{
		gpu_descriptor_heaps[1] = sampler_descriptor_heap_mgr->GetDescriptorHeap();
		command_list_mgr->ExecuteQueuedWork();
	}
}

void CreateDescriptorHeaps()
{
	// Create D3D12 GPU and CPU descriptor heaps.

	{
		// We bind 8 textures per draw, let's assume a maximum of 4096 draws per frame, with
		// a maximum of 4096 textures created at once. Should be sufficient?
		static constexpr size_t MAX_ACTIVE_TEXTURES = 4096;
		static constexpr size_t MAX_DRAWS_PER_FRAME = 4096;
		static constexpr size_t TEMPORARY_SLOTS = MAX_DRAWS_PER_FRAME * 8;
		gpu_descriptor_heap_mgr = D3DDescriptorHeapManager::Create(
			device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			TEMPORARY_SLOTS + MAX_ACTIVE_TEXTURES, TEMPORARY_SLOTS);
		if (!gpu_descriptor_heap_mgr)
			PanicAlert("Failed to create descriptor heap");

		// Allocate null descriptor for use when filling tables
		gpu_descriptor_heap_mgr->Allocate(nullptr, &null_srv_cpu, &null_srv_cpu_shadow, nullptr);
		gpu_descriptor_heaps[0] = gpu_descriptor_heap_mgr->GetDescriptorHeap();
		resource_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	{
		// Should be more than sufficient. This also includes rendertarget textures.
		static constexpr size_t MAX_RTVS = 4096;
		rtv_descriptor_heap_mgr = D3DDescriptorHeapManager::Create(
			device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, MAX_RTVS, 0);
		if (!rtv_descriptor_heap_mgr)
			PanicAlert("Failed to create descriptor heap");
	}

	{
		// Should be more than sufficient.
		static constexpr size_t MAX_DSVS = 2048;
		dsv_descriptor_heap_mgr = D3DDescriptorHeapManager::Create(
			device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, MAX_DSVS, 0);
		if (!dsv_descriptor_heap_mgr)
			PanicAlert("Failed to create descriptor heap");
	}

	{
		static constexpr size_t MAX_SAMPLERS = 2048;

		sampler_descriptor_heap_mgr = D3DSamplerHeapManager::Create(device, MAX_SAMPLERS);
		if (!sampler_descriptor_heap_mgr)
			PanicAlert("Failed to create descriptor heap");
		sampler_descriptor_heap_mgr->RegisterHeapRestartCallback(sampler_descriptor_heap_mgr.get(), HandleSamplerHeapRestart);
		gpu_descriptor_heaps[1] = sampler_descriptor_heap_mgr->GetDescriptorHeap();
		sampler_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	}
}

void CreateRootSignatures()
{
	D3D12_DESCRIPTOR_RANGE desc_range_srv = {
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,     // D3D12_DESCRIPTOR_RANGE_TYPE RangeType;
		16,                                   // UINT NumDescriptors;
		0,                                   // UINT BaseShaderRegister;
		0,                                   // UINT RegisterSpace;
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND // UINT OffsetInDescriptorsFromTableStart;
	};

	D3D12_DESCRIPTOR_RANGE desc_range_sampler = {
		D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, // D3D12_DESCRIPTOR_RANGE_TYPE RangeType;
		8,                                  // UINT NumDescriptors;
		0,                                   // UINT BaseShaderRegister;
		0,                                   // UINT RegisterSpace;
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND // UINT OffsetInDescriptorsFromTableStart;
	};

	D3D12_ROOT_PARAMETER root_parameters[NUM_GRAPHICS_ROOT_PARAMETERS];

	D3D12_DESCRIPTOR_RANGE desc_range_uav = {
		D3D12_DESCRIPTOR_RANGE_TYPE_UAV,     // D3D12_DESCRIPTOR_RANGE_TYPE RangeType;
		1,                                   // UINT NumDescriptors;
		2,                                   // UINT BaseShaderRegister;
		0,                                   // UINT RegisterSpace;
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND // UINT OffsetInDescriptorsFromTableStart;
	};

	root_parameters[DESCRIPTOR_TABLE_PS_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[DESCRIPTOR_TABLE_PS_SRV].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[DESCRIPTOR_TABLE_PS_SRV].DescriptorTable.pDescriptorRanges = &desc_range_srv;
	root_parameters[DESCRIPTOR_TABLE_PS_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	root_parameters[DESCRIPTOR_TABLE_PS_SAMPLER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[DESCRIPTOR_TABLE_PS_SAMPLER].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[DESCRIPTOR_TABLE_PS_SAMPLER].DescriptorTable.pDescriptorRanges = &desc_range_sampler;
	root_parameters[DESCRIPTOR_TABLE_PS_SAMPLER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	root_parameters[DESCRIPTOR_TABLE_VS_CBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_VS_CBV].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_VS_CBV].Descriptor.ShaderRegister = 0;
	root_parameters[DESCRIPTOR_TABLE_VS_CBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	root_parameters[DESCRIPTOR_TABLE_PS_CBVONE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_PS_CBVONE].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_PS_CBVONE].Descriptor.ShaderRegister = 0;
	root_parameters[DESCRIPTOR_TABLE_PS_CBVONE].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	root_parameters[DESCRIPTOR_TABLE_PS_CBVTWO].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_PS_CBVTWO].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_PS_CBVTWO].Descriptor.ShaderRegister = 1;
	root_parameters[DESCRIPTOR_TABLE_PS_CBVTWO].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	root_parameters[DESCRIPTOR_TABLE_PS_UAV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[DESCRIPTOR_TABLE_PS_UAV].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[DESCRIPTOR_TABLE_PS_UAV].DescriptorTable.pDescriptorRanges = &desc_range_uav;
	root_parameters[DESCRIPTOR_TABLE_PS_UAV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	root_parameters[DESCRIPTOR_TABLE_GS_CBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_GS_CBV].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_GS_CBV].Descriptor.ShaderRegister = 0;
	root_parameters[DESCRIPTOR_TABLE_GS_CBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;

	root_parameters[DESCRIPTOR_TABLE_DS_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[DESCRIPTOR_TABLE_DS_SRV].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[DESCRIPTOR_TABLE_DS_SRV].DescriptorTable.pDescriptorRanges = &desc_range_srv;
	root_parameters[DESCRIPTOR_TABLE_DS_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;

	root_parameters[DESCRIPTOR_TABLE_DS_SAMPLER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[DESCRIPTOR_TABLE_DS_SAMPLER].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[DESCRIPTOR_TABLE_DS_SAMPLER].DescriptorTable.pDescriptorRanges = &desc_range_sampler;
	root_parameters[DESCRIPTOR_TABLE_DS_SAMPLER].ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;

	root_parameters[DESCRIPTOR_TABLE_HS_CBV0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_HS_CBV0].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_HS_CBV0].Descriptor.ShaderRegister = 0;
	root_parameters[DESCRIPTOR_TABLE_HS_CBV0].ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;

	root_parameters[DESCRIPTOR_TABLE_HS_CBV1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_HS_CBV1].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_HS_CBV1].Descriptor.ShaderRegister = 1;
	root_parameters[DESCRIPTOR_TABLE_HS_CBV1].ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;

	root_parameters[DESCRIPTOR_TABLE_HS_CBV2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_HS_CBV2].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_HS_CBV2].Descriptor.ShaderRegister = 2;
	root_parameters[DESCRIPTOR_TABLE_HS_CBV2].ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;

	root_parameters[DESCRIPTOR_TABLE_DS_CBV0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_DS_CBV0].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_DS_CBV0].Descriptor.ShaderRegister = 0;
	root_parameters[DESCRIPTOR_TABLE_DS_CBV0].ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;

	root_parameters[DESCRIPTOR_TABLE_DS_CBV1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_DS_CBV1].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_DS_CBV1].Descriptor.ShaderRegister = 1;
	root_parameters[DESCRIPTOR_TABLE_DS_CBV1].ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;

	root_parameters[DESCRIPTOR_TABLE_DS_CBV2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_DS_CBV2].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_DS_CBV2].Descriptor.ShaderRegister = 2;
	root_parameters[DESCRIPTOR_TABLE_DS_CBV2].ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;



	D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
	root_signature_desc.pParameters = root_parameters;
	root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	root_signature_desc.NumParameters = DESCRIPTOR_TABLE_GS_CBV;

	ComPtr<ID3DBlob> text_root_signature_blob;
	ComPtr<ID3DBlob> text_root_signature_error_blob;

	CheckHR(d3d12_serialize_root_signature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, text_root_signature_blob.ReleaseAndGetAddressOf(), text_root_signature_error_blob.ReleaseAndGetAddressOf()));
	CheckHR(D3D::device->CreateRootSignature(0, text_root_signature_blob->GetBufferPointer(), text_root_signature_blob->GetBufferSize(), IID_PPV_ARGS(root_signatures[0].ReleaseAndGetAddressOf())));
	D3D::SetDebugObjectName12(root_signatures[0].Get(), "Primary root signature");

	root_signature_desc.NumParameters = DESCRIPTOR_TABLE_DS_SRV;
	CheckHR(d3d12_serialize_root_signature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, text_root_signature_blob.ReleaseAndGetAddressOf(), text_root_signature_error_blob.ReleaseAndGetAddressOf()));
	CheckHR(D3D::device->CreateRootSignature(0, text_root_signature_blob->GetBufferPointer(), text_root_signature_blob->GetBufferSize(), IID_PPV_ARGS(root_signatures[1].ReleaseAndGetAddressOf())));
	D3D::SetDebugObjectName12(root_signatures[1].Get(), "GS root signature");

	root_signature_desc.NumParameters = NUM_GRAPHICS_ROOT_PARAMETERS;
	CheckHR(d3d12_serialize_root_signature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, text_root_signature_blob.ReleaseAndGetAddressOf(), text_root_signature_error_blob.ReleaseAndGetAddressOf()));
	CheckHR(D3D::device->CreateRootSignature(0, text_root_signature_blob->GetBufferPointer(), text_root_signature_blob->GetBufferSize(), IID_PPV_ARGS(root_signatures[2].ReleaseAndGetAddressOf())));
	D3D::SetDebugObjectName12(root_signatures[2].Get(), "Tessellation GS root signature");
	root_signature_index = 0;
}

ID3D12RootSignature* GetRootSignature()
{
	return root_signatures[root_signature_index].Get();
}

ID3D12RootSignature* GetBasicRootSignature()
{
	return root_signatures[0].Get();
}

void SetRootSignature(bool geometryenabled, bool tesselationenabled, bool applychange)
{
	bool changed = false;
	size_t new_signature_index = 0;
	if (tesselationenabled)
	{
		new_signature_index = 2;
	}
	else if (geometryenabled)
	{
		new_signature_index = 1;
	}
	changed = new_signature_index != root_signature_index;
	root_signature_index = new_signature_index;
	if (changed && applychange)
	{
		current_command_list->SetGraphicsRootSignature(D3D::GetRootSignature());
	}
}

bool TessellationEnabled()
{
	return root_signature_index == 2;
}
bool GeormetryShadersEnabled()
{
	return root_signature_index > 0;
}

void WaitForOutstandingRenderingToComplete(bool terminate)
{
	command_list_mgr->ExecuteQueuedWork(true, terminate);
}

void Close()
{
	// we can't release the swapchain while in fullscreen.
	s_swap_chain->SetFullscreenState(false, nullptr);

	// Release all back buffer references
	for (UINT i = 0; i < ARRAYSIZE(s_backbuf); i++)
	{
		SAFE_RELEASE(s_backbuf[i]);
	}

	D3D::CleanupPersistentD3DTextureResources();

	s_swap_chain.Reset();

	gpu_descriptor_heap_mgr.reset();
	sampler_descriptor_heap_mgr.reset();
	rtv_descriptor_heap_mgr.reset();
	dsv_descriptor_heap_mgr.reset();

	command_list_mgr.reset();
	command_queue.Reset();

	for (size_t i = 0; i < root_signatures.size(); i++)
	{
		root_signatures[i].Reset();
	}


	ULONG remaining_references = device->Release();
	if ((!s_debug_device && remaining_references) || (s_debug_device && remaining_references > 1))
	{
		ERROR_LOG(VIDEO, "Unreleased D3D12 references: %i.", remaining_references);
	}
	else
	{
		NOTICE_LOG(VIDEO, "Successfully released all D3D12 device references!");
	}

	if (g_ActiveConfig.bEnableValidationLayer)
	{
		if (s_debug_device)
		{
			--remaining_references; // the debug interface increases the refcount of the device, subtract that.
			if (remaining_references)
			{
				// print out alive objects, but only if we actually have pending references
				// note this will also print out internal live objects to the debug console
				s_debug_device->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
			}
			s_debug_device.Reset();
		}
	}

	device = nullptr;
	current_command_list = nullptr;

	// unload DLLs
	UnloadD3D();
	UnloadDXGI();
}

const char* VertexShaderVersionString()
{
	return "vs_5_0";
}

const char* GeometryShaderVersionString()
{
	return "gs_5_0";
}

const char* HullShaderVersionString()
{
	return "hs_5_0";
}
const char* DomainShaderVersionString()
{
	return "ds_5_0";
}

const char* PixelShaderVersionString()
{
	return "ps_5_0";
}

const char* ComputeShaderVersionString()
{
	return "cs_5_0";
}

D3DTexture2D* &GetBackBuffer()
{
	return s_backbuf[s_current_back_buf];
}

unsigned int GetBackBufferWidth()
{
	return s_xres;
}

unsigned int GetBackBufferHeight()
{
	return s_yres;
}

void Reset()
{
	// release all back buffer references
	for (UINT i = 0; i < ARRAYSIZE(s_backbuf); i++)
	{
		SAFE_RELEASE(s_backbuf[i]);
	}

	// Block until all commands have finished.
	// This will also final-release all pending resources (including the backbuffer above)
	command_list_mgr->ExecuteQueuedWork(true);

	// resize swapchain buffers
	RECT client;
	GetClientRect(hWnd, &client);
	s_xres = client.right - client.left;
	s_yres = client.bottom - client.top;

	CheckHR(s_swap_chain->ResizeBuffers(
		SWAP_CHAIN_BUFFER_COUNT,
		s_xres,
		s_yres,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		0));

	// recreate back buffer textures

	HRESULT hr = S_OK;

	for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
	{
		ComPtr<ID3D12Resource> buf;
		hr = s_swap_chain->GetBuffer(i, IID_PPV_ARGS(buf.GetAddressOf()));

		CHECK(SUCCEEDED(hr), "Retrieve back buffer texture");

		s_backbuf[i] = new D3DTexture2D(buf.Get(),
			TEXTURE_BIND_FLAG_RENDER_TARGET,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			false,
			D3D12_RESOURCE_STATE_PRESENT
		);

		SetDebugObjectName12(s_backbuf[i]->GetTex(), "backbuffer texture");
	}

	// The 'about-to-be-presented' back buffer index is always set back to '0' upon ResizeBuffers, just like
	// creating a new swap chain.
	s_current_back_buf = 0;

	s_backbuf[s_current_back_buf]->TransitionToResourceState(current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

bool BeginFrame()
{
	if (s_frame_in_progress)
	{
		PanicAlert("BeginFrame called although a frame is already in progress");
		return false;
	}
	s_frame_in_progress = true;
	return (device != nullptr);
}

void EndFrame()
{
	if (!s_frame_in_progress)
	{
		PanicAlert("EndFrame called although no frame is in progress");
		return;
	}
	s_frame_in_progress = false;
}

void Present()
{
	// The Present function contains logic to ensure we never Present faster than Windows can
	// send to the monitor. If we Present too fast, the Present call will start to block, and we'll be
	// throttled - obviously not desired if vsync is disabled and the emulated CPU speed is > 100%.

	// The throttling logic ensures that we don't Present more than twice in a given monitor vsync.
	// This is accomplished through timing data - there is a programmatic way to determine if a
	// Present call will block, however after investigation that is not feasible here (without invasive
	// workarounds), due to the fact this method does not actually call Present - we just queue a Present
	// command for the background thread to dispatch.

	// The monitor refresh rate is determined in Create().

	static LARGE_INTEGER s_last_present_qpc;

	LARGE_INTEGER current_qpc;
	QueryPerformanceCounter(&current_qpc);

	const double time_elapsed_since_last_present = static_cast<double>(current_qpc.QuadPart - s_last_present_qpc.QuadPart) / s_qpc_frequency.QuadPart;

	unsigned int present_flags = 0;

	if (g_ActiveConfig.IsVSync() == false &&
		time_elapsed_since_last_present < (1.0 / static_cast<double>(s_monitor_refresh_rate)) / 2.0
		)
	{
		present_flags = DXGI_PRESENT_TEST; // Causes Present to be a no-op.
	}
	else
	{
		s_last_present_qpc = current_qpc;

		s_backbuf[s_current_back_buf]->TransitionToResourceState(current_command_list, D3D12_RESOURCE_STATE_PRESENT);
		s_current_back_buf = (s_current_back_buf + 1) % SWAP_CHAIN_BUFFER_COUNT;
	}

	command_list_mgr->ExecuteQueuedWorkAndPresent(s_swap_chain.Get(), g_ActiveConfig.IsVSync() ? 1 : 0, present_flags);


}

HRESULT SetFullscreenState(bool enable_fullscreen)
{
	return S_OK;
}

bool GetFullscreenState()
{
	// Fullscreen exclusive intentionally not supported in DX12 backend. No performance
	// difference between it and windowed full-screen due to usage of a FLIP swap chain.
	return false;
}

}  // namespace D3D

}  // namespace DX12
