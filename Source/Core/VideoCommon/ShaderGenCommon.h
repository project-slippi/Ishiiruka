// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <algorithm>

#include "Common/CommonTypes.h"
#include "Common/Hash.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

/**
* Common interface for classes that need to go through the shader generation path (GenerateVertexShader, GeneratePixelShader)
* In particular, this includes the shader code generator (ShaderCode).
* A different class (ShaderUid) can be used to uniquely identify each ShaderCode object.
* More interesting things can be done with this, e.g. ShaderConstantProfile checks what shader constants are being used. This can be used to optimize buffer management.
* Each of the ShaderCode, ShaderUid and ShaderConstantProfile child classes only implement the subset of ShaderGeneratorInterface methods that are required for the specific tasks.
*/
class ShaderGeneratorInterface
{
public:
	/*
	* Clears The UID data to its Default Value
	*/
	void ClearUID(){}
	/*
	* Calculates UID data Hash
	*/
	void CalculateUIDHash(){ }
	/*
	* Used when the shader generator would write a piece of ShaderCode.
	* Can be used like printf.
	* @note In the ShaderCode implementation, this does indeed write the parameter string to an internal buffer. However, you're free to do whatever you like with the parameter.
	*/
	void Write(const char* fmt, ...) {}

	/*
	* Returns a read pointer to the internal buffer.
	* @note When implementing this method in a child class, you likely want to return the argument of the last SetBuffer call here
	* @note SetBuffer() should be called before using GetBuffer().
	*/
	char* GetBuffer() { return NULL; }

	/*
	* Can be used to give the object a place to write to. This should be called before using Write().
	* @param buffer pointer to a char buffer that the object can write to
	*/
	void SetBuffer(char* buffer) { }

	/*
	* Returns a pointer to an internally stored object of the uid_data type.
	* @warning since most child classes use the default implementation you shouldn't access this directly without adding precautions against NULL access (e.g. via adding a dummy structure, cf. the vertex/pixel shader generators)
	*/
	template<class uid_data>
	uid_data& GetUidData() { return *(uid_data*)NULL; }
};

/**
* Shader UID class used to uniquely identify the ShaderCode output written in the shader generator.
* uid_data can be any struct of parameters that uniquely identify each shader code output.
* Unless performance is not an issue, uid_data should be tightly packed to reduce memory footprint.
* Shader generators will write to specific uid_data fields; ShaderUid methods will only read raw u32 values from a union.
*/
template<class uid_data>
class ShaderUid : public ShaderGeneratorInterface
{
public:
	ShaderUid() : HASH(0){}

	void ClearUID()
	{
		memset(values, 0, sizeof(uid_data));
	}

	void CalculateUIDHash()
	{
		if (HASH == 0)
		{
			HASH = (std::size_t)HashAdler32(values + data.StartValue(), data.NumValues());
		}
	}

	bool operator == (const ShaderUid& obj) const
	{
		return memcmp(this->values + data.StartValue(), obj.values + data.StartValue(), data.NumValues() * sizeof(*values)) == 0;
	}

	bool operator != (const ShaderUid& obj) const
	{
		return memcmp(this->values + data.StartValue(), obj.values + data.StartValue(), data.NumValues() * sizeof(*values)) != 0;
	}

	// determines the storage order inside STL containers
	bool operator < (const ShaderUid& obj) const
	{
		return memcmp(this->values + data.StartValue(), obj.values + data.StartValue(), data.NumValues() * sizeof(*values)) < 0;
	}

	template<class T>
	inline T& GetUidData() { return data; }

	const uid_data& GetUidData() const { return data; }
	size_t GetUidDataSize() const { return sizeof(uid_data); }
	struct ShaderUidHasher
	{
		std::size_t operator()(const ShaderUid<uid_data>& k) const
		{
			return k.HASH;
		}
	};
private:
	union
	{
		uid_data data;
		u8 values[sizeof(uid_data)];
	};
	std::size_t HASH;
};



class ShaderCode : public ShaderGeneratorInterface
{
public:
	ShaderCode() : buf(NULL), write_ptr(NULL)
	{

	}

	void ClearUID(){}

	void CalculateUIDHash(){ }

	void Write(const char* fmt, ...)
	{
		va_list arglist;
		va_start(arglist, fmt);
		write_ptr += vsprintf(write_ptr, fmt, arglist);
		va_end(arglist);
	}

	char* GetBuffer() { return buf; }
	void SetBuffer(char* buffer) { buf = buffer; write_ptr = buffer; }
	ptrdiff_t BufferSize() { return write_ptr - buf; }
private:
	char* buf;
	char* write_ptr;
};

template<class T, API_TYPE api_type>
static inline void WriteRegister(T& object, const char *prefix, const u32 num)
{
	if (api_type == API_OPENGL || api_type == API_D3D11)
		return; // Nothing to do here

	object.Write(" : register(%s%d)", prefix, num);
}

template<class T, API_TYPE api_type>
static inline void WriteLocation(T& object)
{
	if (api_type == API_OPENGL || api_type == API_D3D11)
		return;

	object.Write("uniform ");
}

template<class T, API_TYPE api_type>
static inline void DeclareUniform(T& object, const u32 num, const char* type, const char* name)
{
	WriteLocation<T, api_type>(object);
	object.Write("%s %s ", type, name);
	WriteRegister<T, api_type>(object, "c", num);
	object.Write(";\n");
}

/**
* Checks if there has been
*/
template<class UidT, class CodeT>
class UidChecker
{
public:
	void Invalidate()
	{
		m_shaders.clear();
		m_uids.clear();
	}

	void AddToIndexAndCheck(CodeT& new_code, const UidT& new_uid, const char* shader_type, const char* dump_prefix)
	{
		bool uid_is_indexed = std::find(m_uids.begin(), m_uids.end(), new_uid) != m_uids.end();
		if (!uid_is_indexed)
		{
			m_uids.push_back(new_uid);
			m_shaders[new_uid] = new_code.GetBuffer();
		}
		else
		{
			// uid is already in the index => check if there's a shader with the same uid but different code
			auto& old_code = m_shaders[new_uid];
			if (strcmp(old_code.c_str(), new_code.GetBuffer()) != 0)
			{
				static int num_failures = 0;

				char szTemp[MAX_PATH];
				sprintf(szTemp, "%s%ssuid_mismatch_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(),
					dump_prefix,
					++num_failures);

				// TODO: Should also dump uids
				std::ofstream file;
				OpenFStream(file, szTemp, std::ios_base::out);
				file << "Old shader code:\n" << old_code;
				file << "\n\nNew shader code:\n" << new_code.GetBuffer();
				file << "\n\nShader uid:\n";
				for (unsigned int i = 0; i < new_uid.GetUidDataSize(); ++i)
				{
					u32 value = ((u32*)&new_uid.GetUidData())[i];
					if ((i % 4) == 0)
					{
						auto last_value = (i + 3 < new_uid.GetUidDataSize() - 1) ? i + 3 : new_uid.GetUidDataSize();
						file << std::setfill(' ') << std::dec;
						file << "Values " << std::setw(2) << i << " - " << last_value << ": ";
					}

					file << std::setw(8) << std::setfill('0') << std::hex << value << std::setw(1);
					if ((i % 4) < 3)
						file << ' ';
					else
						file << std::endl;
				}
				file.close();

				ERROR_LOG(VIDEO, "%s shader uid mismatch! See %s for details", shader_type, szTemp);
			}
		}
	}

private:
	std::map<UidT, std::string> m_shaders;
	std::vector<UidT> m_uids;
};


template<class T, API_TYPE api_type>
void DefineVSOutputStructMember(T& object, const char* qualifier, const char* type, const char* name, const char* sufix, int var_index, const char* semantic, int semantic_index = -1)
{
	if (qualifier != nullptr)
		object.Write("\t%s %s %s%s", qualifier, type, name, sufix);
	else
		object.Write("\t%s %s%s", type, name, sufix);

	if (var_index != -1)
		object.Write("%d", var_index);

	if (api_type == API_OPENGL)
		object.Write(";\n");
	else
	{
		if (semantic_index != -1)
			object.Write(" : %s%d;\n", semantic, semantic_index);
		else
			object.Write(" : %s;\n", semantic);
	}
}

template<class T, API_TYPE api_type>
inline void GenerateVSOutputMembers(T& object, bool enable_pl, const XFMemory &xfr, const char* qualifier = nullptr)
{
	DefineVSOutputStructMember<T, api_type>(object, qualifier, "float4", "pos", "", -1, api_type == API_D3D11 ? "SV_Position" : "POSITION");
	DefineVSOutputStructMember<T, api_type>(object, qualifier, "float4", "colors_", "", 0, "COLOR", 0);
	DefineVSOutputStructMember<T, api_type>(object, qualifier, "float4", "colors_", "", 1, "COLOR", 1);

	if (xfr.numTexGen.numTexGens < 7)
	{
		for (unsigned int i = 0; i < xfr.numTexGen.numTexGens; ++i)
			DefineVSOutputStructMember<T, api_type>(object, qualifier, "float3", "tex", "", i, "TEXCOORD", i);
		const char * sufix = (api_type == API_OPENGL) ? "_2" : "";
		DefineVSOutputStructMember<T, api_type>(object, qualifier, "float4", "clipPos", sufix, -1, "TEXCOORD", xfr.numTexGen.numTexGens);

		if (enable_pl)
			DefineVSOutputStructMember<T, api_type>(object, qualifier, "float4", "Normal", sufix, -1, "TEXCOORD", xfr.numTexGen.numTexGens + 1);
	}
	else
	{
		// Store clip position in the w component of first 4 texcoords
		int num_texcoords = enable_pl ? 8 : xfr.numTexGen.numTexGens;
		for (int i = 0; i < num_texcoords; ++i)
			DefineVSOutputStructMember<T, api_type>(object, qualifier, (enable_pl || i < 4) ? "float4" : "float3", "tex", "", i, "TEXCOORD", i);
	}
}

template<class T, API_TYPE api_type>
static inline void AssignVSOutputMembers(T& object, const char* a, const char* b, bool enable_pl, const XFMemory &xfr)
{
	object.Write("\t%s.pos = %s.pos;\n", a, b);
	object.Write("\t%s.colors_0 = %s.colors_0;\n", a, b);
	object.Write("\t%s.colors_1 = %s.colors_1;\n", a, b);

	if (xfr.numTexGen.numTexGens < 7)
	{
		for (unsigned int i = 0; i < xfr.numTexGen.numTexGens; ++i)
			object.Write("\t%s.tex%d = %s.tex%d;\n", a, i, b, i);
		const char * sufix = (api_type == API_OPENGL) ? "_2" : "";
		object.Write("\t%s.clipPos%s = %s.clipPos%s;\n", a, sufix, b, sufix);

		if (enable_pl)
			object.Write("\t%s.Normal%s = %s.Normal%s;\n", a, sufix, b, sufix);
	}
	else
	{
		// Store clip position in the w component of first 4 texcoords
		int num_texcoords = enable_pl ? 8 : xfr.numTexGen.numTexGens;
		for (int i = 0; i < num_texcoords; ++i)
			object.Write("\t%s.tex%d = %s.tex%d;\n", a, i, b, i);
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
static inline const char* GetInterpolationQualifier(API_TYPE api_type, bool in = true, bool in_out = false)
{
	if (!(g_ActiveConfig.iMultisamples <= 1) || (api_type & API_D3D9))
		return "";

	if (!g_ActiveConfig.bSSAA)
	{
		if (in_out && api_type == API_OPENGL && !g_ActiveConfig.backend_info.bSupportsBindingLayout)
			return in ? "centroid in" : "centroid out";
		return "centroid";
	}

	return "sample";
}
