// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/CompressedBlob.h"
#include "DiscIO/DiscScrubber.h"

namespace DiscIO
{
static constexpr u64 uncompressed_flag = 1ULL << 63;

bool IsGCZBlob(File::IOFile& file);

CompressedBlobReader::CompressedBlobReader(File::IOFile file, const std::string& filename)
    : m_file(std::move(file)), m_file_name(filename)
{
  m_valid = Initialize();
}

bool CompressedBlobReader::Initialize()
{
  m_file_size = m_file.GetSize();
  m_file.Seek(0, SEEK_SET);
  if (!m_file.ReadArray(&m_header, 1))
    return false;

  if (m_header.magic_cookie != GCZ_MAGIC)
    return false;

  // SectorReader takes the block size as an int and divides by it
  if (m_header.block_size == 0 || m_header.block_size > 0x7FFFFFFF - 64)
  {
    ERROR_LOG(DISCIO, "GCZ file has an invalid block size");
    return false;
  }

  const u64 block_pointers_size = u64(m_header.num_blocks) * sizeof(u64);
  const u64 hashes_size = u64(m_header.num_blocks) * sizeof(u32);

  const u64 header_size = sizeof(CompressedBlobHeader) + block_pointers_size + hashes_size;

  // Basic sanity check for size before we start allocating
  if (header_size > m_file_size)
  {
    ERROR_LOG(DISCIO, "Headers' size is larger than file size");
    return false;
  }

  if (m_header.compressed_data_size > m_file_size - header_size)
  {
    ERROR_LOG(DISCIO, "Data size is larger than file size.");
    return false;
  }

  if (m_header.num_blocks == 0)
  {
    ERROR_LOG(DISCIO, "GCZ file has zero blocks");
    return false;
  }

  // cache block pointers and hashes
  m_block_pointers.resize(m_header.num_blocks);
  if (!m_file.ReadArray(m_block_pointers.data(), m_header.num_blocks))
    return false;

  m_hashes.resize(m_header.num_blocks);
  if (!m_file.ReadArray(m_hashes.data(), m_header.num_blocks))
    return false;

  m_data_offset = header_size;

  // A compressed block is never ever longer than a decompressed block, so just header.block_size
  // should be fine.
  // I still add some safety margin.
  const u32 zlib_buffer_size = m_header.block_size + 64;
  m_zlib_buffer.resize(zlib_buffer_size);

  SetSectorSize(m_header.block_size);

  return ValidateBlockPointers();
}

std::unique_ptr<CompressedBlobReader> CompressedBlobReader::Create(File::IOFile file,
                                                                   const std::string& filename)
{
  if (IsGCZBlob(file))
  {
    std::unique_ptr<CompressedBlobReader> reader(
        new CompressedBlobReader(std::move(file), filename));

    if (reader->m_valid)
      return reader;
  }

  return nullptr;
}

CompressedBlobReader::~CompressedBlobReader()
{
}

// IMPORTANT: Calling this function invalidates all earlier pointers gotten from this function.
u64 CompressedBlobReader::GetBlockCompressedSize(u64 block_num) const
{
  u64 start = m_block_pointers[block_num] & ~uncompressed_flag;
  if (block_num < m_header.num_blocks - 1)
    return (m_block_pointers[block_num + 1] & ~uncompressed_flag) - start;
  else if (block_num == m_header.num_blocks - 1)
    return m_header.compressed_data_size - start;
  else
    PanicAlert("GetBlockCompressedSize - illegal block number %i", (int)block_num);
  return 0;
}

bool CompressedBlobReader::GetBlock(u64 block_num, u8* out_ptr)
{
  if (block_num >= m_header.num_blocks)
    return false;

  bool uncompressed = false;
  u64 read_size = GetBlockCompressedSize(block_num);
  u64 offset = m_block_pointers[block_num] + m_data_offset;

  if (offset & uncompressed_flag)
  {
    if (read_size != m_header.block_size)
    {
      PanicAlert("Uncompressed block with wrong size");
      return false;
    }
    uncompressed = true;
    offset &= ~uncompressed_flag;
  }
  else
  {
    if (read_size > m_zlib_buffer.size())
    {
      PanicAlert("Compressed block is too large");
      return false;
    }
  }

  // clear unused part of zlib buffer. maybe this can be deleted when it works fully.
  memset(&m_zlib_buffer[read_size], 0, m_zlib_buffer.size() - read_size);

  m_file.Seek(offset, SEEK_SET);
  if (!m_file.ReadBytes(m_zlib_buffer.data(), read_size))
  {
    PanicAlertT("The disc image \"%s\" is truncated, some of the data is missing.",
                m_file_name.c_str());
    m_file.Clear();
    return false;
  }

  // First, check hash.
  u32 block_hash = HashAdler32(m_zlib_buffer.data(), read_size);
  if (block_hash != m_hashes[block_num])
    PanicAlertT("The disc image \"%s\" is corrupt.\n"
                "Hash of block %" PRIu64 " is %08x instead of %08x.",
                m_file_name.c_str(), block_num, block_hash, m_hashes[block_num]);

  if (uncompressed)
  {
    std::copy(m_zlib_buffer.begin(), m_zlib_buffer.begin() + m_header.block_size, out_ptr);
  }
  else
  {
    z_stream z = {};
    z.next_in = m_zlib_buffer.data();
    z.avail_in = static_cast<uInt>(read_size);
    if (z.avail_in > m_header.block_size)
    {
      PanicAlert("We have a problem");
    }
    z.next_out = out_ptr;
    z.avail_out = m_header.block_size;
    inflateInit(&z);
    int status = inflate(&z, Z_FULL_FLUSH);
    u32 uncomp_size = m_header.block_size - z.avail_out;
    if (status != Z_STREAM_END)
    {
      // this seem to fire wrongly from time to time
      // to be sure, don't use compressed isos :P
      PanicAlert("Failure reading block %" PRIu64 " - out of data and not at end.", block_num);
    }
    inflateEnd(&z);
    if (uncomp_size != m_header.block_size)
    {
      PanicAlert("Wrong block size");
      return false;
    }
  }
  return true;
}

bool CompressedBlobReader::ValidateBlockPointers() const
{
  size_t valid_pointers = 0;

  // Validate block pointers
  for (u32 i = 0; i < m_header.num_blocks; ++i)
  {
    u64 next;
    if (i + 1 < m_header.num_blocks)
      next = m_block_pointers[i + 1] & ~uncompressed_flag;
    else
      next = m_header.compressed_data_size;

    if (next > m_header.compressed_data_size)
      continue;

    u64 offset = m_block_pointers[i] & ~uncompressed_flag;
    if (offset > next)
      continue;

    bool uncompressed = (m_block_pointers[i] & uncompressed_flag) != 0;
    u64 size = next - offset;

    if (uncompressed && size != m_header.block_size)
      continue;

    if (!uncompressed && size > m_zlib_buffer.size())
      continue;

    valid_pointers++;
  }

  size_t invalid_pointers = m_header.num_blocks - valid_pointers;

  if (invalid_pointers > 0)
    ERROR_LOG(DISCIO, "GCZ file has %zu invalid block pointers", invalid_pointers);

  return invalid_pointers == 0;
}

bool CompressFileToBlob(const std::string& infile_path, const std::string& outfile_path,
                        u32 sub_type, int block_size, CompressCB callback, void* arg)
{
  bool scrubbing = false;

  File::IOFile infile(infile_path, "rb");
  if (IsGCZBlob(infile))
  {
    PanicAlertT("\"%s\" is already compressed! Cannot compress it further.", infile_path.c_str());
    return false;
  }

  if (!infile)
  {
    PanicAlertT("Failed to open the input file \"%s\".", infile_path.c_str());
    return false;
  }

  File::IOFile outfile(outfile_path, "wb");
  if (!outfile)
  {
    PanicAlertT("Failed to open the output file \"%s\".\n"
                "Check that you have permissions to write the target folder and that the media can "
                "be written.",
                outfile_path.c_str());
    return false;
  }

  DiscScrubber disc_scrubber;
  if (sub_type == 1)
  {
    if (!disc_scrubber.SetupScrub(infile_path, block_size))
    {
      PanicAlertT("\"%s\" failed to be scrubbed. Probably the image is corrupt.",
                  infile_path.c_str());
      return false;
    }

    scrubbing = true;
  }

  z_stream z = {};
  if (deflateInit(&z, 9) != Z_OK)
    return false;

  callback(GetStringT("Files opened, ready to compress."), 0, arg);

  CompressedBlobHeader header;
  header.magic_cookie = GCZ_MAGIC;
  header.sub_type = sub_type;
  header.block_size = block_size;
  header.data_size = infile.GetSize();

  // round upwards!
  header.num_blocks = (u32)((header.data_size + (block_size - 1)) / block_size);

  std::vector<u64> offsets(header.num_blocks);
  std::vector<u32> hashes(header.num_blocks);
  std::vector<u8> out_buf(block_size);
  std::vector<u8> in_buf(block_size);

  // seek past the header (we will write it at the end)
  outfile.Seek(sizeof(CompressedBlobHeader), SEEK_CUR);
  // seek past the offset and hash tables (we will write them at the end)
  outfile.Seek((sizeof(u64) + sizeof(u32)) * header.num_blocks, SEEK_CUR);

  // Now we are ready to write compressed data!
  u64 position = 0;
  int num_compressed = 0;
  int num_stored = 0;
  int progress_monitor = std::max<int>(1, header.num_blocks / 1000);
  bool success = true;

  for (u32 i = 0; i < header.num_blocks; i++)
  {
    if (i % progress_monitor == 0)
    {
      const u64 inpos = infile.Tell();
      int ratio = 0;
      if (inpos != 0)
        ratio = (int)(100 * position / inpos);

      std::string temp =
          StringFromFormat(GetStringT("%i of %i blocks. Compression ratio %i%%").c_str(), i,
                           header.num_blocks, ratio);
      bool was_cancelled = !callback(temp, (float)i / (float)header.num_blocks, arg);
      if (was_cancelled)
      {
        success = false;
        break;
      }
    }

    offsets[i] = position;

    size_t read_bytes;
    if (scrubbing)
      read_bytes = disc_scrubber.GetNextBlock(infile, in_buf.data());
    else
      infile.ReadArray(in_buf.data(), header.block_size, &read_bytes);
    if (read_bytes < header.block_size)
      std::fill(in_buf.begin() + read_bytes, in_buf.begin() + header.block_size, 0);

    int retval = deflateReset(&z);
    z.next_in = in_buf.data();
    z.avail_in = header.block_size;
    z.next_out = out_buf.data();
    z.avail_out = block_size;

    if (retval != Z_OK)
    {
      ERROR_LOG(DISCIO, "Deflate failed");
      success = false;
      break;
    }

    int status = deflate(&z, Z_FINISH);
    int comp_size = block_size - z.avail_out;

    u8* write_buf;
    int write_size;
    if ((status != Z_STREAM_END) || (z.avail_out < 10))
    {
      // PanicAlert("%i %i Store %i", i*block_size, position, comp_size);
      // let's store uncompressed
      write_buf = in_buf.data();
      offsets[i] |= 0x8000000000000000ULL;
      write_size = block_size;
      num_stored++;
    }
    else
    {
      // let's store compressed
      // PanicAlert("Comp %i to %i", block_size, comp_size);
      write_buf = out_buf.data();
      write_size = comp_size;
      num_compressed++;
    }

    if (!outfile.WriteBytes(write_buf, write_size))
    {
      PanicAlertT("Failed to write the output file \"%s\".\n"
                  "Check that you have enough space available on the target drive.",
                  outfile_path.c_str());
      success = false;
      break;
    }

    position += write_size;

    hashes[i] = HashAdler32(write_buf, write_size);
  }

  header.compressed_data_size = position;

  if (!success)
  {
    // Remove the incomplete output file.
    outfile.Close();
    File::Delete(outfile_path);
  }
  else
  {
    // Okay, go back and fill in headers
    outfile.Seek(0, SEEK_SET);
    outfile.WriteArray(&header, 1);
    outfile.WriteArray(offsets.data(), header.num_blocks);
    outfile.WriteArray(hashes.data(), header.num_blocks);
  }

  // Cleanup
  deflateEnd(&z);

  if (success)
  {
    callback(GetStringT("Done compressing disc image."), 1.0f, arg);
  }
  return success;
}

bool DecompressBlobToFile(const std::string& infile_path, const std::string& outfile_path,
                          CompressCB callback, void* arg)
{
  std::unique_ptr<CompressedBlobReader> reader;
  {
    File::IOFile infile(infile_path, "rb");
    if (!IsGCZBlob(infile))
    {
      PanicAlertT("File not compressed");
      return false;
    }

    reader = CompressedBlobReader::Create(std::move(infile), infile_path);
  }

  if (!reader)
  {
    PanicAlertT("Failed to open the input file \"%s\".", infile_path.c_str());
    return false;
  }

  File::IOFile outfile(outfile_path, "wb");
  if (!outfile)
  {
    PanicAlertT("Failed to open the output file \"%s\".\n"
                "Check that you have permissions to write the target folder and that the media can "
                "be written.",
                outfile_path.c_str());
    return false;
  }

  const CompressedBlobHeader& header = reader->GetHeader();
  static const size_t BUFFER_BLOCKS = 32;
  size_t buffer_size = header.block_size * BUFFER_BLOCKS;
  size_t last_buffer_size = header.block_size * (header.num_blocks % BUFFER_BLOCKS);
  std::vector<u8> buffer(buffer_size);
  u32 num_buffers = (header.num_blocks + BUFFER_BLOCKS - 1) / BUFFER_BLOCKS;
  int progress_monitor = std::max<int>(1, num_buffers / 100);
  bool success = true;

  for (u64 i = 0; i < num_buffers; i++)
  {
    if (i % progress_monitor == 0)
    {
      bool was_cancelled = !callback(GetStringT("Unpacking"), (float)i / (float)num_buffers, arg);
      if (was_cancelled)
      {
        success = false;
        break;
      }
    }
    const size_t sz = i == num_buffers - 1 ? last_buffer_size : buffer_size;
    reader->Read(i * buffer_size, sz, buffer.data());
    if (!outfile.WriteBytes(buffer.data(), sz))
    {
      PanicAlertT("Failed to write the output file \"%s\".\n"
                  "Check that you have enough space available on the target drive.",
                  outfile_path.c_str());
      success = false;
      break;
    }
  }

  if (!success)
  {
    // Remove the incomplete output file.
    outfile.Close();
    File::Delete(outfile_path);
  }
  else
  {
    outfile.Resize(header.data_size);
  }

  return true;
}

bool IsGCZBlob(File::IOFile& file)
{
  CompressedBlobHeader header;
  return file.Seek(0, SEEK_SET) && file.ReadArray(&header, 1) && header.magic_cookie == GCZ_MAGIC;
}

}  // namespace
