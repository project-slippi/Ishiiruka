// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DBlob.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/NativeVertexFormat.h"
#include "VideoBackends/D3D12/VertexManager.h"

namespace DX12
{

std::unique_ptr<NativeVertexFormat> VertexManager::CreateNativeVertexFormat(const PortableVertexDeclaration &vtx_decl)
{
	return std::make_unique<D3DVertexFormat>(vtx_decl);
}

DXGI_FORMAT VarToD3D(EVTXComponentFormat t, int size)
{
	DXGI_FORMAT retval = DXGI_FORMAT_UNKNOWN;
	static const constexpr DXGI_FORMAT lookup[4][5] =
	{
		{
			DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_SNORM, DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_SNORM, DXGI_FORMAT_R32_FLOAT
		},
		{
			DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_SNORM, DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16_SNORM, DXGI_FORMAT_R32G32_FLOAT
		},
		{
			DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32G32B32_FLOAT
		},
		{
			DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_SNORM, DXGI_FORMAT_R32G32B32A32_FLOAT
		}
	};
	if (size < 5)
	{
		retval = lookup[size - 1][t];
	}
	if (retval == DXGI_FORMAT_UNKNOWN)
	{
		PanicAlert("VarToD3D: Invalid type/size combo %i , %i", (int)t, size);
	}
	return retval;
}

D3D12_INPUT_ELEMENT_DESC D3DVertexFormat::GetInputElementDescFromAttributeFormat(const AttributeFormat* format, const char* semantic_name, unsigned int semantic_index) const
{
	D3D12_INPUT_ELEMENT_DESC desc = {};

	desc.AlignedByteOffset = format->offset;
	desc.Format = VarToD3D(format->type, format->components);
	desc.InputSlot = 0;
	desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	desc.SemanticName = semantic_name;
	desc.SemanticIndex = semantic_index;

	return desc;
}

D3DVertexFormat::~D3DVertexFormat()
{}

D3DVertexFormat::D3DVertexFormat(const PortableVertexDeclaration &vtx_decl)
{
	this->vtx_decl = vtx_decl;
	const AttributeFormat* format = &vtx_decl.position;

	m_elems[m_num_elems] = GetInputElementDescFromAttributeFormat(format, "POSITION", 0);
	++m_num_elems;

	for (int i = 0; i < 3; i++)
	{
		format = &vtx_decl.normals[i];
		if (format->enable)
		{
			m_elems[m_num_elems] = GetInputElementDescFromAttributeFormat(format, "NORMAL", i);
			++m_num_elems;
		}
	}

	for (int i = 0; i < 2; i++)
	{
		format = &vtx_decl.colors[i];
		if (format->enable)
		{
			m_elems[m_num_elems] = GetInputElementDescFromAttributeFormat(format, "COLOR", i);
			++m_num_elems;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		format = &vtx_decl.texcoords[i];
		if (format->enable)
		{
			m_elems[m_num_elems] = GetInputElementDescFromAttributeFormat(format, "TEXCOORD", i);
			++m_num_elems;
		}
	}

	format = &vtx_decl.posmtx;
	if (format->enable)
	{
		m_elems[m_num_elems] = GetInputElementDescFromAttributeFormat(format, "BLENDINDICES", 0);
		++m_num_elems;
	}

	m_layout.NumElements = m_num_elems;
	m_layout.pInputElementDescs = m_elems.data();
}

void D3DVertexFormat::SetupVertexPointers()
{
	// No-op on DX12.
}

} // namespace DX12
