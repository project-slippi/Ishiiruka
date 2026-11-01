#pragma once

#include "Common/CommonTypes.h"

// The full pad report in the game's format
#define SLIPPI_PAD_FULL_SIZE 0xC
// Buttons, sticks and triggers. Input suggestions only have this part
#define SLIPPI_PAD_INPUT_SIZE 0x8
// The inputs and the RNG bits, what gets sent over the network for each frame
#define SLIPPI_PAD_WIRE_SIZE (SLIPPI_PAD_INPUT_SIZE + 1)

class SlippiPad
{
public:
  SlippiPad(int32_t frame);
  SlippiPad(int32_t frame, u8* padBuf);
  SlippiPad(int32_t frame, s32 checksumFrame, u32 checksum, u8 *padBuf);
  ~SlippiPad();

  s32 frame;
  s32 checksumFrame;
  u32 checksum;
  u8 padBuf[SLIPPI_PAD_FULL_SIZE];
};

