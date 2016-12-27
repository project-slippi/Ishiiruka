// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <functional>
#include <windows.h>

#include "Common/MsgHandler.h"

#include "Core/Core.h"
#include "Core/ConfigManager.h"
#include "AudioCommon/DSoundStream.h"

bool DSound::CreateBuffer()
{
	PCMWAVEFORMAT pcmwf;
	DSBUFFERDESC dsbdesc;

	memset(&pcmwf, 0, sizeof(PCMWAVEFORMAT));
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));

	pcmwf.wf.wFormatTag = WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels = 2;
	pcmwf.wf.nSamplesPerSec = m_mixer->GetSampleRate();
	pcmwf.wf.nBlockAlign = 4;
	pcmwf.wf.nAvgBytesPerSec = pcmwf.wf.nSamplesPerSec * pcmwf.wf.nBlockAlign;
	pcmwf.wBitsPerSample = 16;

	// Fill out DSound buffer description.
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS;
	dsbdesc.dwBufferBytes = bufferSize = SOUND_FRAME_SIZE * SOUND_SAMPLES_STEREO * (SConfig::GetInstance().iLatency + SOUND_BUFFER_COUNT) * sizeof(s16);
	dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&pcmwf;
	dsbdesc.guid3DAlgorithm = DS3DALG_DEFAULT;

	HRESULT res = ds->CreateSoundBuffer(&dsbdesc, &dsBuffer, nullptr);
	if (SUCCEEDED(res))
	{
		dsBuffer->SetCurrentPosition(0);
		dsBuffer->SetVolume(m_volume);
		return true;
	}
	else
	{
		// Failed.
		PanicAlertT("Sound buffer creation failed: %08x", res);
		dsBuffer = nullptr;
		return false;
	}
}

bool DSound::WriteDataToBuffer(DWORD dwOffset,                  // Our own write cursor.
	char* soundData, // Start of our data.
	DWORD dwSoundBytes) // Size of block to copy.
{
	// I want to record the regular audio to, how do I do that?

	void *ptr1, *ptr2;
	DWORD numBytes1, numBytes2;
	// Obtain memory address of write block. This will be in two parts if the block wraps around.
	HRESULT hr = dsBuffer->Lock(dwOffset, dwSoundBytes, &ptr1, &numBytes1, &ptr2, &numBytes2, 0);

	// If the buffer was lost, restore and retry lock.
	if (DSERR_BUFFERLOST == hr)
	{
		dsBuffer->Restore();
		hr = dsBuffer->Lock(dwOffset, dwSoundBytes, &ptr1, &numBytes1, &ptr2, &numBytes2, 0);
	}
	if (SUCCEEDED(hr))
	{
		memcpy(ptr1, soundData, numBytes1);
		if (ptr2 != 0)
			memcpy(ptr2, soundData + numBytes1, numBytes2);

		// Release the data back to DirectSound.
		dsBuffer->Unlock(ptr1, numBytes1, ptr2, numBytes2);
		return true;
	}

	return false;
}


void DSound::InitializeSoundLoop()
{
	currentPos = 0;
	lastPos = 0;
	dsBuffer->Play(0, 0, DSBPLAY_LOOPING);
}

u32 DSound::SamplesNeeded()
{
	dsBuffer->GetCurrentPosition((DWORD*)&currentPos, 0);
	u32 numBytesToRender = FIX128(ModBufferSize(currentPos - lastPos));
	return numBytesToRender / 4;
}

void DSound::WriteSamples(s16 *src, u32 numsamples)
{
	u32 numBytesToRender = numsamples * 4;
	WriteDataToBuffer(lastPos, (char*)src, numBytesToRender);
	lastPos = ModBufferSize(lastPos + numBytesToRender);
}

bool DSound::SupportSurroundOutput()
{
	return false;
}

bool DSound::Start()
{
	if (FAILED(DirectSoundCreate8(0, &ds, 0)))
		return false;
	if (hWnd)
	{
		HRESULT hr = ds->SetCooperativeLevel((HWND)hWnd, DSSCL_PRIORITY);
	}
	if (!CreateBuffer())
		return false;

	DWORD num1;
	short* p1;
	dsBuffer->Lock(0, bufferSize, (void* *)&p1, &num1, 0, 0, DSBLOCK_ENTIREBUFFER);
	memset(p1, 0, num1);
	dsBuffer->Unlock(p1, num1, 0, 0);
	return SoundStream::Start();
}

void DSound::SetVolume(int volume)
{
	// This is in "dBA attenuation" from 0 to -10000, logarithmic
	m_volume = (int)floor(log10((float)volume) * 5000.0f) - 10000;

	if (dsBuffer != nullptr)
		dsBuffer->SetVolume(m_volume);
}

void DSound::Clear(bool mute)
{
	SoundStream::Clear(mute);
	if (dsBuffer != nullptr)
	{
		if (m_muted)
		{
			dsBuffer->Stop();
		}
		else
		{
			dsBuffer->Play(0, 0, DSBPLAY_LOOPING);
		}
	}
}

void DSound::Stop()
{
	SoundStream::Stop();
	dsBuffer->Stop();
	dsBuffer->Release();
	ds->Release();
}

