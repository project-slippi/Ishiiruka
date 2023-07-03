// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "AudioCommon/SoundStream.h"
#include "Common/CommonTypes.h"

class CMixer;

extern std::unique_ptr<SoundStream> g_sound_stream;

// SlippiChange: Added as a hook for the Jukebox to call.
//
// Note that this must exist outside of the C++ namespace to ensure that
// there's no issues passing it to the Rust side of things.
//
// The naming is unfortunate, yes - but it at least "fits" with the namespacing.
int AudioCommonGetCurrentVolume();

namespace AudioCommon
{
void InitSoundStream(void* hwnd);
void ShutdownSoundStream();
std::vector<std::string> GetSoundBackends();
bool SupportsDPL2Decoder(const std::string& backend);
bool SupportsLatencyControl(const std::string& backend);
bool SupportsVolumeChanges(const std::string& backend);
void UpdateSoundStream();
void ClearAudioBuffer(bool mute);
void SendAIBuffer(const short* samples, unsigned int num_samples);
void StartAudioDump();
void StopAudioDump();
void IncreaseVolume(unsigned short offset);
void DecreaseVolume(unsigned short offset);
void ToggleMuteVolume();
}
