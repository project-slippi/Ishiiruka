// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11_2.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <vector>

#include "Common/Common.h"
#include "Common/MsgHandler.h"

namespace DX11
{

#define SAFE_RELEASE(x) { if (x) (x)->Release(); (x) = nullptr; }
#define SAFE_DELETE(x) { delete (x); (x) = nullptr; }
#define SAFE_DELETE_ARRAY(x) { delete[] (x); (x) = nullptr; }
#define CHECK(cond, Message, ...) if (!(cond)) { PanicAlert(__FUNCTION__ " failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }
#define CHECKANDEXIT(cond, Message, ...) if (!(cond)) { PanicAlert(__FUNCTION__ " failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); return;}

class D3DTexture2D;

namespace D3D
{

HRESULT LoadDXGI();
HRESULT LoadD3D();
void UnloadDXGI();
void UnloadD3D();

D3D_FEATURE_LEVEL GetFeatureLevel(IDXGIAdapter* adapter);
std::vector<DXGI_SAMPLE_DESC> EnumAAModes(IDXGIAdapter* adapter);

HRESULT Create(HWND wnd);
void Close();

extern ID3D11Device* device;
extern ID3D11Device1* device1;
extern ID3D11DeviceContext* context;
extern ID3D11DeviceContext1* context1;
extern HWND hWnd;
extern bool bFrameInProgress;

void Reset();
bool BeginFrame();
void EndFrame();
void Present();

D3D_FEATURE_LEVEL GetFeatureLevel();
bool GetLogicOpSupported();
unsigned int GetBackBufferWidth();
unsigned int GetBackBufferHeight();
D3DTexture2D* &GetBackBuffer();
const char* PixelShaderVersionString();
const char* GeometryShaderVersionString();
const char* VertexShaderVersionString();
const char* HullShaderVersionString();
const char* DomainShaderVersionString();
const char* ComputeShaderVersionString();
DXGI_FORMAT GetBaseBufferFormat();
bool BGRATexturesSupported();
bool BGRA565TexturesSupported();

u32 GetMaxTextureSize(D3D_FEATURE_LEVEL feature_level);
bool SupportPartialContantBufferUpdate();

HRESULT SetFullscreenState(bool enable_fullscreen);
bool GetFullscreenState();

// Ihis function will assign a name to the given resource.
// The DirectX debug layer will make it easier to identify resources that way,
// e.g. when listing up all resources who have unreleased references.
template <typename T>
void SetDebugObjectName(T resource, const char* name)
{
	static_assert(std::is_convertible<T, ID3D11DeviceChild*>::value,
		"resource must be convertible to ID3D11DeviceChild*");
#if defined(_DEBUG) || defined(DEBUGFAST)
	if (name && resource) resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
#endif
}

}  // namespace D3D

typedef HRESULT(WINAPI *CREATEDXGIFACTORY)(REFIID, void**);
extern CREATEDXGIFACTORY PCreateDXGIFactory;
typedef HRESULT(WINAPI *D3D11CREATEDEVICE)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, CONST D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

}  // namespace DX11
