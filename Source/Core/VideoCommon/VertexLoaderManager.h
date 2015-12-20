// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified for Ishiiruka by Tino
#pragma once
#include <string>
#include "Common/Common.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexLoaderBase.h"

namespace VertexLoaderManager
{
	typedef std::map<PortableVertexDeclaration, std::unique_ptr<NativeVertexFormat>> NativeVertexLoaderMap;

	void Init();
	void Shutdown();
	
	void MarkAllDirty();

	NativeVertexLoaderMap* GetNativeVertexFormatMap();

	int GetVertexSize(const VertexLoaderParameters &parameters);

	bool ConvertVertices(VertexLoaderParameters &parameters, u32 &readsize, u32 &writesize);

	void GetVertexSizeAndComponents(const VertexLoaderParameters &parameters, u32 &vertexsize, u32 &components);

	// For debugging
	void AppendListToString(std::string *dest);

	void UpdateVertexArrayPointers();

	NativeVertexFormat* GetCurrentVertexFormat();
};