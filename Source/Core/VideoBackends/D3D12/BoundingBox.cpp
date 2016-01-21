// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DQueuedCommandList.h"
#include "VideoBackends/D3D12/D3DUtil.h"

#include "VideoBackends/D3D12/BoundingBox.h"


#include "VideoCommon/VideoConfig.h"

// D3D12TODO: Support bounding box behavior.
namespace DX12
{

static ID3D12Resource* s_bbox_buffer = nullptr;
static ID3D12Resource* s_bbox_readback_buffer = nullptr;
static void*           s_bbox_readback_buffer_data = nullptr;
static D3D12_CPU_DESCRIPTOR_HANDLE s_bbox_uav_cpu = {};
static D3D12_GPU_DESCRIPTOR_HANDLE s_bbox_uav_gpu = {};

static ID3D12Resource* s_bbox_upload_heap = nullptr;
static void*           s_bbox_upload_heap_data_pointer = nullptr;

static UINT            s_bbox_upload_heap_current_offset = 0;
static const UINT      s_bbox_upload_heap_size = 2 * 1024 * 1024;

// Guaranteed to not run out of room. Updates to bounding box are 16 bytes each.
// Unusual to have more than 25,000 draws a frame, even if bounding box updated
// every single draw, 2MB is plenty for two frames.

D3D12_GPU_DESCRIPTOR_HANDLE BBox::GetUAV()
{
	return s_bbox_uav_gpu;
}

void BBox::Init()
{
	if (g_ActiveConfig.backend_info.bSupportsBBox)
	{
		CheckHR(
			D3D::device12->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(4 * sizeof(s32), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&s_bbox_buffer)
				)
			);

		D3D::SetDebugObjectName12(s_bbox_buffer, "BoundingBox Buffer");

		CheckHR(
			D3D::device12->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(4 * sizeof(s32)),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&s_bbox_readback_buffer)
				)
			);

		D3D::SetDebugObjectName12(s_bbox_readback_buffer, "BoundingBox Readback Buffer");

		D3D::gpu_descriptor_heap_mgr->Allocate(&s_bbox_uav_cpu, &s_bbox_uav_gpu, nullptr, false);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.Format = DXGI_FORMAT_R32_SINT;
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		uav_desc.Buffer.NumElements = 4;

		D3D::device12->CreateUnorderedAccessView(s_bbox_buffer, nullptr, &uav_desc, s_bbox_uav_cpu);

		CheckHR(
			D3D::device12->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(s_bbox_upload_heap_size),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&s_bbox_upload_heap)
				)
			);
		CheckHR(s_bbox_upload_heap->Map(0, nullptr, &s_bbox_upload_heap_data_pointer));
	}
}

void BBox::Shutdown()
{
	if (s_bbox_upload_heap != nullptr)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_bbox_upload_heap);
		s_bbox_upload_heap = nullptr;
	}

	if (s_bbox_buffer != nullptr)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_bbox_buffer);
		s_bbox_buffer = nullptr;
	}

	if (s_bbox_readback_buffer != nullptr)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(s_bbox_readback_buffer);
		s_bbox_readback_buffer = nullptr;
	}
}

void BBox::Set(int index, int value)
{
	unsigned int upload_size = sizeof(value);

	memcpy(static_cast<u8*>(s_bbox_upload_heap_data_pointer) + s_bbox_upload_heap_current_offset, &value, upload_size);

	D3D::ResourceBarrier(D3D::current_command_list, s_bbox_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST, 0);
	D3D::current_command_list->CopyBufferRegion(s_bbox_buffer, index * sizeof(s32), s_bbox_upload_heap, s_bbox_upload_heap_current_offset, upload_size);
	D3D::ResourceBarrier(D3D::current_command_list, s_bbox_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0);

	s_bbox_upload_heap_current_offset = (s_bbox_upload_heap_current_offset + upload_size) % s_bbox_upload_heap_size;
}

int BBox::Get(int index)
{
	int data = 0;
	D3D::ResourceBarrier(D3D::current_command_list, s_bbox_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
	D3D::current_command_list->CopyBufferRegion(s_bbox_readback_buffer, index * sizeof(s32), s_bbox_buffer, index * sizeof(s32), sizeof(32));
	D3D::ResourceBarrier(D3D::current_command_list, s_bbox_buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0);

	D3D::command_list_mgr->ExecuteQueuedWork(true);

	data = static_cast<s32*>(s_bbox_readback_buffer_data)[index];

	return data;
}

}
