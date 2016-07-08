// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified for Ishiiruka By Tino
#pragma once

#include <memory>

#include "VideoCommon/NativeVertexFormat.h"
extern const float fractionTable[32];

struct VertexLoaderParameters
{
	u8* source;
	u8* destination;
	const TVtxDesc *VtxDesc;
	const VAT *VtxAttr;
	size_t buf_size;
	int vtx_attr_group;
	int primitive;
	int count;
	bool skip_draw;
	bool needloaderrefresh;
};

class VertexLoaderUID
{
	u32 vid[4];
	u64 hash;
	size_t platformhash;
public:
	VertexLoaderUID(const TVtxDesc& VtxDesc, const VAT& vat);
	bool operator < (const VertexLoaderUID &other) const;
	bool operator == (const VertexLoaderUID& rh) const;
	u64 GetHash() const;
	size_t GetplatformHash() const;
	u32 GetElement(u32 idx) const;
private:
	u64 CalculateHash();
};

namespace std
{
template <>
struct hash<VertexLoaderUID>
{
	size_t operator()(const VertexLoaderUID& uid) const
	{
		return uid.GetplatformHash();
	}
};

}

class VertexLoaderBase
{
public:
	static std::unique_ptr<VertexLoaderBase> CreateVertexLoader(const TVtxDesc &vtx_desc, const VAT &vtx_attr);
	virtual ~VertexLoaderBase()
	{
		m_fallback.reset();
	}
	void SetFallback(std::unique_ptr<VertexLoaderBase>& obj)
	{
		m_fallback = std::move(obj);
	}
	VertexLoaderBase* GetFallback()
	{
		return m_fallback.get();
	}
	virtual bool EnvironmentIsSupported()
	{
		return true;
	}
	virtual bool IsPrecompiled()
	{
		return false;
	}
	virtual s32 RunVertices(const VertexLoaderParameters &parameters) = 0;

	virtual bool IsInitialized() = 0;

	// For debugging / profiling
	void AppendToString(std::string *dest) const;
	std::string GetName() const;

	// per loader public state
	s32 m_VertexSize;      // number of bytes of a raw GC vertex
	s32 m_native_stride;
	PortableVertexDeclaration m_native_vtx_decl;
	u32 m_native_components;

	// used by VertexLoaderManager
	NativeVertexFormat* m_native_vertex_format;
	u64 m_numLoadedVertices;

protected:
	VertexLoaderBase(const TVtxDesc &vtx_desc, const VAT &vtx_attr);
	void InitializeVertexData();
	void SetVAT(const VAT &vtx_attr);

	// GC vertex format
	TVtxAttr m_VtxAttr;  // VAT decoded into easy format
	TVtxDesc m_VtxDesc;  // Not really used currently - or well it is, but could be easily avoided.
	VAT m_vat;
	std::unique_ptr<VertexLoaderBase> m_fallback;
};