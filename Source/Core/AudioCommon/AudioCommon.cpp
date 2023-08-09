// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/AOSoundStream.h"
#include "AudioCommon/AlsaSoundStream.h"
#include "AudioCommon/CoreAudioSoundStream.h"
#include "AudioCommon/CubebStream.h"
#include "AudioCommon/DSoundStream.h"
#include "AudioCommon/Mixer.h"
#include "AudioCommon/NullSoundStream.h"
#include "AudioCommon/OpenALStream.h"
#include "AudioCommon/OpenSLESStream.h"
#include "AudioCommon/PulseAudioStream.h"
#include "AudioCommon/WASAPIStream.h"
#include "AudioCommon/XAudio2Stream.h"
#include "AudioCommon/XAudio2_7Stream.h"
#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/ConfigManager.h"
#include "Core/Movie.h"

#include <string>

// This shouldn't be a global, at least not here.
std::unique_ptr<SoundStream> g_sound_stream;

static bool s_audio_dump_start = false;

// SlippiChange: Added as a hook for the Jukebox to call.
int AudioCommonGetCurrentVolume()
{
	return SConfig::GetInstance().m_IsMuted ? 0 : SConfig::GetInstance().m_Volume;
}

namespace AudioCommon
{
static const int AUDIO_VOLUME_MIN = 0;
static const int AUDIO_VOLUME_MAX = 100;

void InitSoundStream(void *hWnd)
{
	std::string backend = SConfig::GetInstance().sBackend;
	if (backend == BACKEND_CUBEB)
		g_sound_stream = std::make_unique<CubebStream>();
	else if (backend == BACKEND_OPENAL && OpenALStream::isValid())
		g_sound_stream = std::make_unique<OpenALStream>();
	else if (backend == BACKEND_NULLSOUND && NullSound::isValid())
		g_sound_stream = std::make_unique<NullSound>();
	else if (backend == BACKEND_DIRECTSOUND && DSound::isValid())
		g_sound_stream = std::make_unique<DSound>(hWnd);
	else if (backend == BACKEND_SHARED_WASAPI && WASAPIStream::isValid())
		g_sound_stream = std::make_unique<WASAPIStream>(false, "");
	else if (backend.find(BACKEND_EXCLUSIVE_WASAPI) != std::string::npos && WASAPIStream::isValid())
		g_sound_stream = std::make_unique<WASAPIStream>(
		    true, backend.substr((BACKEND_EXCLUSIVE_WASAPI + std::string(" on ")).size(), std::string::npos));
	else if (backend == BACKEND_XAUDIO2)
	{
		if (XAudio2::isValid())
			g_sound_stream = std::make_unique<XAudio2>();
		else if (XAudio2_7::isValid())
			g_sound_stream = std::make_unique<XAudio2_7>();
	}
	else if (backend == BACKEND_AOSOUND && AOSound::isValid())
		g_sound_stream = std::make_unique<AOSound>();
	else if (backend == BACKEND_ALSA && AlsaSound::isValid())
		g_sound_stream = std::make_unique<AlsaSound>();
	else if (backend == BACKEND_COREAUDIO && CoreAudioSound::isValid())
		g_sound_stream = std::make_unique<CoreAudioSound>();
	else if (backend == BACKEND_PULSEAUDIO && PulseAudio::isValid())
		g_sound_stream = std::make_unique<PulseAudio>();
	else if (backend == BACKEND_OPENSLES && OpenSLESStream::isValid())
		g_sound_stream = std::make_unique<OpenSLESStream>();

	if (!g_sound_stream && NullSound::isValid())
	{
		WARN_LOG(AUDIO, "Could not initialize backend %s, using %s instead.", backend.c_str(), BACKEND_NULLSOUND);
		g_sound_stream = std::make_unique<NullSound>();
	}

	UpdateSoundStream();

	if (!g_sound_stream->Start())
	{
		ERROR_LOG(AUDIO, "Could not start backend %s, using %s instead", backend.c_str(), BACKEND_NULLSOUND);

		g_sound_stream = std::make_unique<NullSound>();
		g_sound_stream->Start();
	}

	if (SConfig::GetInstance().m_DumpAudio && !s_audio_dump_start)
		StartAudioDump();
}

void ShutdownSoundStream()
{
	INFO_LOG(AUDIO, "Shutting down sound stream");

	if (g_sound_stream)
	{
		g_sound_stream->Stop();

		if (SConfig::GetInstance().m_DumpAudio && s_audio_dump_start)
			StopAudioDump();

		g_sound_stream.reset();
	}

	INFO_LOG(AUDIO, "Done shutting down sound stream");
}

std::vector<std::string> GetSoundBackends()
{
	std::vector<std::string> backends;

	if (NullSound::isValid())
		backends.push_back(BACKEND_NULLSOUND);
	backends.push_back(BACKEND_CUBEB);
	if (DSound::isValid())
		backends.push_back(BACKEND_DIRECTSOUND);
	if (XAudio2_7::isValid()
#ifndef HAVE_DXSDK
	    || XAudio2::isValid()
#endif
	)
		backends.push_back(BACKEND_XAUDIO2);
	if (AOSound::isValid())
		backends.push_back(BACKEND_AOSOUND);
	if (AlsaSound::isValid())
		backends.push_back(BACKEND_ALSA);
	if (CoreAudioSound::isValid())
		backends.push_back(BACKEND_COREAUDIO);
	if (PulseAudio::isValid())
		backends.push_back(BACKEND_PULSEAUDIO);
	if (OpenALStream::isValid())
		backends.push_back(BACKEND_OPENAL);
	if (OpenSLESStream::isValid())
		backends.push_back(BACKEND_OPENSLES);
	if (WASAPIStream::isValid())
	{
		// backends.push_back(BACKEND_SHARED_WASAPI);
		// disable shared-mode for now, not working correctly
		backends.push_back(std::string(BACKEND_EXCLUSIVE_WASAPI) + " on default device");

		for (const std::string &device : WASAPIStream::GetAudioDevices())
			backends.push_back(std::string(BACKEND_EXCLUSIVE_WASAPI) + " on " + device);
	}
	return backends;
}

bool SupportsDPL2Decoder(const std::string &backend)
{
#ifndef __APPLE__
	if (backend == BACKEND_OPENAL)
		return true;
#endif
	if (backend == BACKEND_PULSEAUDIO)
		return true;
	if (backend == BACKEND_XAUDIO2)
		return true;
	return false;
}

bool SupportsLatencyControl(const std::string &backend)
{
	return true;
}

bool SupportsVolumeChanges(const std::string &backend)
{
	// FIXME: this one should ask the backend whether it supports it.
	//       but getting the backend from string etc. is probably
	//       too much just to enable/disable a stupid slider...
	return backend == BACKEND_COREAUDIO || backend == BACKEND_OPENAL || backend == BACKEND_XAUDIO2 ||
	       backend == BACKEND_DIRECTSOUND || backend == BACKEND_CUBEB ||
	       backend.find(BACKEND_EXCLUSIVE_WASAPI) != std::string::npos || backend == BACKEND_SHARED_WASAPI;
}

void UpdateSoundStream()
{
	if (g_sound_stream)
	{
		int volume = SConfig::GetInstance().m_IsMuted ? 0 : SConfig::GetInstance().m_Volume;
		g_sound_stream->SetVolume(volume);
	}
}

void ClearAudioBuffer(bool mute)
{
	if (g_sound_stream)
		g_sound_stream->Clear(mute);
}

void SendAIBuffer(const short *samples, unsigned int num_samples)
{
	if (!g_sound_stream)
		return;

	if (SConfig::GetInstance().m_DumpAudio && !s_audio_dump_start)
		StartAudioDump();
	else if (!SConfig::GetInstance().m_DumpAudio && s_audio_dump_start)
		StopAudioDump();

	CMixer *pMixer = g_sound_stream->GetMixer();

	if (pMixer && samples)
	{
		pMixer->PushSamples(samples, num_samples);
	}

	g_sound_stream->Update();
}

void StartAudioDump()
{
	std::string audio_file_name_dtk = File::GetUserPath(D_DUMPAUDIO_IDX) + "dtkdump.wav";
	std::string s_dump_directory;
	std::string audio_file_name_dsp;
	if (!SConfig::GetInstance().m_strOutputDirectory.empty())
		s_dump_directory = SConfig::GetInstance().m_strOutputDirectory;
	else
		s_dump_directory = File::GetUserPath(D_DUMPAUDIO_IDX);
	if (!SConfig::GetInstance().m_strOutputFilenameBase.empty())
		audio_file_name_dsp = s_dump_directory + SConfig::GetInstance().m_strOutputFilenameBase + ".wav";
	else
		audio_file_name_dsp = s_dump_directory + "dspdump.wav";
	File::CreateFullPath(audio_file_name_dtk);
	File::CreateFullPath(audio_file_name_dsp);
	g_sound_stream->GetMixer()->StartLogDTKAudio(audio_file_name_dtk);
	g_sound_stream->GetMixer()->StartLogDSPAudio(audio_file_name_dsp);
	s_audio_dump_start = true;
}

void StopAudioDump()
{
	g_sound_stream->GetMixer()->StopLogDTKAudio();
	g_sound_stream->GetMixer()->StopLogDSPAudio();
	s_audio_dump_start = false;
}

void IncreaseVolume(unsigned short offset)
{
	SConfig::GetInstance().m_IsMuted = false;
	int &currentVolume = SConfig::GetInstance().m_Volume;
	currentVolume += offset;
	if (currentVolume > AUDIO_VOLUME_MAX)
		currentVolume = AUDIO_VOLUME_MAX;
	UpdateSoundStream();
}

void DecreaseVolume(unsigned short offset)
{
	SConfig::GetInstance().m_IsMuted = false;
	int &currentVolume = SConfig::GetInstance().m_Volume;
	currentVolume -= offset;
	if (currentVolume < AUDIO_VOLUME_MIN)
		currentVolume = AUDIO_VOLUME_MIN;
	UpdateSoundStream();
}

void ToggleMuteVolume()
{
	bool &isMuted = SConfig::GetInstance().m_IsMuted;
	isMuted = !isMuted;
	UpdateSoundStream();
}
} // namespace AudioCommon
