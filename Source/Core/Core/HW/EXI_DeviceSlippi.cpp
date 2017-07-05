// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI_DeviceSlippi.h"

#include <unordered_map>
#include <stdexcept>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "wx/datetime.h"

void CEXISlippi::createNewFile() {
	if (m_file) {
		// If we already have a file, return
		return;
	}

	File::CreateDir("Slippi");
	std::string filepath = GenerateFileName();
	m_file = File::IOFile(filepath, "wb");
}

void CEXISlippi::closeFile() {
	if (!m_file) {
		// If we have no file or payload is not game end, do nothing
		return;
	}

	// If this is the end of the game end payload, reset the file so that we create a new one
	m_file.Close();
	m_file = nullptr;
}

void CEXISlippi::writeFileContents(u8* toWrite, u32 length) {
	if (!m_file) {
		// If no file, do nothing
		return;
	}
	
	bool result = m_file.WriteBytes(toWrite, length);
	
	if (!result) {
		ERROR_LOG(EXPANSIONINTERFACE, "Failed to write data to file.");
	}
}

#pragma optimize( "", off )
void CEXISlippi::ImmWrite(u32 data, u32 size)
{
	//init();
	INFO_LOG(EXPANSIONINTERFACE, "EXI SLIPPI ImmWrite: %08x, size: %d", data, size);

	bool lookingForMessage = m_payload_type == CMD_UNKNOWN;
	if (lookingForMessage) {
		// If the size is not one, this can't be the start of a command
		if (size != 1) {
			return;
		}

		m_payload_type = data >> 24;

		// Attempt to get payload size for this command. If not found, don't do anything
		try {
			payloadSizes.at(m_payload_type);
		}
		catch (std::out_of_range) {
			m_payload_type = CMD_UNKNOWN;
			return;
		}
	}

	// Read and incremement our payload location
	m_payload_loc += size;

	// Add new data to payload
	for (u32 i = 0; i < size; i++) {
		int shiftAmount = 8 * (3 - i);
		u8 byte = 0xFF & (data >> shiftAmount);
		m_payload.push_back(byte);
	}

	// This section deals with saying we are done handling the payload
	// add one because we count the command as part of the total size
	u32 payloadSize = payloadSizes[m_payload_type];
	if (m_payload_loc >= payloadSize + 1) {
		// Handle payloads
		switch (m_payload_type) {
		case CMD_GAME_START:
			// Here we create a new file if one doesn't exist already
			createNewFile();
			writeFileContents(&m_payload[0], m_payload_loc);
			break;
		case CMD_FRAME_UPDATE:
			writeFileContents(&m_payload[0], m_payload_loc);
			break;
		case CMD_GAME_END:
			writeFileContents(&m_payload[0], m_payload_loc);
			closeFile();
			break;
		case CMD_PREPARE_REPLAY:
			// To implement
			break;
		}

		// reset payload loc and type so we look for next command
		m_payload_loc = 0;
		m_payload_type = CMD_UNKNOWN;
		m_payload.clear();
	}
}
#pragma optimize( "", on )

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
	return StringFromFormat("Slippi/Game_%s.slp", str.c_str());
}