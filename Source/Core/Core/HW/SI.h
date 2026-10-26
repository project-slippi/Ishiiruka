// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "Common/CommonTypes.h"
#include <chrono>

class PointerWrap;
class ISIDevice;
enum SIDevices : int;
namespace MMIO
{
class Mapping;
}

// SI number of channels
enum
{
	MAX_SI_CHANNELS = 0x04
};

namespace SerialInterface
{
void Init();
void Shutdown();
void DoState(PointerWrap& p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void UpdateDevices();

void RemoveDevice(int _iDeviceNumber);
void AddDevice(const SIDevices _device, int _iDeviceNumber);
void AddDevice(std::unique_ptr<ISIDevice> device);

void ChangeDevice(SIDevices device, int channel);
void ChangeDeviceDeterministic(SIDevices device, int channel);

SIDevices GetDeviceType(int channel);

// The origin and analog mode the game was given by the GameCube controller on a channel. Returns false
// if that device is not a GameCube controller or the game has not asked for its origin yet.
bool GetGCControllerOrigin(int channel, u8 origin[6], u8 *analog_mode);

u32 GetPollXLines();

extern std::chrono::time_point<std::chrono::high_resolution_clock> last_si_read;

}  // end of namespace SerialInterface
