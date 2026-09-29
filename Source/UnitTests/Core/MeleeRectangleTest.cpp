// Copyright 2025 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <map>
#include <utility>

#include "InputCommon/GCPadStatus.h"
#include "InputCommon/MeleeRectangle.h"

using namespace MeleeRectangle;

namespace
{
constexpr s64 MS = 1000;

Inputs Press(std::initializer_list<bool Inputs::*> buttons)
{
  Inputs inputs;
  for (auto button : buttons)
    inputs.*button = true;
  return inputs;
}

// Samples the controller once per millisecond, like the firmware's sample loop.
struct Harness
{
  Controller controller;
  Settings settings;
  s64 now = 0;

  GCPadStatus Step(const Inputs& inputs)
  {
    const GCPadStatus status = controller.Update(inputs, settings, now);
    now += MS;
    return status;
  }

  // Returns the output of the last sample taken.
  GCPadStatus Hold(const Inputs& inputs, int milliseconds)
  {
    GCPadStatus status = {};
    for (int i = 0; i < milliseconds; ++i)
      status = Step(inputs);
    return status;
  }
};

bool Within(int value, int target, int tolerance)
{
  return value >= target - tolerance && value <= target + tolerance;
}
}  // namespace

TEST(MeleeRectangle, MapsCoordinates)
{
  const Settings settings;

  auto pad = MapToPadStatus(Press({&Inputs::right}), settings);
  EXPECT_EQ(240, pad.stickX);
  EXPECT_EQ(128, pad.stickY);

  pad = MapToPadStatus(Press({&Inputs::mod_x, &Inputs::right}), settings);
  EXPECT_EQ(128 + 53, pad.stickX);

  pad = MapToPadStatus(Press({&Inputs::mod_y, &Inputs::right}), settings);
  EXPECT_EQ(128 + 27, pad.stickX);

  pad = MapToPadStatus(Press({&Inputs::up, &Inputs::right}), settings);
  EXPECT_EQ(128 + 56, pad.stickX);
  EXPECT_EQ(128 + 61, pad.stickY);

  // Firefox angles
  pad = MapToPadStatus(Press({&Inputs::mod_x, &Inputs::up, &Inputs::right}), settings);
  EXPECT_EQ(128 + 58, pad.stickX);
  EXPECT_EQ(128 + 25, pad.stickY);
  pad = MapToPadStatus(Press({&Inputs::mod_x, &Inputs::up, &Inputs::left, &Inputs::c_down}), settings);
  EXPECT_EQ(128 - 56, pad.stickX);
  EXPECT_EQ(128 + 29, pad.stickY);
  pad = MapToPadStatus(Press({&Inputs::mod_y, &Inputs::up, &Inputs::right, &Inputs::b, &Inputs::c_up}),
                       settings);
  EXPECT_EQ(128 + 46, pad.stickX);
  EXPECT_EQ(128 + 63, pad.stickY);

  // Wavedash
  pad = MapToPadStatus(Press({&Inputs::mod_x, &Inputs::down, &Inputs::right, &Inputs::r}), settings);
  EXPECT_EQ(128 + 51, pad.stickX);
  EXPECT_EQ(128 - 30, pad.stickY);
  EXPECT_TRUE(pad.button & PAD_TRIGGER_R);
  EXPECT_EQ(140, pad.triggerRight);

  // Turnaround neutral-B nerf
  pad = MapToPadStatus(Press({&Inputs::mod_y, &Inputs::left, &Inputs::b}), settings);
  EXPECT_EQ(128 - 80, pad.stickX);

  // Angled forward smash and C-stick diagonals
  pad = MapToPadStatus(Press({&Inputs::mod_x, &Inputs::up, &Inputs::c_right}), settings);
  EXPECT_EQ(128 + 68, pad.substickX);
  EXPECT_EQ(128 + 42, pad.substickY);
  pad = MapToPadStatus(Press({&Inputs::c_up, &Inputs::c_left}), settings);
  EXPECT_EQ(128 - 42, pad.substickX);
  EXPECT_EQ(128 + 68, pad.substickY);

  pad = MapToPadStatus(Press({&Inputs::lightshield}), settings);
  EXPECT_EQ(49, pad.triggerRight);
  EXPECT_FALSE(pad.button & PAD_TRIGGER_R);
  pad = MapToPadStatus(Press({&Inputs::midshield}), settings);
  EXPECT_EQ(94, pad.triggerRight);

  Settings crouch_walk;
  crouch_walk.crouch_walk_os = true;
  pad = MapToPadStatus(Press({&Inputs::down, &Inputs::right}), crouch_walk);
  EXPECT_EQ(128 + 61, pad.stickX);
  EXPECT_EQ(128 - 56, pad.stickY);
}

TEST(MeleeRectangle, ModifiersTurnCButtonsIntoDPad)
{
  const auto pad = MapToPadStatus(Press({&Inputs::mod_x, &Inputs::mod_y, &Inputs::c_up}), Settings());
  EXPECT_TRUE(pad.button & PAD_BUTTON_UP);
  EXPECT_EQ(128, pad.substickX);
  EXPECT_EQ(128, pad.substickY);
}

TEST(MeleeRectangle, NeutralSocd)
{
  Harness h;
  EXPECT_EQ(128, h.Hold(Press({&Inputs::left, &Inputs::right}), 50).stickX);
  EXPECT_EQ(128, h.Hold(Press({&Inputs::down, &Inputs::up}), 50).stickY);
  EXPECT_TRUE(Within(h.Hold(Press({&Inputs::right}), 50).stickX, 240, 1));
  // Holding right and pressing left goes to neutral rather than left.
  EXPECT_EQ(128, h.Hold(Press({&Inputs::left, &Inputs::right}), 50).stickX);
}

TEST(MeleeRectangle, LegacySocdIsSecondInputPriority)
{
  Harness h;
  h.settings.ruleset_limits = false;
  EXPECT_EQ(16, h.Step(Press({&Inputs::left})).stickX);
  EXPECT_EQ(240, h.Step(Press({&Inputs::left, &Inputs::right})).stickX);
  // No reactivation: releasing right while left is still held gives neutral.
  EXPECT_EQ(128, h.Step(Press({&Inputs::left})).stickX);
  EXPECT_EQ(128, h.Step(Inputs()).stickX);
  EXPECT_EQ(16, h.Step(Press({&Inputs::left})).stickX);
}

TEST(MeleeRectangle, LegacyModeIsInstant)
{
  Harness h;
  h.settings.ruleset_limits = false;
  EXPECT_EQ(128 + 53, h.Step(Press({&Inputs::mod_x, &Inputs::right})).stickX);
  EXPECT_EQ(128 + 53, h.Step(Press({&Inputs::mod_x, &Inputs::right})).stickX);
}

TEST(MeleeRectangle, TravelTimeToRimIsAbout8ms)
{
  Harness h;
  const auto right = Press({&Inputs::right});
  EXPECT_EQ(128, h.Step(right).stickX);   // t = 0
  h.Hold(right, 3);                       // t = 1..3
  const u8 halfway = h.Step(right).stickX;  // t = 4
  EXPECT_GT(halfway, 175);
  EXPECT_LT(halfway, 190);
  const u8 settled = h.Hold(right, 5).stickX;  // t = 9
  EXPECT_TRUE(Within(settled, 240, 1));
  EXPECT_EQ(128, h.Step(right).stickY);
  // The fuzzed coordinate is kept for as long as the input is held.
  for (int i = 0; i < 500; ++i)
    ASSERT_EQ(settled, h.Step(right).stickX);
}

TEST(MeleeRectangle, TravelTimeToDiagonalIs6ms)
{
  Harness h;
  const auto diagonal = Press({&Inputs::up, &Inputs::right});
  h.Hold(diagonal, 4);  // t = 0..3
  EXPECT_FALSE(Within(h.Step(diagonal).stickY, 189, 1));  // t = 4
  h.Hold(diagonal, 2);                                    // t = 5, 6
  const auto pad = h.Step(diagonal);                      // t = 7
  EXPECT_TRUE(Within(pad.stickX, 128 + 56, 1));
  EXPECT_TRUE(Within(pad.stickY, 128 + 61, 1));
}

TEST(MeleeRectangle, TravelTimeToInternalCoordinateIs12ms)
{
  Harness h;
  const auto tilt = Press({&Inputs::mod_x, &Inputs::right});
  h.Hold(tilt, 7);  // t = 0..6
  EXPECT_TRUE(Within(h.Step(tilt).stickX, 128 + 31, 2));  // t = 7
  h.Hold(tilt, 3);                                         // t = 8..10
  EXPECT_LT(h.Step(tilt).stickX, 128 + 50);                // t = 11, not there yet
  h.Step(tilt);                                            // t = 12, 250/256 of the way
  EXPECT_TRUE(Within(h.Step(tilt).stickX, 128 + 53, 1));   // t = 13
}

TEST(MeleeRectangle, ReturnToNeutralIs12ms)
{
  Harness h;
  h.Hold(Press({&Inputs::right}), 50);
  const Inputs none;
  h.Hold(none, 6);
  EXPECT_GT(h.Step(none).stickX, 128 + 30);
  h.Hold(none, 5);
  EXPECT_GT(h.Step(none).stickX, 128);  // t = 12, 250/256 of the way
  EXPECT_EQ(128, h.Step(none).stickX);  // t = 13
}

TEST(MeleeRectangle, FuzzingDistribution)
{
  Harness h;
  const auto diagonal = Press({&Inputs::up, &Inputs::right});
  const auto right = Press({&Inputs::right});
  std::map<std::pair<int, int>, int> diagonal_counts;
  std::map<int, int> cardinal_counts;
  constexpr int TRIALS = 4000;
  for (int i = 0; i < TRIALS; ++i)
  {
    // Slow enough to never look like tap SDI.
    const auto pad = h.Hold(diagonal, 20);
    diagonal_counts[{pad.stickX - (128 + 56), pad.stickY - (128 + 61)}]++;
    h.Hold(Inputs(), 150);
    const auto cardinal = h.Hold(right, 20);
    ASSERT_EQ(128, cardinal.stickY);
    cardinal_counts[cardinal.stickX - 240]++;
    h.Hold(Inputs(), 150);
  }

  for (const auto& entry : diagonal_counts)
  {
    const int dx = entry.first.first;
    const int dy = entry.first.second;
    ASSERT_TRUE(Within(dx, 0, 1) && Within(dy, 0, 1)) << dx << "," << dy;
    // 1/4 in the center, 1/8 on each edge, 1/16 in each corner.
    const double expected = 1.0 / ((dx == 0 ? 2 : 4) * (dy == 0 ? 2 : 4));
    EXPECT_NEAR(expected, entry.second / static_cast<double>(TRIALS), 0.025) << dx << "," << dy;
  }
  EXPECT_EQ(9u, diagonal_counts.size());

  EXPECT_EQ(3u, cardinal_counts.size());
  EXPECT_NEAR(0.5, cardinal_counts[0] / static_cast<double>(TRIALS), 0.03);
  EXPECT_NEAR(0.25, cardinal_counts[1] / static_cast<double>(TRIALS), 0.03);
  EXPECT_NEAR(0.25, cardinal_counts[-1] / static_cast<double>(TRIALS), 0.03);
}

TEST(MeleeRectangle, ShieldLockoutForcesAtLeast27DegreesImmediately)
{
  // The ruleset coordinate set never asks for a shallow angle while shielding, so drive the limiter
  // directly with a 23 degree up-B angle and then press R.
  Limiter limiter(1234);
  GCPadStatus raw = {};
  raw.stickX = 128 + 58;
  raw.stickY = 128 + 25;
  Inputs inputs = Press({&Inputs::mod_x, &Inputs::up, &Inputs::right});
  GCPadStatus out = {};
  for (s64 t = 0; t <= 30; ++t)
    out = limiter.Apply(t * MS, inputs, raw);
  EXPECT_TRUE(Within(out.stickX, 128 + 58, 1));
  EXPECT_TRUE(Within(out.stickY, 128 + 25, 1));

  inputs.r = true;
  out = limiter.Apply(31 * MS, inputs, raw);
  EXPECT_TRUE(Within(out.stickX, 128 + 51, 1));
  EXPECT_TRUE(Within(out.stickY, 128 + 26, 1));

  inputs.r = false;
  out = limiter.Apply(32 * MS, inputs, raw);
  EXPECT_TRUE(Within(out.stickX, 128 + 58, 1));
  EXPECT_TRUE(Within(out.stickY, 128 + 25, 1));
}

TEST(MeleeRectangle, ShieldChangingCoordinateSkipsTravelTime)
{
  Harness h;
  const auto up_b = Press({&Inputs::mod_x, &Inputs::up, &Inputs::right});
  h.Hold(up_b, 30);
  auto airdodge = up_b;
  airdodge.r = true;
  const auto pad = h.Step(airdodge);
  EXPECT_TRUE(Within(pad.stickX, 128 + 51, 1));
  EXPECT_TRUE(Within(pad.stickY, 128 + 30, 1));
}

TEST(MeleeRectangle, CrouchToUptiltJumps)
{
  const auto up_tilt = Press({&Inputs::mod_x, &Inputs::up});

  Harness h;
  h.Hold(Press({&Inputs::down}), 30);
  int max_y = 0;
  for (int i = 0; i < 60; ++i)
    max_y = std::max<int>(max_y, h.Step(up_tilt).stickY);
  EXPECT_EQ(255, max_y);
  EXPECT_TRUE(Within(h.Step(up_tilt).stickY, 128 + 42, 1));

  // From neutral the up-tilt coordinate is reachable.
  Harness from_neutral;
  from_neutral.Hold(Inputs(), 100);
  max_y = 0;
  for (int i = 0; i < 60; ++i)
    max_y = std::max<int>(max_y, from_neutral.Step(up_tilt).stickY);
  EXPECT_TRUE(Within(max_y, 128 + 42, 1));
}

TEST(MeleeRectangle, PivotUptiltExtendsToRim)
{
  const auto up_tilt = Press({&Inputs::mod_x, &Inputs::up});

  Harness h;
  h.Hold(Press({&Inputs::right}), 100);
  h.Hold(Press({&Inputs::left}), 16);
  h.Hold(Inputs(), 9);
  int max_y = 0;
  for (int i = 0; i < 20; ++i)
    max_y = std::max<int>(max_y, h.Step(up_tilt).stickY);
  EXPECT_GE(max_y, 250);

  Harness no_pivot;
  no_pivot.Hold(Press({&Inputs::right}), 100);
  no_pivot.Hold(Inputs(), 300);
  max_y = 0;
  for (int i = 0; i < 20; ++i)
    max_y = std::max<int>(max_y, no_pivot.Step(up_tilt).stickY);
  EXPECT_TRUE(Within(max_y, 128 + 42, 1));
}

TEST(MeleeRectangle, CardinalTapSdiSlowsTravel)
{
  Harness h;
  const auto right = Press({&Inputs::right});
  h.Hold(right, 8);
  EXPECT_GT(h.Step(right).stickX, 230);  // First press reaches the rim in 8 ms.
  h.Hold(right, 11);
  h.Hold(Inputs(), 20);
  h.Hold(right, 20);
  h.Hold(Inputs(), 20);
  // Third tap in quick succession.
  h.Hold(right, 8);
  EXPECT_LT(h.Step(right).stickX, 160);
}

TEST(MeleeRectangle, CoarseSamplingKeepsTravelTimeInRealTime)
{
  // Dolphin refreshes keyboard state about 4 times per frame rather than every millisecond.
  Controller controller;
  const Settings settings;
  const auto right = Press({&Inputs::right});
  const s64 step = 4167;
  EXPECT_EQ(128, controller.Update(right, settings, 0).stickX);
  const u8 partway = controller.Update(right, settings, step).stickX;
  EXPECT_GT(partway, 128);
  EXPECT_LT(partway, 239);
  EXPECT_TRUE(Within(controller.Update(right, settings, 2 * step + 100).stickX, 240, 1));
}

TEST(MeleeRectangle, SavestateLoadResets)
{
  Harness h;
  const auto right = Press({&Inputs::right});
  h.Hold(right, 100);
  h.now = 0;
  EXPECT_EQ(128, h.Step(right).stickX);
}
