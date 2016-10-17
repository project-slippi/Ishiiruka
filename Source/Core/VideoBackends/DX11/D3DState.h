// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <stack>
#include <unordered_map>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "VideoBackends/DX11/D3DBase.h"
#include "VideoBackends/DX11/D3DPtr.h"
#include "VideoCommon/BPMemory.h"

struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;

namespace DX11
{

union RasterizerState
{
	BitField<0, 2, D3D11_CULL_MODE> cull_mode;

	u32 packed;
};

union BlendState
{
	BitField<0, 1, u32> blend_enable;
	BitField<1, 3, D3D11_BLEND_OP> blend_op;
	BitField<4, 4, u32> write_mask;
	BitField<8, 5, D3D11_BLEND> src_blend;
	BitField<13, 5, D3D11_BLEND> dst_blend;
	BitField<18, 1, u32> use_dst_alpha;
	BitField<19, 1, u32> logic_op_enabled;
	BitField<20, 4, D3D11_LOGIC_OP> logic_op;
	u32 packed;
};

union SamplerState
{
	BitField<0, 3, u64> min_filter;
	BitField<3, 1, u64> mag_filter;
	BitField<4, 8, u64> min_lod;
	BitField<12, 8, u64> max_lod;
	BitField<20, 8, s64> lod_bias;
	BitField<28, 2, u64> wrap_s;
	BitField<30, 2, u64> wrap_t;
	BitField<32, 5, u64> max_anisotropy;

	u64 packed;
};

union DepthState 
{
	BitField<0, 1, u32> testenable;
	BitField<1, 4, u32> func;
	BitField<5, 1, u32> updateenable;
	BitField<6, 1, u32> reversed_depth;

	u32 packed;
};

class StateCache
{
public:

	// Get existing or create new render state.
	// Returned objects is owned by the cache and does not need to be released.
	ID3D11SamplerState* Get(SamplerState state);
	ID3D11BlendState* Get(BlendState state);
	ID3D11RasterizerState* Get(RasterizerState state);
	ID3D11DepthStencilState* Get(DepthState state);

	// Release all cached states and clear hash tables.
	void Clear();

private:

	std::unordered_map<u32, D3D::DepthStencilStatePtr> m_depth;
	std::unordered_map<u32, D3D::RasterizerStatePtr> m_raster;
	std::unordered_map<u32, D3D::BlendStatePtr> m_blend;
	std::unordered_map<u64, D3D::SamplerStatePtr> m_sampler;
};

namespace D3D
{
template<typename T> class AutoState
{
public:
	AutoState(const T* object);
	AutoState(const AutoState<T> &source);
	~AutoState();

	const inline T* get() const
	{
		return state;
	}

private:
	const T* state;
};

typedef AutoState<ID3D11BlendState> AutoBlendState;
typedef AutoState<ID3D11DepthStencilState> AutoDepthStencilState;
typedef AutoState<ID3D11RasterizerState> AutoRasterizerState;

class StateManager
{
public:
	StateManager();

	// call any of these to change the affected states
	void PushBlendState(const ID3D11BlendState* state);
	void PushDepthState(const ID3D11DepthStencilState* state);
	void PushRasterizerState(const ID3D11RasterizerState* state);

	// call these after drawing
	void PopBlendState();
	void PopDepthState();
	void PopRasterizerState();

	void SetTexture(u32 index, ID3D11ShaderResourceView* texture)
	{
		if (m_current.textures[index] != texture)
			m_dirtyFlags |= DirtyFlag_Texture0 << index;

		m_pending.textures[index] = texture;
	}

	void SetSampler(u32 index, ID3D11SamplerState* sampler)
	{
		if (m_current.samplers[index] != sampler)
			m_dirtyFlags |= DirtyFlag_Sampler0 << index;

		m_pending.samplers[index] = sampler;
	}

	void SetPixelConstants(u32 idx, BufferDescriptor& buffer0)
	{
		if (m_current.pixelConstants[idx] != std::get<0>(buffer0)
			|| m_current.pixelConstantsOffset[idx] != std::get<1>(buffer0)
			|| m_current.pixelConstantsSize[idx] != std::get<2>(buffer0))
			m_dirtyFlags |= DirtyFlag_PixelConstants;

		m_pending.pixelConstants[idx] = std::get<0>(buffer0);
		m_pending.pixelConstantsOffset[idx] = std::get<1>(buffer0);
		m_pending.pixelConstantsSize[idx] = std::get<2>(buffer0);
	}

	void SetPixelConstants(ID3D11Buffer* buffer0, ID3D11Buffer* buffer1 = nullptr)
	{
		if (m_current.pixelConstants[0] != buffer0 || m_current.pixelConstants[1] != buffer1)
			m_dirtyFlags |= DirtyFlag_PixelConstants;

		m_pending.pixelConstants[0] = buffer0;
		m_pending.pixelConstantsOffset[0] = 0;
		m_pending.pixelConstantsSize[0] = 0;
		m_pending.pixelConstants[1] = buffer1;
		m_pending.pixelConstantsOffset[1] = 0;
		m_pending.pixelConstantsSize[1] = 0;
	}

	void SetVertexConstants(BufferDescriptor& buffer)
	{
		if (m_current.vertexConstants != std::get<0>(buffer)
			|| m_current.vertexConstantsOffset != std::get<1>(buffer)
			|| m_current.vertexConstantsSize != std::get<2>(buffer))
			m_dirtyFlags |= DirtyFlag_VertexConstants;

		m_pending.vertexConstants = std::get<0>(buffer);
		m_pending.vertexConstantsOffset = std::get<1>(buffer);
		m_pending.vertexConstantsSize = std::get<2>(buffer);
	}

	void SetVertexConstants(ID3D11Buffer* buffer)
	{
		if (m_current.vertexConstants != buffer)
			m_dirtyFlags |= DirtyFlag_VertexConstants;

		m_pending.vertexConstants = buffer;
		m_pending.vertexConstantsOffset = 0;
		m_pending.vertexConstantsSize = 0;
	}

	void SetGeometryConstants(BufferDescriptor& buffer)
	{
		if (m_current.geometryConstants != std::get<0>(buffer)
			|| m_current.geometryConstantsOffset != std::get<1>(buffer)
			|| m_current.geometryConstantsSize != std::get<2>(buffer))
			m_dirtyFlags |= DirtyFlag_GeometryConstants;

		m_pending.geometryConstants = std::get<0>(buffer);
		m_pending.geometryConstantsOffset = std::get<1>(buffer);
		m_pending.geometryConstantsSize = std::get<2>(buffer);
	}

	void SetGeometryConstants(ID3D11Buffer* buffer)
	{
		if (m_current.geometryConstants != buffer)
			m_dirtyFlags |= DirtyFlag_GeometryConstants;

		m_pending.geometryConstants = buffer;
		m_pending.geometryConstantsOffset = 0;
		m_pending.geometryConstantsSize = 0;
	}


	void SetHullDomainConstants(u32 idx, BufferDescriptor& buffer)
	{
		if (m_current.hulldomainConstants[idx] != std::get<0>(buffer)
			|| m_current.hulldomainConstantsOffset[idx] != std::get<1>(buffer)
			|| m_current.hulldomainConstantsSize[idx] != std::get<2>(buffer))
			m_dirtyFlags |= DirtyFlag_HullDomainConstants;

		m_pending.hulldomainConstants[idx] = std::get<0>(buffer);
		m_pending.hulldomainConstantsOffset[idx] = std::get<1>(buffer);
		m_pending.hulldomainConstantsSize[idx] = std::get<2>(buffer);
	}

	void SetVertexBuffer(ID3D11Buffer* buffer, u32 stride, u32 offset)
	{
		if (m_current.vertexBuffer != buffer ||
			m_current.vertexBufferStride != stride ||
			m_current.vertexBufferOffset != offset)
			m_dirtyFlags |= DirtyFlag_VertexBuffer;

		m_pending.vertexBuffer = buffer;
		m_pending.vertexBufferStride = stride;
		m_pending.vertexBufferOffset = offset;
	}

	void SetIndexBuffer(ID3D11Buffer* buffer)
	{
		if (m_current.indexBuffer != buffer)
			m_dirtyFlags |= DirtyFlag_IndexBuffer;

		m_pending.indexBuffer = buffer;
	}

	void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology)
	{
		if (m_current.topology != topology)
			m_dirtyFlags |= DirtyFlag_InputAssembler;

		m_pending.topology = topology;
	}

	void SetInputLayout(ID3D11InputLayout* layout)
	{
		if (m_current.inputLayout != layout)
			m_dirtyFlags |= DirtyFlag_InputAssembler;

		m_pending.inputLayout = layout;
	}

	void SetPixelShader(ID3D11PixelShader* shader)
	{
		if (m_current.pixelShader != shader)
			m_dirtyFlags |= DirtyFlag_PixelShader;

		m_pending.pixelShader = shader;
	}

	void SetPixelShaderDynamic(ID3D11PixelShader* shader, ID3D11ClassInstance * const * classInstances, u32 classInstancesCount)
	{
		D3D::context->PSSetShader(shader, classInstances, classInstancesCount);
		m_current.pixelShader = shader;
		m_pending.pixelShader = shader;
	}

	void SetVertexShader(ID3D11VertexShader* shader)
	{
		if (m_current.vertexShader != shader)
			m_dirtyFlags |= DirtyFlag_VertexShader;

		m_pending.vertexShader = shader;
	}

	void SetGeometryShader(ID3D11GeometryShader* shader)
	{
		if (m_current.geometryShader != shader)
			m_dirtyFlags |= DirtyFlag_GeometryShader;

		m_pending.geometryShader = shader;
	}

	void SetHullShader(ID3D11HullShader* shader)
	{
		if (m_current.hullShader != shader)
			m_dirtyFlags |= DirtyFlag_HullShader;

		m_pending.hullShader = shader;
	}

	void SetDomainShader(ID3D11DomainShader* shader)
	{
		if (m_current.domainShader != shader)
			m_dirtyFlags |= DirtyFlag_DomainShader;

		m_pending.domainShader = shader;
	}

	// removes currently set texture from all slots, returns mask of previously bound slots
	u64 UnsetTexture(ID3D11ShaderResourceView* srv);
	void SetTextureByMask(u64 textureSlotMask, ID3D11ShaderResourceView* srv);

	// call this immediately before any drawing operation or to explicitly apply pending resource state changes
	void Apply();

private:

	std::stack<AutoBlendState> m_blendStates;
	std::stack<AutoDepthStencilState> m_depthStates;
	std::stack<AutoRasterizerState> m_rasterizerStates;

	ID3D11BlendState* m_currentBlendState;
	ID3D11DepthStencilState* m_currentDepthState;
	ID3D11RasterizerState* m_currentRasterizerState;

	enum DirtyFlags : u64
	{
		DirtyFlag_Texture0 = 1ull << 0,
		DirtyFlag_Texture1 = 1ull << 1,
		DirtyFlag_Texture2 = 1ull << 2,
		DirtyFlag_Texture3 = 1ull << 3,
		DirtyFlag_Texture4 = 1ull << 4,
		DirtyFlag_Texture5 = 1ull << 5,
		DirtyFlag_Texture6 = 1ull << 6,
		DirtyFlag_Texture7 = 1ull << 7,
		DirtyFlag_Texture8 = 1ull << 8,
		DirtyFlag_Texture9 = 1ull << 9,
		DirtyFlag_Texture10 = 1ull << 10,
		DirtyFlag_Texture11 = 1ull << 11,
		DirtyFlag_Texture12 = 1ull << 12,
		DirtyFlag_Texture13 = 1ull << 13,
		DirtyFlag_Texture14 = 1ull << 14,
		DirtyFlag_Texture15 = 1ull << 15,
		DirtyFlag_Textures = 0xFFFF,

		DirtyFlag_Sampler0 = 1ull << 16,
		DirtyFlag_Sampler1 = 1ull << 17,
		DirtyFlag_Sampler2 = 1ull << 18,
		DirtyFlag_Sampler3 = 1ull << 19,
		DirtyFlag_Sampler4 = 1ull << 20,
		DirtyFlag_Sampler5 = 1ull << 21,
		DirtyFlag_Sampler6 = 1ull << 22,
		DirtyFlag_Sampler7 = 1ull << 23,
		DirtyFlag_Sampler8 = 1ull << 24,
		DirtyFlag_Sampler9 = 1ull << 25,
		DirtyFlag_Sampler10 = 1ull << 26,
		DirtyFlag_Sampler11 = 1ull << 27,
		DirtyFlag_Sampler12 = 1ull << 28,
		DirtyFlag_Sampler13 = 1ull << 29,
		DirtyFlag_Sampler14 = 1ull << 30,
		DirtyFlag_Sampler15 = 1ull << 31,
		DirtyFlag_Samplers = 0xFFFF0000,

		DirtyFlag_PixelConstants = 1ull << 32,
		DirtyFlag_VertexConstants = 1ull << 33,
		DirtyFlag_GeometryConstants = 1ull << 34,
		DirtyFlag_HullDomainConstants = 1ull << 35,
		DirtyFlag_Constants = 1ull << 32 | 1ull << 33 | 1ull << 34 | 1ull << 35,

		DirtyFlag_VertexBuffer = 1ull << 36,
		DirtyFlag_IndexBuffer = 1ull << 37,
		DirtyFlag_Buffers = 1ull << 36 | 1ull << 37,

		DirtyFlag_PixelShader = 1ull << 38,
		DirtyFlag_VertexShader = 1ull << 39,
		DirtyFlag_GeometryShader = 1ull << 40,
		DirtyFlag_HullShader = 1ull << 41,
		DirtyFlag_DomainShader = 1ull << 42,
		DirtyFlag_Shaders = 1ull << 38 | 1ull << 39 | 1ull << 40 | 1ull << 41 | 1ull << 42,

		DirtyFlag_InputAssembler = 1ull << 43,
	};

	u64 m_dirtyFlags;
	bool use_partial_buffer_update;
	struct Resources
	{
		ID3D11ShaderResourceView* textures[16];
		ID3D11SamplerState* samplers[16];
		ID3D11Buffer* pixelConstants[2];
		UINT pixelConstantsOffset[2];
		UINT pixelConstantsSize[2];
		ID3D11Buffer* vertexConstants;
		UINT vertexConstantsOffset;
		UINT vertexConstantsSize;
		ID3D11Buffer* geometryConstants;
		UINT geometryConstantsOffset;
		UINT geometryConstantsSize;

		ID3D11Buffer* hulldomainConstants[3];
		UINT hulldomainConstantsOffset[3];
		UINT hulldomainConstantsSize[3];

		ID3D11Buffer* vertexBuffer;
		ID3D11Buffer* indexBuffer;
		u32 vertexBufferStride;
		u32 vertexBufferOffset;
		D3D11_PRIMITIVE_TOPOLOGY topology;
		ID3D11InputLayout* inputLayout;
		ID3D11PixelShader* pixelShader;
		ID3D11VertexShader* vertexShader;
		ID3D11GeometryShader* geometryShader;
		ID3D11HullShader* hullShader;
		ID3D11DomainShader* domainShader;
	};

	Resources m_pending;
	Resources m_current;
};

extern StateManager* stateman;

}  // namespace

}  // namespace DX11