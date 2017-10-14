#include "SlippiGame.h"

namespace Slippi {

	//**********************************************************************
	//*                         Event Handlers
	//**********************************************************************
	//The read operators will read a value and increment the index so the next read will read in the correct location
	uint8_t readByte(uint8_t* a, int& idx) {
		return a[idx++];
	}

	uint16_t readHalf(uint8_t* a, int& idx) {
		uint16_t value = a[idx] << 8 | a[idx + 1];
		idx += 2;
		return value;
	}

	uint32_t readWord(uint8_t* a, int& idx) {
		uint32_t value = a[idx] << 24 | a[idx + 1] << 16 | a[idx + 2] << 8 | a[idx + 3];
		idx += 4;
		return value;
	}

	float readFloat(uint8_t* a, int& idx) {
		uint32_t bytes = readWord(a, idx);
		return *(float*)(&bytes);
	}

	void handleGameInit(Game* game) {
		int idx = 0;

		// Read version number
		for (int i = 0; i < 4; i++) {
			game->version[i] = readByte(data, idx);
		}

		// Read entire game info header
		for (int i = 0; i < GAME_INFO_HEADER_SIZE; i++) {
			game->settings.header[i] = readWord(data, idx);
		}

		//Load stage ID
		game->settings.randomSeed = readWord(data, idx);
	}

	void handleGameStart(Game* game) {
		int idx = 0;

		//Load stage ID
		game->settings.stage = readHalf(data, idx);

		PlayerSettings* p = new PlayerSettings();

		//Load player data
		p->controllerPort = readByte(data, idx);
		p->characterId = readByte(data, idx);
		p->playerType = readByte(data, idx);
		p->characterColor = readByte(data, idx);

		//Add player settings to result
		game->settings.players[p->controllerPort] = *p;
	}

	void handleUpdate(Game* game) {
		int idx = 0;

		//Check frame count
		int32_t frameCount = readWord(data, idx);
		game->frameCount = frameCount;

		FrameData* frame = new FrameData();
		if (game->frameData.count(frameCount)) {
			// If this frame already exists, this is probably another player
			// in this frame, so let's fetch it.
			frame = &game->frameData[frameCount];
		}

		frame->frame = frameCount;
		frame->randomSeed = readWord(data, idx);

		PlayerFrameData* p = new PlayerFrameData();

		uint8_t playerSlot = readByte(data, idx);

		//Load player data
		p->internalCharacterId = readByte(data, idx);
		p->animation = readHalf(data, idx);
		p->locationX = readFloat(data, idx);
		p->locationY = readFloat(data, idx);
		p->facingDirection = readFloat(data, idx);

		//Controller information
		p->joystickX = readFloat(data, idx);
		p->joystickY = readFloat(data, idx);
		p->cstickX = readFloat(data, idx);
		p->cstickY = readFloat(data, idx);
		p->trigger = readFloat(data, idx);
		p->buttons = readWord(data, idx);

		//More data
		p->percent = readFloat(data, idx);
		p->shieldSize = readFloat(data, idx);
		p->lastMoveHitId = readByte(data, idx);
		p->comboCount = readByte(data, idx);
		p->lastHitBy = readByte(data, idx);
		p->stocks = readByte(data, idx);

		//Raw controller information
		p->physicalButtons = readHalf(data, idx);
		p->lTrigger = readFloat(data, idx);
		p->rTrigger = readFloat(data, idx);

		//Add player data to frame
		frame->players[playerSlot] = *p;

		// Add frame to game
		game->frameData[frameCount] = *frame;
	}

	void handleGameEnd(Game* game) {
		int idx = 0;

		game->winCondition = readByte(data, idx);
	}

	// This function gets the position where the raw data starts
	int getRawDataPosition(std::ifstream* f) {
		char buffer[2];
		f->seekg(0, std::ios::beg);
		f->read(buffer, 2);

		if (buffer[0] == 0x36) {
			return 0;
		}

		if (buffer[0] != '{') {
			// TODO: Do something here to cause an error
			return 0;
		}

		// TODO: Read ubjson file to find the "raw" element and return the start of it
		// TODO: For now since raw is the first element the data will always start at 15
		return 15;
	}

	int getRawDataLength(std::ifstream* f, int position, int fileSize) {
		if (position == 0) {
			return fileSize;
		}

		char buffer[4];
		f->seekg(position - 4, std::ios::beg);
		f->read(buffer, 4);

		return buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
	}

	std::unordered_map<uint8_t, uint32_t> getMessageSizes(std::ifstream* f, int position) {
		// Support old file format
		if (position == 0) {
			return {
				{ EVENT_GAME_INIT, 0x140 },
				{ EVENT_GAME_START, 0x6 },
				{ EVENT_UPDATE, 0x46 },
				{ EVENT_GAME_END, 0x1 }
			};
		}

		char buffer[2];
		f->seekg(position, std::ios::beg);
		f->read(buffer, 2);
		if (buffer[0] != EVENT_PAYLOAD_SIZES) {
			return {};
		}

		std::unordered_map<uint8_t, uint32_t> messageSizes;

		int payloadLength = buffer[1];
		char messageSizesBuffer[payloadLength - 1];
		f->read(messageSizesBuffer, payloadLength - 1);
		for (int i = 0; i < payloadLength - 1; i += 3) {
			uint8_t command = messageSizesBuffer[i];
			uint16_t size = messageSizesBuffer[i + 1] << 8 | messageSizesBuffer[i + 2];
			messageSizes[command] = size;
		}

		return messageSizes;
	}

	SlippiGame* SlippiGame::processFile(char* rawData, uint64_t rawDataLength) {
		SlippiGame* result = new SlippiGame();
		result->game = new Game();

		// Iterate through the data and process frames
		for (int i = 0; i < rawDataLength; i++) {
			int code = rawData[i];
			int msgLength = asmEvents[code];
			if (!msgLength) {
				return nullptr;
			}

			data = (uint8_t*)&rawData[i + 1];
			switch (code) {
			case EVENT_GAME_INIT:
				handleGameInit(result->game);
				break;
			case EVENT_GAME_START:
				handleGameStart(result->game);
				break;
			case EVENT_UPDATE:
				handleUpdate(result->game);
				break;
			case EVENT_GAME_END:
				handleGameEnd(result->game);
				break;
			}
			i += msgLength;
		}

		return result;
	}

	SlippiGame* SlippiGame::FromFile(std::string path) {
		std::ifstream file(path, std::ios::in|std::ios::binary|std::ios::ate);
		if (!file.is_open()) {
			return nullptr;
		}

		int fileLength = file.tellg();
		int rawDataPos = getRawDataPosition(&file);
		int rawDataLength = getRawDataLength(&file, rawDataPos, fileLength);
		asmEvents = getMessageSizes(&file, rawDataPos);

		char rawData[rawDataLength];
		file.seekg(rawDataPos, std::ios::beg);
		file.read(rawData, rawDataLength);

		SlippiGame* result = processFile(rawData, rawDataLength);

		return result;
	}

	bool SlippiGame::DoesFrameExist(int32_t frame) {
		return (bool)game->frameData.count(frame);
	}

	FrameData* SlippiGame::GetFrame(int32_t frame) {
		// Get the frame we want
		return &game->frameData.at(frame);
	}

	GameSettings* SlippiGame::GetSettings() {
		return &game->settings;
	}

	bool SlippiGame::DoesPlayerExist(int8_t port) {
		return game->settings.players.find(port) != game->settings.players.end();
	}
}