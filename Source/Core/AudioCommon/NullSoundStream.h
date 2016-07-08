// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "AudioCommon/SoundStream.h"

class NullSound final: public SoundStream
{
public:
	NullSound()
	{}

	virtual ~NullSound()
	{}

	virtual bool Start() override;
	virtual void SetVolume(int volume) override;
	virtual void Stop() override;
	virtual void Clear(bool mute) override;
	static bool isValid()
	{
		return true;
	}
	virtual void Update() override;
private:
	static constexpr size_t BUFFER_SIZE = 48000 * 4 / 32;

	// Playback position
	std::array<short, BUFFER_SIZE / sizeof(short)> m_realtime_buffer;
};
