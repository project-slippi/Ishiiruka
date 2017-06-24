// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI_DeviceSlippi.h"

#include <map>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "wx/datetime.h"

void CEXISlippi::ImmWrite(u32 data, u32 size)
{
	INFO_LOG(EXPANSIONINTERFACE, "EXI SLIPPI ImmWrite: %08x, size: %d", data, size);

	bool lookingForMessage = m_payload_type == CMD_UNKNOWN;
	if (lookingForMessage) {
		// If the size is not one, this can't be the start of a command
		if (size != 1) {
			return;
		}

		u32 dataCopy = data;
		m_payload_type = dataCopy >> 24;
		switch (m_payload_type) {
		case CMD_GAME_START:
			// If currently don't have a file open, create one
			if (!m_file) {
				std::string filepath = GenerateFileName();
				m_file = File::IOFile(filepath, "wb");
			}
			break;
		case CMD_FRAME_UPDATE:
		case CMD_GAME_END:
			// Do nothing, we just need to read the payload
			break;
		default:
			m_payload_type = CMD_UNKNOWN;
			return;
		}
	}

	if (!m_file) {
		return;
	}

	// Here we are simply copying data from the message payload to the output file
	u32 dataSwapped = Common::FromBigEndian(data);
	bool result = m_file.WriteBytes(&dataSwapped, size);
	m_payload_loc += size;

	if (!result) {
		ERROR_LOG(EXPANSIONINTERFACE, "Failed to write data to file.");
	}

	u32 payloadSize = payloadSizes.find(m_payload_type)->second;
	//  WARN_LOG(EXPANSIONINTERFACE, "Payload Size: %08x", payloadSize);
	if (m_payload_loc >= payloadSize + 1) {
		if (m_payload_type == CMD_GAME_END) {
			// If this is the end of the game end payload, reset the file so that we create a new one
			m_file.Close();
			m_file = nullptr;
		}

		// reset payload loc and type so we look for next command
		m_payload_loc = 0;
		m_payload_type = CMD_UNKNOWN;
	}
}

u32 CEXISlippi::ImmRead(u32 size)
{
	// Not implemented yet. Might be used in the future to play back replays
	INFO_LOG(EXPANSIONINTERFACE, "EXI SLIPPI ImmRead");
	return 0;
}

bool CEXISlippi::IsPresent() const
{
	return true;
}

void CEXISlippi::TransferByte(u8& byte)
{
}

std::string CEXISlippi::GenerateFileName()
{
	std::string str = wxDateTime::Now().Format(wxT("%Y%m%dT%H%M%S"));
	return StringFromFormat("Game_%s.slp", str.c_str());
}