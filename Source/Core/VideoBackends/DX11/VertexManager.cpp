// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

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
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

// TODO: Find sensible values for these two
const u32 MAX_BUFFER_SIZE = 20*1024*1024;

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

	m_pCurBufferPointer = m_pBaseBufferPointer = &LocalVBuffer[0];
	m_pEndBufferPointer = m_pBaseBufferPointer + LocalVBuffer.size();

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
	u32 vertexBufferSize = u32(m_pCurBufferPointer - m_pBaseBufferPointer);
	u32 indexBufferSize = IndexGenerator::GetIndexLen() * sizeof(u16);
	u32 totalBufferSize = vertexBufferSize + indexBufferSize;

	u32 cursor = m_bufferCursor;
	u32 padding = cursor % stride;
	if (padding)
	{
		cursor += stride - padding;
	}
	D3D11_MAP MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if (cursor + totalBufferSize >= MAX_BUFFER_SIZE)
	{
		// Wrap around
		m_currentBuffer = (m_currentBuffer + 1) % MAX_BUFFER_COUNT;
		cursor = 0;
		MapType = D3D11_MAP_WRITE_DISCARD;
	}
	m_vertexDrawOffset = cursor;
	m_indexDrawOffset = cursor + vertexBufferSize;

	D3D::context->Map(m_buffers[m_currentBuffer].get(), 0, MapType, 0, &map);
	u8* mappedData = reinterpret_cast<u8*>(map.pData);
	memcpy(mappedData + m_vertexDrawOffset, m_pBaseBufferPointer, vertexBufferSize);
	memcpy(mappedData + m_indexDrawOffset, m_index_buffer_start, indexBufferSize);
	D3D::context->Unmap(m_buffers[m_currentBuffer].get(), 0);

	m_bufferCursor = cursor + totalBufferSize;

	ADDSTAT(stats.thisFrame.bytesVertexStreamed, vertexBufferSize);
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, indexBufferSize);
}

void VertexManager::Draw(UINT stride)
{
	u32 indices = IndexGenerator::GetIndexLen();

	D3D::stateman->SetVertexBuffer(m_buffers[m_currentBuffer].get(), stride, 0);
	D3D::stateman->SetIndexBuffer(m_buffers[m_currentBuffer].get());

	u32 baseVertex = m_vertexDrawOffset / stride;
	u32 startIndex = m_indexDrawOffset / sizeof(u16);

	if (m_current_primitive_type == PRIMITIVE_TRIANGLES)
	{
		auto pt = HullDomainShaderCache::GetActiveHullShader() != nullptr ?
			D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST :
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		D3D::stateman->SetPrimitiveTopology(pt);
	}
	else
	{
		D3D::stateman->SetPrimitiveTopology(m_current_primitive_type == PRIMITIVE_LINES ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST : D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		static_cast<Renderer*>(g_renderer.get())->ApplyCullDisable();
	}

	D3D::stateman->Apply();
	D3D::context->DrawIndexed(indices, startIndex, baseVertex);
	INCSTAT(stats.thisFrame.numDrawCalls);

	if (m_current_primitive_type != PRIMITIVE_TRIANGLES)
	{
		static_cast<Renderer*>(g_renderer.get())->RestoreCull();
	}
}

void VertexManager::PrepareShaders(PrimitiveType primitive, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread)
{
	bool useDstAlpha = bpm.dstalpha.enable && bpm.blendmode.alphaupdate &&
		bpm.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	VertexShaderCache::PrepareShader(components, xfr, bpm, ongputhread);
	GeometryShaderCache::PrepareShader(primitive, xfr, components, ongputhread);
	PixelShaderCache::PrepareShader(useDstAlpha ? PSRM_DUAL_SOURCE_BLEND : PSRM_DEFAULT, components, xfr, bpm, ongputhread);
	if (g_ActiveConfig.backend_info.bSupportsTessellation)
	{
		HullDomainShaderCache::PrepareShader(xfr, bpm, primitive, components, ongputhread);
	}
}

void VertexManager::vFlush(bool useDstAlpha)
{
	if (!VertexShaderCache::TestShader())
	{
		return;
	}
	if (g_ActiveConfig.iStereoMode > 0 || m_current_primitive_type != PrimitiveType::PRIMITIVE_TRIANGLES)
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
	if (g_ActiveConfig.TessellationEnabled())
	{
		if (!HullDomainShaderCache::TestShader())
		{
			return;
		}
	}
	BBox::Update();
	NativeVertexFormat* current_vertex_format = VertexLoaderManager::GetCurrentVertexFormat();
	u32 stride = current_vertex_format->GetVertexStride();
	PrepareDrawBuffers(stride);
	current_vertex_format->SetupVertexPointers();
	g_renderer->ApplyState(useDstAlpha);

	Draw(stride);

	g_renderer->RestoreState();
}

void VertexManager::ResetBuffer(u32 stride)
{
	m_pCurBufferPointer = m_pBaseBufferPointer;
	IndexGenerator::Start(m_index_buffer_start);
}

}  // namespace
