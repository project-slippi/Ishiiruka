// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <memory>
#include <vector>
#include <wrl/client.h>

#include <d3dx12.h>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"


namespace DX12
{
using Microsoft::WRL::ComPtr;
#define SAFE_RELEASE(x) { if (x) (x)->Release(); (x) = nullptr; }
#define CHECK(cond, Message, ...) if (!(cond)) { PanicAlert(__FUNCTION__ " failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); __debugbreak();}

// DEBUGCHECK is for high-frequency functions that we only want to check on debug builds.
#if defined(_DEBUG) || defined(DEBUGFAST)
#define DEBUGCHECK(cond, Message, ...) if (!(cond)) { PanicAlert(__FUNCTION__ " failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }
#else
#define DEBUGCHECK(cond, Message, ...)
#endif

#define CheckHR(hr) CHECK(SUCCEEDED(hr), "Failed HRESULT : %X", hr)

class D3DCommandListManager;
class D3DDescriptorHeapManager;
class D3DSamplerHeapManager;
class D3DTexture2D;

enum GRAPHICS_ROOT_PARAMETER : u32
{
	DESCRIPTOR_TABLE_PS_SRV,
	DESCRIPTOR_TABLE_PS_SAMPLER,
	DESCRIPTOR_TABLE_VS_CBV,
	DESCRIPTOR_TABLE_PS_CBVONE,
	DESCRIPTOR_TABLE_PS_CBVTWO,
	DESCRIPTOR_TABLE_PS_UAV,
	DESCRIPTOR_TABLE_GS_CBV,
	DESCRIPTOR_TABLE_DS_SRV,
	DESCRIPTOR_TABLE_DS_SAMPLER,
	DESCRIPTOR_TABLE_HS_CBV0,
	DESCRIPTOR_TABLE_HS_CBV1,
	DESCRIPTOR_TABLE_HS_CBV2,
	DESCRIPTOR_TABLE_DS_CBV0,
	DESCRIPTOR_TABLE_DS_CBV1,
	DESCRIPTOR_TABLE_DS_CBV2,
	NUM_GRAPHICS_ROOT_PARAMETERS
};

namespace D3D
{
HRESULT LoadDXGI();
HRESULT LoadD3D();
void UnloadDXGI();
void UnloadD3D();

std::vector<DXGI_SAMPLE_DESC> EnumAAModes(ID3D12Device* device);

HRESULT Create(HWND wnd);

void CreateDescriptorHeaps();
void CreateRootSignatures();

void WaitForOutstandingRenderingToComplete(bool terminate = false);
void Close();

extern ID3D12Device* device;

extern unsigned int resource_descriptor_size;
extern unsigned int sampler_descriptor_size;
extern std::unique_ptr<D3DDescriptorHeapManager> gpu_descriptor_heap_mgr;
extern std::unique_ptr<D3DDescriptorHeapManager> dsv_descriptor_heap_mgr;
extern std::unique_ptr<D3DDescriptorHeapManager> rtv_descriptor_heap_mgr;
extern std::unique_ptr<D3DSamplerHeapManager> sampler_descriptor_heap_mgr;
extern std::array<ID3D12DescriptorHeap*, 2> gpu_descriptor_heaps;

extern D3D12_CPU_DESCRIPTOR_HANDLE null_srv_cpu;
extern D3D12_CPU_DESCRIPTOR_HANDLE null_srv_cpu_shadow;

extern std::unique_ptr<D3DCommandListManager> command_list_mgr;
extern ID3D12GraphicsCommandList* current_command_list;
bool TessellationEnabled();
D3D_FEATURE_LEVEL GetFeatureLevel();
bool GetLogicOpSupported();
ID3D12RootSignature* GetRootSignature();
ID3D12RootSignature* GetBasicRootSignature();
void SetRootSignature(bool geometryenabled, bool tesselationenabled, bool applychange = true);


extern HWND hWnd;
extern bool frame_in_progress;

void Reset();
bool BeginFrame();
void EndFrame();
void Present();

unsigned int GetBackBufferWidth();
unsigned int GetBackBufferHeight();
D3DTexture2D* &GetBackBuffer();
const char* PixelShaderVersionString();
const char* GeometryShaderVersionString();
const char* VertexShaderVersionString();
const char* HullShaderVersionString();
const char* DomainShaderVersionString();
const char* ComputeShaderVersionString();

HRESULT SetFullscreenState(bool enable_fullscreen);
bool GetFullscreenState();

// This function will assign a name to the given resource.
// The DirectX debug layer will make it easier to identify resources that way,
// e.g. when listing up all resources who have unreleased references.
template<typename T>
static void SetDebugObjectName12(T* resource, LPCSTR name)
{
	HRESULT hr = resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)(name ? strlen(name) : 0), name);
	if (FAILED(hr))
	{
		throw std::exception("Failure setting name for D3D12 object");
	}
}

static std::string GetDebugObjectName12(ID3D12Resource* resource)
{
	std::string name;
	if (resource)
	{
		UINT size = 0;
		resource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, nullptr); //get required size
		name.resize(size);
		resource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, const_cast<char*>(name.data()));
	}
	return name;
}

}  // namespace D3D

typedef HRESULT(WINAPI* CREATEDXGIFACTORY)(REFIID, void**);
extern CREATEDXGIFACTORY create_dxgi_factory;

typedef HRESULT(WINAPI* D3D12CREATEDEVICE)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
extern D3D12CREATEDEVICE d3d12_create_device;
typedef HRESULT(WINAPI* D3D12SERIALIZEROOTSIGNATURE)(const D3D12_ROOT_SIGNATURE_DESC* pRootSignature, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob** ppBlob, ID3DBlob** ppErrorBlob);
typedef HRESULT(WINAPI* D3D12GETDEBUGINTERFACE)(REFIID riid, void** ppvDebug);

}  // namespace DX12
