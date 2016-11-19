// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d12.h>
#include "VideoBackends/D3D12/D3DUtil.h"

namespace DX12
{

enum TEXTURE_BIND_FLAG : u32
{
	TEXTURE_BIND_FLAG_SHADER_RESOURCE = (1 << 0),
	TEXTURE_BIND_FLAG_RENDER_TARGET = (1 << 1),
	TEXTURE_BIND_FLAG_DEPTH_STENCIL = (1 << 2)
};

namespace D3D
{
void ReplaceTexture2D(ID3D12Resource* pTexture, const u8* buffer, DXGI_FORMAT fmt, unsigned int width, unsigned int height, unsigned int src_pitch, unsigned int level, D3D12_RESOURCE_STATES current_resource_state = D3D12_RESOURCE_STATE_COMMON);
void CleanupPersistentD3DTextureResources();
}

class D3DTexture2D
{

public:
	// there are two ways to create a D3DTexture2D object:
	//     either create an ID3D12Resource object, pass it to the constructor and specify what views to create
	//     or let the texture automatically be created by D3DTexture2D::Create

	D3DTexture2D(ID3D12Resource* texptr, u32 bind, DXGI_FORMAT fmt, DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN, DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN, DXGI_FORMAT rtv_format = DXGI_FORMAT_UNKNOWN, bool multisampled = false, D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_COMMON);
	static D3DTexture2D* Create(unsigned int width, unsigned int height, u32 bind, DXGI_FORMAT fmt, unsigned int levels = 1, unsigned int slices = 1, D3D12_SUBRESOURCE_DATA* data = nullptr);
	inline void TransitionToResourceState(ID3D12GraphicsCommandList* command_list, D3D12_RESOURCE_STATES state_after)
	{
		if (m_resource_state != state_after)
		{
			DX12::D3D::ResourceBarrier(command_list, m_tex.Get(), m_resource_state, state_after, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			m_resource_state = state_after;
		}
	}

	// reference counting, use AddRef() when creating a new reference and Release() it when you don't need it anymore
	void AddRef();
	UINT Release();

	inline D3D12_RESOURCE_STATES D3DTexture2D::GetResourceUsageState() const
	{
		return m_resource_state;
	}

	inline bool D3DTexture2D::GetMultisampled() const
	{
		return m_multisampled;
	}

	inline ID3D12Resource* D3DTexture2D::GetTex() const
	{
		return m_tex.Get();
	}

	inline D3D12_CPU_DESCRIPTOR_HANDLE D3DTexture2D::GetSRVCPU() const
	{
		return m_srv_cpu;
	}

	inline D3D12_CPU_DESCRIPTOR_HANDLE D3DTexture2D::GetSRVCPUShadow() const
	{
		return m_srv_cpu_shadow;
	}

	inline D3D12_GPU_DESCRIPTOR_HANDLE D3DTexture2D::GetSRVGPU() const
	{
		return m_srv_gpu;
	}

	inline D3D12_CPU_DESCRIPTOR_HANDLE D3DTexture2D::GetDSV() const
	{
		return m_dsv;
	}

	inline D3D12_CPU_DESCRIPTOR_HANDLE D3DTexture2D::GetRTV() const
	{
		return m_rtv;
	}

	inline DXGI_FORMAT D3DTexture2D::GetFormat() const
	{
		return m_format;
	}

private:
	~D3DTexture2D();
	DXGI_FORMAT m_format = {};
	ComPtr<ID3D12Resource> m_tex;
	DXGI_FORMAT m_srv_format = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_srv_cpu = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_srv_cpu_shadow = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_srv_gpu = {};
	size_t m_srv_index = 0;

	DXGI_FORMAT m_dsv_format = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_dsv = {};
	size_t m_dsv_index = 0;

	DXGI_FORMAT m_rtv_format = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_rtv = {};
	size_t m_rtv_index = 0;

	D3D12_RESOURCE_STATES m_resource_state = D3D12_RESOURCE_STATE_COMMON;

	bool m_multisampled{};

	std::atomic<unsigned long> m_ref = 1;
	u32 m_bind_falgs = {};
	void InitalizeSRV();
	void InitalizeRTV();
	void InitalizeDSV();
};

}  // namespace DX12
