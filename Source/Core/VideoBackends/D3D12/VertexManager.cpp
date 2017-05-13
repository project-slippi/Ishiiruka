// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#include "Common/CommonTypes.h"

#include "VideoBackends/D3D12/BoundingBox.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DStreamBuffer.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/PerfQuery.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/ShaderCache.h"
#include "VideoBackends/D3D12/VertexManager.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

// EXISTINGD3D11TODO: Find sensible values for these two
static constexpr unsigned int MAX_IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16) * 16;
static constexpr unsigned int MAX_VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE * 2;

void VertexManager::SetIndexBuffer()
{
	D3D12_INDEX_BUFFER_VIEW ibView = {
		m_stream_buffer->GetBaseGPUAddress(),          // D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
		static_cast<UINT>(m_stream_buffer->GetSize()), // UINT SizeInBytes;
		DXGI_FORMAT_R16_UINT                                 // DXGI_FORMAT Format;
	};

	D3D::current_command_list->IASetIndexBuffer(&ibView);
}

void VertexManager::CreateDeviceObjects()
{
	m_vertex_draw_offset = 0;
	m_index_draw_offset = 0;
	m_stream_buffer = std::make_unique<D3DStreamBuffer>((MAX_VBUFFER_SIZE + MAX_IBUFFER_SIZE) / 2, MAX_VBUFFER_SIZE + MAX_IBUFFER_SIZE, &m_stream_buffer_reallocated);
	m_stream_buffer_reallocated = true;
	m_vertex_cpu_buffer.resize(MAXVBUFFERSIZE);
	m_index_cpu_buffer.resize(MAXIBUFFERSIZE);
}

void VertexManager::DestroyDeviceObjects()
{
	m_stream_buffer.reset();
	m_vertex_cpu_buffer.clear();
	m_index_cpu_buffer.clear();
}

VertexManager::VertexManager()
{
	CreateDeviceObjects();
}

VertexManager::~VertexManager()
{
	DestroyDeviceObjects();
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
	u32 vertex_data_size = IndexGenerator::GetNumVerts() * stride;
	u32 index_data_size = IndexGenerator::GetIndexLen() * sizeof(u16);
	size_t total_size = vertex_data_size + index_data_size;
	bool current_command_list_executed = m_stream_buffer->AllocateSpaceInBuffer(total_size, stride);

	if (m_stream_buffer_reallocated)
	{
		D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_VERTEX_BUFFER, true);
		SetIndexBuffer();
		m_stream_buffer_reallocated = false;
	}

	m_vertex_draw_offset = static_cast<u32>(m_stream_buffer->GetOffsetOfCurrentAllocation());
	m_index_draw_offset = m_vertex_draw_offset + vertex_data_size;
	u8* dest = static_cast<u8*>(m_stream_buffer->GetCPUAddressOfCurrentAllocation());
	memcpy(dest, m_vertex_cpu_buffer.data(), vertex_data_size);
	memcpy(dest + vertex_data_size, m_index_cpu_buffer.data(), index_data_size);

	if (current_command_list_executed)
	{
		g_renderer->RestoreAPIState();
	}

	ADDSTAT(stats.thisFrame.bytesVertexStreamed, vertex_data_size);
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, index_data_size);
}

void VertexManager::Draw(u32 stride)
{
	static u32 s_previous_stride = UINT_MAX;

	u32 indices = IndexGenerator::GetIndexLen();

	if (D3D::command_list_mgr->GetCommandListDirtyState(COMMAND_LIST_STATE_VERTEX_BUFFER) || s_previous_stride != stride)
	{
		D3D12_VERTEX_BUFFER_VIEW vbView = {
			m_stream_buffer->GetBaseGPUAddress(),          // D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
			static_cast<UINT>(m_stream_buffer->GetSize()), // UINT SizeInBytes;
			stride                                                // UINT StrideInBytes;
		};

		D3D::current_command_list->IASetVertexBuffers(0, 1, &vbView);

		D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_VERTEX_BUFFER, false);
		s_previous_stride = stride;
	}

	D3D_PRIMITIVE_TOPOLOGY d3d_primitive_topology = ShaderCache::GetActiveDomainShaderBytecode().pShaderBytecode != nullptr ? D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	switch (m_current_primitive_type)
	{
	case PRIMITIVE_POINTS:
		d3d_primitive_topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		break;
	case PRIMITIVE_LINES:
		d3d_primitive_topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		break;
	}

	if (D3D::command_list_mgr->GetCommandListPrimitiveTopology() != d3d_primitive_topology)
	{
		D3D::current_command_list->IASetPrimitiveTopology(d3d_primitive_topology);
		D3D::command_list_mgr->SetCommandListPrimitiveTopology(d3d_primitive_topology);
	}

	u32 base_vertex = m_vertex_draw_offset / stride;
	u32 start_index = m_index_draw_offset / sizeof(u16);
	if (PerfQueryBase::ShouldEmulate())
		static_cast<PerfQuery*>(g_perf_query.get())->StartQuery();
	D3D::current_command_list->DrawIndexedInstanced(indices, 1, start_index, base_vertex, 0);
	if (PerfQueryBase::ShouldEmulate())
		static_cast<PerfQuery*>(g_perf_query.get())->EndQuery();
	INCSTAT(stats.thisFrame.numDrawCalls);
}

void VertexManager::PrepareShaders(PrimitiveType primitive, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread)
{
	bool use_dst_alpha = bpm.dstalpha.enable && bpm.blendmode.alphaupdate &&
		bpm.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	ShaderCache::PrepareShaders(use_dst_alpha ? PSRM_DUAL_SOURCE_BLEND : PSRM_DEFAULT, primitive, components, xfr, bpm, ongputhread);
}

void VertexManager::vFlush(bool use_dst_alpha)
{
	if (!ShaderCache::TestShaders())
	{
		return;
	}

	u32 stride = VertexLoaderManager::GetCurrentVertexFormat()->GetVertexStride();
	BBox::Bind();
	PrepareDrawBuffers(stride);

	g_renderer->ApplyState(use_dst_alpha);

	Draw(stride);

	// Many Gamecube/Wii titles read from the EFB each frame to determine what new rendering work to submit, e.g. where sun rays are
	// occluded and where they aren't. When the CPU wants to read this data (done in Renderer::AccessEFB), it requires that the GPU
	// finish all oustanding work. As an optimization, when we detect that the CPU is likely to read back data this frame, we break
	// up the rendering work and submit it more frequently to the GPU (via ExecuteCommandList). Thus, when the CPU finally needs the
	// the GPU to finish all of its work, there is (hopefully) less work outstanding to wait on at that moment.

	// D3D12TODO: Decide right threshold for drawCountSinceAsyncFlush at runtime depending on 
	// amount of stall measured in AccessEFB.
	D3D::command_list_mgr->EnsureDrawLimit();
}

u16* VertexManager::GetIndexBuffer()
{
	return IndexGenerator::GetBasePointer();
}

void VertexManager::ResetBuffer(u32 stride)
{
	m_pCurBufferPointer = m_vertex_cpu_buffer.data();
	m_pBaseBufferPointer = m_vertex_cpu_buffer.data();
	m_pEndBufferPointer = m_pCurBufferPointer + MAXVBUFFERSIZE;

	IndexGenerator::Start(reinterpret_cast<u16*>(m_index_cpu_buffer.data()));
}

}  // namespace
