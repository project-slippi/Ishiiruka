// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Hash.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

/**
* Shader UID class used to uniquely identify the ShaderCode output written in the shader generator.
* uid_data can be any struct of parameters that uniquely identify each shader code output.
* Unless performance is not an issue, uid_data should be tightly packed to reduce memory footprint.
* Shader generators will write to specific uid_data fields; ShaderUid methods will only read raw u32 values from a union.
*/
template<class uid_data>
class ShaderUid
{
public:
	ShaderUid() : HASH(0)
	{}

	inline void ClearUID()
	{
		data = {};
	}

	inline void ClearHASH()
	{
		HASH = 0;
	}

	inline void CalculateUIDHash()
	{
		if (HASH == 0)
		{
			data.ClearUnused();
			HASH = (std::size_t)GetHash64(reinterpret_cast<u8*>(&data) + data.StartValue(), data.NumValues(), 0);
		}
	}

	bool operator == (const ShaderUid& obj) const
	{
		return data.StartValue() == obj.data.StartValue()
			&& data.NumValues() == obj.data.NumValues()
			&& !memcmp(reinterpret_cast<const u8*>(&data) + data.StartValue(), reinterpret_cast<const u8*>(&obj.data) + data.StartValue(), data.NumValues());
	}

	bool operator != (const ShaderUid& obj) const
	{
		return data.StartValue() != obj.data.StartValue()
			|| data.NumValues() != obj.data.NumValues()
			|| !!memcmp(reinterpret_cast<const u8*>(&data) + data.StartValue(), reinterpret_cast<const u8*>(&obj.data) + data.StartValue(), data.NumValues());
	}

	// determines the storage order inside STL containers
	bool operator < (const ShaderUid& obj) const
	{
		return memcmp(reinterpret_cast<const u8*>(&data) + data.StartValue(), reinterpret_cast<const u8*>(&obj.data) + obj.data.StartValue(), std::max(obj.data.NumValues(), data.NumValues())) < 0;
	}

	template<class T>
	inline T& GetUidData()
	{
		return data;
	}

	const uid_data& GetUidData() const
	{
		return data;
	}
	size_t GetUidDataSize() const
	{
		return sizeof(uid_data);
	}
	struct ShaderUidHasher
	{
		std::size_t operator()(const ShaderUid<uid_data>& k) const
		{
			return k.HASH;
		}
	};
private:
	uid_data data;
	std::size_t HASH;
};

class ShaderCode
{
public:
	ShaderCode() : buf(NULL), write_ptr(NULL)
	{

	}

	void Write(const char* fmt, ...)
	{
		va_list arglist;
		va_start(arglist, fmt);
		write_ptr += vsprintf(write_ptr, fmt, arglist);
		va_end(arglist);
	}

	char* GetBuffer()
	{
		return buf;
	}
	void SetBuffer(char* buffer)
	{
		buf = buffer; write_ptr = buffer;
	}
	ptrdiff_t BufferSize()
	{
		return write_ptr - buf;
	}
private:
	char* buf;
	char* write_ptr;
};

template<API_TYPE api_type>
inline void WriteRegister(ShaderCode& object, const char *prefix, const u32 num)
{
	if (!(api_type & API_D3D9))
		return; // Nothing to do here

	object.Write(" : register(%s%d)", prefix, num);
}

template<API_TYPE api_type>
inline void WriteLocation(ShaderCode& object)
{
	if (!(api_type & API_D3D9))
		return;

	object.Write("uniform ");
}

template<API_TYPE api_type>
inline void DeclareUniform(ShaderCode& object, const u32 num, const char* type, const char* name)
{
	WriteLocation<api_type>(object);
	object.Write("%s %s ", type, name);
	WriteRegister<api_type>(object, "c", num);
	object.Write(";\n");
}

template<API_TYPE api_type>
inline void DefineVSOutputStructMember(ShaderCode& object, const char* qualifier, const char* type, const char* name, const char* sufix, int var_index, const char* semantic, int semantic_index = -1)
{
	if (qualifier != nullptr)
		object.Write("\t%s %s %s%s", qualifier, type, name, sufix);
	else
		object.Write("\t%s %s%s", type, name, sufix);

	if (var_index != -1)
		object.Write("%d", var_index);

	if (api_type == API_OPENGL || api_type == API_VULKAN)
		object.Write(";\n");
	else
	{
		if (semantic_index != -1)
			object.Write(" : %s%d;\n", semantic, semantic_index);
		else
			object.Write(" : %s;\n", semantic);
	}
}

template<API_TYPE api_type>
inline void GenerateVSOutputMembers(ShaderCode& object, bool enable_pl, u32 numtexgens, const char* qualifier = nullptr)
{
	DefineVSOutputStructMember<api_type>(object, qualifier, "float4", "pos", "", -1, api_type == API_D3D11 ? "SV_Position" : "POSITION");
	DefineVSOutputStructMember<api_type>(object, qualifier, "float4", "colors_", "", 0, "COLOR", 0);
	DefineVSOutputStructMember<api_type>(object, qualifier, "float4", "colors_", "", 1, "COLOR", 1);

	if (numtexgens < 7)
	{
		for (unsigned int i = 0; i < numtexgens; ++i)
			DefineVSOutputStructMember<api_type>(object, qualifier, "float3", "tex", "", i, "TEXCOORD", i);
		const char * sufix = (api_type == API_OPENGL || api_type == API_VULKAN) ? "_2" : "";
		DefineVSOutputStructMember<api_type>(object, qualifier, "float4", "clipPos", sufix, -1, "TEXCOORD", numtexgens);

		if (enable_pl)
			DefineVSOutputStructMember<api_type>(object, qualifier, "float4", "Normal", sufix, -1, "TEXCOORD", numtexgens + 1);
	}
	else
	{
		// Store clip position in the w component of first 4 texcoords
		int num_texcoords = enable_pl ? 8 : numtexgens;
		for (int i = 0; i < num_texcoords; ++i)
			DefineVSOutputStructMember<api_type>(object, qualifier, (enable_pl || i < 4) ? "float4" : "float3", "tex", "", i, "TEXCOORD", i);
	}
	if (g_ActiveConfig.backend_info.bSupportsDepthClamp)
	{
		DefineVSOutputStructMember<api_type>(object, qualifier, "float2", "clipDist", "", -1, "SV_ClipDistance", 0);
	}
}

template<API_TYPE api_type>
inline void AssignVSOutputMembers(ShaderCode& object, const char* a, const char* b, bool enable_pl, u32 numtexgens)
{
	object.Write("\t%s.pos = %s.pos;\n", a, b);
	object.Write("\t%s.colors_0 = %s.colors_0;\n", a, b);
	object.Write("\t%s.colors_1 = %s.colors_1;\n", a, b);

	if (numtexgens < 7)
	{
		for (unsigned int i = 0; i < numtexgens; ++i)
			object.Write("\t%s.tex%d = %s.tex%d;\n", a, i, b, i);
		const char * sufix = (api_type == API_OPENGL || api_type == API_VULKAN) ? "_2" : "";
		object.Write("\t%s.clipPos%s = %s.clipPos%s;\n", a, sufix, b, sufix);

		if (enable_pl)
			object.Write("\t%s.Normal%s = %s.Normal%s;\n", a, sufix, b, sufix);
	}
	else
	{
		// Store clip position in the w component of first 4 texcoords
		int num_texcoords = enable_pl ? 8 : numtexgens;
		for (int i = 0; i < num_texcoords; ++i)
			object.Write("\t%s.tex%d = %s.tex%d;\n", a, i, b, i);
	}
	if (g_ActiveConfig.backend_info.bSupportsDepthClamp)
	{
		object.Write("\t%s.clipDist.x = %s.clipDist.x;\n", a, b);
		object.Write("\t%s.clipDist.y = %s.clipDist.y;\n", a, b);
	}
}

// We use the flag "centroid" to fix some MSAA rendering bugs. With MSAA, the
// pixel shader will be executed for each pixel which has at least one passed sample.
// So there may be rendered pixels where the center of the pixel isn't in the primitive.
// As the pixel shader usually renders at the center of the pixel, this position may be
// outside the primitive. This will lead to sampling outside the texture, sign changes, ...
// As a workaround, we interpolate at the centroid of the coveraged pixel, which
// is always inside the primitive.
// Without MSAA, this flag is defined to have no effect.
inline const char* GetInterpolationQualifier(API_TYPE api_type, bool msaa, bool ssaa, bool in = true, bool in_out = false)
{
	if (!msaa || (api_type & API_D3D9))
		return "";

	if (!ssaa)
	{
		if (in_out && (api_type == API_OPENGL || api_type == API_VULKAN) && !g_ActiveConfig.backend_info.bSupportsBindingLayout)
			return in ? "centroid in" : "centroid out";
		return "centroid";
	}

	return "sample";
}
