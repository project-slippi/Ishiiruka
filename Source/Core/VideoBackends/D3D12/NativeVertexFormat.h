// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
#pragma once

#include "VideoCommon/NativeVertexFormat.h"
#include <d3d11.h>
#include <d3d12.h>

namespace DX12
{
	class D3DVertexFormat final : public NativeVertexFormat
	{
		std::array<D3D12_INPUT_ELEMENT_DESC, 15> m_elems;
		UINT m_num_elems;

		D3D12_INPUT_LAYOUT_DESC m_layout;

	public:
		D3DVertexFormat(const PortableVertexDeclaration &vtx_decl);
		~D3DVertexFormat();

		void SetupVertexPointers();

		D3D12_INPUT_LAYOUT_DESC GetActiveInputLayout() const { return m_layout; };

	private:
		D3D12_INPUT_ELEMENT_DESC GetInputElementDescFromAttributeFormat(const AttributeFormat* format, const char* semantic_name, unsigned int semantic_index) const;
	};
}