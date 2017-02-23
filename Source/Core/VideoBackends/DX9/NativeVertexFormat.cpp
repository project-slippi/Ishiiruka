// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#include <array>

#include "Common/MemoryUtil.h"
#include "Common/x64Emitter.h"
#include "Common/x64ABI.h"

#include "VideoBackends/DX9/D3DBase.h"
#include "VideoBackends/DX9/VertexManager.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX9
{

class D3DVertexFormat : public NativeVertexFormat
{
public:
	D3DVertexFormat(const PortableVertexDeclaration &_vtx_decl);
	~D3DVertexFormat();
	void SetupVertexPointers() override;
	std::array<D3DVERTEXELEMENT9, 16> m_elems{};
	int m_num_elems = 0;
private:


	LPDIRECT3DVERTEXDECLARATION9 d3d_decl{};
};

std::unique_ptr<NativeVertexFormat> VertexManager::CreateNativeVertexFormat(const PortableVertexDeclaration &_vtx_decl)
{
	return std::make_unique<D3DVertexFormat>(_vtx_decl);
}

void DX9::VertexManager::GetElements(NativeVertexFormat* format, D3DVERTEXELEMENT9** elems, int* num)
{
#if defined(_DEBUG) || defined(DEBUGFAST)
	*elems = ((D3DVertexFormat*)format)->m_elems.data();
	*num = ((D3DVertexFormat*)format)->m_num_elems;
#else
	*elems = NULL;
	*num = 0;
#endif
}

D3DVertexFormat::~D3DVertexFormat()
{
	if (d3d_decl)
	{
		d3d_decl->Release();
		d3d_decl = NULL;
	}
}

D3DDECLTYPE VarToD3D(EVTXComponentFormat t, int size)
{
	if (t < 0 || t > 4)
	{
		PanicAlert("VarToD3D: Invalid VarType %i", t);
	}
	// Sadly, D3D9 has no SBYTE4N. D3D10 does, though.
	static const D3DDECLTYPE lookup[4][5] =
	{
		{
			D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_FLOAT1
		},
		{
			D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_USHORT2N, D3DDECLTYPE_SHORT2N, D3DDECLTYPE_FLOAT2
		},
		{
			D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_UNUSED, D3DDECLTYPE_FLOAT3
		},
		{
			D3DDECLTYPE_UBYTE4N, D3DDECLTYPE_UNUSED, D3DDECLTYPE_USHORT4N, D3DDECLTYPE_SHORT4N, D3DDECLTYPE_FLOAT4
		}
	};
	D3DDECLTYPE retval = D3DDECLTYPE_UNUSED;
	if (size < 5)
	{
		retval = lookup[size - 1][t];
	}
	if (retval == D3DDECLTYPE_UNUSED)
	{
		PanicAlert("VarToD3D: Invalid type/size combo %i , %i", (int)t, size);
	}
	return retval;
}

D3DVertexFormat::D3DVertexFormat(const PortableVertexDeclaration &_vtx_decl) : d3d_decl(nullptr)
{
	vtx_decl = _vtx_decl;

	// There's only one stream and it's 0, so the above memset takes care of that - no need to set Stream.
	// Same for method.
	const AttributeFormat* format = &vtx_decl.position;
	int elem_idx = 0;
	if (format->enable)
	{
		// So, here we go. First position:		
		m_elems[elem_idx].Offset = format->offset;
		m_elems[elem_idx].Type = VarToD3D(format->type, format->components);
		m_elems[elem_idx].Usage = D3DDECLUSAGE_POSITION;
		++elem_idx;
	}

	for (int i = 0; i < 3; i++)
	{
		format = &vtx_decl.normals[i];
		if (format->enable)
		{
			m_elems[elem_idx].Offset = format->offset;
			m_elems[elem_idx].Type = VarToD3D(format->type, format->components);
			m_elems[elem_idx].Usage = D3DDECLUSAGE_NORMAL;
			m_elems[elem_idx].UsageIndex = i;
			++elem_idx;
		}
	}

	for (int i = 0; i < 2; i++)
	{
		format = &vtx_decl.colors[i];
		if (format->enable)
		{
			m_elems[elem_idx].Offset = format->offset;
			m_elems[elem_idx].Type = VarToD3D(format->type, 4);
			m_elems[elem_idx].Usage = D3DDECLUSAGE_COLOR;
			m_elems[elem_idx].UsageIndex = i;
			++elem_idx;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		format = &vtx_decl.texcoords[i];
		if (format->enable)
		{
			m_elems[elem_idx].Offset = format->offset;
			m_elems[elem_idx].Type = VarToD3D(format->type, format->components);
			m_elems[elem_idx].Usage = D3DDECLUSAGE_TEXCOORD;
			m_elems[elem_idx].UsageIndex = i;
			++elem_idx;
		}
	}

	if (vtx_decl.posmtx.enable)
	{
		m_elems[elem_idx].Offset = vtx_decl.posmtx.offset;
		m_elems[elem_idx].Usage = D3DDECLUSAGE_BLENDINDICES;
		m_elems[elem_idx].Type = D3DDECLTYPE_D3DCOLOR;
		m_elems[elem_idx].UsageIndex = 0;
		++elem_idx;
	}

	// End marker
	m_elems[elem_idx].Stream = 0xff;
	m_elems[elem_idx].Type = D3DDECLTYPE_UNUSED;
	++elem_idx;
	m_num_elems = elem_idx;
}

void D3DVertexFormat::SetupVertexPointers()
{
	if (!d3d_decl)
	{
		if (FAILED(DX9::D3D::dev->CreateVertexDeclaration(m_elems.data(), &d3d_decl)))
		{
			PanicAlert("Failed to create D3D vertex declaration!");
			return;
		}
	}
	if (d3d_decl)
		DX9::D3D::SetVertexDeclaration(d3d_decl);
	else
		ERROR_LOG(VIDEO, "Invalid D3D decl");
}

} // namespace DX9
