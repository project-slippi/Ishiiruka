// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <list>
#include <cctype>
#include <list>
#include <string>

#include "Common/Align.h"

#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DShader.h"
#include "VideoBackends/DX11/D3DState.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/FramebufferManager.h"
#include "VideoBackends/DX11/GeometryShaderCache.h"
#include "VideoBackends/DX11/PixelShaderCache.h"
#include "VideoBackends/DX11/VertexShaderCache.h"

#include "VideoCommon/VideoConfig.h"

namespace DX11
{

namespace D3D
{

UtilVertexBuffer::UtilVertexBuffer(u32 size) : m_buf(nullptr), m_offset(0), m_max_size(size)
{
	D3D11_BUFFER_DESC desc = CD3D11_BUFFER_DESC(m_max_size, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	device->CreateBuffer(&desc, nullptr, &m_buf);
}

UtilVertexBuffer::~UtilVertexBuffer()
{
	m_buf->Release();
}
// returns vertex offset to the new data
int UtilVertexBuffer::AppendData(void* data, u32 size, u32 vertex_size)
{
	_dbg_assert_(VIDEO, size < m_max_size);

	D3D11_MAPPED_SUBRESOURCE map;
	u32 aligned_offset = Common::AlignUp(m_offset, vertex_size); // align offset to vertex_size bytes
	if (aligned_offset + size > m_max_size)
	{
		// wrap buffer around and notify observers
		aligned_offset = 0;
		context->Map(m_buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);

		for (bool* observer : m_observers)
			*observer = true;
	}
	else
	{
		context->Map(m_buf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
	}
	memcpy((u8*)map.pData + aligned_offset, data, size);
	context->Unmap(m_buf, 0);

	m_offset = aligned_offset + size;
	return aligned_offset / vertex_size;
}

int UtilVertexBuffer::BeginAppendData(void** write_ptr, u32 size, u32 vertex_size)
{
	_dbg_assert_(VIDEO, size < m_max_size);

	D3D11_MAPPED_SUBRESOURCE map;
	u32 aligned_offset = Common::AlignUp(m_offset, vertex_size); // align offset to vertex_size bytes
	if (aligned_offset + size > m_max_size)
	{
		// wrap buffer around and notify observers
		aligned_offset = 0;
		context->Map(m_buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);

		for (bool* observer : m_observers)
			*observer = true;
	}
	else
	{
		context->Map(m_buf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
	}

	*write_ptr = reinterpret_cast<byte*>(map.pData) + aligned_offset;
	m_offset = aligned_offset + size;
	return aligned_offset / vertex_size;
}

void UtilVertexBuffer::EndAppendData()
{
	context->Unmap(m_buf, 0);
}

void UtilVertexBuffer::AddWrapObserver(bool* observer)
{
	m_observers.push_back(observer);
}


ConstantStreamBuffer::ConstantStreamBuffer(u32 size) : m_max_size(Common::AlignUpSizePow2(size, 256)), m_need_init(true)
{
	m_use_partial_buffer_update = D3D::SupportPartialContantBufferUpdate();
	D3D11_BUFFER_DESC desc = CD3D11_BUFFER_DESC(m_max_size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	device->CreateBuffer(&desc, nullptr, &m_buf);
}

ConstantStreamBuffer::~ConstantStreamBuffer()
{
	m_buf->Release();
}

// returns vertex offset to the new data
void ConstantStreamBuffer::AppendData(void* data, u32 size)
{
	D3D11_MAPPED_SUBRESOURCE map;
	m_offset = Common::AlignUpSizePow2(m_offset, 256); // align offset to 256 bytes (16 units) as requested by microsoft documentation
	if (m_offset + size >= m_max_size || m_need_init)
	{
		// wrap buffer around
		m_offset = 0;
		context->Map(m_buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		m_need_init = !m_use_partial_buffer_update;
	}
	else
	{
		context->Map(m_buf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
	}
	memcpy((u8*)map.pData + m_offset, data, size);
	context->Unmap(m_buf, 0);
	m_current_size = size;
	m_offset += size;
}

CD3DFont font;
UtilVertexBuffer* util_vbuf = nullptr;

#define MAX_NUM_VERTICES 50*6
struct FONT2DVERTEX
{
	float x, y, z;
	float col[4];
	float tu, tv;
};

inline FONT2DVERTEX InitFont2DVertex(float x, float y, u32 color, float tu, float tv)
{
	FONT2DVERTEX v;   v.x = x; v.y = y; v.z = 0;  v.tu = tu; v.tv = tv;
	v.col[0] = ((float)((color >> 16) & 0xFF)) / 255.f;
	v.col[1] = ((float)((color >> 8) & 0xFF)) / 255.f;
	v.col[2] = ((float)((color >> 0) & 0xFF)) / 255.f;
	v.col[3] = ((float)((color >> 24) & 0xFF)) / 255.f;
	return v;
}

CD3DFont::CD3DFont() : m_dwTexWidth(512), m_dwTexHeight(512)
{

}

const char fontpixshader[] = {
	"Texture2D tex2D : register(t8);\n"
	"SamplerState linearSampler\n"
	"{\n"
	"	Filter = MIN_MAG_MIP_LINEAR;\n"
	"	AddressU = D3D11_TEXTURE_ADDRESS_BORDER;\n"
	"	AddressV = D3D11_TEXTURE_ADDRESS_BORDER;\n"
	"	BorderColor = float4(0.f, 0.f, 0.f, 0.f);\n"
	"};\n"
	"struct PS_INPUT\n"
	"{\n"
	"	float4 pos : SV_POSITION;\n"
	"	float4 col : COLOR;\n"
	"	float2 tex : TEXCOORD;\n"
	"};\n"
	"float4 main( PS_INPUT input ) : SV_Target\n"
	"{\n"
	"	return tex2D.Sample( linearSampler, input.tex ) * input.col;\n"
	"};\n"
};

const char fontvertshader[] = {
	"struct VS_INPUT\n"
	"{\n"
	"	float4 pos : POSITION;\n"
	"	float4 col : COLOR;\n"
	"	float2 tex : TEXCOORD;\n"
	"};\n"
	"struct PS_INPUT\n"
	"{\n"
	"	float4 pos : SV_POSITION;\n"
	"	float4 col : COLOR;\n"
	"	float2 tex : TEXCOORD;\n"
	"};\n"
	"PS_INPUT main( VS_INPUT input )\n"
	"{\n"
	"	PS_INPUT output;\n"
	"	output.pos = input.pos;\n"
	"	output.col = input.col;\n"
	"	output.tex = input.tex;\n"
	"	return output;\n"
	"};\n"
};

int CD3DFont::Init()
{
	// Create vertex buffer for the letters
	HRESULT hr;

	// Prepare to create a bitmap
	unsigned int* pBitmapBits;
	BITMAPINFO bmi;
	ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = (int)m_dwTexWidth;
	bmi.bmiHeader.biHeight = -(int)m_dwTexHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount = 32;

	// Create a DC and a bitmap for the font
	HDC hDC = CreateCompatibleDC(nullptr);
	HBITMAP hbmBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, (void**)&pBitmapBits, nullptr, 0);
	SetMapMode(hDC, MM_TEXT);

	// create a GDI font
	HFONT hFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE,
		FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
		VARIABLE_PITCH, _T("Tahoma"));
	if (nullptr == hFont) return E_FAIL;

	HGDIOBJ hOldbmBitmap = SelectObject(hDC, hbmBitmap);
	HGDIOBJ hOldFont = SelectObject(hDC, hFont);

	// Set text properties
	SetTextColor(hDC, 0xFFFFFF);
	SetBkColor(hDC, 0);
	SetTextAlign(hDC, TA_TOP);

	TEXTMETRICW tm;
	GetTextMetricsW(hDC, &tm);
	m_LineHeight = tm.tmHeight;

	// Loop through all printable characters and output them to the bitmap
	// Meanwhile, keep track of the corresponding tex coords for each character.
	int x = 0, y = 0;
	char str[2] = "\0";
	for (int c = 0; c < 127 - 32; c++)
	{
		str[0] = c + 32;
		SIZE size;
		GetTextExtentPoint32A(hDC, str, 1, &size);
		if ((int)(x + size.cx + 1) > m_dwTexWidth)
		{
			x = 0;
			y += m_LineHeight;
		}

		ExtTextOutA(hDC, x + 1, y + 0, ETO_OPAQUE | ETO_CLIPPED, nullptr, str, 1, nullptr);
		m_fTexCoords[c][0] = ((float)(x + 0)) / m_dwTexWidth;
		m_fTexCoords[c][1] = ((float)(y + 0)) / m_dwTexHeight;
		m_fTexCoords[c][2] = ((float)(x + 0 + size.cx)) / m_dwTexWidth;
		m_fTexCoords[c][3] = ((float)(y + 0 + size.cy)) / m_dwTexHeight;

		x += size.cx + 3;  // 3 to work around annoying ij conflict (part of the j ends up with the i)
	}

	// Create a new texture for the font
	// possible optimization: store the converted data in a buffer and fill the texture on creation.
	//							That way, we can use a static texture
	ID3D11Texture2D* buftex;
	D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(D3D::GetBaseBufferFormat(), m_dwTexWidth, m_dwTexHeight,
		1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC,
		D3D11_CPU_ACCESS_WRITE);
	hr = device->CreateTexture2D(&texdesc, nullptr, &buftex);
	if (FAILED(hr))
	{
		PanicAlert("Failed to create font texture");
		return hr;
	}
	D3D::SetDebugObjectName(buftex, "texture of a CD3DFont object");

	// Lock the surface and write the alpha values for the set pixels
	D3D11_MAPPED_SUBRESOURCE texmap;
	hr = context->Map(buftex, 0, D3D11_MAP_WRITE_DISCARD, 0, &texmap);
	if (FAILED(hr)) PanicAlert("Failed to map a texture at %s %d\n", __FILE__, __LINE__);

	for (y = 0; y < m_dwTexHeight; y++)
	{
		u32* pDst32 = (u32*)((u8*)texmap.pData + y * texmap.RowPitch);
		for (x = 0; x < m_dwTexWidth; x++)
		{
			const u8 bAlpha = (pBitmapBits[m_dwTexWidth * y + x] & 0xff);
			*pDst32++ = (((bAlpha << 4) | bAlpha) << 24) | 0xFFFFFF;
		}
	}

	// Done updating texture, so clean up used objects
	context->Unmap(buftex, 0);
	hr = D3D::device->CreateShaderResourceView(buftex, nullptr, ToAddr(m_pTexture));
	if (FAILED(hr)) PanicAlert("Failed to create shader resource view at %s %d\n", __FILE__, __LINE__);
	SAFE_RELEASE(buftex);

	SelectObject(hDC, hOldbmBitmap);
	DeleteObject(hbmBitmap);

	SelectObject(hDC, hOldFont);
	DeleteObject(hFont);

	// setup device objects for drawing
	m_pshader = D3D::CompileAndCreatePixelShader(fontpixshader);
	if (m_pshader == nullptr) PanicAlert("Failed to create pixel shader, %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName(m_pshader.get(), "pixel shader of a CD3DFont object");

	D3DBlob vsbytecode;
	D3D::CompileShader(DX11::D3D::ShaderType::Vertex, fontvertshader, vsbytecode);
	if (vsbytecode.Data() == nullptr) PanicAlert("Failed to compile vertex shader, %s %d\n", __FILE__, __LINE__);
	m_vshader = D3D::CreateVertexShaderFromByteCode(vsbytecode);
	if (m_vshader.get() == nullptr) PanicAlert("Failed to create vertex shader, %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName(m_vshader.get(), "vertex shader of a CD3DFont object");

	const D3D11_INPUT_ELEMENT_DESC desc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = D3D::device->CreateInputLayout(desc, 3, vsbytecode.Data(), vsbytecode.Size(), ToAddr(m_InputLayout));
	if (FAILED(hr)) PanicAlert("Failed to create input layout, %s %d\n", __FILE__, __LINE__);

	D3D11_BLEND_DESC blenddesc;
	blenddesc.AlphaToCoverageEnable = FALSE;
	blenddesc.IndependentBlendEnable = FALSE;
	blenddesc.RenderTarget[0].BlendEnable = TRUE;
	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blenddesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blenddesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blenddesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blenddesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	hr = D3D::device->CreateBlendState(&blenddesc, ToAddr(m_blendstate));
	CHECK(hr == S_OK, "Create font blend state");
	D3D::SetDebugObjectName(m_blendstate.get(), "blend state of a CD3DFont object");

	D3D11_RASTERIZER_DESC rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0.f, false, false, false, false);
	hr = D3D::device->CreateRasterizerState(&rastdesc, ToAddr(m_raststate));
	CHECK(hr == S_OK, "Create font rasterizer state");
	D3D::SetDebugObjectName(m_raststate.get(), "rasterizer state of a CD3DFont object");

	D3D11_BUFFER_DESC vbdesc = CD3D11_BUFFER_DESC(MAX_NUM_VERTICES * sizeof(FONT2DVERTEX), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	if (FAILED(hr = device->CreateBuffer(&vbdesc, nullptr, ToAddr(m_pVB))))
	{
		PanicAlert("Failed to create font vertex buffer at %s, line %d\n", __FILE__, __LINE__);
		return hr;
	}
	D3D::SetDebugObjectName(m_pVB.get(), "vertex buffer of a CD3DFont object");
	return S_OK;
}

int CD3DFont::Shutdown()
{
	m_pTexture.reset();
	m_pVB.reset();
	m_InputLayout.reset();
	m_pshader.reset();
	m_vshader.reset();
	m_blendstate.reset();
	m_raststate.reset();
	return S_OK;
}

int CD3DFont::DrawTextScaled(float x, float y, float size, float spacing, u32 dwColor, const std::string& Text, float scalex, float scaley)
{
	if (!m_pVB)
		return 0;

	UINT stride = sizeof(FONT2DVERTEX);
	UINT bufoffset = 0;

	float sizeratio = size / (float)m_LineHeight;

	// translate starting positions
	float sx = x * scalex - 1.f;
	float sy = 1.f - y * scaley;

	// Fill vertex buffer
	FONT2DVERTEX* pVertices;
	int dwNumTriangles = 0L;

	D3D11_MAPPED_SUBRESOURCE vbmap;
	HRESULT hr = context->Map(m_pVB.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &vbmap);
	if (FAILED(hr)) PanicAlert("Mapping vertex buffer failed, %s %d\n", __FILE__, __LINE__);
	pVertices = (D3D::FONT2DVERTEX*)vbmap.pData;

	// set general pipeline state
	D3D::stateman->PushBlendState(m_blendstate.get());
	D3D::stateman->PushRasterizerState(m_raststate.get());

	D3D::stateman->SetPixelShader(m_pshader.get());
	D3D::stateman->SetVertexShader(m_vshader.get());
	D3D::stateman->SetGeometryShader(nullptr);


	D3D::stateman->SetInputLayout(m_InputLayout.get());
	D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	D3D::stateman->SetTexture(8, m_pTexture.get());

	float fStartX = sx;
	for (char c : Text)
	{
		if (c == ('\n'))
		{
			sx = fStartX;
			sy -= scaley * size;
		}
		if (c < (' '))
			continue;

		c -= 32;
		float tx1 = m_fTexCoords[c][0];
		float ty1 = m_fTexCoords[c][1];
		float tx2 = m_fTexCoords[c][2];
		float ty2 = m_fTexCoords[c][3];

		float w = (float)(tx2 - tx1) * m_dwTexWidth * scalex * sizeratio;
		float h = (float)(ty1 - ty2) * m_dwTexHeight * scaley * sizeratio;

		FONT2DVERTEX v[6];
		v[0] = InitFont2DVertex(sx, sy + h, dwColor, tx1, ty2);
		v[1] = InitFont2DVertex(sx, sy, dwColor, tx1, ty1);
		v[2] = InitFont2DVertex(sx + w, sy + h, dwColor, tx2, ty2);
		v[3] = InitFont2DVertex(sx + w, sy, dwColor, tx2, ty1);
		v[4] = v[2];
		v[5] = v[1];

		memcpy(pVertices, v, 6 * sizeof(FONT2DVERTEX));

		pVertices += 6;
		dwNumTriangles += 2;

		if (dwNumTriangles * 3 > (MAX_NUM_VERTICES - 6))
		{
			context->Unmap(m_pVB.get(), 0);

			D3D::stateman->SetVertexBuffer(m_pVB.get(), stride, bufoffset);

			D3D::stateman->Apply();
			D3D::context->Draw(3 * dwNumTriangles, 0);

			dwNumTriangles = 0;
			D3D11_MAPPED_SUBRESOURCE _vbmap;
			hr = context->Map(m_pVB.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &_vbmap);
			if (FAILED(hr)) PanicAlert("Mapping vertex buffer failed, %s %d\n", __FILE__, __LINE__);
			pVertices = (D3D::FONT2DVERTEX*)_vbmap.pData;
		}
		sx += w + spacing * scalex * size;
	}

	// Unlock and render the vertex buffer
	context->Unmap(m_pVB.get(), 0);
	if (dwNumTriangles > 0)
	{
		D3D::stateman->SetVertexBuffer(m_pVB.get(), stride, bufoffset);

		D3D::stateman->Apply();
		D3D::context->Draw(3 * dwNumTriangles, 0);
	}
	D3D::stateman->PopBlendState();
	D3D::stateman->PopRasterizerState();
	D3D::stateman->Apply();
	return S_OK;
}

SamplerStatePtr linear_copy_sampler;
SamplerStatePtr point_copy_sampler;

struct STQVertex
{
	float x, y, z, u0, v0, s, u1, v1, g;
};
struct ColVertex
{
	float x, y, z; u32 col;
};

struct
{
	float u1, v1, u2, v2, S, u3, v3, G;
} tex_quad_data;

struct
{
	float x1, y1, x2, y2, z;
	u32 col;
} draw_quad_data;

struct
{
	u32 col;
	float z;
} clear_quad_data;

// ring buffer offsets
int stq_offset, stsq_offset, cq_offset, clearq_offset;

// observer variables for ring buffer wraps
bool stq_observer, stsq_observer, cq_observer, clearq_observer;

void InitUtils()
{
	util_vbuf = new UtilVertexBuffer(65535);

	float border[4] = { 0.f, 0.f, 0.f, 0.f };
	D3D11_SAMPLER_DESC samDesc = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, 0.f, 1, D3D11_COMPARISON_ALWAYS, border, 0.f, 0.f);
	HRESULT hr = D3D::device->CreateSamplerState(&samDesc, ToAddr(point_copy_sampler));
	if (FAILED(hr)) PanicAlert("Failed to create sampler state at %s %d\n", __FILE__, __LINE__);
	else SetDebugObjectName(point_copy_sampler.get(), "point copy sampler state");

	samDesc = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER, 0.f, 1, D3D11_COMPARISON_ALWAYS, border, 0.f, 0.f);
	hr = D3D::device->CreateSamplerState(&samDesc, ToAddr(linear_copy_sampler));
	if (FAILED(hr)) PanicAlert("Failed to create sampler state at %s %d\n", __FILE__, __LINE__);
	else SetDebugObjectName(linear_copy_sampler.get(), "linear copy sampler state");

	// cached data used to avoid unnecessarily reloading the vertex buffers
	memset(&tex_quad_data, 0, sizeof(tex_quad_data));
	memset(&draw_quad_data, 0, sizeof(draw_quad_data));
	memset(&clear_quad_data, 0, sizeof(clear_quad_data));

	// make sure to properly load the vertex data whenever the corresponding functions get called the first time
	stq_observer = stsq_observer = cq_observer = clearq_observer = true;
	util_vbuf->AddWrapObserver(&stq_observer);
	util_vbuf->AddWrapObserver(&stsq_observer);
	util_vbuf->AddWrapObserver(&cq_observer);
	util_vbuf->AddWrapObserver(&clearq_observer);

	font.Init();
}

void ShutdownUtils()
{
	linear_copy_sampler.reset();
	point_copy_sampler.reset();
	font.Shutdown();
	SAFE_DELETE(util_vbuf);
}

void SetPointCopySampler()
{
	D3D::stateman->SetSampler(0, point_copy_sampler.get());
}

void SetLinearCopySampler()
{
	D3D::stateman->SetSampler(0, linear_copy_sampler.get());
}

ID3D11SamplerState* GetPointCopySampler()
{
	return point_copy_sampler.get();
}

ID3D11SamplerState* GetLinearCopySampler()
{
	return linear_copy_sampler.get();
}

void drawShadedTexQuad(
	ID3D11ShaderResourceView* texture,
	const D3D11_RECT* rSource,
	int SourceWidth,
	int SourceHeight,
	ID3D11PixelShader* PShader,
	ID3D11VertexShader* VShader,
	ID3D11InputLayout* layout,
	ID3D11GeometryShader* GShader,
	float Gamma,
	u32 slice,
	int DestWidth,
	int DestHeight)
{
	float sw = 1.0f / (float)SourceWidth;
	float sh = 1.0f / (float)SourceHeight;
	float dw = 1.0f / (float)SourceWidth;
	float dh = 1.0f / (float)SourceHeight;
	float u1 = 0.f;
	float u2 = 1.f;
	float v1 = 0.f;
	float v2 = 1.f;
	if (rSource != nullptr)
	{
		u1 = ((float)rSource->left) * sw;
		u2 = ((float)rSource->right) * sw;
		v1 = ((float)rSource->top) * sh;
		v2 = ((float)rSource->bottom) * sh;
	}
	float G = 1.0f / Gamma;
	float S = (float)slice;

	STQVertex coords[4] = {
		{ -1.0f,  1.0f, 0.0f, u1, v1, S, dw, dh, G },
		{  1.0f,  1.0f, 0.0f, u2, v1, S, dw, dh, G },
		{ -1.0f, -1.0f, 0.0f, u1, v2, S, dw, dh, G },
		{  1.0f, -1.0f, 0.0f, u2, v2, S, dw, dh, G },
	};

	// only upload the data to VRAM if it changed
	if (stq_observer ||
		tex_quad_data.u1 != u1 || tex_quad_data.v1 != v1 ||
		tex_quad_data.u2 != u2 || tex_quad_data.v2 != v2 ||
		tex_quad_data.u3 != dw || tex_quad_data.v3 != dh ||
		tex_quad_data.S != S || tex_quad_data.G != G)
	{
		stq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(STQVertex));
		stq_observer = false;

		tex_quad_data.u1 = u1;
		tex_quad_data.v1 = v1;
		tex_quad_data.u2 = u2;
		tex_quad_data.v2 = v2;
		tex_quad_data.u3 = dw;
		tex_quad_data.v3 = dh;
		tex_quad_data.S = S;
		tex_quad_data.G = G;
	}
	UINT stride = sizeof(STQVertex);
	UINT offset = 0;
	D3D::stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	D3D::stateman->SetInputLayout(layout);
	D3D::stateman->SetVertexBuffer(util_vbuf->GetBuffer(), stride, offset);
	D3D::stateman->SetPixelShader(PShader);
	D3D::stateman->SetTexture(0, texture);
	D3D::stateman->SetVertexShader(VShader);
	D3D::stateman->SetGeometryShader(GShader);
	D3D::stateman->SetHullShader(nullptr);
	D3D::stateman->SetDomainShader(nullptr);

	D3D::stateman->Apply();
	D3D::context->Draw(4, stq_offset);
	D3D::stateman->SetTexture(0, nullptr); // immediately unbind the texture
	D3D::stateman->Apply();

	stateman->SetGeometryShader(nullptr);
}

// Fills a certain area of the current render target with the specified color
// destination coordinates normalized to (-1;1)
void drawColorQuad(u32 Color, float z, float x1, float y1, float x2, float y2)
{
	ColVertex coords[4] = {
		{ x1, y2, z, Color },
		{ x2, y2, z, Color },
		{ x1, y1, z, Color },
		{ x2, y1, z, Color },
	};

	if (cq_observer ||
		draw_quad_data.x1 != x1 || draw_quad_data.y1 != y1 ||
		draw_quad_data.x2 != x2 || draw_quad_data.y2 != y2 ||
		draw_quad_data.col != Color || draw_quad_data.z != z)
	{
		cq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(ColVertex));
		cq_observer = false;

		draw_quad_data.x1 = x1;
		draw_quad_data.y1 = y1;
		draw_quad_data.x2 = x2;
		draw_quad_data.y2 = y2;
		draw_quad_data.col = Color;
		draw_quad_data.z = z;
	}

	stateman->SetVertexShader(VertexShaderCache::GetClearVertexShader());
	stateman->SetGeometryShader(GeometryShaderCache::GetClearGeometryShader());
	stateman->SetPixelShader(PixelShaderCache::GetClearProgram());
	D3D::stateman->SetHullShader(nullptr);
	D3D::stateman->SetDomainShader(nullptr);
	stateman->SetInputLayout(VertexShaderCache::GetClearInputLayout());

	UINT stride = sizeof(ColVertex);
	UINT offset = 0;
	stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	stateman->SetVertexBuffer(util_vbuf->GetBuffer(), stride, offset);

	stateman->Apply();
	context->Draw(4, cq_offset);

	stateman->SetGeometryShader(nullptr);
}

void drawClearQuad(
	u32 Color,
	float z)
{
	ColVertex coords[4] = {
		{ -1.0f, 1.0f, z, Color },
		{ 1.0f, 1.0f, z, Color },
		{ -1.0f, -1.0f, z, Color },
		{ 1.0f, -1.0f, z, Color },
	};

	if (clearq_observer || clear_quad_data.col != Color || clear_quad_data.z != z)
	{
		clearq_offset = util_vbuf->AppendData(coords, sizeof(coords), sizeof(ColVertex));
		clearq_observer = false;

		clear_quad_data.col = Color;
		clear_quad_data.z = z;
	}

	stateman->SetVertexShader(VertexShaderCache::GetClearVertexShader());
	stateman->SetGeometryShader(GeometryShaderCache::GetClearGeometryShader());
	stateman->SetPixelShader(PixelShaderCache::GetClearProgram());
	D3D::stateman->SetHullShader(nullptr);
	D3D::stateman->SetDomainShader(nullptr);
	stateman->SetInputLayout(VertexShaderCache::GetClearInputLayout());

	UINT stride = sizeof(ColVertex);
	UINT offset = 0;
	stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	stateman->SetVertexBuffer(util_vbuf->GetBuffer(), stride, offset);

	stateman->Apply();
	context->Draw(4, clearq_offset);

	stateman->SetGeometryShader(nullptr);
}

inline void InitColVertex(ColVertex* vert, float x, float y, float z, u32 col)
{
	*vert = { x, y, z, col };
}

void DrawEFBPokeQuads(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
	const size_t COL_QUAD_SIZE = sizeof(ColVertex) * 6;

	// Set common state
	stateman->SetVertexShader(VertexShaderCache::GetClearVertexShader());
	stateman->SetGeometryShader(GeometryShaderCache::GetClearGeometryShader());
	stateman->SetPixelShader(PixelShaderCache::GetClearProgram());
	stateman->SetInputLayout(VertexShaderCache::GetClearInputLayout());
	stateman->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	stateman->SetVertexBuffer(util_vbuf->GetBuffer(), sizeof(ColVertex), 0);
	stateman->Apply();

	// if drawing a large number of points at once, this will have to be split into multiple passes.
	size_t points_per_draw = util_vbuf->GetSize() / COL_QUAD_SIZE;
	size_t current_point_index = 0;
	while (current_point_index < num_points)
	{
		size_t points_to_draw = std::min(num_points - current_point_index, points_per_draw);
		size_t required_bytes = COL_QUAD_SIZE * points_to_draw;

		// map and reserve enough buffer space for this draw
		void* buffer_ptr;
		int base_vertex_index = util_vbuf->BeginAppendData(&buffer_ptr, (int)required_bytes, sizeof(ColVertex));

		// generate quads for each efb point
		ColVertex* base_vertex_ptr = reinterpret_cast<ColVertex*>(buffer_ptr);
		for (size_t i = 0; i < points_to_draw; i++)
		{
			// generate quad from the single point (clip-space coordinates)
			const EfbPokeData point = points[current_point_index];
			float x1 = float(point.x) * 2.0f / EFB_WIDTH - 1.0f;
			float y1 = -float(point.y) * 2.0f / EFB_HEIGHT + 1.0f;
			float x2 = float(point.x + 1) * 2.0f / EFB_WIDTH - 1.0f;
			float y2 = -float(point.y + 1) * 2.0f / EFB_HEIGHT + 1.0f;
			float z = (type == EFBAccessType::PokeZ) ? (1.0f - float(point.data & 0xFFFFFF) / 16777216.0f) : 0.0f;
			u32 col = (type == EFBAccessType::PokeZ) ? 0 : RGBA8ToBGRA8(point.data);
			current_point_index++;

			// quad -> triangles
			ColVertex* vertex = &base_vertex_ptr[i * 6];
			InitColVertex(&vertex[0], x1, y1, z, col);
			InitColVertex(&vertex[1], x2, y1, z, col);
			InitColVertex(&vertex[2], x1, y2, z, col);
			InitColVertex(&vertex[3], x1, y2, z, col);
			InitColVertex(&vertex[4], x2, y1, z, col);
			InitColVertex(&vertex[5], x2, y2, z, col);

			if (type == EFBAccessType::PokeColor)
				FramebufferManager::SetEFBCachedColor(point.x, point.y, col);
			else
				FramebufferManager::SetEFBCachedDepth(point.x, point.y, z);
		}

		// unmap the util buffer, and issue the draw
		util_vbuf->EndAppendData();
		context->Draw(6 * (UINT)points_to_draw, base_vertex_index);
	}

	stateman->SetGeometryShader(GeometryShaderCache::GetClearGeometryShader());
}

}  // namespace D3D

}  // namespace DX11
