// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka By Tino

#include <png.h>

#include "ImageLoader.h"


static void PNGErrorFn(png_structp png_ptr, png_const_charp error_message)
{
	ERROR_LOG(VIDEO, "libpng error %s", error_message);
}

static void PNGWarnFn(png_structp png_ptr, png_const_charp error_message)
{
	WARN_LOG(VIDEO, "libpng warning %s", error_message);
}

bool ImageLoader::ReadPNG(ImageLoaderParams &ImgInfo)
{
	// Set up libpng reading.
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, PNGErrorFn, PNGWarnFn);
	if (!png_ptr)
		return false;
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return false;
	}
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		return false;
	}
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return false;
	}
	// We read using a FILE*; this could be changed.
	File::IOFile file;
	if (!file.Open(ImgInfo.Path, "rb"))
	{
		return false;
	}
	png_init_io(png_ptr, file.GetHandle());
	png_set_sig_bytes(png_ptr, 0);
	// Process PNG header, etc.
	png_read_info(png_ptr, info_ptr);
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type, compression_type, filter_method;
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_method);
	// Force RGB (8 or 16-bit).
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);
	// Force 8-bit RGB.
	if (bit_depth == 16)
		png_set_strip_16(png_ptr);
	// Force alpha channel (combined with the above, 8-bit RGBA).
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);
	else if ((color_type & PNG_COLOR_MASK_ALPHA) == 0)
		png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
	png_read_update_info(png_ptr, info_ptr);
	ImgInfo.Width = width;
	ImgInfo.Height = height;
	ImgInfo.data_size = width * height * 4;
	ImgInfo.dst = ImgInfo.request_buffer_delegate(ImgInfo.data_size, false);
	std::vector<u8*> row_pointers(height);
	u8* row_pointer = ImgInfo.dst;
	for (unsigned i = 0; i < height; ++i)
	{
		row_pointers[i] = row_pointer;
		row_pointer += width * 4;
	}
	png_read_image(png_ptr, row_pointers.data());
	png_read_end(png_ptr, end_info);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	return true;
}