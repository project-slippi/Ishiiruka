// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoBackends/DX11/D3DPtr.h"

struct ID3D10Blob;

namespace DX11
{

using ID3DBlobPtr = D3D::UniquePtr<ID3D10Blob>;
// use this class instead ID3D10Blob or ID3D11Blob whenever possible
class D3DBlob
{
public:
	D3DBlob() = default;
	D3DBlob(D3DBlob const &) = delete;
	D3DBlob& operator=(D3DBlob const &) = delete;
	D3DBlob(D3DBlob && b) : blob_{ b.blob_.release() }, data_{ std::move(b.data_) }, size_(b.size_) {
	}

	D3DBlob& operator=(D3DBlob && b)
	{
		*this = nullptr;
		blob_.reset(b.blob_.release());
		data_.reset(b.data_.release());
		size_ = b.size_;
		return *this;
	}

	D3DBlob& operator=(std::nullptr_t)
	{
		if (blob_)
			data_.release();
		blob_.reset();
		size_ = 0;
		return *this;
	}

	void* operator new(std::size_t) = delete;
	void operator delete(void*) = delete;
	void* operator new[](std::size_t) = delete;
	void operator delete[](void*) = delete;


	// memory will be copied into an own buffer
	D3DBlob(unsigned int blob_size, const u8* init_data = nullptr);

	D3DBlob(ID3DBlobPtr && d3dblob);
	D3DBlob& operator=(ID3DBlobPtr && b)
	{
		*this = nullptr;
		blob_.reset(b.release());
		data_.reset((u8*)blob_->GetBufferPointer());
		size_ = blob_->GetBufferSize();
		return *this;
	}

	//void AddRef();
	//unsigned int Release();

	size_t Size() const;
	u8 const* Data() const;

	~D3DBlob();

private:
	ID3DBlobPtr blob_;
	std::unique_ptr<u8[]> data_; // if blob_ is not nil, then the destructor will release.
	size_t size_;

};

}  // namespace