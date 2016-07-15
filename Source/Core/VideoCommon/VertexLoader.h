// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified for Ishiiruka By Tino
#pragma once
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexLoaderBase.h"

class VertexLoader : public VertexLoaderBase
{
public:
	VertexLoader(const TVtxDesc &vtx_desc, const VAT &vtx_attr);
	~VertexLoader();

	s32 RunVertices(const VertexLoaderParameters &parameters) override;

	// For debugging / profiling
	bool IsInitialized() override
	{
		return true;
	} // This vertex loader supports all formats
private:
	// Pipeline.
	TPipelineFunction m_PipelineStages[32];
	s32 m_numPipelineStages;

	void CompileVertexTranslator();

	void WriteCall(TPipelineFunction);
};