// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D12/NativeVertexFormat.h"


#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DBlob.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/VertexManager.h"
#include "VideoBackends/D3D12/VertexShaderCache.h"

namespace DX12
{

NativeVertexFormat* VertexManager::CreateNativeVertexFormat(const PortableVertexDeclaration &_vtx_decl)
{
	return new D3DVertexFormat(_vtx_decl);
}

DXGI_FORMAT VarToD3D(EVTXComponentFormat t, int size)
{
	DXGI_FORMAT retval = DXGI_FORMAT_UNKNOWN;
	static const DXGI_FORMAT lookup[4][5] =
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

D3DVertexFormat::D3DVertexFormat(const PortableVertexDeclaration &_vtx_decl) : m_num_elems(0), m_layout12({}), m_elems()
{
	this->vtx_decl = _vtx_decl;
	memset(m_elems, 0, sizeof(m_elems));
	const AttributeFormat* format = &_vtx_decl.position;

	m_elems[m_num_elems].SemanticName = "POSITION";
	m_elems[m_num_elems].AlignedByteOffset = format->offset;
	m_elems[m_num_elems].Format = VarToD3D(format->type, format->components);
	m_elems[m_num_elems].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	++m_num_elems;

	for (int i = 0; i < 3; i++)
	{
		format = &_vtx_decl.normals[i];
		if (format->enable)
		{
			m_elems[m_num_elems].SemanticName = "NORMAL";
			m_elems[m_num_elems].SemanticIndex = i;
			m_elems[m_num_elems].AlignedByteOffset = format->offset;
			m_elems[m_num_elems].Format = VarToD3D(format->type, format->components);
			m_elems[m_num_elems].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			++m_num_elems;
		}
	}

	for (int i = 0; i < 2; i++)
	{
		format = &_vtx_decl.colors[i];
		if (format->enable)
		{
			m_elems[m_num_elems].SemanticName = "COLOR";
			m_elems[m_num_elems].SemanticIndex = i;
			m_elems[m_num_elems].AlignedByteOffset = format->offset;
			m_elems[m_num_elems].Format = VarToD3D(format->type, format->components);
			m_elems[m_num_elems].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			++m_num_elems;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		format = &_vtx_decl.texcoords[i];
		if (format->enable)
		{
			m_elems[m_num_elems].SemanticName = "TEXCOORD";
			m_elems[m_num_elems].SemanticIndex = i;
			m_elems[m_num_elems].AlignedByteOffset = format->offset;
			m_elems[m_num_elems].Format = VarToD3D(format->type, format->components);
			m_elems[m_num_elems].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			++m_num_elems;
		}
	}

	format = &_vtx_decl.posmtx;
	if (format->enable)
	{
		m_elems[m_num_elems].SemanticName = "BLENDINDICES";
		m_elems[m_num_elems].AlignedByteOffset = format->offset;
		m_elems[m_num_elems].Format = VarToD3D(format->type, format->components);
		m_elems[m_num_elems].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		++m_num_elems;
	}

	m_layout12.NumElements = m_num_elems;
	m_layout12.pInputElementDescs = m_elems;
}

void D3DVertexFormat::SetupVertexPointers()
{
	// No-op on DX12.
}

} // namespace DX12
