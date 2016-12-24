// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma	once
#include <string>
#include <d3d11_2.h>

#include "Common/Align.h"

#include "VideoBackends/DX11/D3DPtr.h"

#include "Common/CommonTypes.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoBackendBase.h"

namespace DX11
{

namespace D3D
{
// Font creation flags
static constexpr unsigned int D3DFONT_BOLD = 0x0001;
static constexpr unsigned int D3DFONT_ITALIC = 0x0002;

// Font rendering flags
static constexpr unsigned int D3DFONT_CENTERED = 0x0001;

class CD3DFont
{
	SrvPtr m_pTexture;
	BufferPtr m_pVB;
	InputLayoutPtr m_InputLayout;
	PixelShaderPtr m_pshader;
	VertexShaderPtr m_vshader;
	BlendStatePtr m_blendstate;
	RasterizerStatePtr m_raststate;
	const int m_dwTexWidth;
	const int m_dwTexHeight;
	unsigned int m_LineHeight;
	float m_fTexCoords[128 - 32][4];

public:
	CD3DFont();
	// 2D text drawing function
	// Initializing and destroying device-dependent objects
	int Init();
	int Shutdown();
	int DrawTextScaled(float x, float y,
		float size,
		float spacing, u32 dwColor,
		const std::string& strText, float scalex, float scaley);
};

// Ring buffer class, shared between the draw* functions
class UtilVertexBuffer
{
public:
	UtilVertexBuffer(u32 size);
	~UtilVertexBuffer();

	// returns vertex offset to the new data
	int AppendData(void* data, u32 size, u32 vertex_size);
	int BeginAppendData(void** write_ptr, u32 size, u32 vertex_size);
	void EndAppendData();
	void AddWrapObserver(bool* observer);

	inline ID3D11Buffer* &GetBuffer()
	{
		return m_buf;
	}
	inline u32 GetSize() const
	{
		return m_max_size;
	}
private:
	ID3D11Buffer* m_buf;
	u32 m_offset;
	u32 m_max_size;

	std::list<bool*> m_observers;
};

// Ring Constant buffer class, only works as a ring if
// platform supports it
class ConstantStreamBuffer
{
public:
	ConstantStreamBuffer(u32 size);
	~ConstantStreamBuffer();

	// returns vertex offset to the new data
	void AppendData(void* data, u32 size);

	inline ID3D11Buffer* &GetBuffer()
	{
		return  m_buf;
	}
	inline D3D::BufferDescriptor GetDescriptor()
	{
		return D3D::BufferDescriptor(m_buf, (m_offset - m_current_size) >> 4, Common::AlignUpSizePow2(m_current_size, 256) >> 4);
	};
private:
	ID3D11Buffer* m_buf = nullptr;
	u32 m_offset = 0;
	u32 m_max_size = 0;
	u32 m_current_size = 0;
	bool m_use_partial_buffer_update = false;
	bool m_need_init = true;
};

extern CD3DFont font;

void InitUtils();
void ShutdownUtils();

void SetPointCopySampler();
void SetLinearCopySampler();

ID3D11SamplerState* GetPointCopySampler();

ID3D11SamplerState* GetLinearCopySampler();

void drawShadedTexQuad(ID3D11ShaderResourceView* texture,
	const D3D11_RECT* rSource,
	int SourceWidth,
	int SourceHeight,
	ID3D11PixelShader* PShader,
	ID3D11VertexShader* VShader,
	ID3D11InputLayout* layout,
	ID3D11GeometryShader* GShader = nullptr,
	float Gamma = 1.0f,
	u32 slice = 0,
	int DestWidth = 1,
	int DestHeight = 1);
void drawClearQuad(u32 Color, float z);
void drawColorQuad(u32 Color, float z, float x1, float y1, float x2, float y2);
void DrawEFBPokeQuads(EFBAccessType type, const EfbPokeData* points, size_t num_points);
}

}