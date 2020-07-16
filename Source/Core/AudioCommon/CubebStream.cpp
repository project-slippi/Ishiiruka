// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cubeb/cubeb.h>

#include "AudioCommon/CubebStream.h"
#include "AudioCommon/CubebUtils.h"
#include "AudioCommon/DPL2Decoder.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"

// SSBM outputs samples in 5 ms batches - ensures we always have at least one extra batch buffered
constexpr u32 MINIMUM_FRAMES = 480;

long CubebStream::DataCallback(cubeb_stream* stream, void* user_data, const void* /*input_buffer*/,
                               void* output_buffer, long num_frames)
{
  auto* self = static_cast<CubebStream*>(user_data);

  self->m_mixer->Mix(static_cast<short*>(output_buffer), num_frames);

  return num_frames;
}

void CubebStream::StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state)
{
}

bool CubebStream::Start()
{
  m_ctx = CubebUtils::GetContext();
  if (!m_ctx)
    return false;

  cubeb_stream_params params;
  params.rate = m_mixer->GetSampleRate();
  params.channels = 2;
  params.format = CUBEB_SAMPLE_S16NE;
  params.layout = CUBEB_LAYOUT_STEREO;

  u32 minimum_latency = 0; 
  if (cubeb_get_min_latency(m_ctx.get(), &params, &minimum_latency) != CUBEB_OK)
    ERROR_LOG(AUDIO, "Error getting minimum latency");
  minimum_latency = std::max(minimum_latency, MINIMUM_FRAMES);

  INFO_LOG(AUDIO, "Minimum latency: %i frames", minimum_latency);

  if (cubeb_stream_init(m_ctx.get(), &m_stream, "Dolphin Audio Output", nullptr, nullptr, nullptr,
                        &params, minimum_latency, DataCallback,
                        StateCallback, this) != CUBEB_OK)
  {
    ERROR_LOG(AUDIO, "Error initializing cubeb stream");
    return false;
  }

  if (cubeb_stream_start(m_stream) != CUBEB_OK)
  {
	ERROR_LOG(AUDIO, "Error starting cubeb stream");
	return false;
  }

  int volume = SConfig::GetInstance().m_IsMuted ? 0 : SConfig::GetInstance().m_Volume;
  cubeb_stream_set_volume(m_stream, volume / 100.0f);

  return true;
}

void CubebStream::Stop()
{
  if (cubeb_stream_stop(m_stream) != CUBEB_OK)
  {
    ERROR_LOG(AUDIO, "Error stopping cubeb stream");
}

  cubeb_stream_destroy(m_stream);
  m_ctx.reset();
}

void CubebStream::SetVolume(int volume)
{
  cubeb_stream_set_volume(m_stream, volume / 100.0f);
}
