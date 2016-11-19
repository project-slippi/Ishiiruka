// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/MathUtil.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DStreamBuffer.h"
#include "VideoCommon/RenderBase.h"
namespace DX12
{

extern StateCache gx_state_cache;

namespace D3D
{
void ResourceBarrier(ID3D12GraphicsCommandList* command_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after, UINT subresource);

// Font creation flags
static const unsigned int s_d3dfont_bold = 0x0001;
static const unsigned int s_d3dfont_italic = 0x0002;

// Font rendering flags
static const unsigned int s_d3dfont_centered = 0x0001;

class CD3DFont
{
public:
	CD3DFont();
	// 2D text drawing function
	// Initializing and destroying device-dependent objects
	int Init();
	int Shutdown();
	int DrawTextScaled(float x, float y,
		float size,
		float spacing, u32 dwColor,
		const std::string& text);

private:
	ID3D12Resource* m_texture = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE m_texture_cpu = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_texture_gpu = {};

	std::unique_ptr<D3DStreamBuffer> m_vertex_buffer;

	D3D12_INPUT_LAYOUT_DESC m_input_layout = {};
	D3D12_SHADER_BYTECODE m_pshader = {};
	D3D12_SHADER_BYTECODE m_vshader = {};
	D3D12_BLEND_DESC m_blendstate = {};
	D3D12_RASTERIZER_DESC m_raststate = {};
	ID3D12PipelineState* m_pso = nullptr;

	unsigned int m_line_height = 0;
	float m_tex_coords[128 - 32][4] = {};

	const int m_tex_width;
	const int m_tex_height;	
	void InitalizeSRV();
};


// Ring buffer class, shared between the draw* functions
class UtilVertexBuffer
{
public:
	UtilVertexBuffer(size_t size);
	~UtilVertexBuffer();

	// returns vertex offset to the new data
	size_t AppendData(void* data, size_t size, size_t vertex_size);
	size_t ReserveData(void** write_ptr, size_t size, size_t vertex_size);

	inline ID3D12Resource* GetBuffer()
	{
		return m_stream_buffer->GetBuffer();
	}
	inline size_t GetSize() const
	{
		return m_stream_buffer->GetSize();
	}
private:
	std::unique_ptr<D3DStreamBuffer> m_stream_buffer;
};

extern CD3DFont font;

void InitUtils();
void ShutdownUtils();

void SetPointCopySampler();
void SetLinearCopySampler();

inline void SetViewportAndScissor(int top_left_x, int top_left_y, int width, int height, float min_depth = D3D12_MIN_DEPTH, float max_depth = D3D12_MAX_DEPTH)
{
	D3D12_VIEWPORT viewport = {
		static_cast<float>(top_left_x),
		static_cast<float>(top_left_y),
		static_cast<float>(width),
		static_cast<float>(height),
		min_depth,
		max_depth
	};

	D3D12_RECT scissor = {
		static_cast<LONG>(top_left_x),
		static_cast<LONG>(top_left_y),
		static_cast<LONG>(top_left_x + width),
		static_cast<LONG>(top_left_y + height)
	};

	D3D::current_command_list->RSSetViewports(1, &viewport);
	D3D::current_command_list->RSSetScissorRects(1, &scissor);
}

void DrawShadedTexQuad(D3DTexture2D* texture,
	const D3D12_RECT* source,
	int source_width,
	int source_height,
	D3D12_SHADER_BYTECODE pshader12 = {},
	D3D12_SHADER_BYTECODE vshader12 = {},
	D3D12_INPUT_LAYOUT_DESC layout12 = {},
	D3D12_SHADER_BYTECODE gshader12 = {},
	u32 slice = 0,
	DXGI_FORMAT rt_format = DXGI_FORMAT_R8G8B8A8_UNORM,
	bool inherit_srv_binding = false,
	bool rt_multisampled = false,
	D3D12_DEPTH_STENCIL_DESC* depth_stencil_desc_override = nullptr
);

void DrawClearQuad(u32 Color, float z, D3D12_BLEND_DESC* blend_desc, D3D12_DEPTH_STENCIL_DESC* depth_stencil_desc, bool rt_multisampled);
void DrawEFBPokeQuads(EFBAccessType type,
	const EfbPokeData* points,
	size_t num_points,
	D3D12_BLEND_DESC* blend_desc,
	D3D12_DEPTH_STENCIL_DESC* depth_stencil_desc,
	D3D12_CPU_DESCRIPTOR_HANDLE* render_target,
	D3D12_CPU_DESCRIPTOR_HANDLE* depth_buffer,
	bool rt_multisampled);
}

}
