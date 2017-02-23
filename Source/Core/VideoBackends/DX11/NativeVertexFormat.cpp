// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#include <array>

#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DBlob.h"
#include "VideoBackends/DX11/D3DState.h"
#include "VideoBackends/DX11/VertexManager.h"
#include "VideoBackends/DX11/VertexShaderCache.h"
#include "VideoCommon/NativeVertexFormat.h"

namespace DX11
{

class D3DVertexFormat : public NativeVertexFormat
{
public:
	D3DVertexFormat(const PortableVertexDeclaration &_vtx_decl);
	void SetupVertexPointers() override;
private:
	std::array<D3D11_INPUT_ELEMENT_DESC, 16> m_elems{};
	UINT m_num_elems = 0;

	D3D::InputLayoutPtr m_layout;
};

std::unique_ptr<NativeVertexFormat> VertexManager::CreateNativeVertexFormat(const PortableVertexDeclaration &_vtx_decl)
{
	return std::make_unique<D3DVertexFormat>(_vtx_decl);
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

D3DVertexFormat::D3DVertexFormat(const PortableVertexDeclaration &_vtx_decl) : m_num_elems(0)
{
	vtx_decl = _vtx_decl;
	const AttributeFormat* format = &vtx_decl.position;

	m_elems[m_num_elems].SemanticName = "POSITION";
	m_elems[m_num_elems].AlignedByteOffset = format->offset;
	m_elems[m_num_elems].Format = VarToD3D(format->type, format->components);
	m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	++m_num_elems;

	for (int i = 0; i < 3; i++)
	{
		format = &vtx_decl.normals[i];
		if (format->enable)
		{
			m_elems[m_num_elems].SemanticName = "NORMAL";
			m_elems[m_num_elems].SemanticIndex = i;
			m_elems[m_num_elems].AlignedByteOffset = format->offset;
			m_elems[m_num_elems].Format = VarToD3D(format->type, format->components);
			m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			++m_num_elems;
		}
	}

	for (int i = 0; i < 2; i++)
	{
		format = &vtx_decl.colors[i];
		if (format->enable)
		{
			m_elems[m_num_elems].SemanticName = "COLOR";
			m_elems[m_num_elems].SemanticIndex = i;
			m_elems[m_num_elems].AlignedByteOffset = format->offset;
			m_elems[m_num_elems].Format = VarToD3D(format->type, format->components);
			m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			++m_num_elems;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		format = &vtx_decl.texcoords[i];
		if (format->enable)
		{
			m_elems[m_num_elems].SemanticName = "TEXCOORD";
			m_elems[m_num_elems].SemanticIndex = i;
			m_elems[m_num_elems].AlignedByteOffset = format->offset;
			m_elems[m_num_elems].Format = VarToD3D(format->type, format->components);
			m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			++m_num_elems;
		}
	}

	format = &vtx_decl.posmtx;
	if (format->enable)
	{
		m_elems[m_num_elems].SemanticName = "BLENDINDICES";
		m_elems[m_num_elems].AlignedByteOffset = format->offset;
		m_elems[m_num_elems].Format = VarToD3D(format->type, format->components);
		m_elems[m_num_elems].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		++m_num_elems;
	}
}

void D3DVertexFormat::SetupVertexPointers()
{
	if (!m_layout)
	{
		// CreateInputLayout requires a shader input, but it only looks at the
		// signature of the shader, so we don't need to recompute it if the shader
		// changes.
		auto& vs_bytecode = DX11::VertexShaderCache::GetActiveShaderBytecode();

		HRESULT hr = DX11::D3D::device->CreateInputLayout(m_elems.data(), m_num_elems, vs_bytecode.Data(), vs_bytecode.Size(), D3D::ToAddr(m_layout));
		if (FAILED(hr)) PanicAlert("Failed to create input layout, %s %d\n", __FILE__, __LINE__);
		DX11::D3D::SetDebugObjectName(m_layout.get(), "input layout used to emulate the GX pipeline");
	}
	DX11::D3D::stateman->SetInputLayout(m_layout.get());
}

} // namespace DX11
