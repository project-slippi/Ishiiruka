// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified for Ishiiruka By Tino
#pragma once
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexLoaderBase.h"

class VertexLoaderCompiled : public VertexLoaderBase
{
public:
	static void Initialize();
	VertexLoaderCompiled(const TVtxDesc &vtx_desc, const VAT &vtx_attr);
	~VertexLoaderCompiled();
	bool IsPrecompiled() override
	{
		return true;
	}
	s32 RunVertices(const VertexLoaderParameters &parameters) override;

	bool IsInitialized() override
	{
		return m_initialized;
	} // This vertex loader supports all formats
private:
	static bool s_PrecompiledLoadersInitialized;
	bool m_initialized;
	TCompiledLoaderFunction m_precompiledfunc;
	void InitializeVertexData();
};