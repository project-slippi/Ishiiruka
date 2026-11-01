// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>

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

// An input report is a report id byte followed by 9 bytes per port
constexpr int ADAPTER_PAYLOAD_SIZE = 37;

// A report as received from the adapter, along with how many times the game had polled each port (see
// GetGamePollCount) when the report was stored
struct AdapterReport
{
	u64 game_poll_counts[4];
	u8 payload[ADAPTER_PAYLOAD_SIZE];
};

// Called on the adapter read thread for every report received without error. The callback must be
// safe to call at any time from that thread.
using ReportCallback = void (*)(const AdapterReport &report);
void SetReportCallback(ReportCallback callback);
// How many times the game itself has polled a port
u64 GetGamePollCount(int chan);

void ResetAdapterIfNecessary();
bool IsReadingAtReducedRate();
double ReadRate();

void Init();
void ResetRumble();
void Shutdown();
void SetAdapterCallback(std::function<void(void)> func);
void StartScanThread();
void StopScanThread();
// is_game_poll should be set when the game itself is reading the pad, which counts towards GetGamePollCount
GCPadStatus Input(int chan, bool is_game_poll = false);
void Output(int chan, u8 rumble_command);
bool IsDetected();
bool IsDriverDetected();
bool DeviceConnected(int chan);
bool UseAdapter();

}  // end of namespace GCAdapter
