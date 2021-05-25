// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <mutex>

#include "Common/CommonTypes.h"

struct GCPadStatus;

namespace GCAdapter
{

enum ControllerTypes
{
	CONTROLLER_NONE = 0,
	CONTROLLER_WIRED = 1,
	CONTROLLER_WIRELESS = 2
};

bool IsReadingAtReducedRate();
double ReadRate();

void Init();
void ResetRumble();
void Shutdown();
void SetAdapterCallback(std::function<void(void)> func);
void StartScanThread();
void StopScanThread();
GCPadStatus Input(int chan, std::chrono::high_resolution_clock::time_point *tp=nullptr);
void Output(int chan, u8 rumble_command);
bool IsDetected();
bool IsDriverDetected();
bool DeviceConnected(int chan);
bool UseAdapter();

static std::mutex kristal_callback_mutex;
void SetKristalInputCallback(
    std::function<void(const GCPadStatus &, std::chrono::high_resolution_clock::time_point, int)> callback);
void ClearKristalInputCallback();

void InformPadModeSet(int chan);

}  // end of namespace GCAdapter
