// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2++
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/DX11/BoundingBox.h"
#include "VideoBackends/DX11/D3DPtr.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

static D3D::BufferPtr s_bbox_buffer;
static D3D::BufferPtr s_bbox_staging_buffer;
static D3D::UavPtr  s_bbox_uav;
alignas(128) static s32 s_values[4];
static bool s_cpu_dirty;
static bool s_gpu_dirty;

void BBox::Init()
{
	if (g_ActiveConfig.backend_info.bSupportsBBox)
	{
		// Create 2 buffers here.
		// First on Default pool.
		auto desc = CD3D11_BUFFER_DESC(4 * sizeof(s32), D3D11_BIND_UNORDERED_ACCESS, D3D11_USAGE_DEFAULT, 0, 0, 4);
		s_values[0] = s_values[1] = s_values[2] = s_values[3] = 0;
		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = s_values;
		data.SysMemPitch = 4 * sizeof(s32);
		data.SysMemSlicePitch = 0;
		HRESULT hr;
		hr = D3D::device->CreateBuffer(&desc, &data, ToAddr(s_bbox_buffer));
		CHECK(SUCCEEDED(hr), "Create BoundingBox buffer.");
		D3D::SetDebugObjectName(s_bbox_buffer.get(), "BoundingBox buffer");
		// Second to use as a staging buffer.
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.BindFlags = 0;
		hr = D3D::device->CreateBuffer(&desc, nullptr, ToAddr(s_bbox_staging_buffer));
		CHECK(SUCCEEDED(hr), "Create BoundingBox staging buffer.");
		D3D::SetDebugObjectName(s_bbox_staging_buffer.get(), "BoundingBox staging buffer");
		// UAV is required to allow concurrent access.
		D3D11_UNORDERED_ACCESS_VIEW_DESC UAVdesc = {};
		UAVdesc.Format = DXGI_FORMAT_R32_SINT;
		UAVdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		UAVdesc.Buffer.FirstElement = 0;
		UAVdesc.Buffer.Flags = 0;
		UAVdesc.Buffer.NumElements = 4;
		hr = D3D::device->CreateUnorderedAccessView(s_bbox_buffer.get(), &UAVdesc, ToAddr(s_bbox_uav));
		CHECK(SUCCEEDED(hr), "Create BoundingBox UAV.");
		s_cpu_dirty = true;
		s_gpu_dirty = true;
	}
}

void BBox::Shutdown()
{
	s_bbox_buffer.reset();
	s_bbox_staging_buffer.reset();
	s_bbox_uav.reset();
}

void BBox::Update()
{
	if (g_ActiveConfig.backend_info.bSupportsBBox
		&& BoundingBox::active
		&& g_ActiveConfig.iBBoxMode == BBoxGPU)
	{
		if (s_cpu_dirty)
		{
			D3D11_BOX box{ 0, 0, 0, 4 * sizeof(s32), 1, 1 };
			D3D::context->UpdateSubresource(s_bbox_buffer.get(), 0, &box, &s_values, 0, 0);
			s_cpu_dirty = false;
		}
		D3D::context->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr, 2, 1, D3D::ToAddr(s_bbox_uav), nullptr);
		s_gpu_dirty = true;
	}
}

void BBox::Set(s32 index, s32 value)
{
	if (s_values[index] != value)
	{
		s_values[index] = value;
		s_cpu_dirty = true;
	}
}

s32 BBox::Get(s32 index)
{
	if (s_gpu_dirty && g_ActiveConfig.iBBoxMode == BBoxGPU)
	{
		D3D::context->CopyResource(s_bbox_staging_buffer.get(), s_bbox_buffer.get());
		D3D11_MAPPED_SUBRESOURCE map;
		HRESULT hr = D3D::context->Map(s_bbox_staging_buffer.get(), 0, D3D11_MAP_READ, 0, &map);
		if (SUCCEEDED(hr))
		{
			memcpy(s_values, map.pData, 4 * sizeof(s32));
		}
		D3D::context->Unmap(s_bbox_staging_buffer.get(), 0);
		s_gpu_dirty = false;
	}
	return s_values[index];
}

};
