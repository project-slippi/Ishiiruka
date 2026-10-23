#include "Core/Slippi/SlippiStun.h"

#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <random>
#include <vector>

#include "Common/StringUtil.h"
#include "Common/Timer.h"

namespace
{
// Tried in order
const struct
{
	const char *host;
	u16 port;
} SERVERS[] = {
    {"stun.l.google.com", 19302},
    {"stun1.l.google.com", 19302},
    {"stun.cloudflare.com", 3478},
};
const u32 MAGIC_COOKIE = 0x2112A442;
const u32 TIMEOUT_MS = 1000;
const int SERVICE_INTERVAL_MS = 20;

const char PUNCH[] = "SlippiPunch";
const int PUNCH_COUNT = 3;

// Replies are kept by host until they're taken
struct Reply
{
	u8 transaction[12] = {};
	std::string ip;
	u16 port = 0;
	bool isReceived = false;
};
std::mutex s_lock;
std::map<ENetHost *, Reply> s_replies;
std::default_random_engine s_generator(Common::Timer::GetTimeMs());

u16 readU16(const u8 *data)
{
	return static_cast<u16>((data[0] << 8) | data[1]);
}

u32 readU32(const u8 *data)
{
	return (static_cast<u32>(data[0]) << 24) | (static_cast<u32>(data[1]) << 16) | (static_cast<u32>(data[2]) << 8) |
	       data[3];
}
} // namespace

namespace SlippiStun
{
int ENET_CALLBACK Intercept(ENetHost *host, ENetEvent *event)
{
	const u8 *data = host->receivedData;
	size_t length = host->receivedDataLength;
	if (length == sizeof(PUNCH) && memcmp(data, PUNCH, sizeof(PUNCH)) == 0)
		return 1;

	// A binding success response: its type, then the length, magic cookie and our transaction
	if (length < 20 || readU16(data) != 0x0101 || readU32(data + 4) != MAGIC_COOKIE)
		return 0;

	std::lock_guard<std::mutex> lk(s_lock);
	auto it = s_replies.find(host);
	if (it == s_replies.end() || memcmp(data + 8, it->second.transaction, 12) != 0)
		return 1;

	// The mapped address is an attribute. The newer kind hides the address behind the cookie
	size_t pos = 20;
	while (pos + 4 <= length)
	{
		u16 type = readU16(data + pos);
		u16 attrLength = readU16(data + pos + 2);
		const u8 *value = data + pos + 4;
		if (pos + 4 + attrLength > length)
			break;

		bool isXor = type == 0x0020;
		if ((isXor || type == 0x0001) && attrLength >= 8 && value[1] == 0x01)
		{
			u16 port = readU16(value + 2);
			u32 ip = readU32(value + 4);
			if (isXor)
			{
				port ^= static_cast<u16>(MAGIC_COOKIE >> 16);
				ip ^= MAGIC_COOKIE;
			}

			it->second.ip = StringFromFormat("%u.%u.%u.%u", ip >> 24, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
			it->second.port = port;
			it->second.isReceived = true;
			if (isXor)
				break;
		}
		pos += 4 + ((attrLength + 3) & ~3);
	}
	return 1;
}

void SendRequest(ENetHost *host, int server)
{
	ENetAddress addr;
	if (enet_address_set_host(&addr, SERVERS[server].host) != 0)
		return;
	addr.port = SERVERS[server].port;

	u8 request[20] = {0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xA4, 0x42};
	{
		std::lock_guard<std::mutex> lk(s_lock);
		Reply &reply = s_replies[host];
		for (u8 &b : reply.transaction)
			b = static_cast<u8>(s_generator());
		reply.isReceived = false;
		memcpy(request + 8, reply.transaction, 12);
	}

	ENetBuffer buffer;
	buffer.data = request;
	buffer.dataLength = sizeof(request);
	enet_socket_send(host->socket, &addr, &buffer, 1);
}

bool TakeReply(ENetHost *host, std::string &ip, u16 &port)
{
	std::lock_guard<std::mutex> lk(s_lock);
	auto it = s_replies.find(host);
	if (it == s_replies.end() || !it->second.isReceived)
		return false;

	it->second.isReceived = false;
	ip = it->second.ip;
	port = it->second.port;
	return true;
}

bool Discover(ENetHost *host, std::string &ip, u16 &port)
{
	for (int server = 0; server < static_cast<int>(sizeof(SERVERS) / sizeof(SERVERS[0])); server++)
	{
		SendRequest(host, server);
		u32 startMs = Common::Timer::GetTimeMs();
		while (Common::Timer::GetTimeMs() - startMs < TIMEOUT_MS)
		{
			ENetEvent netEvent;
			enet_host_service(host, &netEvent, SERVICE_INTERVAL_MS);
			if (TakeReply(host, ip, port))
				return true;
		}
	}
	return false;
}

void Punch(ENetHost *host, const std::string &address)
{
	std::vector<std::string> parts;
	SplitString(address, ':', parts);
	if (parts.size() != 2)
		return;

	ENetAddress addr;
	if (enet_address_set_host(&addr, parts[0].c_str()) != 0)
		return;
	addr.port = static_cast<u16>(std::atoi(parts[1].c_str()));
	if (addr.port == 0)
		return;

	ENetBuffer buffer;
	buffer.data = const_cast<char *>(PUNCH);
	buffer.dataLength = sizeof(PUNCH);
	for (int i = 0; i < PUNCH_COUNT; i++)
		enet_socket_send(host->socket, &addr, &buffer, 1);
}

void Forget(ENetHost *host)
{
	std::lock_guard<std::mutex> lk(s_lock);
	s_replies.erase(host);
}
} // namespace SlippiStun
