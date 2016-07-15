// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified for Ishiiruka By Tino

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/VertexLoaderCompiled.h"
#include "VideoCommon/VertexLoader_Color.h"
#include "VideoCommon/VertexLoader_Normal.h"
#include "VideoCommon/VertexLoader_Position.h"
#include "VideoCommon/VertexLoader_TextCoord.h"
#include "VideoCommon/VertexLoader_Mtx.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"

// Precompiled Loaders
#include "VideoCommon/G_G4BP08_pvt.h"
#include "VideoCommon/G_GB4P51_pvt.h"
#include "VideoCommon/G_GFZE01_pvt.h"
#include "VideoCommon/G_GLMP01_pvt.h"
#include "VideoCommon/G_GM8E01_pvt.h"
#include "VideoCommon/G_GNUEDA_pvt.h"
#include "VideoCommon/G_GSAE01_pvt.h"
#include "VideoCommon/G_GZ2P01_pvt.h"
#include "VideoCommon/G_RBUP08_pvt.h"
#include "VideoCommon/G_R5WEA4_pvt.h"
#include "VideoCommon/G_RMCP01_pvt.h"
#include "VideoCommon/G_RMGP01_pvt.h"
#include "VideoCommon/G_RSBP01_pvt.h"
#include "VideoCommon/G_SDWP18_pvt.h"
#include "VideoCommon/G_SMNP01_pvt.h"
#include "VideoCommon/G_SPDE52_pvt.h"
#include "VideoCommon/G_SPXP41_pvt.h"
#include "VideoCommon/G_SX4E01_pvt.h"

typedef std::map<u64, TCompiledLoaderFunction> PrecompiledVertexLoaderMap;
static PrecompiledVertexLoaderMap s_PrecompiledVertexLoaderMap;
bool VertexLoaderCompiled::s_PrecompiledLoadersInitialized = false;
void VertexLoaderCompiled::Initialize()
{
	VertexLoader_Normal::Init();
	VertexLoader_Position::Init();
	VertexLoader_TextCoord::Init();
	if (!s_PrecompiledLoadersInitialized)
	{
		s_PrecompiledLoadersInitialized = true;
		G_G4BP08_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_GB4P51_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_GFZE01_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_GLMP01_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_GM8E01_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_GNUEDA_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_GSAE01_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_GZ2P01_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_R5WEA4_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_RBUP08_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_RMCP01_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_RMGP01_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_RSBP01_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_SDWP18_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_SMNP01_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_SPDE52_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_SPXP41_pvt::Initialize(s_PrecompiledVertexLoaderMap);
		G_SX4E01_pvt::Initialize(s_PrecompiledVertexLoaderMap);
	}
}

VertexLoaderCompiled::VertexLoaderCompiled(const TVtxDesc &vtx_desc, const VAT &vtx_attr)
	: VertexLoaderBase(vtx_desc, vtx_attr)
{
	VertexLoaderCompiled::Initialize();
	VertexLoaderUID uid(vtx_desc, vtx_attr);
	PrecompiledVertexLoaderMap::iterator piter = s_PrecompiledVertexLoaderMap.find(uid.GetHash());
	m_precompiledfunc = nullptr;
	if (piter != s_PrecompiledVertexLoaderMap.end())
	{
		m_precompiledfunc = piter->second;
		m_initialized = true;
		InitializeVertexData();
	}
	else
	{
		m_initialized = false;
	}
}

VertexLoaderCompiled::~VertexLoaderCompiled()
{

}

s32 VertexLoaderCompiled::RunVertices(const VertexLoaderParameters &parameters)
{
	m_numLoadedVertices += parameters.count;
	const VAT &vat = *parameters.VtxAttr;
	if (m_native_components & VB_HAS_UVALL)
	{
		g_PipelineState.tcScale[0] = fractionTable[vat.g0.Tex0Frac];
		g_PipelineState.tcScale[1] = fractionTable[vat.g1.Tex1Frac];
		g_PipelineState.tcScale[2] = fractionTable[vat.g1.Tex2Frac];
		g_PipelineState.tcScale[3] = fractionTable[vat.g1.Tex3Frac];
		g_PipelineState.tcScale[4] = fractionTable[vat.g2.Tex4Frac];
		g_PipelineState.tcScale[5] = fractionTable[vat.g2.Tex5Frac];
		g_PipelineState.tcScale[6] = fractionTable[vat.g2.Tex6Frac];
		g_PipelineState.tcScale[7] = fractionTable[vat.g2.Tex7Frac];
	}
	g_PipelineState.flags = g_ActiveConfig.iBBoxMode == BBoxCPU && BoundingBox::active ? TPS_USE_BBOX : TPS_NONE;
	g_PipelineState.stride = m_native_stride;
	g_PipelineState.skippedVertices = 0;
	g_PipelineState.posScale = fractionTable[vat.g0.PosFrac];
	g_PipelineState.curposmtx = g_main_cp_state.matrix_index_a.PosNormalMtxIdx;
	for (int i = 0; i < 2; i++)
		g_PipelineState.colElements[i] = m_VtxAttr.color[i].Elements;
	// Prepare bounding box
	if (g_ActiveConfig.iBBoxMode == BBoxCPU && BoundingBox::active)
		BoundingBox::Prepare(vat, parameters.primitive, m_VtxDesc, m_native_vtx_decl);
	g_PipelineState.count = parameters.count;
	g_PipelineState.Initialize(parameters.source, parameters.source + parameters.buf_size, parameters.destination);
	m_precompiledfunc(g_PipelineState);
	return parameters.count - g_PipelineState.skippedVertices;
}


void VertexLoaderCompiled::InitializeVertexData()
{
	m_native_components = 0;
	m_VertexSize = 0;
	// Colors
	const u64 col[2] = { m_VtxDesc.Color0, m_VtxDesc.Color1 };
	// TextureCoord
	const u64 tc[8] = {
		m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord, m_VtxDesc.Tex3Coord,
		m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord, m_VtxDesc.Tex6Coord, m_VtxDesc.Tex7Coord
	};

	// Position in pc vertex format.
	int nat_offset = 0;
	memset(&m_native_vtx_decl, 0, sizeof(m_native_vtx_decl));

	// Position Matrix Index
	if (m_VtxDesc.PosMatIdx)
	{
		m_VertexSize += 1;
	}

	if (m_VtxDesc.Tex0MatIdx)
	{
		m_VertexSize += 1; m_native_components |= VB_HAS_TEXMTXIDX0;
	}
	if (m_VtxDesc.Tex1MatIdx)
	{
		m_VertexSize += 1; m_native_components |= VB_HAS_TEXMTXIDX1;
	}
	if (m_VtxDesc.Tex2MatIdx)
	{
		m_VertexSize += 1; m_native_components |= VB_HAS_TEXMTXIDX2;
	}
	if (m_VtxDesc.Tex3MatIdx)
	{
		m_VertexSize += 1; m_native_components |= VB_HAS_TEXMTXIDX3;
	}
	if (m_VtxDesc.Tex4MatIdx)
	{
		m_VertexSize += 1; m_native_components |= VB_HAS_TEXMTXIDX4;
	}
	if (m_VtxDesc.Tex5MatIdx)
	{
		m_VertexSize += 1; m_native_components |= VB_HAS_TEXMTXIDX5;
	}
	if (m_VtxDesc.Tex6MatIdx)
	{
		m_VertexSize += 1; m_native_components |= VB_HAS_TEXMTXIDX6;
	}
	if (m_VtxDesc.Tex7MatIdx)
	{
		m_VertexSize += 1; m_native_components |= VB_HAS_TEXMTXIDX7;
	}

	m_VertexSize += VertexLoader_Position::GetSize(m_VtxDesc.Position, m_VtxAttr.PosFormat, m_VtxAttr.PosElements);
	nat_offset += 12;
	m_native_vtx_decl.position.components = 3;
	m_native_vtx_decl.position.enable = true;
	m_native_vtx_decl.position.offset = 0;
	m_native_vtx_decl.position.type = FORMAT_FLOAT;

	// Normals
	if (m_VtxDesc.Normal != NOT_PRESENT)
	{
		m_VertexSize += VertexLoader_Normal::GetSize(m_VtxDesc.Normal,
			m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);

		TPipelineFunction pFunc = VertexLoader_Normal::GetFunction(m_VtxDesc.Normal,
			m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);

		if (pFunc == 0)
		{
			PanicAlert("VertexLoader_Normal::GetFunction(%i %i %i %i) returned zero!",
				m_VtxDesc.Normal, m_VtxAttr.NormalFormat,
				m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);
		}
		for (int i = 0; i < (m_VtxAttr.NormalElements ? 3 : 1); i++)
		{
			m_native_vtx_decl.normals[i].components = 3;
			m_native_vtx_decl.normals[i].enable = true;
			m_native_vtx_decl.normals[i].offset = nat_offset;
			m_native_vtx_decl.normals[i].type = FORMAT_FLOAT;
			nat_offset += 12;
		}

		m_native_components |= VB_HAS_NRM0;
		if (m_VtxAttr.NormalElements == 1)
			m_native_components |= VB_HAS_NRM1 | VB_HAS_NRM2;
	}

	for (int i = 0; i < 2; i++)
	{
		switch (col[i])
		{
		case NOT_PRESENT:
			break;
		case DIRECT:
			switch (m_VtxAttr.color[i].Comp)
			{
			case FORMAT_16B_565:	m_VertexSize += 2; break;
			case FORMAT_24B_888:	m_VertexSize += 3; break;
			case FORMAT_32B_888x:	m_VertexSize += 4; break;
			case FORMAT_16B_4444:	m_VertexSize += 2; break;
			case FORMAT_24B_6666:	m_VertexSize += 3; break;
			case FORMAT_32B_8888:	m_VertexSize += 4; break;
			default: _assert_(0); break;
			}
			break;
		case INDEX8:
			m_VertexSize += 1;
			break;
		case INDEX16:
			m_VertexSize += 2;
			break;
		}
		// Common for the three bottom cases
		if (col[i] != NOT_PRESENT)
		{
			m_native_components |= VB_HAS_COL0 << i;
			m_native_vtx_decl.colors[i].components = 4;
			m_native_vtx_decl.colors[i].type = FORMAT_UBYTE;
			m_native_vtx_decl.colors[i].offset = nat_offset;
			m_native_vtx_decl.colors[i].enable = true;
			nat_offset += 4;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		const int format = m_VtxAttr.texCoord[i].Format;
		const int elements = m_VtxAttr.texCoord[i].Elements;

		if (tc[i] != NOT_PRESENT)
		{
			_assert_msg_(VIDEO, DIRECT <= tc[i] && tc[i] <= INDEX16, "Invalid texture coordinates!\n(tc[i] = %d)", tc[i]);
			_assert_msg_(VIDEO, FORMAT_UBYTE <= format && format <= FORMAT_FLOAT, "Invalid texture coordinates format!\n(format = %d)", format);
			_assert_msg_(VIDEO, 0 <= elements && elements <= 1, "Invalid number of texture coordinates elements!\n(elements = %d)", elements);

			m_native_components |= VB_HAS_UV0 << i;
			m_VertexSize += VertexLoader_TextCoord::GetSize((u32)tc[i], format, elements);
		}

		if (m_native_components & (VB_HAS_TEXMTXIDX0 << i))
		{
			m_native_vtx_decl.texcoords[i].enable = true;
			m_native_vtx_decl.texcoords[i].offset = nat_offset;
			m_native_vtx_decl.texcoords[i].type = FORMAT_FLOAT;
			if (tc[i] != NOT_PRESENT)
			{
				// if texmtx is included, texcoord will always be 3 floats, z will be the texmtx index
				m_native_vtx_decl.texcoords[i].components = 3;
				nat_offset += 12;
			}
			else
			{
				m_native_components |= VB_HAS_UV0 << i; // have to include since using now
				m_native_vtx_decl.texcoords[i].components = 3;
				nat_offset += 12; // still include the texture coordinate, but this time as 6 + 2 bytes
			}
		}
		else
		{
			if (tc[i] != NOT_PRESENT)
			{
				m_native_vtx_decl.texcoords[i].enable = true;
				m_native_vtx_decl.texcoords[i].offset = nat_offset;
				m_native_vtx_decl.texcoords[i].type = FORMAT_FLOAT;
				m_native_vtx_decl.texcoords[i].components = m_VtxAttr.texCoord[i].Elements ? 2 : 1;
				nat_offset += 4 * (m_VtxAttr.texCoord[i].Elements ? 2 : 1);
			}
		}
		if (tc[i] == NOT_PRESENT)
		{
			// if there's more tex coords later, have to write a dummy call
			int j = i + 1;
			for (; j < 8; ++j)
			{
				if (tc[j] != NOT_PRESENT)
				{
					break;
				}
			}
			// tricky!
			if (j == 8 && !((m_native_components & VB_HAS_TEXMTXIDXALL) & (VB_HAS_TEXMTXIDXALL << (i + 1))))
			{
				// no more tex coords and tex matrices, so exit loop
				break;
			}
		}
	}
	m_native_vtx_decl.posmtx.enable = true;
	m_native_vtx_decl.posmtx.components = 4;
	m_native_vtx_decl.posmtx.offset = nat_offset;
	m_native_vtx_decl.posmtx.type = FORMAT_UBYTE;
	nat_offset += 4;
	m_native_stride = nat_offset;
	m_native_vtx_decl.stride = m_native_stride;
}