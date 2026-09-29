// Copyright 2025 Dolphin Emulator Project
// Licensed under GPLv3
// Contains logic ported from HayBox (https://github.com/JonnyHaystack/HayBox and
// https://github.com/CarVac/HayBox), which is licensed under the GNU GPL version 3.

// Digital "rectangle" (B0XX-style) GameCube controller logic for Super Smash Bros. Melee.
//
// Translates digital button presses into GameCube stick/trigger values and, when enabled, applies the
// restrictions of the Melee Controller Ruleset (2024) for controllers that replace the control stick
// with buttons: neutral SOCD, travel time simulation, output fuzzing, the L/R 27 degree angle lockout
// and the SDI / pivot tilt / crouch-to-uptilt timing lockouts.
//
// The coordinates and limiter are a port of the ruleset-compliant HayBox firmware
// (https://github.com/CarVac/HayBox, modes/Melee20Button.cpp and modes/MeleeLimits.cpp). The firmware
// counts time in controller samples; this port works on real timestamps in microseconds so it can be
// driven at whatever rate the emulator samples keyboard state.

#pragma once

#include <array>
#include <random>

#include "Common/CommonTypes.h"
#include "InputCommon/GCPadStatus.h"

namespace MeleeRectangle
{
struct Inputs
{
	bool left = false;
	bool right = false;
	bool down = false;
	bool up = false;
	bool mod_x = false;
	bool mod_y = false;
	bool c_left = false;
	bool c_right = false;
	bool c_down = false;
	bool c_up = false;
	bool a = false;
	bool b = false;
	bool x = false;
	bool y = false;
	bool z = false;
	bool l = false;
	bool r = false;
	bool lightshield = false;
	bool midshield = false;
	bool start = false;
	bool dpad_up = false;
	bool dpad_down = false;
	bool dpad_left = false;
	bool dpad_right = false;
};

struct Settings
{
	// Neutral SOCD, travel time, fuzzing and lockouts. When disabled, SOCD falls back to second input
	// priority without reactivation and coordinates are output instantly.
	bool ruleset_limits = true;
	// Use a crouch-walk-safe coordinate for the down diagonals.
	bool crouch_walk_os = false;
};

// Resolves simultaneous opposite cardinal directions for one axis.
class SocdPair
{
public:
	// Both pressed -> neither pressed. Required by the ruleset.
	static void Neutral(bool& dir1, bool& dir2);
	// The most recent press wins; the older direction stays suppressed until it is pressed again.
	void SecondInputPriorityNoReactivation(bool& dir1, bool& dir2);
	void Reset();

private:
	bool m_was_dir1 = false;
	bool m_was_dir2 = false;
	bool m_lock_dir1 = false;
	bool m_lock_dir2 = false;
};

// Maps SOCD-resolved buttons to controller output with the ruleset coordinate set (no timing effects).
GCPadStatus MapToPadStatus(const Inputs& inputs, const Settings& settings);

// Applies travel time, fuzzing and lockouts to the control stick. Stateful; call it every time new
// input is sampled (and whenever the console polls) with a monotonic timestamp.
class Limiter
{
public:
	explicit Limiter(u32 seed = std::random_device{}());

	void Reset();
	GCPadStatus Apply(s64 now_us, const Inputs& inputs, const GCPadStatus& raw);

private:
	static constexpr int HISTORY_LENGTH = 5;

	struct StickTarget
	{
		s64 timestamp;
		u8 travel_ms;
		// Requested coordinate before fuzzing.
		u8 x;
		u8 y;
		u8 x_start;
		u8 y_start;
		// Fuzzed destination.
		u8 x_end;
		u8 y_end;
	};

	// Only movement between zones is recorded, movement within a zone is ignored.
	struct ZoneChange
	{
		s64 timestamp;
		u8 zone;
		bool stale;
	};

	void Initialize(s64 now_us, const Inputs& inputs);
	void Fuzz(u8& x, u8& y);
	u8 DetectTapSdi(s64 now_us) const;

	std::mt19937 m_rng;
	bool m_initialized = false;
	Inputs m_prev_inputs;
	s64 m_last_sample_us = 0;

	std::array<StickTarget, HISTORY_LENGTH> m_stick_history;
	int m_stick_index = 0;
	bool m_done_traveling = true;
	u8 m_prev_x = GCPadStatus::MAIN_STICK_CENTER_X;
	u8 m_prev_y = GCPadStatus::MAIN_STICK_CENTER_Y;

	// Ring buffer, indexed by m_sdi_index.
	std::array<ZoneChange, HISTORY_LENGTH> m_sdi_history;
	int m_sdi_index = 0;
	// Shift register, element 0 is the newest.
	std::array<ZoneChange, HISTORY_LENGTH> m_pivot_history;

	bool m_wavedash_was_nerfed = false;
	bool m_sdi_is_nerfed = false;
	s64 m_sdi_slow_until = 0;

	bool m_above_deadzone = false;
	s64 m_above_deadzone_since = 0;
	s64 m_last_crouch = 0;
	bool m_down_up_jumping = false;
	s64 m_jump_start = 0;
};

// Full per-controller pipeline: SOCD -> coordinate mapping -> ruleset limiter.
class Controller
{
public:
	GCPadStatus Update(const Inputs& inputs, const Settings& settings, s64 now_us);
	void Reset();

private:
	std::array<SocdPair, 4> m_socd;
	Limiter m_limiter;
	bool m_has_last_update = false;
	s64 m_last_update_us = 0;
	bool m_last_ruleset_limits = true;
};
}  // namespace MeleeRectangle
