// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11_2.h>
#include <memory>

namespace DX11
{
namespace D3D
{
template <typename T>
struct ToAddrImpl;
// This is a specialized simpler version of std::unique_ptr dedicaced to D3D objects (with AddRef/Release methods).
// The single ownership semantics of std::unique_ptr is used and we hide the AddRef/Release API to prevent invalid uses.
// When VS2014 will be here, think to add the noexcept where it is possible
template <typename T>
struct UniquePtr
{
	friend struct ToAddrImpl<T>;
	// http://en.cppreference.com/w/cpp/memory/unique_ptr/unique_ptr
	UniquePtr() = default;

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/unique_ptr
	UniquePtr(std::nullptr_t)
	{}

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/unique_ptr
	explicit UniquePtr(T* p) :
		m_ptr{ p }
	{}

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/unique_ptr
	// Copy semantics is unwanted
	UniquePtr(UniquePtr const &) = delete;

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/unique_ptr
	UniquePtr& operator=(UniquePtr const &) = delete;

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/unique_ptr
	UniquePtr(UniquePtr && p) : m_ptr{ p.release() }
	{}

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/operator%3D
	UniquePtr& operator=(UniquePtr && p)
	{
		// the next line is important as it is safe with self assignment
		reset(p.release());
		return *this;
	}

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/operator%3D
	UniquePtr& operator=(std::nullptr_t)
	{
		reset();
		return *this;
	}

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/~unique_ptr
	~UniquePtr()
	{
		if (m_ptr)
			m_ptr->Release();
	}

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/release
	T* release()
	{
		auto p = m_ptr;
		m_ptr = nullptr;
		return p;
	}

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/reset
	void reset(T* p = nullptr)
	{
		// the std enforce the order of the operation ( first assign, then delete )
		auto old = m_ptr;
		m_ptr = p;
		if (old)
			old->Release();
	}

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/reset
	void reset(std::nullptr_t)
	{
		reset();
	}

	// this is the ugly part but there is no real clean way to hide the unwanted API
	struct ReleaseAndAddRefHiddenT : public T
	{
	private:
		virtual ULONG Release(void) override;
		virtual ULONG AddRef(void) override;
	};

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/get
	inline ReleaseAndAddRefHiddenT* get() const
	{
		return static_cast<ReleaseAndAddRefHiddenT*>(m_ptr);
	}

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/operator*
	ReleaseAndAddRefHiddenT* operator->() const
	{
		return static_cast<ReleaseAndAddRefHiddenT*>(m_ptr);
	}
	ReleaseAndAddRefHiddenT& operator*() const
	{
		return *static_cast<ReleaseAndAddRefHiddenT*>(m_ptr);
	}

	// Because D3D ojjects are ref counted, we are able to clone if really we need it,
	// still it is better to keep owership in one place and use naked pointer in the rendering code
	// to prune unneccessary AddRef/Release cycles
	UniquePtr Share()
	{
		if (m_ptr)
			m_ptr->AddRef();
		return UniquePtr{ m_ptr };
	}

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/operator_bool
	explicit operator bool() const
	{
		return m_ptr != nullptr;
	}

	// http://en.cppreference.com/w/cpp/memory/unique_ptr/operator_cmp
	friend bool operator!=(UniquePtr const& a, UniquePtr const& b)
	{
		return a.m_ptr != b.m_ptr;
	}
	friend bool operator!=(UniquePtr const& p, std::nullptr_t)
	{
		return p.m_ptr != nullptr;
	}
	friend bool operator!=(std::nullptr_t, UniquePtr const& p)
	{
		return p.m_ptr != nullptr;
	}
	friend bool operator==(UniquePtr const& a, UniquePtr const& b)
	{
		return a.m_ptr == b.m_ptr;
	}
	friend bool operator==(UniquePtr const& p, std::nullptr_t)
	{
		return p.m_ptr == nullptr;
	}
	friend bool operator==(std::nullptr_t, UniquePtr const& p)
	{
		return p.m_ptr == nullptr;
	}
private:
	T* m_ptr{};
};

// Alias to UniquePtr of most of the D3D object 
using VertexShaderPtr = UniquePtr<ID3D11VertexShader>;
using PixelShaderPtr = UniquePtr<ID3D11PixelShader>;
using GeometryShaderPtr = UniquePtr<ID3D11GeometryShader>;
using HullShaderPtr = UniquePtr<ID3D11HullShader>;
using DomainShaderPtr = UniquePtr<ID3D11DomainShader>;
using ComputeShaderPtr = UniquePtr<ID3D11ComputeShader>;
using BufferPtr = UniquePtr<ID3D11Buffer>;
using SrvPtr = UniquePtr<ID3D11ShaderResourceView>;
using UavPtr = UniquePtr<ID3D11UnorderedAccessView>;
using Texture1dPtr = UniquePtr<ID3D11Texture1D>;
using Texture2dPtr = UniquePtr<ID3D11Texture2D>;
using RtvPtr = UniquePtr<ID3D11RenderTargetView>;
using DsvPtr = UniquePtr<ID3D11DepthStencilView>;
using ClkPtr = UniquePtr<ID3D11ClassLinkage>;
using CiPtr = UniquePtr<ID3D11ClassInstance>;
using InputLayoutPtr = UniquePtr<ID3D11InputLayout>;
using BlendStatePtr = UniquePtr<ID3D11BlendState>;

using RasterizerStatePtr = UniquePtr<ID3D11RasterizerState>;
using DepthStencilStatePtr = UniquePtr<ID3D11DepthStencilState>;
using SamplerStatePtr = UniquePtr<ID3D11SamplerState>;
using BufferDescriptor = std::tuple<ID3D11Buffer*, UINT, UINT>;


// helper class to use a UniquePtr in the various CreateSomething( (T**)/(void**) result )
// use with the following function
template <typename T>
struct ToAddrImpl
{
	ToAddrImpl(UniquePtr<T> & ptr) : m_ptr{ ptr }
	{}
	ToAddrImpl(ToAddrImpl const &) = delete;
	ToAddrImpl& operator=(ToAddrImpl const &) = delete;

	operator void**()
	{
		return (void**)&m_ptr.m_ptr;
	}
	operator T**()
	{
		return &m_ptr.m_ptr;
	}
private:
	UniquePtr<T> &m_ptr;
};

// usage example : device->CreateBuffer( bla, ToAddr( m_myBuffer ) );
template <typename T>
inline ToAddrImpl<T> ToAddr(UniquePtr<T>& ptr)
{
	return{ ptr };
}

}  // namespace D3D
}  // namespace DX11
