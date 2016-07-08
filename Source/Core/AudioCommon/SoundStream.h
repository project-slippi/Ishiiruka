// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <atomic>
#include <memory>

#include "AudioCommon/Mixer.h"
#include "AudioCommon/WaveFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"


#define SOUND_FRAME_SIZE 2048u
#define SOUND_SAMPLES_STEREO 2u
#define SOUND_SAMPLES_SURROUND 6u
#define SOUND_STEREO_FRAME_SIZE (SOUND_FRAME_SIZE * SOUND_SAMPLES_STEREO)
#define SOUND_SURROUND_FRAME_SIZE (SOUND_FRAME_SIZE * SOUND_SAMPLES_SURROUND)
#define SOUND_STEREO_FRAME_SIZE_BYTES (SOUND_STEREO_FRAME_SIZE * sizeof(s16))
#define SOUND_SURROUND_FRAME_SIZE_BYTES (SOUND_SURROUND_FRAME_SIZE * sizeof(s16))

#define SOUND_MAX_FRAME_SIZE (SOUND_FRAME_SIZE * SOUND_SAMPLES_SURROUND)
#define SOUND_MAX_FRAME_SIZE_BYTES (SOUND_MAX_FRAME_SIZE * sizeof(s16))
#define SOUND_BUFFER_COUNT 3u

class SoundStream
{
protected:
	bool m_enablesoundloop;
	std::unique_ptr<CMixer> m_mixer;
	std::atomic<bool> threadData;
	bool m_logAudio;
	WaveFileWriter g_wave_writer;
	bool m_muted;
	std::unique_ptr<std::thread> thread;
	virtual void SoundLoop();
	virtual void InitializeSoundLoop()
	{}
	virtual u32 SamplesNeeded()
	{
		return 0;
	}
	virtual void WriteSamples(s16 *src, u32 numsamples)
	{}
	virtual bool SupportSurroundOutput()
	{
		return false;
	};
public:
	SoundStream();
	virtual ~SoundStream();

	static  bool isValid()
	{
		return false;
	}
	CMixer *GetMixer() const
	{
		return m_mixer.get();
	}
	virtual bool Start();
	virtual void SetVolume(int)
	{}
	virtual void Stop();
	virtual void Clear(bool mute);
	virtual void Update()
	{};
	bool IsMuted() const
	{
		return m_muted;
	}
	void StartLogAudio(const char *filename);
	void StopLogAudio();
};
