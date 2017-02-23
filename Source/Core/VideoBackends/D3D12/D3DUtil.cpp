// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cctype>
#include <list>
#include <string>

#include "Common/Align.h"

#include "VideoBackends/D3D12/D3DBlob.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"

#include "VideoCommon/VideoConfig.h"

namespace DX12
{

static D3D12_BLEND_DESC s_reset_blend_desc;
static D3D12_DEPTH_STENCIL_DESC s_reset_depth_desc;
static D3D12_RASTERIZER_DESC s_reset_rast_desc;

namespace D3D
{
void ResourceBarrier(ID3D12GraphicsCommandList* command_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after, UINT subresource)
{
	if (state_before == state_after)
		return;

	CHECK(resource, "NULL resource passed to ResourceBarrier.");

	D3D12_RESOURCE_BARRIER resourceBarrierDesc = {
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, // D3D12_RESOURCE_TRANSITION_BARRIER_DESC Transition
		D3D12_RESOURCE_BARRIER_FLAG_NONE,       // D3D12_RESOURCE_BARRIER_FLAGS Flags

		// D3D12_RESOURCE_TRANSITION_BARRIER_DESC Transition
		{
			resource,    // ID3D12Resource *pResource;
			subresource, // UINT Subresource;
			state_before, // UINT StateBefore;
			state_after   // UINT StateAfter;
		}
	};

	command_list->ResourceBarrier(1, &resourceBarrierDesc);
}

UtilVertexBuffer::UtilVertexBuffer(size_t size)
{
	m_stream_buffer = std::make_unique<D3DStreamBuffer>(size, size * 4, nullptr);
}

UtilVertexBuffer::~UtilVertexBuffer()
{
	m_stream_buffer.reset();
}

// returns vertex offset to the new data
size_t UtilVertexBuffer::AppendData(void* data, size_t size, size_t vertex_size)
{
	m_stream_buffer->AllocateSpaceInBuffer(size, vertex_size, false);

	memcpy(static_cast<u8*>(m_stream_buffer->GetCPUAddressOfCurrentAllocation()), data, size);

	return m_stream_buffer->GetOffsetOfCurrentAllocation() / vertex_size;
}

size_t UtilVertexBuffer::ReserveData(void** write_ptr, size_t size, size_t vertex_size)
{
	m_stream_buffer->AllocateSpaceInBuffer(size, vertex_size, false);

	*write_ptr = m_stream_buffer->GetCPUAddressOfCurrentAllocation();

	return m_stream_buffer->GetOffsetOfCurrentAllocation() / vertex_size;
}

CD3DFont font;
std::unique_ptr<UtilVertexBuffer> util_vbuf_stq;
std::unique_ptr<UtilVertexBuffer> util_vbuf_clearq;
std::unique_ptr<UtilVertexBuffer> util_vbuf_efbpokequads;

static const unsigned int s_max_num_vertices = 8000 * 6;

struct FONT2DVERTEX
{
	float x, y, z;
	float col[4];
	float tu, tv;
};

FONT2DVERTEX InitFont2DVertex(float x, float y, u32 color, float tu, float tv)
{
	FONT2DVERTEX v;   v.x = x; v.y = y; v.z = 0;  v.tu = tu; v.tv = tv;
	v.col[0] = (static_cast<float>((color >> 16) & 0xFF)) / 255.f;
	v.col[1] = (static_cast<float>((color >> 8) & 0xFF)) / 255.f;
	v.col[2] = (static_cast<float>((color >> 0) & 0xFF)) / 255.f;
	v.col[3] = (static_cast<float>((color >> 24) & 0xFF)) / 255.f;
	return v;
}

CD3DFont::CD3DFont() : m_tex_width(512), m_tex_height(512)
{}

constexpr const char fontpixshader[] = {
	"Texture2D tex2D;\n"
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

constexpr const char fontvertshader[] = {
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

void CD3DFont::InitalizeSRV()
{
	D3D::gpu_descriptor_heap_mgr->Allocate(nullptr, &m_texture_cpu, nullptr, &m_texture_gpu);

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = -1;

	D3D::device->CreateShaderResourceView(m_texture, &srv_desc, m_texture_cpu);
}

int CD3DFont::Init()
{
	// Create vertex buffer for the letters

	// Prepare to create a bitmap
	unsigned int* bitmap_bits;
	BITMAPINFO bmi;
	ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = (int)m_tex_width;
	bmi.bmiHeader.biHeight = -(int)m_tex_height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount = 32;

	// Create a DC and a bitmap for the font
	HDC hDC = CreateCompatibleDC(nullptr);
	HBITMAP hbmBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&bitmap_bits), nullptr, 0);
	SetMapMode(hDC, MM_TEXT);

	// create a GDI font
	HFONT hFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE,
		FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
		VARIABLE_PITCH, _T("Tahoma"));

	if (nullptr == hFont)
		return E_FAIL;

	HGDIOBJ hOldbmBitmap = SelectObject(hDC, hbmBitmap);
	HGDIOBJ hOldFont = SelectObject(hDC, hFont);

	// Set text properties
	SetTextColor(hDC, 0xFFFFFF);
	SetBkColor(hDC, 0);
	SetTextAlign(hDC, TA_TOP);

	TEXTMETRICW tm;
	GetTextMetricsW(hDC, &tm);
	m_line_height = tm.tmHeight;

	// Loop through all printable characters and output them to the bitmap
	// Meanwhile, keep track of the corresponding tex coords for each character.
	int x = 0, y = 0;
	char str[2] = "\0";
	for (int c = 0; c < 127 - 32; c++)
	{
		str[0] = c + 32;
		SIZE size;
		GetTextExtentPoint32A(hDC, str, 1, &size);
		if ((int)(x + size.cx + 1) > m_tex_width)
		{
			x = 0;
			y += m_line_height;
		}

		ExtTextOutA(hDC, x + 1, y + 0, ETO_OPAQUE | ETO_CLIPPED, nullptr, str, 1, nullptr);
		m_tex_coords[c][0] = (static_cast<float>(x + 0)) / m_tex_width;
		m_tex_coords[c][1] = (static_cast<float>(y + 0)) / m_tex_height;
		m_tex_coords[c][2] = (static_cast<float>(x + 0 + size.cx)) / m_tex_width;
		m_tex_coords[c][3] = (static_cast<float>(y + 0 + size.cy)) / m_tex_height;

		x += size.cx + 3;  // 3 to work around annoying ij conflict (part of the j ends up with the i)
	}

	// Create a new texture for the font
	// possible optimization: store the converted data in a buffer and fill the texture on creation.
	// That way, we can use a static texture
	std::vector<byte> tex_initial_data(m_tex_width * m_tex_height * 4);

	for (y = 0; y < m_tex_height; y++)
	{
		u32* pDst32 = reinterpret_cast<u32*>(tex_initial_data.data() + y * m_tex_width * 4);
		for (x = 0; x < m_tex_width; x++)
		{
			const u8 bAlpha = (bitmap_bits[m_tex_width * y + x] & 0xff);

			*pDst32++ = (((bAlpha << 4) | bAlpha) << 24) | 0xFFFFFF;
		}
	}

	CheckHR(
		D3D::device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_tex_width, m_tex_height, 1, 1),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&m_texture)
		)
	);

	D3D::SetDebugObjectName12(m_texture, "texture of a CD3DFont object");

	ID3D12Resource* temporaryFontTextureUploadBuffer;
	CheckHR(
		D3D::device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(Common::AlignUpSizePow2(static_cast<u32>(m_tex_width * 4), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * m_tex_height),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&temporaryFontTextureUploadBuffer)
		)
	);

	D3D12_SUBRESOURCE_DATA subresource_data_dest = {
		tex_initial_data.data(), // const void *pData;
		m_tex_width * 4,        // LONG_PTR RowPitch;
		0                       // LONG_PTR SlicePitch;
	};

	D3D::ResourceBarrier(D3D::current_command_list, m_texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	CHECK(0 != UpdateSubresources(D3D::current_command_list, m_texture, temporaryFontTextureUploadBuffer, 0, 0, 1, &subresource_data_dest), "UpdateSubresources call failed.");

	command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(temporaryFontTextureUploadBuffer);

	InitalizeSRV();

	D3D::ResourceBarrier(D3D::current_command_list, m_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	SelectObject(hDC, hOldbmBitmap);
	DeleteObject(hbmBitmap);

	SelectObject(hDC, hOldFont);
	DeleteObject(hFont);

	// setup device objects for drawing
	D3DBlob* psbytecode = nullptr;
	D3D::CompilePixelShader(fontpixshader, &psbytecode);
	if (psbytecode == nullptr)
		PanicAlert("Failed to compile pixel shader, %s %d\n", __FILE__, __LINE__);

	m_pshader.pShaderBytecode = psbytecode->Data();
	m_pshader.BytecodeLength = psbytecode->Size();

	D3DBlob* vsbytecode = nullptr;
	D3D::CompileVertexShader(fontvertshader, &vsbytecode);
	if (vsbytecode == nullptr)
		PanicAlert("Failed to compile vertex shader, %s %d\n", __FILE__, __LINE__);

	m_vshader.pShaderBytecode = vsbytecode->Data();
	m_vshader.BytecodeLength = vsbytecode->Size();

	const D3D12_INPUT_ELEMENT_DESC desc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	m_input_layout.NumElements = ARRAYSIZE(desc);
	m_input_layout.pInputElementDescs = desc;

	D3D12_BLEND_DESC blenddesc = {};
	blenddesc.AlphaToCoverageEnable = FALSE;
	blenddesc.IndependentBlendEnable = FALSE;
	blenddesc.RenderTarget[0].BlendEnable = TRUE;
	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blenddesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blenddesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blenddesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	blenddesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blenddesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	blenddesc.RenderTarget[0].LogicOpEnable = FALSE;
	m_blendstate = blenddesc;

	D3D12_RASTERIZER_DESC rastdesc = { D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, false, 0, 0.f, 0.f, false, false, false, false };
	m_raststate = rastdesc;
	const unsigned int text_vb_size = s_max_num_vertices * sizeof(FONT2DVERTEX);
	m_vertex_buffer = std::make_unique<D3DStreamBuffer>(text_vb_size * 2, text_vb_size * 16, nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC text_pso_desc = {
		GetBasicRootSignature(),                           // ID3D12RootSignature *pRootSignature;
		{ vsbytecode->Data(), vsbytecode->Size() },       // D3D12_SHADER_BYTECODE VS;
		{ psbytecode->Data(), psbytecode->Size() },       // D3D12_SHADER_BYTECODE PS;
		{},                                               // D3D12_SHADER_BYTECODE DS;
		{},                                               // D3D12_SHADER_BYTECODE HS;
		{},                                               // D3D12_SHADER_BYTECODE GS;
		{},                                               // D3D12_STREAM_OUTPUT_DESC StreamOutput
		blenddesc,                                        // D3D12_BLEND_DESC BlendState;
		UINT_MAX,                                         // UINT SampleMask;
		rastdesc,                                         // D3D12_RASTERIZER_DESC RasterizerState
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),        // D3D12_DEPTH_STENCIL_DESC DepthStencilState
		m_input_layout,                                 // D3D12_INPUT_LAYOUT_DESC InputLayout
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF,        // D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IndexBufferProperties
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,           // D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType
		1,                                                // UINT NumRenderTargets
		{ DXGI_FORMAT_R8G8B8A8_UNORM },                   // DXGI_FORMAT RTVFormats[8]
		DXGI_FORMAT_UNKNOWN,                              // DXGI_FORMAT DSVFormat
		{ 1 /* UINT Count */, 0 /* UINT Quality */ }      // DXGI_SAMPLE_DESC SampleDesc
	};

	CheckHR(DX12::gx_state_cache.GetPipelineStateObjectFromCache(text_pso_desc, &m_pso));

	SAFE_RELEASE(psbytecode);
	SAFE_RELEASE(vsbytecode);

	return S_OK;
}

int CD3DFont::Shutdown()
{
	m_vertex_buffer.reset();
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_texture);

	return S_OK;
}

int CD3DFont::DrawTextScaled(float x, float y, float size, float spacing, u32 dwColor, const std::string& text)
{
	if (!m_vertex_buffer)
		return 0;

	float scale_x = 1 / static_cast<float>(D3D::GetBackBufferWidth()) * 2.f;
	float scale_y = 1 / static_cast<float>(D3D::GetBackBufferHeight()) * 2.f;
	float sizeratio = size / static_cast<float>(m_line_height);

	// translate starting positions
	float sx = x * scale_x - 1.f;
	float sy = 1.f - y * scale_y;

	// set general pipeline state
	D3D::SetRootSignature(false, false);
	D3D::current_command_list->SetPipelineState(m_pso);
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, true);

	D3D::current_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	D3D::command_list_mgr->SetCommandListPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, m_texture_gpu);

	// upper bound is nchars * 6, assuming no spaces
	m_vertex_buffer->AllocateSpaceInBuffer(static_cast<u32>(text.length()) * 6 * sizeof(FONT2DVERTEX), sizeof(FONT2DVERTEX), false);

	FONT2DVERTEX* vertices12 = reinterpret_cast<FONT2DVERTEX*>(m_vertex_buffer->GetCPUAddressOfCurrentAllocation());
	int num_triangles = 0;

	float start_x = sx;
	for (char c : text)
	{
		if (c == '\n')
		{
			sx = start_x;
			sy -= scale_y * size;
		}
		if (!std::isprint(c))
			continue;

		c -= 32;
		float tx1 = m_tex_coords[c][0];
		float ty1 = m_tex_coords[c][1];
		float tx2 = m_tex_coords[c][2];
		float ty2 = m_tex_coords[c][3];

		float w = static_cast<float>(tx2 - tx1) * m_tex_width * scale_x * sizeratio;
		float h = static_cast<float>(ty1 - ty2) * m_tex_height * scale_y * sizeratio;

		FONT2DVERTEX v[6];
		v[0] = InitFont2DVertex(sx, sy + h, dwColor, tx1, ty2);
		v[1] = InitFont2DVertex(sx, sy, dwColor, tx1, ty1);
		v[2] = InitFont2DVertex(sx + w, sy + h, dwColor, tx2, ty2);
		v[3] = InitFont2DVertex(sx + w, sy, dwColor, tx2, ty1);
		v[4] = v[2];
		v[5] = v[1];

		memcpy(vertices12, v, 6 * sizeof(FONT2DVERTEX));
		vertices12 += 6;

		num_triangles += 2;

		sx += w + spacing * scale_x * size;
	}

	// Render the vertex buffer
	if (num_triangles > 0)
	{
		u32 written_size = num_triangles * 3 * sizeof(FONT2DVERTEX);
		m_vertex_buffer->OverrideSizeOfPreviousAllocation(written_size);

		D3D12_VERTEX_BUFFER_VIEW vb_view = { m_vertex_buffer->GetGPUAddressOfCurrentAllocation(), written_size, sizeof(FONT2DVERTEX) };
		D3D::current_command_list->IASetVertexBuffers(0, 1, &vb_view);
		D3D::current_command_list->DrawInstanced(3 * num_triangles, 1, 0, 0);
	}
	return S_OK;
}

D3D12_CPU_DESCRIPTOR_HANDLE linear_copy_sampler12CPU;
D3D12_GPU_DESCRIPTOR_HANDLE linear_copy_sampler12GPU;
D3D12_CPU_DESCRIPTOR_HANDLE point_copy_sampler12CPU;
D3D12_GPU_DESCRIPTOR_HANDLE point_copy_sampler12GPU;

struct STQVertex
{
	float x, y, z, u, v, w;
};

struct ColVertex
{
	float x, y, z;
	u32 col;
};

struct
{
	float u1, v1, u2, v2, S, G;
} tex_quad_data;

struct
{
	u32 col;
	float z;
} clear_quad_data;

// ring buffer offsets
size_t stq_offset;
size_t clearq_offset;
static constexpr size_t vb_buff_size = 0x100000;

static void InitBasicSamplers(void* obj)
{
	D3D12_SAMPLER_DESC point_sampler_desc = {
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.f,
		1,
		D3D12_COMPARISON_FUNC_ALWAYS,
		{ 0.f, 0.f, 0.f, 0.f },
		0.f,
		0.f
	};

	D3D::sampler_descriptor_heap_mgr->Allocate(&point_copy_sampler12CPU, &point_copy_sampler12GPU);
	D3D::device->CreateSampler(&point_sampler_desc, point_copy_sampler12CPU);

	D3D12_SAMPLER_DESC linear_sampler_desc = {
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.f,
		1,
		D3D12_COMPARISON_FUNC_ALWAYS,
		{ 0.f, 0.f, 0.f, 0.f },
		0.f,
		0.f
	};

	D3D::sampler_descriptor_heap_mgr->Allocate(&linear_copy_sampler12CPU, &linear_copy_sampler12GPU);
	D3D::device->CreateSampler(&linear_sampler_desc, linear_copy_sampler12CPU);
}

void InitUtils()
{
	s_reset_blend_desc = Renderer::GetResetBlendDesc();
	s_reset_depth_desc = Renderer::GetResetDepthStencilDesc();
	s_reset_rast_desc = Renderer::GetResetRasterizerDesc();

	util_vbuf_stq = std::make_unique<UtilVertexBuffer>(vb_buff_size);
	util_vbuf_clearq = std::make_unique<UtilVertexBuffer>(vb_buff_size);
	util_vbuf_efbpokequads = std::make_unique<UtilVertexBuffer>(vb_buff_size);

	// Init default samplers
	InitBasicSamplers(nullptr);
	D3D::sampler_descriptor_heap_mgr->RegisterHeapRestartCallback(nullptr, InitBasicSamplers);
	// cached data used to avoid unnecessarily reloading the vertex buffers
	memset(&tex_quad_data, 0, sizeof(tex_quad_data));
	memset(&clear_quad_data, 0, sizeof(clear_quad_data));

	font.Init();
}

void ShutdownUtils()
{
	D3D::sampler_descriptor_heap_mgr->RemoveHeapRestartCallback(nullptr);
	font.Shutdown();
	util_vbuf_stq.reset();
	util_vbuf_clearq.reset();
	util_vbuf_efbpokequads.reset();
}

void SetPointCopySampler()
{
	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SAMPLER, point_copy_sampler12GPU);
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_SAMPLERS, true);
}

void SetLinearCopySampler()
{
	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SAMPLER, linear_copy_sampler12GPU);
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_SAMPLERS, true);
}

void DrawShadedTexQuad(D3DTexture2D* texture,
	const D3D12_RECT* rSource,
	int source_width,
	int source_height,
	D3D12_SHADER_BYTECODE pshader12,
	D3D12_SHADER_BYTECODE vshader12,
	D3D12_INPUT_LAYOUT_DESC layout12,
	D3D12_SHADER_BYTECODE gshader12,
	u32 slice,
	DXGI_FORMAT rt_format,
	bool inherit_srv_binding,
	bool rt_multisampled,
	D3D12_DEPTH_STENCIL_DESC* depth_stencil_desc_override
)
{
	D3D::SetRootSignature(gshader12.pShaderBytecode != nullptr, false);
	float sw = 1.0f / static_cast<float>(source_width);
	float sh = 1.0f / static_cast<float>(source_height);
	float u1 = static_cast<float>(rSource->left) * sw;
	float u2 = static_cast<float>(rSource->right) * sw;
	float v1 = static_cast<float>(rSource->top) * sh;
	float v2 = static_cast<float>(rSource->bottom) * sh;
	float S = static_cast<float>(slice);

	STQVertex coords[4] = {
		{ -1.0f, 1.0f, 0.0f, u1, v1, S },
		{ 1.0f, 1.0f, 0.0f, u2, v1, S },
		{ -1.0f, -1.0f, 0.0f, u1, v2, S },
		{ 1.0f, -1.0f, 0.0f, u2, v2, S },
	};

	// only upload the data to VRAM if it changed
	if (tex_quad_data.u1 != u1 || tex_quad_data.v1 != v1 ||
		tex_quad_data.u2 != u2 || tex_quad_data.v2 != v2 ||
		tex_quad_data.S != S)
	{
		stq_offset = util_vbuf_stq->AppendData(coords, sizeof(coords), sizeof(STQVertex));

		tex_quad_data.u1 = u1;
		tex_quad_data.v1 = v1;
		tex_quad_data.u2 = u2;
		tex_quad_data.v2 = v2;
		tex_quad_data.S = S;
	}

	D3D::current_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	D3D::command_list_mgr->SetCommandListPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	D3D12_VERTEX_BUFFER_VIEW vb_view = {
		util_vbuf_stq->GetBuffer()->GetGPUVirtualAddress(),   // D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
		static_cast<UINT>(util_vbuf_stq->GetSize()),          // UINT SizeInBytes; This is the size of the entire buffer, not just the size of the vertex data for one draw call, since the offsetting is done in the draw call itself.
		sizeof(STQVertex)                                     // UINT StrideInBytes;
	};

	D3D::current_command_list->IASetVertexBuffers(0, 1, &vb_view);
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_VERTEX_BUFFER, true);

	if (!inherit_srv_binding && texture != nullptr)
	{
		texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, texture->GetSRVGPU());
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
		D3D::GetRootSignature(),                     // ID3D12RootSignature *pRootSignature;
		vshader12,                                        // D3D12_SHADER_BYTECODE VS;
		pshader12,                                        // D3D12_SHADER_BYTECODE PS;
		{},                                               // D3D12_SHADER_BYTECODE DS;
		{},                                               // D3D12_SHADER_BYTECODE HS;
		gshader12,                                        // D3D12_SHADER_BYTECODE GS;
		{},                                               // D3D12_STREAM_OUTPUT_DESC StreamOutput
		s_reset_blend_desc,                               // D3D12_BLEND_DESC BlendState;
		UINT_MAX,                                         // UINT SampleMask;
		s_reset_rast_desc,                                // D3D12_RASTERIZER_DESC RasterizerState
		depth_stencil_desc_override ? *depth_stencil_desc_override : s_reset_depth_desc,    // D3D12_DEPTH_STENCIL_DESC DepthStencilState
		layout12,                                         // D3D12_INPUT_LAYOUT_DESC InputLayout
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF,        // D3D12_INDEX_BUFFER_PROPERTIES IndexBufferProperties
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,           // D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType
		1,                                                // UINT NumRenderTargets
		{ rt_format },                                    // DXGI_FORMAT RTVFormats[8]
		DXGI_FORMAT_D32_FLOAT,                    // DXGI_FORMAT DSVFormat
		{ 1 /* UINT Count */, 0 /* UINT Quality */ }      // DXGI_SAMPLE_DESC SampleDesc
	};

	if (rt_multisampled)
	{
		pso_desc.SampleDesc.Count = g_ActiveConfig.iMultisamples;
	}

	ID3D12PipelineState* pso = nullptr;
	CheckHR(DX12::gx_state_cache.GetPipelineStateObjectFromCache(pso_desc, &pso));

	D3D::current_command_list->SetPipelineState(pso);
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, true);

	D3D::current_command_list->DrawInstanced(4, 1, static_cast<UINT>(stq_offset), 0);
}

void DrawClearQuad(u32 Color, float z, D3D12_BLEND_DESC* blend_desc, D3D12_DEPTH_STENCIL_DESC* depth_stencil_desc, bool rt_multisampled)
{
	D3D::SetRootSignature(g_ActiveConfig.iStereoMode > 0, false);
	ColVertex coords[4] = {
		{-1.0f,  1.0f, z, Color},
		{ 1.0f,  1.0f, z, Color},
		{-1.0f, -1.0f, z, Color},
		{ 1.0f, -1.0f, z, Color},
	};

	if (clear_quad_data.col != Color || clear_quad_data.z != z)
	{
		clearq_offset = util_vbuf_clearq->AppendData(coords, sizeof(coords), sizeof(ColVertex));

		clear_quad_data.col = Color;
		clear_quad_data.z = z;
	}

	D3D::current_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	D3D::command_list_mgr->SetCommandListPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	D3D12_VERTEX_BUFFER_VIEW vb_view = {
		util_vbuf_clearq->GetBuffer()->GetGPUVirtualAddress(), // D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
		static_cast<UINT>(util_vbuf_clearq->GetSize()),        // UINT SizeInBytes; This is the size of the entire buffer, not just the size of the vertex data for one draw call, since the offsetting is done in the draw call itself.
		sizeof(ColVertex)                                      // UINT StrideInBytes;
	};

	D3D::current_command_list->IASetVertexBuffers(0, 1, &vb_view);
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_VERTEX_BUFFER, true);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
		D3D::GetRootSignature(),                           // ID3D12RootSignature *pRootSignature;
		StaticShaderCache::GetClearVertexShader(),        // D3D12_SHADER_BYTECODE VS;
		StaticShaderCache::GetClearPixelShader(),         // D3D12_SHADER_BYTECODE PS;
		{},                                               // D3D12_SHADER_BYTECODE DS;
		{},                                               // D3D12_SHADER_BYTECODE HS;
		g_ActiveConfig.iStereoMode > 0 ?
		StaticShaderCache::GetClearGeometryShader() :
		D3D12_SHADER_BYTECODE(),                          // D3D12_SHADER_BYTECODE GS;
		{},                                               // D3D12_STREAM_OUTPUT_DESC StreamOutput
		*blend_desc,                                      // D3D12_BLEND_DESC BlendState;
		UINT_MAX,                                         // UINT SampleMask;
		s_reset_rast_desc,                                // D3D12_RASTERIZER_DESC RasterizerState
		*depth_stencil_desc,                              // D3D12_DEPTH_STENCIL_DESC DepthStencilState
		StaticShaderCache::GetClearVertexShaderInputLayout(), // D3D12_INPUT_LAYOUT_DESC InputLayout
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF,        // D3D12_INDEX_BUFFER_PROPERTIES IndexBufferProperties
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,           // D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType
		1,                                                // UINT NumRenderTargets
		{ DXGI_FORMAT_R8G8B8A8_UNORM },                   // DXGI_FORMAT RTVFormats[8]
		DXGI_FORMAT_D32_FLOAT,                    // DXGI_FORMAT DSVFormat
		{ 1 /* UINT Count */, 0 /* UINT Quality */ }      // DXGI_SAMPLE_DESC SampleDesc
	};

	if (rt_multisampled)
	{
		pso_desc.SampleDesc.Count = g_ActiveConfig.iMultisamples;
	}

	ID3D12PipelineState* pso = nullptr;
	CheckHR(DX12::gx_state_cache.GetPipelineStateObjectFromCache(pso_desc, &pso));

	D3D::current_command_list->SetPipelineState(pso);
	D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, true);

	D3D::current_command_list->DrawInstanced(4, 1, static_cast<UINT>(clearq_offset), 0);
}

inline void InitColVertex(ColVertex* vert, float x, float y, float z, u32 col)
{
	*vert = { x, y, z, col };
}

void DrawEFBPokeQuads(EFBAccessType type,
	const EfbPokeData* points,
	size_t num_points,
	D3D12_BLEND_DESC* blend_desc,
	D3D12_DEPTH_STENCIL_DESC* depth_stencil_desc,
	D3D12_CPU_DESCRIPTOR_HANDLE* render_target,
	D3D12_CPU_DESCRIPTOR_HANDLE* depth_buffer,
	bool rt_multisampled)
{
	D3D::SetRootSignature(g_ActiveConfig.iStereoMode > 0, false);
	// The viewport and RT/DB are passed in so we can reconstruct the state if we need to execute in the middle of building the vertex buffer.
	D3D::current_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	D3D::command_list_mgr->SetCommandListPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
		D3D::GetRootSignature(),                          // ID3D12RootSignature *pRootSignature;
		StaticShaderCache::GetClearVertexShader(),        // D3D12_SHADER_BYTECODE VS;
		StaticShaderCache::GetClearPixelShader(),         // D3D12_SHADER_BYTECODE PS;
		{},                                               // D3D12_SHADER_BYTECODE DS;
		{},                                               // D3D12_SHADER_BYTECODE HS;
		g_ActiveConfig.iStereoMode > 0 ?
		StaticShaderCache::GetClearGeometryShader() :
		D3D12_SHADER_BYTECODE(),                          // D3D12_SHADER_BYTECODE GS;
		{},                                               // D3D12_STREAM_OUTPUT_DESC StreamOutput
		*blend_desc,                                      // D3D12_BLEND_DESC BlendState;
		UINT_MAX,                                         // UINT SampleMask;
		s_reset_rast_desc,                                // D3D12_RASTERIZER_DESC RasterizerState
		*depth_stencil_desc,                              // D3D12_DEPTH_STENCIL_DESC DepthStencilState
		StaticShaderCache::GetClearVertexShaderInputLayout(), // D3D12_INPUT_LAYOUT_DESC InputLayout
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF,        // D3D12_INDEX_BUFFER_PROPERTIES IndexBufferProperties
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,           // D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType
		1,                                                // UINT NumRenderTargets
		{ DXGI_FORMAT_R8G8B8A8_UNORM },                   // DXGI_FORMAT RTVFormats[8]
		DXGI_FORMAT_D32_FLOAT,                    // DXGI_FORMAT DSVFormat
		{ 1 /* UINT Count */, 0 /* UINT Quality */ }      // DXGI_SAMPLE_DESC SampleDesc
	};

	if (rt_multisampled)
	{
		pso_desc.SampleDesc.Count = g_ActiveConfig.iMultisamples;
	}

	ID3D12PipelineState* pso = nullptr;
	CheckHR(DX12::gx_state_cache.GetPipelineStateObjectFromCache(pso_desc, &pso));

	// if drawing a large number of points at once, this will have to be split into multiple passes.
	const size_t COL_QUAD_SIZE = sizeof(ColVertex) * 6;
	size_t points_per_draw = util_vbuf_efbpokequads->GetSize() / COL_QUAD_SIZE;

	size_t current_point_index = 0;
	while (current_point_index < num_points)
	{
		// Map and reserve enough buffer space for this draw
		size_t points_to_draw = std::min(num_points - current_point_index, points_per_draw);
		size_t required_bytes = COL_QUAD_SIZE * points_to_draw;

		void* buffer_ptr = nullptr;
		size_t base_vertex_index = util_vbuf_efbpokequads->ReserveData(&buffer_ptr, static_cast<int>(required_bytes), sizeof(ColVertex));

		CHECK(base_vertex_index * 16 + required_bytes <= util_vbuf_efbpokequads->GetSize(), "Uh oh");

		// Corresponding dirty flags set outside loop.
		D3D::current_command_list->OMSetRenderTargets(1, render_target, FALSE, depth_buffer);
		D3D::current_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		D3D::command_list_mgr->SetCommandListPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		D3D12_VERTEX_BUFFER_VIEW vb_view = {
			util_vbuf_efbpokequads->GetBuffer()->GetGPUVirtualAddress(), // D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
			static_cast<UINT>(util_vbuf_efbpokequads->GetSize()),        // UINT SizeInBytes; This is the size of the entire buffer, not just the size of the vertex data for one draw call, since the offsetting is done in the draw call itself.
			sizeof(ColVertex)                                            // UINT StrideInBytes;
		};

		D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_VERTEX_BUFFER, true);

		D3D::current_command_list->IASetVertexBuffers(0, 1, &vb_view);
		D3D::current_command_list->SetPipelineState(pso);
		D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PSO, true);

		// generate quads for each efb point
		ColVertex* base_vertex_ptr = static_cast<ColVertex*>(buffer_ptr);
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

		D3D::current_command_list->DrawInstanced(6 * static_cast<UINT>(points_to_draw), 1, static_cast<UINT>(base_vertex_index), 0);
	}
	g_renderer->RestoreAPIState();
}

}  // namespace D3D

}  // namespace DX12
