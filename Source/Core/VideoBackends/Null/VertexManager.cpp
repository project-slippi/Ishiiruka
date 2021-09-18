// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Null/VertexManager.h"

#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace Null
{
class NullNativeVertexFormat : public NativeVertexFormat
{
public:
  NullNativeVertexFormat(const PortableVertexDeclaration& _vtx_decl) {vtx_decl = _vtx_decl;}
  void SetupVertexPointers() override {}
};

std::unique_ptr<NativeVertexFormat>
VertexManager::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<NullNativeVertexFormat>(vtx_decl);
}

VertexManager::VertexManager() : m_local_v_buffer(MAXVBUFFERSIZE), m_local_i_buffer(MAXIBUFFERSIZE)
{
}

VertexManager::~VertexManager()
{
}

void VertexManager::ResetBuffer(u32 stride)
{
  m_pCurBufferPointer = m_pBaseBufferPointer = m_local_v_buffer.data();
  m_pEndBufferPointer = m_pCurBufferPointer + m_local_v_buffer.size();
  IndexGenerator::Start(&m_local_i_buffer[0]);
}

void VertexManager::vFlush(bool use_dst_alpha) {}

}  // namespace
