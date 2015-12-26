// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/StringUtil.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoBackends/D3D12/VertexShaderCache.h"
#include "VideoCommon/VideoConfig.h"

static const unsigned int s_swap_chain_buffer_count = 4;

namespace DX12
{

CREATEDXGIFACTORY create_dxgi_factory = nullptr;
HINSTANCE dxgi_dll = nullptr;
int dxgi_dll_ref = 0;

D3D12CREATEDEVICE d3d12_create_device = nullptr;
D3D12SERIALIZEROOTSIGNATURE d3d12_serialize_root_signature = nullptr;
D3D12GETDEBUGINTERFACE d3d12_get_debug_interface = nullptr;

HINSTANCE d3d12_dll = nullptr;
int d3d12_dll_ref = 0;

namespace D3D
{

ID3D12Device* device12 = nullptr;

ID3D12CommandQueue* command_queue = nullptr;
D3DCommandListManager* command_list_mgr = nullptr;
ID3D12GraphicsCommandList* current_command_list = nullptr;
ID3D12RootSignature* default_root_signature = nullptr;

static IDXGISwapChain* swapchain = nullptr;
static ID3D12DebugDevice* debug12 = nullptr;

D3D12_CPU_DESCRIPTOR_HANDLE null_srv_cpu = {};
D3D12_CPU_DESCRIPTOR_HANDLE null_srv_cpu_shadow = {};

unsigned int resource_descriptor_size = 0;
unsigned int sampler_descriptor_size = 0;
D3DDescriptorHeapManager* gpu_descriptor_heap_mgr = nullptr;
D3DDescriptorHeapManager* sampler_descriptor_heap_mgr = nullptr;
D3DDescriptorHeapManager* dsv_descriptor_heap_mgr = nullptr;
D3DDescriptorHeapManager* rtv_descriptor_heap_mgr = nullptr;
ID3D12DescriptorHeap* gpu_descriptor_heaps[2];

D3D_FEATURE_LEVEL feat_level;
D3DTexture2D* backbuf[s_swap_chain_buffer_count];
UINT current_back_buf = 0;

HWND hWnd;

std::vector<DXGI_SAMPLE_DESC> aa_modes; // supported AA modes of the current adapter

bool bgra_textures_supported;

#define NUM_SUPPORTED_FEATURE_LEVELS 1
const D3D_FEATURE_LEVEL supported_feature_levels[NUM_SUPPORTED_FEATURE_LEVELS] = {
	D3D_FEATURE_LEVEL_11_0
};

unsigned int xres, yres;

bool frame_in_progress = false;

HRESULT LoadDXGI()
{
	if (dxgi_dll_ref++ > 0)
		return S_OK;

	if (dxgi_dll)
		return S_OK;
	
	dxgi_dll = LoadLibraryA("dxgi.dll");
	if (!dxgi_dll)
	{
		MessageBoxA(nullptr, "Failed to load dxgi.dll", "Critical error", MB_OK | MB_ICONERROR);
		--dxgi_dll_ref;
		return E_FAIL;
	}
	create_dxgi_factory = (CREATEDXGIFACTORY)GetProcAddress(dxgi_dll, "CreateDXGIFactory");
	
	if (create_dxgi_factory == nullptr)
		MessageBoxA(nullptr, "GetProcAddress failed for CreateDXGIFactory!", "Critical error", MB_OK | MB_ICONERROR);

	return S_OK;
}

HRESULT LoadD3D()
{
	if (d3d12_dll_ref++ > 0)
		return S_OK;

	d3d12_dll = LoadLibraryA("d3d12.dll");
	if (!d3d12_dll)
	{
		MessageBoxA(nullptr, "Failed to load d3d12.dll", "Critical error", MB_OK | MB_ICONERROR);
		--d3d12_dll_ref;
		return E_FAIL;
	}

	d3d12_create_device = (D3D12CREATEDEVICE)GetProcAddress(d3d12_dll, "D3D12CreateDevice");
	if (d3d12_create_device == nullptr)
	{
		MessageBoxA(nullptr, "GetProcAddress failed for D3D12CreateDevice!", "Critical error", MB_OK | MB_ICONERROR);
		return E_FAIL;
	}

	d3d12_serialize_root_signature = (D3D12SERIALIZEROOTSIGNATURE)GetProcAddress(d3d12_dll, "D3D12SerializeRootSignature");
	if (d3d12_serialize_root_signature == nullptr)
	{
		MessageBoxA(nullptr, "GetProcAddress failed for D3D12SerializeRootSignature!", "Critical error", MB_OK | MB_ICONERROR);
		return E_FAIL;
	}

	d3d12_get_debug_interface = (D3D12GETDEBUGINTERFACE)GetProcAddress(d3d12_dll, "D3D12GetDebugInterface");
	if (d3d12_get_debug_interface == nullptr)
	{
		MessageBoxA(nullptr, "GetProcAddress failed for D3D12GetDebugInterface!", "Critical error", MB_OK | MB_ICONERROR);
		return E_FAIL;
	}

	return S_OK;
}

void UnloadDXGI()
{
	if (!dxgi_dll_ref)
		return;

	if (--dxgi_dll_ref != 0)
		return;

	if (dxgi_dll)
		FreeLibrary(dxgi_dll);

	dxgi_dll = nullptr;
	create_dxgi_factory = nullptr;
}

void UnloadD3D()
{
	if (!d3d12_dll_ref)
		return;

	if (--d3d12_dll_ref != 0)
		return;

	if (d3d12_dll)
		FreeLibrary(d3d12_dll);

	d3d12_dll = nullptr;
	d3d12_create_device = nullptr;
	d3d12_serialize_root_signature = nullptr;
}

std::vector<DXGI_SAMPLE_DESC> EnumAAModes(IDXGIAdapter* adapter)
{
	std::vector<DXGI_SAMPLE_DESC> aa_modes;

	ID3D12Device* device12;
	d3d12_create_device(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device12));

	for (int samples = 0; samples < D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT; ++samples)
	{
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS multisample_quality_levels = {};
		multisample_quality_levels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		multisample_quality_levels.SampleCount = samples;
				
		device12->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &multisample_quality_levels, sizeof(multisample_quality_levels));

		DXGI_SAMPLE_DESC desc;
		desc.Count = samples;
		desc.Quality = 0;

		if (multisample_quality_levels.NumQualityLevels > 0)
		{
			aa_modes.push_back(desc);
		}
	}

	device12->Release();

	return aa_modes;
}

D3D_FEATURE_LEVEL GetFeatureLevel(IDXGIAdapter* adapter)
{
	return D3D_FEATURE_LEVEL_11_0;
}

HRESULT Create(HWND wnd)
{
	hWnd = wnd;
	HRESULT hr;

	RECT client;
	GetClientRect(hWnd, &client);
	xres = client.right - client.left;
	yres = client.bottom - client.top;

	hr = LoadDXGI();
	if (SUCCEEDED(hr))
		hr = LoadD3D();

	if (FAILED(hr))
	{
		UnloadDXGI();
		UnloadD3D();
		return hr;
	}

	IDXGIFactory* factory;
	IDXGIAdapter* adapter;
	IDXGIOutput* output;
	hr = create_dxgi_factory(__uuidof(IDXGIFactory), (void**)&factory);
	if (FAILED(hr))
		MessageBox(wnd, _T("Failed to create IDXGIFactory object"), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);

	hr = factory->EnumAdapters(g_ActiveConfig.iAdapter, &adapter);
	if (FAILED(hr))
	{
		// try using the first one
		hr = factory->EnumAdapters(0, &adapter);
		if (FAILED(hr))
			MessageBox(wnd, _T("Failed to enumerate adapters"), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);
	}

	// TODO: Make this configurable
	hr = adapter->EnumOutputs(0, &output);
	if (FAILED(hr))
	{
		// try using the first one
		IDXGIAdapter* firstadapter;
		hr = factory->EnumAdapters(0, &firstadapter);
		if (!FAILED(hr))
			hr = firstadapter->EnumOutputs(0, &output);
		if (FAILED(hr))
			MessageBox(wnd,
				_T("Failed to enumerate outputs!\n")
				_T("This usually happens when you've set your video adapter to the Nvidia GPU in an Optimus-equipped system.\n")
				_T("Set Dolphin to use the high-performance graphics in Nvidia's drivers instead and leave Dolphin's video adapter set to the Intel GPU."),
				_T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);

		SAFE_RELEASE(firstadapter);
	}

	// get supported AA modes
	aa_modes = EnumAAModes(adapter);

	if (std::find_if(
		aa_modes.begin(),
		aa_modes.end(),
		[](const DXGI_SAMPLE_DESC& desc) {return desc.Count == g_Config.iMultisamples; }
		) == aa_modes.end())
	{
		g_Config.iMultisamples = 1;
		UpdateActiveConfig();
	}

	DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
	swap_chain_desc.BufferCount = s_swap_chain_buffer_count;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = wnd;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.Windowed = true;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	swap_chain_desc.BufferDesc.Width = xres;
	swap_chain_desc.BufferDesc.Height = yres;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

#if defined(_DEBUG) || defined(DEBUGFAST)
	// Creating debug devices can sometimes fail if the user doesn't have the correct
	// version of the DirectX SDK. If it does, simply fallback to a non-debug device.
	{
		if (SUCCEEDED(hr))
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
				MessageBox(wnd, _T("Failed to initialize Direct3D debug layer."), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);
			}

			hr = d3d12_create_device(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device12));

			feat_level = D3D_FEATURE_LEVEL_11_0;
		}
	}

	if (FAILED(hr))
#endif
	{
		if (SUCCEEDED(hr))
		{
#ifdef USE_D3D12_DEBUG_LAYER
			ID3D12Debug* debug_controller;
			hr = d3d12_get_debug_interface(IID_PPV_ARGS(&debug_controller));
			if (SUCCEEDED(hr))
			{
				debug_controller->EnableDebugLayer();
				debug_controller->Release();
			}
			else
			{
				MessageBox(wnd, _T("Failed to initialize Direct3D debug layer."), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);
			}
#endif
			hr = d3d12_create_device(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device12));

			feat_level = D3D_FEATURE_LEVEL_11_0;
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

		CheckHR(device12->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&command_queue)));

		IDXGIFactory* factory = nullptr;
		adapter->GetParent(IID_PPV_ARGS(&factory));

		CheckHR(factory->CreateSwapChain(command_queue, &swap_chain_desc, &swapchain));

		current_back_buf = 0;

		factory->Release();
	}

	if (FAILED(hr))
	{
		MessageBox(wnd, _T("Failed to initialize Direct3D.\nMake sure your video card supports Direct3D 12."), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);
		SAFE_RELEASE(swapchain);
		return E_FAIL;
	}
	
	ID3D12InfoQueue* info_queue = nullptr;
	if (SUCCEEDED(device12->QueryInterface(&info_queue)))
	{
		CheckHR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
		CheckHR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));

		D3D12_INFO_QUEUE_FILTER filter = {};
		D3D12_MESSAGE_ID id_list[] = {
			D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_DEPTHSTENCILVIEW_NOT_SET, // Benign.
			D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET, // Benign.
			D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_TYPE_MISMATCH, // Benign.
			D3D12_MESSAGE_ID_DRAW_EMPTY_SCISSOR_RECTANGLE, // Benign. Probably.
			D3D12_MESSAGE_ID_INVALID_SUBRESOURCE_STATE,
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE, // Benign.
			D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED, // Benign.
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH // Benign. Probably.
		};
		filter.DenyList.NumIDs = ARRAYSIZE(id_list);
		filter.DenyList.pIDList = id_list;
		info_queue->PushStorageFilter(&filter);

		info_queue->Release();

		// Used at Close time to report live objects.
		CheckHR(device12->QueryInterface(&debug12));
	}

	// prevent DXGI from responding to Alt+Enter, unfortunately DXGI_MWA_NO_ALT_ENTER
	// does not work so we disable all monitoring of window messages. However this
	// may make it more difficult for DXGI to handle display mode changes.
	hr = factory->MakeWindowAssociation(wnd, DXGI_MWA_NO_WINDOW_CHANGES);
	if (FAILED(hr))
		MessageBox(wnd, _T("Failed to associate the window"), _T("Dolphin Direct3D 12 backend"), MB_OK | MB_ICONERROR);

	SAFE_RELEASE(factory);
	SAFE_RELEASE(output);
	SAFE_RELEASE(adapter)

	CreateDescriptorHeaps();
	CreateRootSignatures();

	command_list_mgr = new D3DCommandListManager(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		device12,
		command_queue,
		gpu_descriptor_heaps,
		ARRAYSIZE(gpu_descriptor_heaps),
		default_root_signature
		);

	command_list_mgr->Getcommand_list(&current_command_list);
	command_list_mgr->SetInitialcommand_listState();

	for (UINT i = 0; i < s_swap_chain_buffer_count; i++)
	{
		ID3D12Resource* buf12;
		hr = swapchain->GetBuffer(i, IID_PPV_ARGS(&buf12));

		backbuf[i] = new D3DTexture2D(buf12,
			D3D11_BIND_RENDER_TARGET,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			false,
			D3D12_RESOURCE_STATE_PRESENT // Swap Chain back buffers start out in D3D12_RESOURCE_STATE_PRESENT.
			);

		CHECK(backbuf != nullptr, "Create back buffer texture");

		SAFE_RELEASE(buf12);
		SetDebugObjectName12(backbuf[i]->GetTex12(), "backbuffer texture");
	}	

	backbuf[current_back_buf]->TransitionToResourceState(current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
	current_command_list->OMSetRenderTargets(1, &backbuf[current_back_buf]->GetRTV12(), FALSE, nullptr);

	// BGRA textures are easier to deal with in TextureCache, but might not be supported by the hardware. But are always supported on D3D12.
	bgra_textures_supported = true;

	return S_OK;
}

void CreateDescriptorHeaps()
{
	// Create D3D12 GPU and CPU descriptor heaps.

	{
		D3D12_DESCRIPTOR_HEAP_DESC gpu_descriptor_heap_desc = {};
		gpu_descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		gpu_descriptor_heap_desc.NumDescriptors = 500000;
		gpu_descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

		gpu_descriptor_heap_mgr = new D3DDescriptorHeapManager(&gpu_descriptor_heap_desc, device12, 50000);

		gpu_descriptor_heaps[0] = gpu_descriptor_heap_mgr->GetDescriptorHeap();

		D3D12_CPU_DESCRIPTOR_HANDLE descriptor_heap_cpu_base = gpu_descriptor_heap_mgr->GetDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();

		resource_descriptor_size = device12->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		sampler_descriptor_size = device12->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		D3D12_GPU_DESCRIPTOR_HANDLE null_srv_gpu = {};
		gpu_descriptor_heap_mgr->Allocate(&null_srv_cpu, &null_srv_gpu, &null_srv_cpu_shadow);

		D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc = {};
		null_srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		null_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		device12->CreateShaderResourceView(NULL, &null_srv_desc, null_srv_cpu);

		for (UINT i = 0; i < 500000; i++)
		{
			// D3D12TODO: Make paving of descriptor heap optional.

			D3D12_CPU_DESCRIPTOR_HANDLE destination_descriptor = {};
			destination_descriptor.ptr = descriptor_heap_cpu_base.ptr + i * resource_descriptor_size;

			device12->CreateShaderResourceView(NULL, &null_srv_desc, destination_descriptor);
		}
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC sampler_descriptor_heap_desc = {};
		sampler_descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		sampler_descriptor_heap_desc.NumDescriptors = 2000;
		sampler_descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

		sampler_descriptor_heap_mgr = new D3DDescriptorHeapManager(&sampler_descriptor_heap_desc, device12);

		gpu_descriptor_heaps[1] = sampler_descriptor_heap_mgr->GetDescriptorHeap();
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC dsv_descriptor_heap_desc = {};
		dsv_descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsv_descriptor_heap_desc.NumDescriptors = 2000;
		dsv_descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

		dsv_descriptor_heap_mgr = new D3DDescriptorHeapManager(&dsv_descriptor_heap_desc, device12);
	}

	{
		// D3D12TODO: Temporary workaround.. really need to properly suballocate out of render target heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtv_descriptor_heap_desc = {};
		rtv_descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtv_descriptor_heap_desc.NumDescriptors = 1000000;
		rtv_descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

		rtv_descriptor_heap_mgr = new D3DDescriptorHeapManager(&rtv_descriptor_heap_desc, device12);
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
		8,                                   // UINT NumDescriptors;
		0,                                   // UINT BaseShaderRegister;
		0,                                   // UINT RegisterSpace;
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND // UINT OffsetInDescriptorsFromTableStart;
	};

	D3D12_ROOT_PARAMETER root_parameters[6];

	root_parameters[DESCRIPTOR_TABLE_PS_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[DESCRIPTOR_TABLE_PS_SRV].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[DESCRIPTOR_TABLE_PS_SRV].DescriptorTable.pDescriptorRanges = &desc_range_srv;
	root_parameters[DESCRIPTOR_TABLE_PS_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	
	root_parameters[DESCRIPTOR_TABLE_PS_SAMPLER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[DESCRIPTOR_TABLE_PS_SAMPLER].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[DESCRIPTOR_TABLE_PS_SAMPLER].DescriptorTable.pDescriptorRanges = &desc_range_sampler;
	root_parameters[DESCRIPTOR_TABLE_PS_SAMPLER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	
	root_parameters[DESCRIPTOR_TABLE_GS_CBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_GS_CBV].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_GS_CBV].Descriptor.ShaderRegister = 0;
	root_parameters[DESCRIPTOR_TABLE_GS_CBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;

	root_parameters[DESCRIPTOR_TABLE_VS_CBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_VS_CBV].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_VS_CBV].Descriptor.ShaderRegister = 0;
	root_parameters[DESCRIPTOR_TABLE_VS_CBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	root_parameters[DESCRIPTOR_TABLE_PS_CBVONE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[DESCRIPTOR_TABLE_PS_CBVONE].Descriptor.RegisterSpace = 0;
	root_parameters[DESCRIPTOR_TABLE_PS_CBVONE].Descriptor.ShaderRegister = 0;
	root_parameters[DESCRIPTOR_TABLE_PS_CBVONE].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	if (g_ActiveConfig.bEnablePixelLighting)
	{
		root_parameters[DESCRIPTOR_TABLE_PS_CBVTWO].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[DESCRIPTOR_TABLE_PS_CBVTWO].Descriptor.RegisterSpace = 0;
		root_parameters[DESCRIPTOR_TABLE_PS_CBVTWO].Descriptor.ShaderRegister = 1;
		root_parameters[DESCRIPTOR_TABLE_PS_CBVTWO].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	// D3D12TODO: Add bounding box UAV to root signature.

	D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
	root_signature_desc.pParameters = root_parameters;
	root_signature_desc.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

	root_signature_desc.NumParameters = ARRAYSIZE(root_parameters);

	if (!g_ActiveConfig.bEnablePixelLighting)
		root_signature_desc.NumParameters--;

	ID3DBlob* text_root_signature_blob;
	ID3DBlob* text_root_signature_error_blob;

	CheckHR(d3d12_serialize_root_signature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &text_root_signature_blob, &text_root_signature_error_blob));

	CheckHR(D3D::device12->CreateRootSignature(0, text_root_signature_blob->GetBufferPointer(), text_root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&default_root_signature)));
}

void WaitForOutstandingRenderingToComplete()
{
	command_list_mgr->ClearQueueAndWaitForCompletionOfInflightWork();
}

void Close()
{
	// we can't release the swapchain while in fullscreen.
	swapchain->SetFullscreenState(false, nullptr);

	// Release all back buffer references
	for (UINT i = 0; i < ARRAYSIZE(backbuf); i++)
	{
		SAFE_RELEASE(backbuf[i]);
	}

	command_list_mgr->ImmediatelyDestroyAllResourcesScheduledForDestruction();

	SAFE_RELEASE(swapchain);

	command_list_mgr->Release();
	command_queue->Release();

	default_root_signature->Release();

	gpu_descriptor_heap_mgr->Release();
	sampler_descriptor_heap_mgr->Release();
	rtv_descriptor_heap_mgr->Release();
	dsv_descriptor_heap_mgr->Release();

	D3D::CleanupPersistentD3DTextureResources();

	ULONG references12 = device12->Release();
	if ((!debug12 && references12) || (debug12 && references12 > 1))
	{
		ERROR_LOG(VIDEO, "Unreleased D3D12 references: %i.", references12);
	}
	else
	{
		NOTICE_LOG(VIDEO, "Successfully released all D3D12 device references!");
	}

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (debug12)
	{
		--references12; // the debug interface increases the refcount of the device, subtract that.
		if (references12)
		{
			// print out alive objects, but only if we actually have pending references
			// note this will also print out internal live objects to the debug console
			debug12->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
		}
		SAFE_RELEASE(debug12);
	}
#endif

	device12 = nullptr;
	current_command_list = nullptr;

	// unload DLLs
	UnloadDXGI();
	UnloadD3D();
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
	return backbuf[current_back_buf];
}

unsigned int GetBackBufferWidth()
{
	return xres;
}

unsigned int GetBackBufferHeight()
{
	return yres;
}

bool BGRATexturesSupported()
{
	return bgra_textures_supported;
}

// Returns the maximum width/height of a texture. This value only depends upon the feature level in DX12
unsigned int GetMaxTextureSize()
{
	return D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
}

void Reset()
{
	command_list_mgr->ExecuteQueuedWork(true);

	// release all back buffer references
	for (UINT i = 0; i < ARRAYSIZE(backbuf); i++)
	{
		SAFE_RELEASE(backbuf[i]);
	}

	D3D::command_list_mgr->ImmediatelyDestroyAllResourcesScheduledForDestruction();

	// resize swapchain buffers
	RECT client;
	GetClientRect(hWnd, &client);
	xres = client.right - client.left;
	yres = client.bottom - client.top;

	CheckHR(D3D::swapchain->ResizeBuffers(s_swap_chain_buffer_count, xres, yres, DXGI_FORMAT_R8G8B8A8_UNORM, 0));

	// recreate back buffer textures

	HRESULT hr = S_OK;
	ID3D12Resource* buf12 = nullptr;

	for (UINT i = 0; i < s_swap_chain_buffer_count; i++)
	{
		ID3D12Resource* buf12;
		hr = swapchain->GetBuffer(i, IID_PPV_ARGS(&buf12));

		CHECK(SUCCEEDED(hr), "Create back buffer texture");

		backbuf[i] = new D3DTexture2D(buf12,
			D3D11_BIND_RENDER_TARGET,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			false,
			D3D12_RESOURCE_STATE_PRESENT
			);

		CHECK(backbuf != nullptr, "Create back buffer texture");

		SAFE_RELEASE(buf12);
		SetDebugObjectName12(backbuf[i]->GetTex12(), "backbuffer texture");
	}

	current_back_buf = 0;

	backbuf[current_back_buf]->TransitionToResourceState(current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

bool BeginFrame()
{
	if (frame_in_progress)
	{
		PanicAlert("BeginFrame called although a frame is already in progress");
		return false;
	}
	frame_in_progress = true;
	return (device12 != nullptr);
}

void EndFrame()
{
	if (!frame_in_progress)
	{
		PanicAlert("EndFrame called although no frame is in progress");
		return;
	}
	frame_in_progress = false;
}

LARGE_INTEGER last_present;
LARGE_INTEGER frequency;

UINT sync_refresh_count = 0;

void Present()
{
	UINT present_flags = DXGI_PRESENT_TEST;

	LARGE_INTEGER current_timestamp;
	QueryPerformanceCounter(&current_timestamp);
	QueryPerformanceFrequency(&frequency);

	// Only present at most two times per vblank interval. If the application exhausts available back buffers, the
	// the Present call will block until the next vblank.
	
	if ((UINT)g_ActiveConfig.IsVSync() || (((current_timestamp.QuadPart - last_present.QuadPart) * 1000) / frequency.QuadPart >= (16.667 / 2)))
	{
		last_present = current_timestamp;

		backbuf[current_back_buf]->TransitionToResourceState(current_command_list, D3D12_RESOURCE_STATE_PRESENT);
		present_flags = 0;

		current_back_buf = (current_back_buf + 1) % s_swap_chain_buffer_count;
	}

	command_list_mgr->ExecuteQueuedWorkAndPresent(swapchain, (UINT)g_ActiveConfig.IsVSync(), present_flags);

	backbuf[current_back_buf]->TransitionToResourceState(current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);

#ifdef USE_D3D12_FREQUENT_EXECUTION
	command_list_mgr->m_cpu_access_last_frame = command_list_mgr->m_cpu_access_this_frame;
	command_list_mgr->m_cpu_access_this_frame = false;
	command_list_mgr->m_draws_since_last_execution = 0;
#endif
}

HRESULT SetFullscreenState(bool enable_fullscreen)
{
	return S_OK;
}

HRESULT GetFullscreenState(bool* fullscreen_state)
{
	*fullscreen_state = false;
	return S_OK;
}

}  // namespace D3D

}  // namespace DX12
