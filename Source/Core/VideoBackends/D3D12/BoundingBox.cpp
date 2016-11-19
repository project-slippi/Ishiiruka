// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/D3D12/BoundingBox.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DStreamBuffer.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

const size_t BBOX_BUFFER_SIZE = sizeof(int) * 4;
const size_t BBOX_STREAM_BUFFER_SIZE = BBOX_BUFFER_SIZE * 128;

static ComPtr<ID3D12Resource> s_bbox_buffer;
static ComPtr<ID3D12Resource> s_bbox_staging_buffer;
static std::unique_ptr<D3DStreamBuffer> s_bbox_stream_buffer;
static D3D12_GPU_DESCRIPTOR_HANDLE s_bbox_descriptor_handle{};
static size_t s_bbox_index;
static D3D12_RESOURCE_STATES s_current_bbox_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
static int s_bbox_shadow_copy[4];
static bool s_bbox_cpu_dirty = false;
static bool s_bbox_gpu_dirty = false;
void BBox::Init()
{
	memset(s_bbox_shadow_copy, 0, sizeof(s_bbox_shadow_copy));
	s_bbox_cpu_dirty = true;
	s_bbox_gpu_dirty = true;
	CD3DX12_RESOURCE_DESC buffer_desc(CD3DX12_RESOURCE_DESC::Buffer(BBOX_BUFFER_SIZE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0));
	CD3DX12_RESOURCE_DESC staging_buffer_desc(CD3DX12_RESOURCE_DESC::Buffer(BBOX_BUFFER_SIZE, D3D12_RESOURCE_FLAG_NONE, 0));

	CheckHR(D3D::device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&buffer_desc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(s_bbox_buffer.ReleaseAndGetAddressOf())));
	s_current_bbox_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	CheckHR(D3D::device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&staging_buffer_desc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(s_bbox_staging_buffer.ReleaseAndGetAddressOf())));

	s_bbox_stream_buffer = std::make_unique<D3DStreamBuffer>(BBOX_STREAM_BUFFER_SIZE, BBOX_STREAM_BUFFER_SIZE, nullptr);

	// buffer, we have to use a descriptor table. Luckily, we only have to allocate this once, and
	// it never changes.
	D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle;
	if (!D3D::gpu_descriptor_heap_mgr->Allocate(&s_bbox_index, &cpu_descriptor_handle, nullptr, &s_bbox_descriptor_handle))
		PanicAlert("Failed to create bounding box UAV descriptor");

	D3D12_UNORDERED_ACCESS_VIEW_DESC view_desc = { DXGI_FORMAT_R32_SINT, D3D12_UAV_DIMENSION_BUFFER };
	view_desc.Buffer.FirstElement = 0;
	view_desc.Buffer.NumElements = 4;
	view_desc.Buffer.StructureByteStride = 0;
	view_desc.Buffer.CounterOffsetInBytes = 0;
	view_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	D3D::device->CreateUnorderedAccessView(s_bbox_buffer.Get(), nullptr, &view_desc, cpu_descriptor_handle);

	Bind();
}

void BBox::Bind()
{
	if (!s_bbox_buffer)
	{
		return;
	}
	if (s_bbox_cpu_dirty)
	{
		s_bbox_stream_buffer->AllocateSpaceInBuffer(BBOX_BUFFER_SIZE, BBOX_BUFFER_SIZE);
		// Allocate temporary bytes in upload buffer, then copy to real buffer.
		memcpy(s_bbox_stream_buffer->GetCPUAddressOfCurrentAllocation(), s_bbox_shadow_copy, BBOX_BUFFER_SIZE);
		D3D::ResourceBarrier(D3D::current_command_list, s_bbox_buffer.Get(), s_current_bbox_state, D3D12_RESOURCE_STATE_COPY_DEST, 0);
		s_current_bbox_state = D3D12_RESOURCE_STATE_COPY_DEST;
		D3D::current_command_list->CopyBufferRegion(s_bbox_buffer.Get(), 0, s_bbox_stream_buffer->GetBuffer(), s_bbox_stream_buffer->GetOffsetOfCurrentAllocation(), BBOX_BUFFER_SIZE);
		s_bbox_cpu_dirty = false;
	}
	D3D::ResourceBarrier(D3D::current_command_list, s_bbox_buffer.Get(), s_current_bbox_state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0);
	s_current_bbox_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_UAV, s_bbox_descriptor_handle);
	s_bbox_gpu_dirty = true;
}

void BBox::Shutdown()
{
	if (s_bbox_buffer)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_bbox_buffer.Detach());
	}

	if (s_bbox_staging_buffer)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_bbox_staging_buffer.Detach());
	}
	s_bbox_stream_buffer.reset();
}

void BBox::Set(int index, int value)
{
	if (s_bbox_buffer && s_bbox_shadow_copy[index] != value)
	{
		s_bbox_shadow_copy[index] = value;
		s_bbox_cpu_dirty = true;
	}
}

int BBox::Get(int index)
{
	if (!s_bbox_buffer)
		return 0;

	if (s_bbox_gpu_dirty)
	{
		D3D::command_list_mgr->CPUAccessNotify();

		// Copy from real buffer to staging buffer, then block until we have the results.
		D3D::ResourceBarrier(D3D::current_command_list, s_bbox_buffer.Get(), s_current_bbox_state, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
		s_current_bbox_state = D3D12_RESOURCE_STATE_COPY_SOURCE;
		D3D::current_command_list->CopyBufferRegion(s_bbox_staging_buffer.Get(), 0, s_bbox_buffer.Get(), 0, BBOX_BUFFER_SIZE);

		D3D::command_list_mgr->ExecuteQueuedWork(true);
		D3D12_RANGE read_range = { 0, BBOX_BUFFER_SIZE };
		void* bbox_staging_buffer_map = nullptr;
		CheckHR(s_bbox_staging_buffer->Map(0, &read_range, &bbox_staging_buffer_map));
		memcpy(s_bbox_shadow_copy, bbox_staging_buffer_map, BBOX_BUFFER_SIZE);
		D3D12_RANGE write_range = {};
		s_bbox_staging_buffer->Unmap(0, &write_range);
		s_bbox_gpu_dirty = false;
	}
	return s_bbox_shadow_copy[index];
}

};
