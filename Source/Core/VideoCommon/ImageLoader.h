// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka By Tino

// Copyright 2014 Rodolfo Bogado
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the owner nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once

#include <functional>
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/TextureDecoder.h"

enum DDSCompression
{
	DDSC_NONE,
	DDSC_DXT1,
	DDSC_DXT3,
	DDSC_DXT5
};
struct ImageLoaderParams
{
	std::function<u8*(size_t, bool)> request_buffer_delegate;
	u8* dst;
	const char* Path;
	u32 Width;
	u32 Height;
	u32 data_size;
	s32 forcedchannels;
	s32 formatBPP;
	PC_TexFormat desiredTex;
	PC_TexFormat resultTex;
	u32 nummipmaps;
	ImageLoaderParams()
	{
		Path = nullptr;
		Width = 0;
		Height = 0;
		data_size = 0;
		forcedchannels = 0;
		formatBPP = 0;
		desiredTex = PC_TexFormat::PC_TEX_FMT_NONE;
		resultTex = PC_TexFormat::PC_TEX_FMT_NONE;
		nummipmaps = 0;
	}
};

class ImageLoader
{
public:
	static DDSCompression ReadDDS(ImageLoaderParams& loader_params);
	static bool ReadPNG(ImageLoaderParams& loader_params);
};