// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/SoundStream.h"

#include <string>
#include <vector>

#ifdef _WIN32
#include <audioclient.h>
#endif

class WASAPIStream final : public SoundStream
{
#ifdef _WIN32
public:
	WASAPIStream(bool exclusive_mode, std::string device = "Default") : m_exclusive_mode(exclusive_mode), m_selected_device(device)
	{
		CoInitialize(nullptr);
	}

	~WASAPIStream()
	{
		if(m_need_data_event)
			CloseHandle(m_need_data_event);
		if(m_renderer)
			m_renderer->Release();
		if(m_audio_client)
			m_audio_client->Release();

		CoUninitialize();
	}

	bool Start() override;
	void SoundLoop() override;
	void Stop() override;

	static bool isValid()
	{
		return true;
	}

	static std::vector<std::string> GetAudioDevices();

private:
	IAudioClient* m_audio_client = nullptr;
	IAudioRenderClient * m_renderer = nullptr;
	std::string m_selected_device;

	HANDLE m_need_data_event = nullptr;

	u32 frames_in_buffer = 0;

	WAVEFORMATEXTENSIBLE fmt;

	bool m_exclusive_mode;
#else
public:
  WASAPIStream(bool exclusive_mode, std::string device = "Default") { }

  inline static std::vector<std::string> GetAudioDevices()
  {
	  return {};
  }
#endif
};
