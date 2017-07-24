// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>
#include <deque>

#include "SlippiLib/SlippiGame.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Core/HW/EXI_Device.h"

// Acts
class CEXISlippi : public IEXIDevice
{
public:
	CEXISlippi() {}

	void ImmWrite(u32 data, u32 size) override;
	u32 ImmRead(u32 size) override;

	bool IsPresent() const override;

private:
	enum {
		CMD_UNKNOWN = 0x0,
		CMD_GAME_START = 0x37,
		CMD_FRAME_UPDATE = 0x38,
		CMD_GAME_END = 0x39,
		CMD_PREPARE_REPLAY = 0x75,
		CMD_READ_FRAME = 0x76
	};

	std::unordered_map<u8, u32> payloadSizes = {
		{ CMD_GAME_START, 0xA },
		{ CMD_FRAME_UPDATE, 0x7A },
		{ CMD_GAME_END, 0x1 },
		{ CMD_PREPARE_REPLAY, 0x0 },
		{ CMD_READ_FRAME, 0x5 }
	};

	// .slp File creation stuff
	void createNewFile();
	void writeFileContents(u8* toWrite, u32 length);
	void closeFile();
	std::string generateFileName();

	File::IOFile m_file;
	u32 m_payload_loc = 0;
	u8 m_payload_type = CMD_UNKNOWN;
	std::vector<u8> m_payload;

	// replay playback stuff
	void loadFile(std::string path);
	void prepareFrameData(int32_t frameIndex, uint8_t port);

	std::deque<u32> m_read_queue;
	Slippi::SlippiGame* m_current_game = nullptr;

protected:
	void TransferByte(u8& byte) override;
};
