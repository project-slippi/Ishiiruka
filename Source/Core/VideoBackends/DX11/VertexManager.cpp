// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/DX11/BoundingBox.h"
#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DState.h"
#include "VideoBackends/DX11/GeometryShaderCache.h"
#include "VideoBackends/DX11/HullDomainShaderCache.h"
#include "VideoBackends/DX11/PixelShaderCache.h"
#include "VideoBackends/DX11/Render.h"
#include "VideoBackends/DX11/VertexManager.h"
#include "VideoBackends/DX11/VertexShaderCache.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/MainBase.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace DX11
{

void VertexManager::CreateDeviceObjects()
{
	D3D11_BUFFER_DESC bufdesc = CD3D11_BUFFER_DESC(MAX_BUFFER_SIZE,
		D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

	m_vertexDrawOffset = 0;
	m_indexDrawOffset = 0;

	for (int i = 0; i < MAX_BUFFER_COUNT; i++)
	{
		m_buffers[i] = nullptr;
		CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, nullptr, D3D::ToAddr(m_buffers[i]))), "Failed to create buffer.");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_buffers[i].get(), "Buffer of VertexManager");
	}

	m_currentBuffer = 0;
	m_bufferCursor = MAX_BUFFER_SIZE;
}

void VertexManager::DestroyDeviceObjects()
{
	for (int i = 0; i < MAX_BUFFER_COUNT; i++)
	{
		m_buffers[i].reset();
	}
}

VertexManager::VertexManager()
{
	LocalVBuffer.resize(MAXVBUFFERSIZE);

	s_pCurBufferPointer = s_pBaseBufferPointer = &LocalVBuffer[0];
	s_pEndBufferPointer = s_pBaseBufferPointer + LocalVBuffer.size();

	LocalIBuffer.resize(MAXIBUFFERSIZE);
	m_index_buffer_start = &LocalIBuffer[0];
	CreateDeviceObjects();
}

VertexManager::~VertexManager()
{
	DestroyDeviceObjects();
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
	D3D11_MAPPED_SUBRESOURCE map;
	u32 vertexBufferSize = u32(s_pCurBufferPointer - s_pBaseBufferPointer);
	u32 indexBufferSize = IndexGenerator::GetIndexLen() * sizeof(u16);
	u32 totalBufferSize = vertexBufferSize + indexBufferSize;
	u32 cursor = m_bufferCursor + indexBufferSize;
	u32 padding = cursor % stride;
	if (padding)
	{
		cursor += stride - padding;
	}
	D3D11_MAP MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if (cursor + vertexBufferSize >= MAX_BUFFER_SIZE)
	{
		// Wrap around
		m_currentBuffer = (m_currentBuffer + 1) % MAX_BUFFER_COUNT;
		m_bufferCursor = 0;
		cursor = indexBufferSize;
		padding = cursor % stride;
		if (padding)
		{
			cursor += stride - padding;
		}
		MapType = D3D11_MAP_WRITE_DISCARD;
	}
	m_vertexDrawOffset = cursor;
	m_indexDrawOffset = m_bufferCursor;

	D3D::context->Map(m_buffers[m_currentBuffer].get(), 0, MapType, 0, &map);
	u8* mappedData = reinterpret_cast<u8*>(map.pData);
	memcpy(mappedData + m_indexDrawOffset, m_index_buffer_start, indexBufferSize);
	memcpy(mappedData + m_vertexDrawOffset, s_pBaseBufferPointer, vertexBufferSize);
	D3D::context->Unmap(m_buffers[m_currentBuffer].get(), 0);

	m_bufferCursor = cursor + vertexBufferSize;

	ADDSTAT(stats.thisFrame.bytesVertexStreamed, vertexBufferSize);
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, indexBufferSize);
}

void VertexManager::Draw(UINT stride)
{
	u32 components = g_nativeVertexFmt->m_components;
	u32 indices = IndexGenerator::GetIndexLen();
	D3D::stateman->SetIndexBuffer(m_buffers[m_currentBuffer].get());
	D3D::stateman->SetVertexBuffer(m_buffers[m_currentBuffer].get(), stride, 0);

	u32 baseVertex = m_vertexDrawOffset / stride;
	u32 startIndex = m_indexDrawOffset / sizeof(u16);

	if (current_primitive_type == PRIMITIVE_TRIANGLES)
	{
		auto pt = HullDomainShaderCache::GetActiveHullShader() != nullptr ?
		D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST :
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		D3D::stateman->SetPrimitiveTopology(pt);		
	}
	else if (current_primitive_type == PRIMITIVE_LINES)
	{
		D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		((DX11::Renderer*)g_renderer)->ApplyCullDisable();
	}
	else
	{
		D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		((DX11::Renderer*)g_renderer)->ApplyCullDisable();
	}

	D3D::stateman->Apply();
	D3D::context->DrawIndexed(indices, startIndex, baseVertex);
	INCSTAT(stats.thisFrame.numDrawCalls);

	if (current_primitive_type != PRIMITIVE_TRIANGLES)
	{
		((DX11::Renderer*)g_renderer)->RestoreCull();
	}
}

void VertexManager::PrepareShaders(u32 primitive, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread)
{
	if (ongputhread)
	{
		if (!s_Shader_Refresh_Required)
		{
			return;
		}
		s_Shader_Refresh_Required = false;
	}
	bool useDstAlpha = bpm.dstalpha.enable && bpm.blendmode.alphaupdate &&
		bpm.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	VertexShaderCache::PrepareShader(components, xfr, bpm, ongputhread);
	GeometryShaderCache::PrepareShader(primitive, xfr, components, ongputhread);
	PixelShaderCache::PrepareShader(useDstAlpha ? DSTALPHA_DUAL_SOURCE_BLEND : DSTALPHA_NONE, components, xfr, bpm, ongputhread);
}

void VertexManager::vFlush(bool useDstAlpha)
{
	if (!VertexShaderCache::TestShader())
	{
		return;
	}
	if (g_ActiveConfig.iStereoMode > 0 || current_primitive_type != PrimitiveType::PRIMITIVE_TRIANGLES)
	{
		if (!GeometryShaderCache::TestShader())
		{
			return;
		}
	}
	if (!PixelShaderCache::TestShader())
	{
		return;
	}
	if (g_ActiveConfig.backend_info.bSupportsTessellation)
	{
		HullDomainShaderCache::SetShader(xfmem, current_primitive_type, g_nativeVertexFmt->m_components);
	}
	BBox::Update();
	u32 stride = g_nativeVertexFmt->GetVertexStride();
	PrepareDrawBuffers(stride);
	g_nativeVertexFmt->SetupVertexPointers();
	g_renderer->ApplyState(useDstAlpha);

	Draw(stride);

	g_renderer->RestoreState();
}

void VertexManager::ResetBuffer(u32 stride)
{
	s_pCurBufferPointer = s_pBaseBufferPointer;
	IndexGenerator::Start(m_index_buffer_start);
}

}  // namespace
