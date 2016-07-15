// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <memory>
#include <vector>

#include "Common/Common.h"
#include "Common/CPUDetect.h"
#include "Common/StringUtil.h"

#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderCompiled.h"
#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VideoConfig.h"

#ifdef _M_X86_64
#include "VideoCommon/VertexLoaderX64.h"
#endif

TPipelineState g_PipelineState;

const float fractionTable[32] = {
	1.0f / (1U << 0), 1.0f / (1U << 1), 1.0f / (1U << 2), 1.0f / (1U << 3),
	1.0f / (1U << 4), 1.0f / (1U << 5), 1.0f / (1U << 6), 1.0f / (1U << 7),
	1.0f / (1U << 8), 1.0f / (1U << 9), 1.0f / (1U << 10), 1.0f / (1U << 11),
	1.0f / (1U << 12), 1.0f / (1U << 13), 1.0f / (1U << 14), 1.0f / (1U << 15),
	1.0f / (1U << 16), 1.0f / (1U << 17), 1.0f / (1U << 18), 1.0f / (1U << 19),
	1.0f / (1U << 20), 1.0f / (1U << 21), 1.0f / (1U << 22), 1.0f / (1U << 23),
	1.0f / (1U << 24), 1.0f / (1U << 25), 1.0f / (1U << 26), 1.0f / (1U << 27),
	1.0f / (1U << 28), 1.0f / (1U << 29), 1.0f / (1U << 30), 1.0f / (1U << 31),
};

VertexLoaderUID::VertexLoaderUID(const TVtxDesc& VtxDesc, const VAT& vat)
{
	u32 fullmask = 0xFFFFFFFFu;
	vid[0] = (u32)((VtxDesc.Hex >> 1) & fullmask);
	// Disable unused components		
	u32 mask = ~VAT_0_FRACBITS;
	mask &= VtxDesc.Color0 ? fullmask : ~VAT_0_COL0BITS;
	mask &= VtxDesc.Color1 ? fullmask : ~VAT_0_COL1BITS;
	mask &= VtxDesc.Normal ? fullmask : ~VAT_0_NRMBITS;
	mask &= VtxDesc.Tex0Coord || VtxDesc.Tex0MatIdx ? fullmask : ~VAT_0_TEX0BITS;
	vid[1] = vat.g0.Hex & mask;
	mask = ~VAT_1_FRACBITS;
	mask &= VtxDesc.Tex1Coord || VtxDesc.Tex1MatIdx ? fullmask : ~VAT_1_TEX1BITS;
	mask &= VtxDesc.Tex2Coord || VtxDesc.Tex2MatIdx ? fullmask : ~VAT_1_TEX2BITS;
	mask &= VtxDesc.Tex3Coord || VtxDesc.Tex3MatIdx ? fullmask : ~VAT_1_TEX3BITS;
	mask &= VtxDesc.Tex4Coord || VtxDesc.Tex4MatIdx ? fullmask : ~VAT_1_TEX4BITS;
	vid[2] = vat.g1.Hex & mask;
	// encode posmtxidx in the free bit inside VAT2
	vid[2] = VtxDesc.PosMatIdx ? (vid[2] | 0x80000000u) : (vid[2] & 0x7FFFFFFFu);
	mask = ~VAT_2_FRACBITS;
	mask &= VtxDesc.Tex4Coord || VtxDesc.Tex4MatIdx ? fullmask : ~VAT_2_TEX4BITS;
	mask &= VtxDesc.Tex5Coord || VtxDesc.Tex5MatIdx ? fullmask : ~VAT_2_TEX5BITS;
	mask &= VtxDesc.Tex6Coord || VtxDesc.Tex6MatIdx ? fullmask : ~VAT_2_TEX6BITS;
	mask &= VtxDesc.Tex7Coord || VtxDesc.Tex7MatIdx ? fullmask : ~VAT_2_TEX7BITS;
	vid[3] = vat.g2.Hex & mask;
	hash = CalculateHash();
	if (sizeof(size_t) >= sizeof(u64))
	{
		platformhash = (size_t)hash;
	}
	else
	{
		size_t platformmask = 0;
		platformmask = ~platformmask;
		platformhash = (size_t)(hash & platformmask);
		u32 sl = sizeof(size_t) * 8;
		platformhash = platformhash ^ (size_t)((hash >> sl) && platformmask);
	}
}

bool VertexLoaderUID::operator < (const VertexLoaderUID &other) const
{
	// This is complex because of speed.
	if (vid[0] < other.vid[0])
		return true;
	else if (vid[0] > other.vid[0])
		return false;

	for (int i = 1; i < 4; ++i)
	{
		if (vid[i] < other.vid[i])
			return true;
		else if (vid[i] > other.vid[i])
			return false;
	}

	return false;
}

bool VertexLoaderUID::operator == (const VertexLoaderUID& rh) const
{
	return hash == rh.hash && std::equal(vid, vid + sizeof(vid) / sizeof(vid[0]), rh.vid);
}

u64 VertexLoaderUID::GetHash() const
{
	return hash;
}

size_t VertexLoaderUID::GetplatformHash() const
{
	return platformhash;
}

u32 VertexLoaderUID::GetElement(u32 idx) const
{
	return vid[idx];
}

u64 VertexLoaderUID::CalculateHash()
{
	u64 h = -1;
	for (auto word : vid)
	{
		h = h * 137 + word;
	}
	return h;
}

VertexLoaderBase::VertexLoaderBase(const TVtxDesc &vtx_desc, const VAT &vtx_attr)
{
	m_fallback = nullptr;
	m_native_stride = 0;
	m_numLoadedVertices = 0;
	m_VertexSize = 0;
	m_native_vertex_format = nullptr;
	m_native_components = 0;
	memset(&m_native_vtx_decl, 0, sizeof(m_native_vtx_decl));

	SetVAT(vtx_attr);
	m_VtxDesc = vtx_desc;
	m_vat = vtx_attr;
}

void VertexLoaderBase::SetVAT(const VAT& vat)
{
	m_VtxAttr.PosElements = vat.g0.PosElements;
	m_VtxAttr.PosFormat = vat.g0.PosFormat;
	m_VtxAttr.PosFrac = vat.g0.PosFrac;
	m_VtxAttr.NormalElements = vat.g0.NormalElements;
	m_VtxAttr.NormalFormat = vat.g0.NormalFormat;
	m_VtxAttr.color[0].Elements = vat.g0.Color0Elements;
	m_VtxAttr.color[0].Comp = vat.g0.Color0Comp;
	m_VtxAttr.color[1].Elements = vat.g0.Color1Elements;
	m_VtxAttr.color[1].Comp = vat.g0.Color1Comp;
	m_VtxAttr.texCoord[0].Elements = vat.g0.Tex0CoordElements;
	m_VtxAttr.texCoord[0].Format = vat.g0.Tex0CoordFormat;
	m_VtxAttr.texCoord[0].Frac = vat.g0.Tex0Frac;
	m_VtxAttr.ByteDequant = vat.g0.ByteDequant;
	m_VtxAttr.NormalIndex3 = vat.g0.NormalIndex3;

	m_VtxAttr.texCoord[1].Elements = vat.g1.Tex1CoordElements;
	m_VtxAttr.texCoord[1].Format = vat.g1.Tex1CoordFormat;
	m_VtxAttr.texCoord[1].Frac = vat.g1.Tex1Frac;
	m_VtxAttr.texCoord[2].Elements = vat.g1.Tex2CoordElements;
	m_VtxAttr.texCoord[2].Format = vat.g1.Tex2CoordFormat;
	m_VtxAttr.texCoord[2].Frac = vat.g1.Tex2Frac;
	m_VtxAttr.texCoord[3].Elements = vat.g1.Tex3CoordElements;
	m_VtxAttr.texCoord[3].Format = vat.g1.Tex3CoordFormat;
	m_VtxAttr.texCoord[3].Frac = vat.g1.Tex3Frac;
	m_VtxAttr.texCoord[4].Elements = vat.g1.Tex4CoordElements;
	m_VtxAttr.texCoord[4].Format = vat.g1.Tex4CoordFormat;

	m_VtxAttr.texCoord[4].Frac = vat.g2.Tex4Frac;
	m_VtxAttr.texCoord[5].Elements = vat.g2.Tex5CoordElements;
	m_VtxAttr.texCoord[5].Format = vat.g2.Tex5CoordFormat;
	m_VtxAttr.texCoord[5].Frac = vat.g2.Tex5Frac;
	m_VtxAttr.texCoord[6].Elements = vat.g2.Tex6CoordElements;
	m_VtxAttr.texCoord[6].Format = vat.g2.Tex6CoordFormat;
	m_VtxAttr.texCoord[6].Frac = vat.g2.Tex6Frac;
	m_VtxAttr.texCoord[7].Elements = vat.g2.Tex7CoordElements;
	m_VtxAttr.texCoord[7].Format = vat.g2.Tex7CoordFormat;
	m_VtxAttr.texCoord[7].Frac = vat.g2.Tex7Frac;
};

std::string VertexLoaderBase::GetName() const
{
	std::string dest;
	static const char *posMode[4] = {
		"Inv",
		"Dir",
		"I8",
		"I16",
	};
	static const char *posFormats[5] = {
		"u8", "s8", "u16", "s16", "flt",
	};
	static const char *colorFormat[8] = {
		"565",
		"888",
		"888x",
		"4444",
		"6666",
		"8888",
		"Inv",
		"Inv",
	};

	dest.append(StringFromFormat("P_mtx%zu_%i_%s_%s_", m_VtxDesc.PosMatIdx,
		m_VtxAttr.PosElements ? 3 : 2, posMode[m_VtxDesc.Position], posFormats[m_VtxAttr.PosFormat]));

	if (m_VtxDesc.Normal)
	{
		dest.append(StringFromFormat("Nrm_%i_%i_%s_%s_",
			m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3, posMode[m_VtxDesc.Normal], posFormats[m_VtxAttr.NormalFormat]));
	}

	u64 color_mode[2] = { m_VtxDesc.Color0, m_VtxDesc.Color1 };
	for (int i = 0; i < 2; i++)
	{
		if (color_mode[i])
		{
			dest.append(StringFromFormat("C%i_%i_%s_%s_", i, m_VtxAttr.color[i].Elements, posMode[color_mode[i]], colorFormat[m_VtxAttr.color[i].Comp]));
		}
	}
	const u64 tex_mode[8] = {
		m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord, m_VtxDesc.Tex3Coord,
		m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord, m_VtxDesc.Tex6Coord, (const u64)((m_VtxDesc.Hex >> 31) & 3)
	};
	const u64 tex_mtxidx[8] = {
		m_VtxDesc.Tex0MatIdx, m_VtxDesc.Tex1MatIdx, m_VtxDesc.Tex2MatIdx, m_VtxDesc.Tex3MatIdx,
		m_VtxDesc.Tex4MatIdx, m_VtxDesc.Tex5MatIdx, m_VtxDesc.Tex6MatIdx, m_VtxDesc.Tex7MatIdx
	};
	for (int i = 0; i < 8; i++)
	{
		if (tex_mode[i] || tex_mtxidx[i])
		{
			dest.append(StringFromFormat("T%i_mtx%zu_%i_%s_%s_",
				i, tex_mtxidx[i], m_VtxAttr.texCoord[i].Elements, posMode[tex_mode[i]], posFormats[m_VtxAttr.texCoord[i].Format]));
		}
	};
	return dest;
}

void VertexLoaderBase::AppendToString(std::string *dest) const
{
	dest->reserve(250);

	dest->append(GetName());
	dest->append(StringFromFormat(" - %zu v\n", m_numLoadedVertices));
}

// a hacky implementation to compare two vertex loaders
class VertexLoaderTester : public VertexLoaderBase
{
public:
	VertexLoaderTester(std::unique_ptr<VertexLoaderBase> _a, std::unique_ptr<VertexLoaderBase> _b, const TVtxDesc& vtx_desc, const VAT& vtx_attr)
		: VertexLoaderBase(vtx_desc, vtx_attr), a(std::move(_a)), b(std::move(_b))
	{

		m_initialized = a && b && a->IsInitialized() && b->IsInitialized();
		bool can_test = a->m_VertexSize == b->m_VertexSize &&
			a->m_native_components == b->m_native_components &&
			a->m_native_vtx_decl.stride == b->m_native_vtx_decl.stride &&
			a->m_native_vertex_format == b->m_native_vertex_format;

		if (m_initialized)
		{
			if (can_test)
			{
				m_native_stride = a->m_native_stride;
				m_native_vertex_format = a->m_native_vertex_format;
				m_VertexSize = a->m_VertexSize;
				m_native_components = a->m_native_components;
				memcpy(&m_native_vtx_decl, &a->m_native_vtx_decl, sizeof(PortableVertexDeclaration));
			}
			else
			{
				ERROR_LOG(VIDEO, "Can't compare vertex loaders that expect different vertex formats!");
				ERROR_LOG(VIDEO, "a: m_VertexSize %d, m_native_components 0x%08x, stride %d\n",
					a->m_VertexSize, a->m_native_components, a->m_native_vtx_decl.stride);
				ERROR_LOG(VIDEO, "b: m_VertexSize %d, m_native_components 0x%08x, stride %d\n",
					b->m_VertexSize, b->m_native_components, b->m_native_vtx_decl.stride);
			}
		}

		m_initialized &= can_test;
	}
	~VertexLoaderTester() override
	{
		a.reset();
		b.reset();
	}

	s32 RunVertices(const VertexLoaderParameters &parameters) override
	{
		buffer_a.resize(parameters.count * a->m_native_vtx_decl.stride + 4);
		buffer_b.resize(parameters.count * b->m_native_vtx_decl.stride + 4);
		VertexLoaderParameters params = parameters;
		params.destination = buffer_a.data();
		int count_a = a->RunVertices(params);
		params.destination = buffer_b.data();
		int count_b = b->RunVertices(params);

		if (count_a != count_b)
			ERROR_LOG(VIDEO, "The two vertex loaders have loaded a different amount of vertices (a: %d, b: %d).", count_a, count_b);

		if (memcmp(buffer_a.data(), buffer_b.data(), std::min(count_a, count_b) * m_native_vtx_decl.stride))
			ERROR_LOG(VIDEO, "The two vertex loaders have loaded different data "
				"(guru meditation 0x%016" PRIx64 ", 0x%08x, 0x%08x, 0x%08x).",
				m_VtxDesc.Hex, m_vat.g0.Hex, m_vat.g1.Hex, m_vat.g2.Hex);

		memcpy(parameters.destination, buffer_a.data(), count_a * m_native_vtx_decl.stride);
		m_numLoadedVertices += parameters.count;
		return count_a;
	}
	bool IsInitialized() override
	{
		return m_initialized;
	}

private:
	std::unique_ptr<VertexLoaderBase> a;
	std::unique_ptr<VertexLoaderBase> b;

	bool m_initialized;
	std::vector<u8> buffer_a;
	std::vector<u8> buffer_b;
};

std::unique_ptr<VertexLoaderBase> VertexLoaderBase::CreateVertexLoader(const TVtxDesc& vtx_desc, const VAT& vtx_attr)
{
	std::unique_ptr<VertexLoaderBase> loader;

	//#define COMPARE_VERTEXLOADERS

#if defined(COMPARE_VERTEXLOADERS) && defined(_M_X86_64)
	// first try: Any new VertexLoader vs the old one
	loader = std::make_unique<VertexLoaderTester>(
		std::make_unique<VertexLoader>(vtx_desc, vtx_attr), // the software one
		std::make_unique<VertexLoaderX64>(vtx_desc, vtx_attr), // the new one to compare
		vtx_desc, vtx_attr);
	if (loader->IsInitialized())
		return loader;
	loader.reset();
#elif defined(_M_X86_64)
	loader = std::make_unique<VertexLoaderX64>(vtx_desc, vtx_attr);
	if (!loader->IsInitialized())
	{
		loader.reset();
	}
#endif
	std::unique_ptr<VertexLoaderBase> fallback = std::make_unique<VertexLoaderCompiled>(vtx_desc, vtx_attr);
	if (!fallback->IsInitialized())
	{
		fallback.reset();
	}
	if (!fallback)
	{
		// last try: The old VertexLoader
		fallback = std::make_unique<VertexLoader>(vtx_desc, vtx_attr);
	}
	if (!loader)
	{
		loader = std::move(fallback);
		fallback.reset();
	}
	loader->SetFallback(fallback);
	return loader;
}
