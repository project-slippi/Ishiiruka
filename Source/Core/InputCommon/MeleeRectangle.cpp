// Copyright 2025 Dolphin Emulator Project
// Licensed under GPLv3
// Contains logic ported from HayBox (https://github.com/JonnyHaystack/HayBox and
// https://github.com/CarVac/HayBox), which is licensed under the GNU GPL version 3.

#include "InputCommon/MeleeRectangle.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace MeleeRectangle
{
namespace
{
constexpr int NEUTRAL = 128;

// Cardinals target 1.4 (112 units), outside the Melee unit circle, so that dashing (back) out of crouch
// is never polled in the uncrouch zone. The ruleset permits this and Melee clamps to the unit circle.
constexpr int CARDINAL = 112;

// Melee analog thresholds.
constexpr int DEAD_MIN = NEUTRAL - 22;
constexpr int DEAD_MAX = NEUTRAL + 22;
constexpr int CROUCH = NEUTRAL - 50;
constexpr int TAP_JUMP = NEUTRAL + 53;
constexpr int DASH_LEFT = NEUTRAL - 64;
constexpr int DASH_RIGHT = NEUTRAL + 64;
constexpr int SDI_LEFT = NEUTRAL - 56;
constexpr int SDI_RIGHT = NEUTRAL + 56;
constexpr int UTILT_LEFT = NEUTRAL - 44;
constexpr int UTILT_RIGHT = NEUTRAL + 44;
// Squared radii: diagonal SDI, and the rim at 6/7/8 ms of travel.
constexpr int SDI_RADIUS_SQ = 3136;
constexpr int RIM_RADIUS_SQ_1 = 6185;
constexpr int RIM_RADIUS_SQ_2 = 6858;
constexpr int RIM_RADIUS_SQ_3 = 8979;

// Travel times, in milliseconds.
constexpr u8 TRAVEL_RIM_1 = 6;
constexpr u8 TRAVEL_RIM_2 = 7;
constexpr u8 TRAVEL_RIM_3 = 8;
constexpr u8 TRAVEL_INTERNAL = 12;
// Used to slow down burst tap SDI (5.5 frames).
constexpr u8 TRAVEL_SLOW = 88;

// Timing windows, in microseconds.
constexpr s64 FRAME_US = 16668;
constexpr s64 HALF_FRAME_US = 8332;
constexpr s64 DEBOUNCE_US = 6000;
constexpr s64 SIMULTANEOUS_US = 2000;
constexpr s64 TAP_US = 88000;
constexpr s64 TAP_PLUS_US = 136000;
constexpr s64 CARDINAL_DIAGONAL_US = 128000;
constexpr s64 WANK_US = 88000;
constexpr s64 PIVOT_TILT_US = 128000;
constexpr s64 SDI_SLOW_US = 64000;
constexpr s64 TAP_JUMP_SHUTOFF_US = 64000;
constexpr s64 DOWN_UP_US = 48000;
constexpr s64 JUMP_US = 32000;
constexpr s64 SDI_STALE_US = 256000;
constexpr s64 PIVOT_STALE_US = 240000;
constexpr s64 LONG_AGO = std::numeric_limits<s64>::min() / 2;

constexpr u8 ZONE_U = 0b0000'0001;
constexpr u8 ZONE_D = 0b0000'0010;
constexpr u8 ZONE_L = 0b0000'0100;
constexpr u8 ZONE_R = 0b0000'1000;

constexpr u8 SDI_WANK = 0b0001'0000;
constexpr u8 SDI_TAP_CARDINAL = 0b0010'0000;
constexpr u8 SDI_TAP_DIAGONAL = 0b0100'0000;
constexpr u8 SDI_TAP_CARDINAL_DIAGONAL = 0b1000'0000;

enum class Pivot
{
	None,
	LeftRight,
	RightLeft,
};

int Magnitude(int value)
{
	return std::abs(value - NEUTRAL);
}

// 0 for a coordinate that takes the internal travel time, otherwise 1-3 for rim cardinals and diagonals
// (within 3 units of 45 degrees), from the rim outwards.
int RimLevel(u8 x, u8 y)
{
	const int x_norm = Magnitude(x);
	const int y_norm = Magnitude(y);
	const int radius_sq = x_norm * x_norm + y_norm * y_norm;
	if (radius_sq < RIM_RADIUS_SQ_1)
		return 0;
	const int diff = std::max(x_norm, y_norm) - std::min(x_norm, y_norm);
	if (diff > 6 && x_norm != 0 && y_norm != 0)
		return 0;
	if (radius_sq >= RIM_RADIUS_SQ_3)
		return 3;
	if (radius_sq >= RIM_RADIUS_SQ_2)
		return 2;
	return 1;
}

// Cardinal thresholds are SDI, diagonal thresholds are the deadzone.
u8 SdiZone(u8 x, u8 y)
{
	const int x_norm = Magnitude(x);
	const int y_norm = Magnitude(y);
	const int radius_sq = x_norm * x_norm + y_norm * y_norm;
	if (x >= DEAD_MIN && x <= DEAD_MAX)
	{
		if (y < SDI_LEFT)
			return ZONE_D;
		if (y > SDI_RIGHT)
			return ZONE_U;
		return 0;
	}
	const u8 side = x < DEAD_MIN ? ZONE_L : ZONE_R;
	if (y < DEAD_MIN && radius_sq >= SDI_RADIUS_SQ)
		return ZONE_D | side;
	if (y > DEAD_MAX && radius_sq >= SDI_RADIUS_SQ)
		return ZONE_U | side;
	if ((side == ZONE_L && x <= SDI_LEFT) || (side == ZONE_R && x >= SDI_RIGHT))
		return side;
	return 0;
}

u8 PivotZone(u8 x)
{
	if (x <= DASH_LEFT)
		return ZONE_L;
	if (x >= DASH_RIGHT)
		return ZONE_R;
	return 0;
}

int PopcountZone(u8 bits)
{
	int count = 0;
	for (int i = 0; i < 4; ++i)
		count += (bits >> i) & 1;
	return count;
}

// Is the angle closer than 27 degrees to an axis? 80/157 is the closest ratio to tan(27).
bool IsShallowerThan27Degrees(int x_mag, int y_mag)
{
	return ((y_mag * 157 < x_mag * 80) || (x_mag * 157 < y_mag * 80)) && x_mag != 0 && y_mag != 0;
}

u8 AwayFromCenter(u8 reference, int magnitude)
{
	return static_cast<u8>(reference < NEUTRAL ? NEUTRAL - magnitude : NEUTRAL + magnitude);
}

// Linear travel from start to destination. Matches the firmware's integer math: progress is counted in
// 1/256ths and the output snaps to the destination once the travel time has elapsed.
void Travel(s64 now_us, s64 input_us, u8 travel_ms, u8 start_x, u8 start_y, u8 dest_x, u8 dest_y,
	bool& done_traveling, u8& out_x, u8& out_y)
{
	const s64 elapsed_4us = (now_us - input_us) / 4;
	const int progress = static_cast<int>(std::min<s64>(256, elapsed_4us / travel_ms));
	out_x = static_cast<u8>(start_x + ((dest_x - start_x) * progress) / 256);
	out_y = static_cast<u8>(start_y + ((dest_y - start_y) * progress) / 256);

	if (elapsed_4us > travel_ms * 250)
		done_traveling = true;
	if (done_traveling)
	{
		out_x = dest_x;
		out_y = dest_y;
	}
}
}  // namespace

void SocdPair::Neutral(bool& dir1, bool& dir2)
{
	if (dir1 && dir2)
	{
		dir1 = false;
		dir2 = false;
	}
}

void SocdPair::SecondInputPriorityNoReactivation(bool& dir1, bool& dir2)
{
	bool is_dir1 = false;
	bool is_dir2 = false;
	if (dir1 && dir2)
	{
		if (m_was_dir2)
		{
			is_dir1 = true;
			is_dir2 = false;
			m_lock_dir2 = true;
		}
		if (m_was_dir1)
		{
			is_dir1 = false;
			is_dir2 = true;
			m_lock_dir1 = true;
		}
	}
	if (!dir1 && dir2 && !m_lock_dir2)
	{
		is_dir1 = false;
		is_dir2 = true;
		m_was_dir2 = true;
		m_was_dir1 = false;
		m_lock_dir1 = false;
	}
	if (dir1 && !dir2 && !m_lock_dir1)
	{
		is_dir1 = true;
		is_dir2 = false;
		m_was_dir1 = true;
		m_was_dir2 = false;
		m_lock_dir2 = false;
	}
	if (!dir1 && !dir2)
		Reset();
	dir1 = is_dir1;
	dir2 = is_dir2;
}

void SocdPair::Reset()
{
	m_was_dir1 = false;
	m_was_dir2 = false;
	m_lock_dir1 = false;
	m_lock_dir2 = false;
}

GCPadStatus MapToPadStatus(const Inputs& in, const Settings& settings)
{
	GCPadStatus pad = {};

	if (in.a)
		pad.button |= PAD_BUTTON_A;
	if (in.b)
		pad.button |= PAD_BUTTON_B;
	if (in.x)
		pad.button |= PAD_BUTTON_X;
	if (in.y)
		pad.button |= PAD_BUTTON_Y;
	if (in.z)
		pad.button |= PAD_TRIGGER_Z;
	if (in.l)
		pad.button |= PAD_TRIGGER_L;
	if (in.r)
		pad.button |= PAD_TRIGGER_R;
	if (in.start)
		pad.button |= PAD_BUTTON_START;

	// Holding both modifiers turns the C-buttons into a D-pad.
	const bool dpad_layer = in.mod_x && in.mod_y;
	if (in.dpad_up || (dpad_layer && in.c_up))
		pad.button |= PAD_BUTTON_UP;
	if (in.dpad_down || (dpad_layer && in.c_down))
		pad.button |= PAD_BUTTON_DOWN;
	if (in.dpad_left || (dpad_layer && in.c_left))
		pad.button |= PAD_BUTTON_LEFT;
	if (in.dpad_right || (dpad_layer && in.c_right))
		pad.button |= PAD_BUTTON_RIGHT;

	if (in.a)
		pad.analogA = 0xFF;
	if (in.b)
		pad.analogB = 0xFF;

	const int x = in.left ? -1 : (in.right ? 1 : 0);
	const int y = in.down ? -1 : (in.up ? 1 : 0);
	const int cx = in.c_left ? -1 : (in.c_right ? 1 : 0);
	const int cy = in.c_down ? -1 : (in.c_up ? 1 : 0);
	const bool horizontal = x != 0;
	const bool vertical = y != 0;
	const bool diagonal = horizontal && vertical;

	int stick_x = NEUTRAL + x * CARDINAL;
	int stick_y = NEUTRAL + y * CARDINAL;
	int c_stick_x = NEUTRAL + cx * CARDINAL;
	int c_stick_y = NEUTRAL + cy * CARDINAL;
	const auto set_stick = [&](int x_units, int y_units) {
		stick_x = NEUTRAL + x * x_units;
		stick_y = NEUTRAL + y * y_units;
	};

	const bool shield = in.l || in.r || in.lightshield || in.midshield;

	if (diagonal)
	{
		// 0.7000 0.7000, skewed to 0.6875 0.7125 to leave room for fuzzing.
		set_stick(56, 61);
		// 0.7250 0.6750, keeps shield drops and crouch walking out of the crouch range.
		if (y == -1 && settings.crouch_walk_os)
			set_stick(61, 56);
	}

	if (in.mod_x)
	{
		if (horizontal)
			stick_x = NEUTRAL + x * 53;
		// 0.5250 rather than 0.5375, since y=-0.5500 is the solo Nana ice block coordinate.
		if (vertical)
			stick_y = NEUTRAL + y * 42;

		if (diagonal && shield)
		{
			if (!in.b)
				set_stick(51, 30);  // 30.47 degree wavedash
			else
				set_stick(68, 41);  // extended for DI with similar magnitude
		}

		// Up-B angles
		if (diagonal && !shield)
		{
			if (!in.b)
			{
				set_stick(58, 25);  // 23.32 degrees
				if (in.c_down)
					set_stick(56, 29);  // 27.38 degrees
				if (in.c_left)
					set_stick(53, 33);  // 31.91 degrees
				if (in.c_up)
					set_stick(51, 37);  // 35.96 degrees
				if (in.c_right)
					set_stick(49, 41);  // 39.92 degrees
			}
			else
			{
				set_stick(73, 31);  // 23.01 degrees
				if (in.c_down)
					set_stick(70, 36);  // 27.22 degrees
				if (in.c_left)
					set_stick(68, 42);  // 31.70 degrees
				if (in.c_up)
					set_stick(58, 42);  // 35.91 degrees
				if (in.c_right)
					set_stick(51, 42);  // 39.47 degrees
			}
		}

		// Angled forward smash (control stick up/down + Mod X + C left/right).
		if (cx != 0 && y != 0)
		{
			c_stick_x = NEUTRAL + cx * 68;
			c_stick_y = NEUTRAL + y * 42;
		}
	}

	if (in.mod_y)
	{
		if (horizontal)
			stick_x = NEUTRAL + x * 27;
		// Turnaround neutral-B nerf: B forces a side-B coordinate.
		if (in.b)
			stick_x = NEUTRAL + x * 80;
		if (vertical)
			stick_y = NEUTRAL + y * 59;

		if (diagonal && shield)
		{
			set_stick(38, 70);
			if (y == -1)
				set_stick(40, 68);
		}

		// Up-B angles
		if (diagonal && !shield)
		{
			if (!in.b)
			{
				set_stick(26, 61);  // 66.91 degrees
				if (in.c_down)
					set_stick(29, 56);  // 62.62 degrees
				if (in.c_left)
					set_stick(35, 56);  // 57.99 degrees
				if (in.c_up)
					set_stick(41, 56);  // 53.79 degrees
				if (in.c_right)
					set_stick(46, 57);  // 51.10 degrees
			}
			else
			{
				set_stick(31, 73);  // 66.99 degrees
				if (in.c_down)
					set_stick(37, 70);  // 62.14 degrees
				if (in.c_left)
					set_stick(42, 68);  // 58.30 degrees
				if (in.c_up)
					set_stick(46, 63);  // 53.86 degrees
				if (in.c_right)
					set_stick(46, 57);  // 51.10 degrees
			}
		}
	}

	// C-stick ASDI slideoff angle overrides other C-stick modifiers.
	if (cx != 0 && cy != 0)
	{
		c_stick_x = NEUTRAL + cx * 42;
		c_stick_y = NEUTRAL + cy * 68;
	}

	if (in.lightshield)
		pad.triggerRight = 49;
	if (in.midshield)
		pad.triggerRight = 94;
	if (in.l)
		pad.triggerLeft = 140;
	if (in.r)
		pad.triggerRight = 140;

	if (dpad_layer)
	{
		c_stick_x = NEUTRAL;
		c_stick_y = NEUTRAL;
	}

	pad.stickX = static_cast<u8>(stick_x);
	pad.stickY = static_cast<u8>(stick_y);
	pad.substickX = static_cast<u8>(c_stick_x);
	pad.substickY = static_cast<u8>(c_stick_y);
	return pad;
}

Limiter::Limiter(u32 seed) : m_rng(seed)
{
}

void Limiter::Reset()
{
	m_initialized = false;
}

void Limiter::Initialize(s64 now_us, const Inputs& inputs)
{
	for (auto& target : m_stick_history)
	{
		target.timestamp = now_us;
		target.travel_ms = TRAVEL_RIM_1;
		target.x = target.x_start = target.x_end = NEUTRAL;
		target.y = target.y_start = target.y_end = NEUTRAL;
	}
	for (auto& change : m_sdi_history)
		change = {now_us, 0, true};
	for (auto& change : m_pivot_history)
		change = {now_us, 0, true};

	m_prev_inputs = inputs;
	m_stick_index = 0;
	m_sdi_index = 0;
	m_done_traveling = true;
	m_prev_x = NEUTRAL;
	m_prev_y = NEUTRAL;
	m_wavedash_was_nerfed = false;
	m_sdi_is_nerfed = false;
	m_sdi_slow_until = LONG_AGO;
	m_above_deadzone = false;
	m_above_deadzone_since = now_us;
	m_last_sample_us = now_us;
	m_last_crouch = LONG_AGO;
	m_down_up_jumping = false;
	m_jump_start = LONG_AGO;
	m_initialized = true;
}

// Moves each non-neutral axis one unit in or out with 25% probability each:
//   1 2 1
//   2 4 2  (out of 16)
//   1 2 1
void Limiter::Fuzz(u8& x, u8& y)
{
	const u32 random = m_rng() & 0xF;
	const int left = (random & 0b0011) == 0;
	const int right = (random & 0b0011) == 0b0011;
	const int up = (random & 0b1100) == 0;
	const int down = (random & 0b1100) == 0b1100;
	if (x != NEUTRAL)
		x = static_cast<u8>(x - left + right);
	if (y != NEUTRAL)
		y = static_cast<u8>(y - down + up);
}

// Detects burst SDI patterns in recent zone changes. Returns SDI_* flags, plus the last cardinal zone
// entered before the most recent diagonal (to know which axis to lock).
u8 Limiter::DetectTapSdi(s64 now_us) const
{
	u8 zones[HISTORY_LENGTH];
	s64 times[HISTORY_LENGTH];
	bool stale[HISTORY_LENGTH];
	for (int i = 0; i < HISTORY_LENGTH; ++i)
	{
		const auto& change = m_sdi_history[(m_sdi_index - i + HISTORY_LENGTH) % HISTORY_LENGTH];
		zones[i] = change.zone;
		times[i] = change.timestamp;
		stale[i] = change.stale;
	}

	u8 output = 0;

	// Repeated center-cardinal or cardinal-diagonal alternation.
	if (zones[0] != zones[1] && zones[0] == zones[2] && zones[1] == zones[3])
	{
		const s64 since_start = now_us - times[2];
		const s64 period = times[0] - times[2];
		if (!stale[2] && since_start < TAP_PLUS_US && period < TAP_US && since_start > DEBOUNCE_US)
		{
			if (zones[0] == 0 || zones[1] == 0)
				output |= SDI_TAP_CARDINAL;
			else
				output |= SDI_TAP_DIAGONAL;
		}
	}

	// Center, one cardinal and the same diagonal at least twice within the last five zones.
	{
		u8 diagonal_zone = 0xFF;
		int origin_count = 0;
		int cardinal_count = 0;
		int diagonal_count = 0;
		for (int i = 0; i < HISTORY_LENGTH; ++i)
		{
			const int count = PopcountZone(zones[i]);
			if (count == 0)
			{
				origin_count++;
			}
			else if (count == 1)
			{
				cardinal_count++;
			}
			else
			{
				diagonal_count++;
				diagonal_zone &= zones[i];
			}
		}
		const bool same_diagonal = PopcountZone(diagonal_zone) == 2;
		const bool short_time = (times[0] - times[4]) < CARDINAL_DIAGONAL_US &&
		                        (times[0] - times[1]) > SIMULTANEOUS_US && !stale[4];
		if (same_diagonal && origin_count && cardinal_count && diagonal_count > 1 && short_time)
			output |= SDI_TAP_CARDINAL_DIAGONAL;
	}

	// Quarter circle: center, a cardinal, then two diagonals adjacent to it.
	{
		u8 cardinal_zone = 0xFF;
		u8 diagonal_zone = 0xFF;
		int origin_count = 0;
		int cardinal_count = 0;
		int diagonal_count = 0;
		for (int i = 0; i < HISTORY_LENGTH; ++i)
		{
			const int count = PopcountZone(zones[i]);
			if (count == 0)
			{
				origin_count++;
				break;
			}
			else if (count == 1)
			{
				cardinal_count++;
				cardinal_zone &= zones[i];
			}
			else
			{
				diagonal_count++;
				diagonal_zone &= zones[i];
			}
		}
		const bool adjacent_diagonal = PopcountZone(diagonal_zone & cardinal_zone) == 1;
		const bool short_time = (times[0] - times[3]) < WANK_US && !stale[3];
		if (adjacent_diagonal && origin_count && cardinal_count && diagonal_count > 1 && short_time)
			output |= SDI_WANK;
	}

	// Oscillating around a diagonal, e.g. left, up-left, up, up-left.
	if (!output && zones[0] == zones[2] && PopcountZone(zones[0]) == 2)
	{
		if ((times[0] - times[2]) < WANK_US && !stale[2])
			output |= SDI_WANK | SDI_TAP_CARDINAL_DIAGONAL;
	}

	bool seen_diagonal = false;
	for (int i = 0; i < HISTORY_LENGTH; ++i)
	{
		const int count = PopcountZone(zones[i]);
		if (count == 2)
			seen_diagonal = true;
		if (count == 1 && seen_diagonal)
		{
			output |= zones[i];
			break;
		}
	}
	return output;
}

GCPadStatus Limiter::Apply(s64 now_us, const Inputs& inputs, const GCPadStatus& raw)
{
	if (!m_initialized)
		Initialize(now_us, inputs);

	StickTarget* target = &m_stick_history[m_stick_index];

	// L/R angle lockout: while L or R is held, angles closer than 27 degrees to an axis are pushed out to
	// 27 degrees immediately (no travel time). If pressing L/R is the only thing that changed the
	// coordinate, the new coordinate is also taken immediately, so L/R can't be used as a modifier that
	// restarts travel time.
	bool wavedash_skip = false;
	if (inputs.l || inputs.r)
	{
		const u8 x_in = raw.stickX;
		const u8 y_in = raw.stickY;
		u8 x_end = x_in;
		u8 y_end = y_in;
		const int x_mag = Magnitude(x_in);
		const int y_mag = Magnitude(y_in);
		const int radius_sq = x_mag * x_mag + y_mag * y_mag;

		if (target->x != x_in || target->y != y_in)
		{
			if (IsShallowerThan27Degrees(x_mag, y_mag))
			{
				// The closest coordinates to 27 degrees inside the unit circle are 51,26 (27.01 degrees).
				const int long_coord = radius_sq > 5625 ? 70 : 51;
				const int short_coord = radius_sq > 5625 ? 36 : 26;
				x_end = AwayFromCenter(x_in, x_mag > y_mag ? long_coord : short_coord);
				y_end = AwayFromCenter(y_in, y_mag > x_mag ? long_coord : short_coord);
				Fuzz(x_end, y_end);
				target->x_end = x_end;
				target->y_end = y_end;
				target->x = x_in;
				target->y = y_in;
				m_wavedash_was_nerfed = true;
			}
			else
			{
				m_wavedash_was_nerfed = false;
				const Inputs& prev = m_prev_inputs;
				const bool only_trigger_changed =
					!(prev.l || prev.r) && prev.left == inputs.left && prev.right == inputs.right &&
					prev.down == inputs.down && prev.up == inputs.up && prev.c_left == inputs.c_left &&
					prev.c_right == inputs.c_right && prev.c_down == inputs.c_down && prev.c_up == inputs.c_up &&
					prev.b == inputs.b && prev.lightshield == inputs.lightshield &&
					prev.midshield == inputs.midshield && prev.mod_x == inputs.mod_x && prev.mod_y == inputs.mod_y;
				if (only_trigger_changed)
				{
					Fuzz(x_end, y_end);
					target->x_end = x_end;
					target->y_end = y_end;
					target->x = x_in;
					target->y = y_in;
					wavedash_skip = true;
				}
			}
		}

		// Pressing L/R without moving the coordinate can also make the held angle illegal.
		if (IsShallowerThan27Degrees(x_mag, y_mag) && !m_wavedash_was_nerfed)
		{
			x_end = AwayFromCenter(x_in, x_mag > y_mag ? 51 : 26);
			y_end = AwayFromCenter(y_in, y_mag > x_mag ? 51 : 26);
			Fuzz(x_end, y_end);
			target->x_end = x_end;
			target->y_end = y_end;
			m_wavedash_was_nerfed = true;
		}
	}
	else if (m_prev_inputs.l || m_prev_inputs.r)
	{
		// Undo the lockout once L and R are released.
		if (m_wavedash_was_nerfed)
		{
			u8 x_end = raw.stickX;
			u8 y_end = raw.stickY;
			Fuzz(x_end, y_end);
			target->x_end = x_end;
			target->y_end = y_end;
			m_wavedash_was_nerfed = false;
		}
	}
	m_prev_inputs = inputs;

	u8 stick_x = raw.stickX;
	u8 stick_y = raw.stickY;

	const u8 tap_sdi = DetectTapSdi(now_us);
	// Cardinal tap SDI: slow down travel.
	if (tap_sdi & SDI_TAP_CARDINAL)
		target->travel_ms = std::max(target->travel_ms, TRAVEL_SLOW);
	// Oscillating about a diagonal: slow down new destinations for 4 frames.
	if ((tap_sdi & SDI_WANK) && (tap_sdi & SDI_TAP_CARDINAL_DIAGONAL))
		m_sdi_slow_until = now_us + SDI_SLOW_US;
	if (now_us < m_sdi_slow_until)
		target->travel_ms = std::max(target->travel_ms, TRAVEL_SLOW);

	Travel(now_us, target->timestamp, target->travel_ms, target->x_start, target->y_start, target->x_end,
		target->y_end, m_done_traveling, stick_x, stick_y);

	// Pivot detection: a dash zone held for 0.5 to 1.5 frames, preceded by the opposite dash zone, then a
	// return to the x deadzone.
	if (m_pivot_history[0].zone != PivotZone(stick_x))
	{
		for (int i = HISTORY_LENGTH - 2; i >= 0; --i)
			m_pivot_history[i + 1] = m_pivot_history[i];
		m_pivot_history[0] = {now_us, PivotZone(stick_x), false};
	}
	for (auto& change : m_pivot_history)
	{
		if (now_us - change.timestamp > PIVOT_STALE_US)
			change.stale = true;
	}

	Pivot pivot = Pivot::None;
	if (m_pivot_history[0].zone == 0)
	{
		if (m_pivot_history[1].zone == ZONE_L &&
		    (m_pivot_history[2].zone == ZONE_R || m_pivot_history[3].zone == ZONE_R))
		{
			pivot = Pivot::RightLeft;
		}
		else if (m_pivot_history[1].zone == ZONE_R &&
		         (m_pivot_history[2].zone == ZONE_L || m_pivot_history[3].zone == ZONE_L))
		{
			pivot = Pivot::LeftRight;
		}
	}
	const s64 dash_length = m_pivot_history[0].timestamp - m_pivot_history[1].timestamp;
	if (dash_length < HALF_FRAME_US || dash_length > FRAME_US + HALF_FRAME_US)
		pivot = Pivot::None;
	if (m_pivot_history[0].stale || m_pivot_history[1].stale || m_pivot_history[2].stale ||
	    m_pivot_history[3].stale)
	{
		pivot = Pivot::None;
	}
	if (now_us - m_pivot_history[0].timestamp > PIVOT_TILT_US)
		pivot = Pivot::None;

	// How long the stick has been above the y deadzone (tap jump shuts off after 4 frames), counted from the
	// last sample where it wasn't.
	if (stick_y > DEAD_MAX)
	{
		if (!m_above_deadzone)
			m_above_deadzone_since = m_last_sample_us;
		m_above_deadzone = true;
	}
	else
	{
		m_above_deadzone = false;
	}
	const s64 time_above_deadzone = m_above_deadzone ? now_us - m_above_deadzone_since : 0;

	// Pivot tilt lockout: after a pivot, tilting up or down is extended to the rim (smash / tap jump).
	bool pivot_tilt = false;
	bool up_tilt = false;
	if (pivot != Pivot::None && target->y_end < DEAD_MIN)
		pivot_tilt = true;
	if (pivot != Pivot::None && target->y_end > DEAD_MAX)
	{
		pivot_tilt = true;
		up_tilt = true;
	}
	if (pivot_tilt)
	{
		const int x_coord = stick_x - NEUTRAL;
		const int y_coord = stick_y - NEUTRAL;
		const int x_abs = std::abs(x_coord);
		const int y_abs = std::abs(y_coord);

		if (up_tilt && x_abs > y_abs)
		{
			// Force upward angles to at least 45 degrees to prevent pivot up-angled f-tilt.
			stick_x = static_cast<u8>(NEUTRAL + (x_coord >= 0 ? 127 : -127));
			stick_y = static_cast<u8>(NEUTRAL + 127);
		}
		else if (std::max(x_abs, y_abs) != 0)
		{
			// Keep the angle, stretch the magnitude as close to 127 as possible in steps of 0.5.
			const int stretch = 254 / std::max(x_abs, y_abs);
			stick_x = static_cast<u8>(NEUTRAL + x_coord * stretch / 2);
			stick_y = static_cast<u8>(NEUTRAL + y_coord * stretch / 2);
		}
		if (up_tilt && time_above_deadzone > TAP_JUMP_SHUTOFF_US)
		{
			if (pivot == Pivot::LeftRight)
				stick_x = static_cast<u8>(NEUTRAL + 56);
			else if (pivot == Pivot::RightLeft)
				stick_x = static_cast<u8>(NEUTRAL - 56);
			stick_y = static_cast<u8>(NEUTRAL + 56);
		}
	}

	// Crouch to up-tilt lockout: going from crouch to the up-tilt range too quickly jumps instead.
	if (stick_y < CROUCH)
		m_last_crouch = now_us;
	if (now_us - m_last_crouch < DOWN_UP_US && stick_y > DEAD_MAX && stick_y < TAP_JUMP &&
	    stick_x > UTILT_LEFT && stick_x < UTILT_RIGHT && !m_down_up_jumping)
	{
		m_down_up_jumping = true;
		m_jump_start = now_us;
	}
	if (m_down_up_jumping && now_us - m_jump_start < JUMP_US)
	{
		stick_y = 255;
		// Don't extend the jump if it ends before the lockout window does.
		m_last_crouch = LONG_AGO;
	}
	else
	{
		m_down_up_jumping = false;
	}

	// Diagonal/quarter-circle burst SDI: lock the cross axis.
	if (tap_sdi & (SDI_TAP_DIAGONAL | SDI_TAP_CARDINAL_DIAGONAL | SDI_WANK))
	{
		if (tap_sdi & (ZONE_L | ZONE_R))
		{
			stick_y = NEUTRAL;
			target->y_end = stick_y;
			m_sdi_is_nerfed = true;
		}
		else if (tap_sdi & (ZONE_U | ZONE_D))
		{
			stick_x = NEUTRAL;
			target->x_end = stick_x;
			m_sdi_is_nerfed = true;
		}
	}
	else if (m_sdi_is_nerfed)
	{
		target->x_end = target->x;
		target->y_end = target->y;
		if (stick_x == NEUTRAL && stick_y == NEUTRAL)
			m_sdi_is_nerfed = false;
	}

	// Record the requested coordinate's SDI zone.
	const u8 sdi_zone = SdiZone(raw.stickX, raw.stickY);
	if (m_sdi_history[m_sdi_index].zone != sdi_zone)
	{
		m_sdi_index = (m_sdi_index + 1) % HISTORY_LENGTH;
		m_sdi_history[m_sdi_index] = {now_us, sdi_zone, false};
	}
	for (auto& change : m_sdi_history)
	{
		if (now_us - change.timestamp > SDI_STALE_US)
			change.stale = true;
	}

	// A new requested coordinate starts a new travel from the current output.
	const u8 x_in = raw.stickX;
	const u8 y_in = raw.stickY;
	if ((m_prev_x != x_in || m_prev_y != y_in) && !wavedash_skip)
	{
		m_done_traveling = false;
		m_stick_index = (m_stick_index + 1) % HISTORY_LENGTH;
		target = &m_stick_history[m_stick_index];

		m_prev_x = x_in;
		m_prev_y = y_in;

		u8 x_fuzzed = x_in;
		u8 y_fuzzed = y_in;
		Fuzz(x_fuzzed, y_fuzzed);

		target->timestamp = now_us;
		target->x = x_in;
		target->y = y_in;
		target->x_end = x_fuzzed;
		target->y_end = y_fuzzed;
		target->x_start = stick_x;
		target->y_start = stick_y;

		// An axis that is already at the requested value moves its fuzz offset immediately. (The firmware
		// compares against the already-updated previous coordinate, so this only applies to such axes.)
		if (stick_x == x_in)
		{
			if (x_fuzzed > x_in)
				target->x_start++;
			if (x_fuzzed < x_in)
				target->x_start--;
		}
		if (stick_y == y_in)
		{
			if (y_fuzzed > y_in)
				target->y_start++;
			if (y_fuzzed < y_in)
				target->y_start--;
		}

		switch (RimLevel(x_in, y_in))
		{
		case 1:
			target->travel_ms = TRAVEL_RIM_1;
			break;
		case 2:
			target->travel_ms = TRAVEL_RIM_2;
			break;
		case 3:
			target->travel_ms = TRAVEL_RIM_3;
			break;
		default:
			target->travel_ms = TRAVEL_INTERNAL;
			break;
		}
	}

	m_last_sample_us = now_us;

	GCPadStatus out = raw;
	out.stickX = stick_x;
	out.stickY = stick_y;
	return out;
}

GCPadStatus Controller::Update(const Inputs& raw_inputs, const Settings& settings, s64 now_us)
{
	// Time going backwards means a savestate was loaded; toggling the limits changes the SOCD mode.
	if ((m_has_last_update && now_us < m_last_update_us) || settings.ruleset_limits != m_last_ruleset_limits)
		Reset();
	m_has_last_update = true;
	m_last_update_us = now_us;
	m_last_ruleset_limits = settings.ruleset_limits;

	Inputs inputs = raw_inputs;
	const std::array<std::pair<bool*, bool*>, 4> axes = {{
		{&inputs.left, &inputs.right},
		{&inputs.down, &inputs.up},
		{&inputs.c_left, &inputs.c_right},
		{&inputs.c_down, &inputs.c_up},
	}};
	for (size_t i = 0; i < axes.size(); ++i)
	{
		if (settings.ruleset_limits)
			SocdPair::Neutral(*axes[i].first, *axes[i].second);
		else
			m_socd[i].SecondInputPriorityNoReactivation(*axes[i].first, *axes[i].second);
	}

	const GCPadStatus raw = MapToPadStatus(inputs, settings);
	if (!settings.ruleset_limits)
		return raw;
	return m_limiter.Apply(now_us, inputs, raw);
}

void Controller::Reset()
{
	for (auto& socd : m_socd)
		socd.Reset();
	m_limiter.Reset();
	m_has_last_update = false;
}
}  // namespace MeleeRectangle
