// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/SoundStream.h"

#ifdef _WIN32
#include <Windows.h>
#include <mmsystem.h>
#include <dsound.h>
#endif

class DSound final: public SoundStream
{
#ifdef _WIN32
	void  *hWnd;

	IDirectSound8* ds;
	IDirectSoundBuffer* dsBuffer;

	int bufferSize;     //i bytes
	int m_volume;

	// playback position
	int currentPos;
	int lastPos;

	inline int FIX128(int x)
	{
		return x & (~127);
	}

	inline int ModBufferSize(int x)
	{
		return (x + bufferSize) % bufferSize;
	}

	bool CreateBuffer();
	bool WriteDataToBuffer(DWORD dwOffset, char* soundData, DWORD dwSoundBytes);
protected:
	virtual void InitializeSoundLoop() override;
	virtual u32 SamplesNeeded() override;
	virtual void WriteSamples(s16 *src, u32 numsamples) override;
	virtual bool SupportSurroundOutput() override;
public:
	DSound(void *_hWnd)
		: bufferSize(0)
		, currentPos(0)
		, lastPos(0)
		, dsBuffer(0)
		, ds(0)
		, hWnd(_hWnd)
	{}

	virtual ~DSound()
	{}

	virtual bool Start()  override;
	virtual void SetVolume(int volume)  override;
	virtual void Stop()  override;
	virtual void Clear(bool mute)  override;
	static bool isValid()
	{
		return true;
	}

#else
public:
	DSound(void *_hWnd)
	{}
#endif
};
