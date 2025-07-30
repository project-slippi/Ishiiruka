#include "SlippiPad.h"

// TODO: Confirm the default and padding values are right
static u8 emptyPad[SLIPPI_PAD_FULL_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

SlippiPad::SlippiPad()
{
  this->frame = 0;
  memcpy(this->padBuf, emptyPad, SLIPPI_PAD_FULL_SIZE);
}

SlippiPad::SlippiPad(int32_t frame)
{
	this->frame = frame;
	this->checksum = 0;
	this->checksumFrame = 0;
	memcpy(this->padBuf, emptyPad, SLIPPI_PAD_FULL_SIZE);
}

// It's important that the base constructor be called because it resets padBuf to zero.
// Without that, the padding and status byte values are undefined which causes desyncs.
SlippiPad::SlippiPad(int32_t frame, u8 *padBuf)
    : SlippiPad(frame)
{
	// Overwrite the data portion of the pad
	memcpy(this->padBuf, padBuf, SLIPPI_PAD_DATA_SIZE);
}

SlippiPad::SlippiPad(int32_t frame, s32 checksumFrame, u32 checksum, u8 *padBuf)
    : SlippiPad(frame, padBuf)
{
	this->checksumFrame = checksumFrame;
	this->checksum = checksum;
}

SlippiPad::~SlippiPad()
{
	// Do nothing?
}
