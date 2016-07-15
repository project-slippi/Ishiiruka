// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <d3d11_2.h>

#include "VideoBackends/DX11/D3DBlob.h"

namespace DX11
{

D3DBlob::D3DBlob(unsigned int blob_size, const u8* init_data) :
	data_{ new u8[blob_size] },
	size_{ blob_size }
{
	if (init_data)
		memcpy(data_.get(), init_data, size_);
}

D3DBlob::D3DBlob(ID3DBlobPtr && d3dblob) :
	blob_{ std::move(d3dblob) },
	size_{ 0 }
{
	if (blob_)
	{
		data_.reset((u8*)blob_->GetBufferPointer());
		size_ = blob_->GetBufferSize();
	}
}

D3DBlob::~D3DBlob()
{
	if (blob_)
	{
		data_.release();
	}
}

size_t D3DBlob::Size() const
{
	return size_;
}

u8 const * D3DBlob::Data() const
{
	return data_.get();
}

}  // namespace DX11