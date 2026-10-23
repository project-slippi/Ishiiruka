#include "SlippiRoomSession.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <random>
#include <set>
#include <thread>
#include <vector>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Core/Slippi/SlippiStun.h"

#ifdef USE_UPNP
#include <miniupnpc.h>
#include <upnpcommands.h>
#endif

// #define LOCAL_TESTING

using json = nlohmann::json;

namespace
{
// Long enough for the host to pick a join request up and punch through to the joiner
const int CONNECT_TIMEOUT_MS = 10000;
const int SERVICE_INTERVAL_MS = 20;

// How long a peer can go unheard before its connection counts as dropped
const int PEER_TIMEOUT_MIN_MS = 4000;
const int PEER_TIMEOUT_MAX_MS = 8000;

// When a host is gone, each candidate after the first waits this long per place in line before
// taking over, in case the ones before it are gone too. Long enough for a new host to open its port
const u32 CANDIDATE_WAIT_MS = 15000;
const int RETRY_MS = 1000;

// After taking over, how long the other members have to reconnect before they count as dropped
const u32 RECONNECT_WAIT_MS = 20000;

// Time given to messages already sent before a connection closes
const u32 DRAIN_MS = 1000;

// How often a host checks for join requests to punch through to
const u32 JOIN_POLL_MS = 1500;

// Activity is reported at most this often, and retried after this long when the directory can't be
// reached
const u32 ACTIVITY_INTERVAL_MS = 2000;
const u32 ACTIVITY_RETRY_MS = 5000;

// A room with no members joining or leaving and no sets finishing for this long closes
const u32 IDLE_CLOSE_MS = 60 * 60 * 1000;

// The directory times the hour from the last activity it was told about, which can trail ours
const u32 IDLE_SLACK_MS = 60 * 1000;

#ifdef USE_UPNP
// Looking for a router that isn't there takes several seconds, and a new host can't take over until
// it's done, so it's only looked for until it's known not to be there
std::atomic<bool> s_noUpnpRouter{false};

// Opens the room's port on the router so members can connect to the host. Rooms still work
// without it when the port is forwarded some other way
bool mapPort(u16 port, UPNPUrls &urls, IGDdatas &data)
{
	if (s_noUpnpRouter)
		return false;

	int error = 0;
	UPNPDev *devlist = upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &error);
	if (!devlist)
	{
		WARN_LOG(SLIPPI_ONLINE, "[Rooms] No UPnP devices found, error: %d", error);
		s_noUpnpRouter = true;
		return false;
	}

	char lanAddr[64] = {};
	int igd = UPNP_GetValidIGD(devlist, &urls, &data, lanAddr, sizeof(lanAddr));
	freeUPNPDevlist(devlist);
	if (igd != 1)
	{
		WARN_LOG(SLIPPI_ONLINE, "[Rooms] No connected UPnP router found");
		if (igd != 0)
			FreeUPNPUrls(&urls);
		return false;
	}

	std::string portStr = StringFromFormat("%d", port);
	int result = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, portStr.c_str(), portStr.c_str(), lanAddr,
	                                 "Slippi room", "UDP", nullptr, nullptr);
	if (result != 0)
	{
		WARN_LOG(SLIPPI_ONLINE, "[Rooms] Failed to map port %d with UPnP, error: %d", port, result);
		FreeUPNPUrls(&urls);
		return false;
	}

	INFO_LOG(SLIPPI_ONLINE, "[Rooms] Mapped port %d to %s with UPnP", port, lanAddr);
	return true;
}

// Some routers never remove mappings on their own, so the room always removes its own
void unmapPort(u16 port, UPNPUrls &urls, IGDdatas &data)
{
	std::string portStr = StringFromFormat("%d", port);
	UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, portStr.c_str(), "UDP", nullptr);
	FreeUPNPUrls(&urls);
}
#endif

ENetHost *createRoomHost(u16 &port)
{
	std::default_random_engine generator(Common::Timer::GetTimeMs());
	for (int i = 0; i < 15; i++)
	{
		port = 41000 + (generator() % 10000);

		ENetAddress addr;
		addr.host = ENET_HOST_ANY;
		addr.port = port;
		ENetHost *host = enet_host_create(&addr, ROOM_MAX_MEMBERS, 1, 0, 0);
		if (host)
			return host;
	}
	return nullptr;
}

// Gives messages already sent a moment to go out before the connections close
void drain(ENetHost *host)
{
	u32 startMs = Common::Timer::GetTimeMs();
	while (Common::Timer::GetTimeMs() - startMs < DRAIN_MS)
	{
		ENetEvent netEvent;
		if (enet_host_service(host, &netEvent, SERVICE_INTERVAL_MS) > 0 && netEvent.type == ENET_EVENT_TYPE_RECEIVE)
			enet_packet_destroy(netEvent.packet);

		bool isConnected = false;
		for (size_t i = 0; i < host->peerCount; i++)
		{
			if (host->peers[i].state != ENET_PEER_STATE_DISCONNECTED)
				isConnected = true;
		}
		if (!isConnected)
			break;
	}
}

// Routers only keep an outside port open while packets go out through it, so an idle host asks again
// this often
const u32 STUN_REFRESH_MS = 20000;

void closeHost(ENetHost *host)
{
	SlippiStun::Forget(host);
	enet_host_destroy(host);
}

// Minutes ahead of UTC here at the given time
int utcOffsetAt(std::time_t time)
{
	std::tm local = *std::localtime(&time);
	std::tm utc = *std::gmtime(&time);
	local.tm_isdst = 0;
	return static_cast<int>(std::difftime(std::mktime(&local), std::mktime(&utc)) / 60);
}

// The standard time offset here, which tells the directory which side of North America a room is on.
// Daylight saving is left out, or Mountain in summer would look like Central
int localUtcOffset()
{
	std::time_t now = std::time(nullptr);
	std::tm date = *std::localtime(&now);
	date.tm_mday = 1;
	date.tm_hour = 12;
	date.tm_min = 0;
	date.tm_sec = 0;

	// One of January and July is in daylight saving wherever it's used, and that one is ahead
	date.tm_mon = 0;
	date.tm_isdst = -1;
	int january = utcOffsetAt(std::mktime(&date));
	date.tm_mon = 6;
	date.tm_isdst = -1;
	int july = utcOffsetAt(std::mktime(&date));
	return std::min(january, july);
}

SlippiRoomSession::ConnectionError joinError(SlippiRoomDirectory::JoinStatus status)
{
	switch (status)
	{
	case SlippiRoomDirectory::JoinStatus::NOT_FOUND:
		return SlippiRoomSession::CONNECT_NOT_FOUND;
	case SlippiRoomDirectory::JoinStatus::WRONG_PASSWORD:
		return SlippiRoomSession::CONNECT_WRONG_PASSWORD;
	case SlippiRoomDirectory::JoinStatus::FULL:
		return SlippiRoomSession::CONNECT_FULL;
	case SlippiRoomDirectory::JoinStatus::LOCKED:
		return SlippiRoomSession::CONNECT_LOCKED;
	default:
		return SlippiRoomSession::CONNECT_UNAVAILABLE;
	}
}
} // namespace

void SlippiRoomSession::sendMessage(ENetPeer *peer, const json &msg)
{
	std::string contents = msg.dump();
	ENetPacket *packet = enet_packet_create(contents.c_str(), contents.length(), ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(peer, 0, packet);
}

// Runs a hosted room: members' messages and connections, hand-overs, and closing when idle
class SlippiRoomHost::NetThread
{
  public:
	NetThread(std::shared_ptr<Shared> shared, Identity identity, ENetHost *host, bool tookOver)
	    : m_shared(std::move(shared))
	    , m_identity(std::move(identity))
	    , m_host(host)
	    , m_tookOver(tookOver)
	{
	}

	void Run();

  private:
	void onReceive(ENetPeer *peer, const json &msg);
	void onHello(ENetPeer *peer, const json &msg);
	void onDisconnect(ENetPeer *peer);
	void reject(ENetPeer *peer, const std::string &reason);
	void broadcast(const json &msg);
	void broadcastIfChanged();
	void leave();
	void handBack(const std::string &connectCode);
	void stepDown();
	void close(ConnectionError error);
	void checkDirectory();
	void checkReconnects(u32 now);
	void trackActivity(u32 now);
	void refreshPort();
	void punchJoiners();

	std::shared_ptr<Shared> m_shared;
	Identity m_identity;
	ENetHost *m_host;
	bool m_tookOver;

	std::map<ENetPeer *, std::string> m_peerCodes; // Members are found by connect code
	std::set<ENetPeer *> m_leavingPeers;           // Said they were leaving on purpose
	std::map<std::string, u32> m_awaiting;         // Members yet to reconnect after a take-over
	std::string m_lastBroadcast;
	u32 m_startMs = 0;
	u32 m_lastActivityCount = 0;
	u32 m_lastActivityMs = 0;
	bool m_verified = false;
	bool m_stop = false;
	bool m_drain = false;
	u32 m_lastStunMs = 0;
};

void SlippiRoomHost::NetThread::Run()
{
	// Keeps networking up while this thread runs, even if emulation stops first
	enet_initialize();

	// Tests on one computer connect through localhost, so they need the room's own port instead
	std::string publicIp;
	u16 publicPort = 0;
#ifndef LOCAL_TESTING
	SlippiStun::Discover(m_host, publicIp, publicPort);
#endif
	if (publicPort)
		INFO_LOG(SLIPPI_ONLINE, "[Rooms] The room's outside port is %d", publicPort);
	else
		WARN_LOG(SLIPPI_ONLINE, "[Rooms] No STUN server answered, only a port UPnP opens lets members in");
	{
		std::lock_guard<std::mutex> lk(m_shared->lock);
		m_shared->publicPort = publicPort;
		m_shared->isPortReady = true;
	}

	m_startMs = Common::Timer::GetTimeMs();
	m_lastActivityMs = m_startMs;
	m_lastStunMs = m_startMs;
	{
		std::lock_guard<std::mutex> lk(m_shared->lock);
		SlippiRoom &room = m_shared->room;
		m_lastActivityCount = room.ActivityCount();

		// Everyone in the copy reconnects to us through the room's code
		if (m_tookOver)
		{
			for (int i = 0; i < room.MemberCount(); i++)
			{
				if (room.MemberCode(i) != m_identity.connectCode && !room.IsTestPlayer(i))
					m_awaiting[room.MemberCode(i)] = m_startMs;
			}
		}
	}

	while (!m_stop)
	{
		if (m_shared->leaving)
		{
			leave();
			break;
		}
		if (!m_shared->running)
			break;

		ENetEvent netEvent;
		int net = enet_host_service(m_host, &netEvent, SERVICE_INTERVAL_MS);
		if (net > 0)
		{
			switch (netEvent.type)
			{
			case ENET_EVENT_TYPE_CONNECT:
				enet_peer_timeout(netEvent.peer, 0, PEER_TIMEOUT_MIN_MS, PEER_TIMEOUT_MAX_MS);
				break;
			case ENET_EVENT_TYPE_RECEIVE:
			{
				std::string contents(reinterpret_cast<char *>(netEvent.packet->data), netEvent.packet->dataLength);
				enet_packet_destroy(netEvent.packet);

				// A field of the wrong type throws while the message is read. Whoever sent it isn't running
				// this build's rooms, so they're dropped rather than taking the room down
				json msg = json::parse(contents, nullptr, false);
				bool isBad = !msg.is_object();
				if (!isBad)
				{
					try
					{
						onReceive(netEvent.peer, msg);
					}
					catch (const json::exception &e)
					{
						WARN_LOG(SLIPPI_ONLINE, "[Rooms] Bad message from a member: %s", e.what());
						isBad = true;
					}
				}
				if (isBad)
					enet_peer_disconnect_later(netEvent.peer, 0);
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT:
				onDisconnect(netEvent.peer);
				break;
			default:
				break;
			}
		}

		refreshPort();
		punchJoiners();

		std::lock_guard<std::mutex> lk(m_shared->lock);
		u32 now = Common::Timer::GetTimeMs();
		checkDirectory();
		checkReconnects(now);
		trackActivity(now);
		if (!m_stop)
		{
			m_shared->room.Update();
			broadcastIfChanged();
		}
	}

	// A room stopped without leaving, such as when emulation stops, closes if nobody else is in it
	{
		std::lock_guard<std::mutex> lk(m_shared->lock);
		if (!m_drain && !m_shared->next && m_peerCodes.empty())
			m_shared->closeRoom = true;
	}

	for (auto &it : m_peerCodes)
		enet_peer_disconnect_later(it.first, 0);
	if (m_drain)
		drain(m_host);
	else
		enet_host_flush(m_host);
	closeHost(m_host);

	m_shared->netDone = true;
	enet_deinitialize();
}

// Keeps the router's outside port open, and follows it when the router moves it
void SlippiRoomHost::NetThread::refreshPort()
{
	std::string ip;
	u16 port = 0;
	SlippiStun::TakeReply(m_host, ip, port);
	u32 now = Common::Timer::GetTimeMs();
	bool isRefreshDue;
	{
		std::lock_guard<std::mutex> lk(m_shared->lock);
		if (port && port != m_shared->publicPort)
		{
			INFO_LOG(SLIPPI_ONLINE, "[Rooms] The room's outside port moved to %d", port);
			m_shared->publicPort = port;
			m_shared->isPortChanged = true;
		}
		isRefreshDue = m_shared->publicPort && now - m_lastStunMs >= STUN_REFRESH_MS;
	}

	// Looking the server up can take a moment, so it isn't done while holding the room
	if (isRefreshDue)
	{
		m_lastStunMs = now;
		SlippiStun::SendRequest(m_host);
	}
}

// Joiners connect to the room's outside port, which the router only lets them through once the room
// has sent something their way
void SlippiRoomHost::NetThread::punchJoiners()
{
	std::deque<std::string> punches;
	{
		std::lock_guard<std::mutex> lk(m_shared->lock);
		punches.swap(m_shared->punches);
	}

	for (const std::string &address : punches)
	{
		INFO_LOG(SLIPPI_ONLINE, "[Rooms] Punching through to a joiner");
		SlippiStun::Punch(m_host, address);
	}
}

void SlippiRoomHost::NetThread::onReceive(ENetPeer *peer, const json &msg)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	std::string type = msg.value("type", "");

	if (type == "hello")
		return onHello(peer, msg);

	auto it = m_peerCodes.find(peer);
	if (it == m_peerCodes.end())
		return;

	SlippiRoom &room = m_shared->room;
	int member = room.FindMember(it->second);
	if (type == "action")
	{
		u8 action = msg.value("action", 0xFF);

		// Test players and leaving are only done by the host, leaving by disconnecting
		if (action == SlippiRoom::ACTION_ADD_TEST_PLAYER || action == SlippiRoom::ACTION_FINISH_SET)
			return;

		room.HandleAction(member, action, msg.value("value0", 0), msg.value("value1", 0));
	}
	else if (type == "result")
	{
		room.ReportMatchResult(member, static_cast<SlippiRoom::MatchResult>(msg.value("result", 0)));
	}
	else if (type == "leave")
	{
		m_leavingPeers.insert(peer);
	}
	else if (type == "matchAddress")
	{
		room.SetMatchAddress(member, msg.value("address", ""));
	}
	else if (type == "watchAddress")
	{
		room.SetWatchAddress(member, msg.value("address", ""));
	}
}

void SlippiRoomHost::NetThread::onHello(ENetPeer *peer, const json &msg)
{
	SlippiRoom &room = m_shared->room;
	std::string connectCode = msg.value("connectCode", "");
	if (msg.value("version", 0) != PROTOCOL_VERSION)
		return reject(peer, "version");
	if (!room.Password().empty() && msg.value("password", "") != room.Password())
		return reject(peer, "wrong_password");
	if (connectCode.empty() || connectCode == m_identity.connectCode)
		return reject(peer, "already_joined");

	// A member reconnecting, such as after the host changed, replaces any connection they had
	if (room.FindMember(connectCode) >= 0)
	{
		for (auto it = m_peerCodes.begin(); it != m_peerCodes.end(); ++it)
		{
			if (it->second == connectCode)
			{
				enet_peer_disconnect(it->first, 0);
				m_peerCodes.erase(it);
				break;
			}
		}

		m_peerCodes[peer] = connectCode;
		m_awaiting.erase(connectCode);
		m_lastBroadcast.clear();
		return;
	}

	bool isReturningHost = room.IsReturningHost(connectCode);
	if (room.AddMember(msg.value("name", ""), connectCode,
	                   msg.value("lastChar", static_cast<int>(SlippiRoom::CHAR_RANDOM)), msg.value("lastColor", 0)) < 0)
		return reject(peer, "full");

	m_peerCodes[peer] = connectCode;

	// Make sure the new member gets the room right away
	m_lastBroadcast.clear();

	if (isReturningHost)
		handBack(connectCode);
}

// Members who say they're leaving are gone for good. Anyone else dropped, and keeps their crowns
// for a while in case they come back
void SlippiRoomHost::NetThread::onDisconnect(ENetPeer *peer)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	bool isLeaving = m_leavingPeers.erase(peer) > 0;

	auto it = m_peerCodes.find(peer);
	if (it == m_peerCodes.end())
		return;

	SlippiRoom &room = m_shared->room;
	int member = room.FindMember(it->second);
	if (isLeaving)
		room.RemoveMember(member);
	else
		room.DropMember(member);
	m_peerCodes.erase(it);
}

void SlippiRoomHost::NetThread::reject(ENetPeer *peer, const std::string &reason)
{
	WARN_LOG(SLIPPI_ONLINE, "[Rooms] Rejected a member, reason: %s", reason.c_str());
	sendMessage(peer, {{"type", "reject"}, {"reason", reason}});
	enet_peer_disconnect_later(peer, 0);
}

void SlippiRoomHost::NetThread::broadcast(const json &msg)
{
	for (auto &it : m_peerCodes)
		sendMessage(it.first, msg);
}

// Sends every member the room when anything about it changes, telling each which member they are
void SlippiRoomHost::NetThread::broadcastIfChanged()
{
	SlippiRoom &room = m_shared->room;
	json j = room.ToJson();
	std::string contents = j.dump();
	if (contents == m_lastBroadcast)
		return;
	m_lastBroadcast = contents;

	for (auto &it : m_peerCodes)
		sendMessage(it.first, {{"type", "state"}, {"you", room.FindMember(it.second)}, {"room", j}});
}

// Leaving on purpose hands the room to the member who has been in it longest, or closes it when
// nobody else is in it
void SlippiRoomHost::NetThread::leave()
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	std::string successor;
	for (const std::string &code : m_shared->room.Successors())
	{
		bool isConnected = false;
		for (auto &it : m_peerCodes)
		{
			if (it.second == code)
				isConnected = true;
		}
		if (isConnected)
		{
			successor = code;
			break;
		}
	}

	if (successor.empty())
	{
		m_shared->closeRoom = true;
	}
	else
	{
		INFO_LOG(SLIPPI_ONLINE, "[Rooms] Leaving, %s hosts the room now", successor.c_str());
		broadcast({{"type", "handover"}, {"to", successor}, {"reason", "left"}});
	}

	m_drain = true;
	m_stop = true;
}

// The host who was lost came back in time, so they get the room back and this host stays on as a
// member
void SlippiRoomHost::NetThread::handBack(const std::string &connectCode)
{
	INFO_LOG(SLIPPI_ONLINE, "[Rooms] Handing the room back to %s", connectCode.c_str());

	// Everyone, including the returning host, gets the room as it is now first
	broadcastIfChanged();
	broadcast({{"type", "handover"}, {"to", connectCode}, {"reason", "handback"}});

	SlippiRoom &room = m_shared->room;
	auto next = std::make_unique<Reconnect>();
	next->room = room;
	next->change = SlippiRoom::HOST_HANDED_BACK;
	next->knownGeneration = room.Generation();
	next->candidates.push_back(connectCode);
	for (const std::string &code : room.Successors())
	{
		if (code != connectCode)
			next->candidates.push_back(code);
	}

	m_shared->next = std::move(next);
	m_shared->status = STATUS_RECONNECTING;
	m_drain = true;
	m_stop = true;
}

// Another member took the room over, so this host rejoins them
void SlippiRoomHost::NetThread::stepDown()
{
	INFO_LOG(SLIPPI_ONLINE, "[Rooms] Another member hosts room %s now, rejoining", m_shared->room.Code().c_str());

	auto next = std::make_unique<Reconnect>();
	next->room = m_shared->room;
	next->knownGeneration = m_shared->room.Generation();

	m_shared->next = std::move(next);
	m_shared->status = STATUS_RECONNECTING;
	m_stop = true;
}

void SlippiRoomHost::NetThread::close(ConnectionError error)
{
	broadcast({{"type", "closed"}, {"error", error}});
	m_shared->status = STATUS_FAILED;
	m_shared->error = error;
	m_shared->closeRoom = true;
	m_drain = true;
	m_stop = true;
}

void SlippiRoomHost::NetThread::checkDirectory()
{
	if (m_shared->registered)
		m_shared->room.SetCode(m_shared->reg.code);

	if (m_shared->replaced)
	{
		m_shared->replaced = false;
		stepDown();
	}
	else if (m_shared->gone)
	{
		// The directory drops a room after an hour without activity, or once its host hasn't been heard
		// from for a couple of minutes, such as after losing the internet
		WARN_LOG(SLIPPI_ONLINE, "[Rooms] The directory no longer has room %s", m_shared->room.Code().c_str());
		bool isIdle = Common::Timer::GetTimeMs() - m_lastActivityMs >= IDLE_CLOSE_MS - IDLE_SLACK_MS;
		close(isIdle ? CONNECT_IDLE : CONNECT_DISCONNECTED);
	}
}

// Members who don't reconnect after a take-over dropped too
void SlippiRoomHost::NetThread::checkReconnects(u32 now)
{
	SlippiRoom &room = m_shared->room;
	for (auto it = m_awaiting.begin(); it != m_awaiting.end();)
	{
		if (now - it->second < RECONNECT_WAIT_MS)
		{
			++it;
			continue;
		}

		WARN_LOG(SLIPPI_ONLINE, "[Rooms] %s did not reconnect", it->first.c_str());
		room.DropMember(room.FindMember(it->first));
		it = m_awaiting.erase(it);
	}
}

void SlippiRoomHost::NetThread::trackActivity(u32 now)
{
	SlippiRoom &room = m_shared->room;
	if (room.ActivityCount() != m_lastActivityCount)
	{
		m_lastActivityCount = room.ActivityCount();
		m_lastActivityMs = now;
		m_shared->activityWanted = true;
		m_shared->memberCount = static_cast<u8>(room.MemberCount());
	}

	// When nobody reconnects after a take-over, make sure no other member took the room over instead
	if (m_tookOver && !m_verified && now - m_startMs >= RECONNECT_WAIT_MS)
	{
		m_verified = true;
		if (m_peerCodes.empty())
			m_shared->activityWanted = true;
	}

	if (now - m_lastActivityMs >= IDLE_CLOSE_MS)
	{
		INFO_LOG(SLIPPI_ONLINE, "[Rooms] Closing room %s after an hour without activity", room.Code().c_str());
		close(CONNECT_IDLE);
	}
}

SlippiRoomHost::SlippiRoomHost(const Identity &identity, const SlippiExiTypes::CreateRoomQuery &query)
    : m_identity(identity)
    , m_shared(std::make_shared<Shared>())
{
	// The host is the room's first member, starting on their last pick
	SlippiRoom &room = m_shared->room;
	room = SlippiRoom(query);
	room.AddMember(identity.name, identity.connectCode, query.last_char, query.last_color);
	room.SetHostCode(identity.connectCode);

	SlippiRoomDirectory::RoomInfo info;
	info.listed = room.IsListed();
	info.password = room.Password();
	info.hostName = identity.name;
	info.hostCode = identity.connectCode;
	info.mode = room.Mode();
	info.stageMode = room.StageMode();
	info.capacity = room.Capacity();
	info.utcOffset = localUtcOffset();
	start(info, false);
}

SlippiRoomHost::SlippiRoomHost(const Identity &identity, const SlippiRoom &copy, SlippiRoom::HostChange change)
    : m_identity(identity)
    , m_shared(std::make_shared<Shared>())
{
	SlippiRoom &room = m_shared->room;
	room = copy;
	room.TakeOver(identity.connectCode, change);
	INFO_LOG(SLIPPI_ONLINE, "[Rooms] Taking over room %s", room.Code().c_str());

	SlippiRoomDirectory::RoomInfo info;
	info.listed = room.IsListed();
	info.password = room.Password();
	info.hostName = identity.name;
	info.hostCode = identity.connectCode;
	info.mode = room.Mode();
	info.stageMode = room.StageMode();
	info.capacity = room.Capacity();
	start(info, true);
}

SlippiRoomHost::~SlippiRoomHost()
{
	m_shared->running = false;
}

void SlippiRoomHost::start(SlippiRoomDirectory::RoomInfo info, bool isTakeOver)
{
	u16 port = 0;
	ENetHost *host = createRoomHost(port);
	if (!host)
	{
		ERROR_LOG(SLIPPI_ONLINE, "[Rooms] Could not open a port for the room, it can't be joined");
		m_shared->unavailable = true;
		m_shared->netDone = true;
		return;
	}

	host->intercept = SlippiStun::Intercept;
	info.port = port;
	std::string takeOverCode = isTakeOver ? m_shared->room.Code() : "";
	std::thread(&SlippiRoomHost::directoryThread, m_shared, info, takeOverCode, m_shared->room.Generation()).detach();
	std::thread(&SlippiRoomHost::runNetThread, m_shared, m_identity, host, isTakeOver).detach();
}

void SlippiRoomHost::runNetThread(std::shared_ptr<Shared> shared, Identity identity, ENetHost *host, bool tookOver)
{
	NetThread(std::move(shared), std::move(identity), host, tookOver).Run();
}

// Registers the room or takes it over, then reports activity to the directory as it happens
void SlippiRoomHost::directoryThread(std::shared_ptr<Shared> shared, SlippiRoomDirectory::RoomInfo info,
                                     std::string takeOverCode, int takeOverGeneration)
{
	auto directory = SlippiRoomDirectory::Create();

	bool mapped = false;
	u16 localPort = info.port;
#ifdef USE_UPNP
	UPNPUrls urls = {};
	IGDdatas data = {};
	mapped = mapPort(localPort, urls, data);
#endif

	// Members connect to the port UPnP opened, or else the outside port a STUN server saw, which the
	// room's connection looks up before anything else
	while (!shared->netDone)
	{
		{
			std::lock_guard<std::mutex> lk(shared->lock);
			if (shared->isPortReady)
			{
				if (!mapped && shared->publicPort)
					info.port = shared->publicPort;
				break;
			}
		}
		Common::SleepCurrentThread(50);
	}

	SlippiRoomDirectory::Registration reg;
	bool isRegistered = false;
	bool isTakeOver = !takeOverCode.empty();
	while (shared->running && !shared->netDone && !isRegistered)
	{
		if (isTakeOver)
		{
			auto result = directory->TakeOver(takeOverCode, takeOverGeneration, info);
			if (result.status == SlippiRoomDirectory::TakeOverStatus::OK)
			{
				reg = result.reg;
				isRegistered = true;
				std::lock_guard<std::mutex> lk(shared->lock);
				shared->room.SetGeneration(result.generation);
			}
			else if (result.status == SlippiRoomDirectory::TakeOverStatus::TAKEN)
			{
				std::lock_guard<std::mutex> lk(shared->lock);
				shared->replaced = true;
				break;
			}
			else if (result.status == SlippiRoomDirectory::TakeOverStatus::GONE)
			{
				std::lock_guard<std::mutex> lk(shared->lock);
				shared->gone = true;
				break;
			}
		}
		else
		{
			reg = directory->Register(info);
			isRegistered = !reg.code.empty();
			std::lock_guard<std::mutex> lk(shared->lock);
			shared->unavailable = !isRegistered;
		}

		if (isRegistered)
		{
			INFO_LOG(SLIPPI_ONLINE, "[Rooms] %s room %s", isTakeOver ? "Took over" : "Registered", reg.code.c_str());
			std::lock_guard<std::mutex> lk(shared->lock);
			shared->reg = reg;
			shared->registered = true;
			break;
		}

		Common::SleepCurrentThread(isTakeOver ? RETRY_MS : ACTIVITY_RETRY_MS);
	}

	u32 lastReportMs = 0;
	u32 lastPollMs = 0;
	u32 waitMs = 0;
	s64 lastRequestId = 0;
	std::vector<SlippiRoomDirectory::JoinRequest> requests;
	while (isRegistered && !shared->netDone)
	{
		u32 now = Common::Timer::GetTimeMs();
		if (now - lastPollMs >= JOIN_POLL_MS)
		{
			lastPollMs = now;
			if (directory->JoinRequests(reg, lastRequestId, requests))
			{
				std::lock_guard<std::mutex> lk(shared->lock);
				for (const auto &request : requests)
				{
					lastRequestId = std::max(lastRequestId, request.id);
					shared->punches.push_back(request.address);
				}
			}
		}

		bool isWanted;
		u8 memberCount;
		u16 port = 0;
		{
			std::lock_guard<std::mutex> lk(shared->lock);
			isWanted = shared->activityWanted;
			memberCount = shared->memberCount;
			if (!mapped && shared->isPortChanged)
			{
				isWanted = true;
				port = shared->publicPort;
			}
		}

		if (!isWanted || now - lastReportMs < waitMs)
		{
			Common::SleepCurrentThread(100);
			continue;
		}

		{
			std::lock_guard<std::mutex> lk(shared->lock);
			shared->activityWanted = false;
			shared->isPortChanged = false;
		}
		lastReportMs = now;
		waitMs = ACTIVITY_INTERVAL_MS;

		auto status = directory->Activity(reg, memberCount, port);
		if (status == SlippiRoomDirectory::ActivityStatus::UNAVAILABLE)
		{
			std::lock_guard<std::mutex> lk(shared->lock);
			shared->activityWanted = true;
			shared->isPortChanged = port != 0;
			waitMs = ACTIVITY_RETRY_MS;
		}
		else if (status == SlippiRoomDirectory::ActivityStatus::REPLACED)
		{
			std::lock_guard<std::mutex> lk(shared->lock);
			shared->replaced = true;
			break;
		}
		else if (status == SlippiRoomDirectory::ActivityStatus::GONE)
		{
			std::lock_guard<std::mutex> lk(shared->lock);
			shared->gone = true;
			break;
		}
	}

	// The room is given up only when the host closes it, not when it's handed to another member
	while (!shared->netDone)
		Common::SleepCurrentThread(100);

	if (isRegistered && shared->closeRoom)
		directory->Unregister(reg);

#ifdef USE_UPNP
	if (mapped)
		unmapPort(localPort, urls, data);
#endif
}

void SlippiRoomHost::HandleLocalAction(u8 action, u8 value0, u8 value1)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	SlippiRoom &room = m_shared->room;
	room.HandleAction(room.FindMember(m_identity.connectCode), action, value0, value1);
}

void SlippiRoomHost::ReportMatchResult(SlippiRoom::MatchResult result)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	SlippiRoom &room = m_shared->room;
	room.ReportMatchResult(room.FindMember(m_identity.connectCode), result);
}

SlippiExiTypes::GetRoomStateResponse SlippiRoomHost::GetState()
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	SlippiRoom &room = m_shared->room;
	if (m_shared->status == STATUS_HOSTING)
		room.Update();

	SlippiExiTypes::GetRoomStateResponse resp = room.GetState(room.FindMember(m_identity.connectCode));
	resp.connection_status = m_shared->status;
	resp.connection_error = m_shared->error;
	if (m_shared->status == STATUS_HOSTING && m_shared->unavailable)
		resp.connection_error = CONNECT_UNAVAILABLE;
	return resp;
}

void SlippiRoomHost::AddTestPlayer()
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	m_shared->room.AddTestPlayer();
}

void SlippiRoomHost::SetMatchAddress(const std::string &address)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	SlippiRoom &room = m_shared->room;
	room.SetMatchAddress(room.FindMember(m_identity.connectCode), address);
}

void SlippiRoomHost::SetWatchAddress(const std::string &address)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	SlippiRoom &room = m_shared->room;
	room.SetWatchAddress(room.FindMember(m_identity.connectCode), address);
}

SlippiRoom SlippiRoomHost::CopyRoom()
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	return m_shared->room;
}

void SlippiRoomHost::Leave()
{
	m_shared->leaving = true;
}

std::unique_ptr<SlippiRoomSession> SlippiRoomHost::TakeNext()
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	if (!m_shared->next)
		return nullptr;

	auto member = std::make_unique<SlippiRoomMember>(m_identity, *m_shared->next);
	m_shared->next = nullptr;
	return member;
}

// Finds the host through the room's code, stays connected to them, and when they're gone either
// takes the room over or finds the member who did
class SlippiRoomMember::NetThread
{
  public:
	NetThread(std::shared_ptr<Shared> shared, Identity identity, u8 lastChar, u8 lastColor,
	          std::shared_ptr<Reconnect> reconnect)
	    : m_shared(std::move(shared))
	    , m_identity(std::move(identity))
	    , m_lastChar(lastChar)
	    , m_lastColor(lastColor)
	{
		if (reconnect)
		{
			m_isReconnecting = true;
			m_candidates = reconnect->candidates;
			m_change = reconnect->change;
			m_knownGeneration = reconnect->knownGeneration;
			m_lostMs = Common::Timer::GetTimeMs();
		}
	}

	void Run();

  private:
	// Returns false once this member is done with the room
	bool connect(const SlippiRoomDirectory::JoinResult &joinResult, ENetHost *client);
	bool onHostGone(const std::string &handoverTo, const std::string &handoverReason);
	int candidateRank();
	void promote();
	void fail(ConnectionError error);

	std::shared_ptr<Shared> m_shared;
	Identity m_identity;
	u8 m_lastChar;
	u8 m_lastColor;

	bool m_isReconnecting = false;
	std::vector<std::string> m_candidates;
	SlippiRoom::HostChange m_change = SlippiRoom::HOST_LOST;
	int m_knownGeneration = 0;
	u32 m_lostMs = 0;
};

void SlippiRoomMember::NetThread::Run()
{
	// Keeps networking up while this thread runs, even if emulation stops first
	enet_initialize();

	auto directory = SlippiRoomDirectory::Create();
	while (m_shared->running && !m_shared->leaving)
	{
		int rank = candidateRank();
		if (m_isReconnecting && rank == 0)
		{
			promote();
			break;
		}

		// While the host is changing, only look for whether anyone has taken over. The socket and the
		// join request the new host punches through to are only set up once someone has
		if (m_isReconnecting)
		{
			auto check = directory->Join(m_shared->code, m_shared->password, 0);
			if (!m_shared->running || m_shared->leaving)
				break;
			if (check.status == SlippiRoomDirectory::JoinStatus::NOT_FOUND)
			{
				fail(CONNECT_DISCONNECTED);
				break;
			}

			bool isTakenOver =
			    check.status == SlippiRoomDirectory::JoinStatus::OK && check.generation > m_knownGeneration;
			if (!isTakenOver)
			{
				// Take over once the candidates before this member had their turn
				if (check.status == SlippiRoomDirectory::JoinStatus::OK && rank > 0 &&
				    Common::Timer::GetTimeMs() - m_lostMs >= rank * CANDIDATE_WAIT_MS)
				{
					promote();
					break;
				}

				Common::SleepCurrentThread(RETRY_MS);
				continue;
			}
		}

		// The outside port a STUN server sees for this connection goes with the join, so the host can
		// punch through to it
		ENetHost *client = enet_host_create(nullptr, 1, 1, 0, 0);
		if (!client)
		{
			fail(CONNECT_UNREACHABLE);
			break;
		}
		client->intercept = SlippiStun::Intercept;
		std::string publicIp;
		u16 port = 0;
		SlippiStun::Discover(client, publicIp, port);

		auto joinResult = directory->Join(m_shared->code, m_shared->password, port);
		if (!m_shared->running || m_shared->leaving)
		{
			closeHost(client);
			break;
		}

		if (joinResult.status != SlippiRoomDirectory::JoinStatus::OK)
		{
			closeHost(client);
			if (!m_isReconnecting)
			{
				fail(joinError(joinResult.status));
				break;
			}

			// The room changed since it was looked up, so look again
			Common::SleepCurrentThread(RETRY_MS);
			continue;
		}

		if (!connect(joinResult, client))
			break;
	}

	enet_deinitialize();
}

bool SlippiRoomMember::NetThread::connect(const SlippiRoomDirectory::JoinResult &joinResult, ENetHost *client)
{
	// Rooms connect over IPv4
	std::vector<std::string> parts;
	SplitString(joinResult.address, ':', parts);
	std::string host = parts.size() == 2 ? parts[0] : "";
	u16 port = parts.size() == 2 ? static_cast<u16>(std::atoi(parts[1].c_str())) : 0;

#ifdef LOCAL_TESTING
	// Test clients on one computer can't reach each other through their shared public address
	host = "127.0.0.1";
#endif

	ENetAddress addr;
	enet_address_set_host(&addr, host.c_str());
	addr.port = port;
	ENetPeer *peer = enet_host_connect(client, &addr, 1, 0);
	if (!peer)
	{
		closeHost(client);
		fail(CONNECT_UNREACHABLE);
		return false;
	}
	enet_peer_timeout(peer, 0, CONNECT_TIMEOUT_MS, CONNECT_TIMEOUT_MS);

	INFO_LOG(SLIPPI_ONLINE, "[Rooms] Connecting to room %s", m_shared->code.c_str());

	bool isConnected = false;
	u32 startMs = Common::Timer::GetTimeMs();
	while (m_shared->running && !m_shared->leaving && !isConnected &&
	       Common::Timer::GetTimeMs() - startMs < CONNECT_TIMEOUT_MS)
	{
		ENetEvent netEvent;
		if (enet_host_service(client, &netEvent, SERVICE_INTERVAL_MS) > 0 && netEvent.type == ENET_EVENT_TYPE_CONNECT)
			isConnected = true;
	}

	if (!isConnected)
	{
		closeHost(client);
		if (!m_shared->running || m_shared->leaving)
			return false;
		if (!m_isReconnecting)
		{
			fail(CONNECT_UNREACHABLE);
			return false;
		}

		// Nobody can reach the new host, so the next candidate takes over from them
		WARN_LOG(SLIPPI_ONLINE, "[Rooms] Could not reach the new host %s", joinResult.hostCode.c_str());
		m_candidates.erase(std::remove(m_candidates.begin(), m_candidates.end(), joinResult.hostCode),
		                   m_candidates.end());
		m_knownGeneration = joinResult.generation;
		m_lostMs = Common::Timer::GetTimeMs();
		return true;
	}

	enet_peer_timeout(peer, 0, PEER_TIMEOUT_MIN_MS, PEER_TIMEOUT_MAX_MS);
	sendMessage(peer, {
	                      {"type", "hello"},
	                      {"version", PROTOCOL_VERSION},
	                      {"name", m_identity.name},
	                      {"connectCode", m_identity.connectCode},
	                      {"password", m_shared->password},
	                      {"lastChar", m_lastChar},
	                      {"lastColor", m_lastColor},
	                  });

	std::string handoverTo;
	std::string handoverReason;
	while (true)
	{
		if (m_shared->leaving)
		{
			sendMessage(peer, {{"type", "leave"}});
			enet_peer_disconnect_later(peer, 0);
			drain(client);
			closeHost(client);
			return false;
		}
		if (!m_shared->running)
		{
			enet_peer_disconnect(peer, 0);
			enet_host_flush(client);
			closeHost(client);
			return false;
		}

		ENetEvent netEvent;
		int net = enet_host_service(client, &netEvent, SERVICE_INTERVAL_MS);
		if (net > 0 && netEvent.type == ENET_EVENT_TYPE_RECEIVE)
		{
			std::string contents(reinterpret_cast<char *>(netEvent.packet->data), netEvent.packet->dataLength);
			enet_packet_destroy(netEvent.packet);

			// A field of the wrong type throws while the message is read, and the message is ignored. The
			// room is read into a copy, so a bad one leaves the last good one in place
			json msg = json::parse(contents, nullptr, false);
			try
			{
				std::string type = msg.is_object() ? msg.value("type", "") : "";
				if (type == "state")
				{
					std::lock_guard<std::mutex> lk(m_shared->lock);
					SlippiRoom room = m_shared->room;
					room.FromJson(msg.value("room", json::object()));
					int you = msg.value("you", -1);
					if (you >= 0 && you < room.MemberCount())
					{
						m_shared->room = room;
						m_shared->localMember = you;
						m_shared->status = STATUS_JOINED;
						m_isReconnecting = false;
					}
				}
				else if (type == "handover")
				{
					handoverTo = msg.value("to", "");
					handoverReason = msg.value("reason", "");
				}
				else if (type == "closed")
				{
					fail(static_cast<ConnectionError>(msg.value("error", static_cast<int>(CONNECT_IDLE))));
				}
				else if (type == "reject")
				{
					std::string reason = msg.value("reason", "");
					WARN_LOG(SLIPPI_ONLINE, "[Rooms] The room rejected us, reason: %s", reason.c_str());

					ConnectionError error = CONNECT_REJECTED;
					if (reason == "full")
						error = CONNECT_FULL;
					else if (reason == "wrong_password")
						error = CONNECT_WRONG_PASSWORD;
					fail(error);
				}
			}
			catch (const json::exception &e)
			{
				WARN_LOG(SLIPPI_ONLINE, "[Rooms] Bad message from the room: %s", e.what());
			}
		}
		else if (net > 0 && netEvent.type == ENET_EVENT_TYPE_DISCONNECT)
		{
			closeHost(client);
			return onHostGone(handoverTo, handoverReason);
		}

		std::lock_guard<std::mutex> lk(m_shared->lock);
		while (!m_shared->outgoing.empty())
		{
			sendMessage(peer, m_shared->outgoing.front());
			m_shared->outgoing.pop_front();
		}
	}
}

// The host handed the room to someone or was lost. Returns false when the room is over for this
// member instead
bool SlippiRoomMember::NetThread::onHostGone(const std::string &handoverTo, const std::string &handoverReason)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	if (m_shared->status == STATUS_FAILED)
		return false;
	if (m_shared->status != STATUS_JOINED)
	{
		m_shared->status = STATUS_FAILED;
		m_shared->error = CONNECT_DISCONNECTED;
		return false;
	}

	SlippiRoom &room = m_shared->room;
	m_isReconnecting = true;
	m_knownGeneration = room.Generation();
	m_lostMs = Common::Timer::GetTimeMs();
	m_shared->status = STATUS_RECONNECTING;

	// Results of matches played while the host changed still count. Anything else is out of date
	std::deque<json> results;
	for (const json &msg : m_shared->outgoing)
	{
		if (msg.value("type", "") == "result")
			results.push_back(msg);
	}
	m_shared->outgoing = results;

	m_candidates.clear();
	if (!handoverTo.empty())
	{
		m_candidates.push_back(handoverTo);
		m_change = handoverReason == "handback" ? SlippiRoom::HOST_HANDED_BACK : SlippiRoom::HOST_LEFT;
	}
	else
	{
		m_change = SlippiRoom::HOST_LOST;
	}
	for (const std::string &code : room.Successors())
	{
		if (code != handoverTo)
			m_candidates.push_back(code);
	}

	INFO_LOG(SLIPPI_ONLINE, "[Rooms] The host %s, finding room %s again",
	         handoverTo.empty() ? "was lost" : "handed the room over", m_shared->code.c_str());
	return true;
}

int SlippiRoomMember::NetThread::candidateRank()
{
	for (size_t i = 0; i < m_candidates.size(); i++)
	{
		if (m_candidates[i] == m_identity.connectCode)
			return static_cast<int>(i);
	}
	return -1;
}

void SlippiRoomMember::NetThread::promote()
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	m_shared->promote = true;
	m_shared->promoteChange = m_change;
}

void SlippiRoomMember::NetThread::fail(ConnectionError error)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	m_shared->status = STATUS_FAILED;
	m_shared->error = error;
}

SlippiRoomMember::SlippiRoomMember(const Identity &identity, const SlippiExiTypes::JoinRoomQuery &query)
    : m_identity(identity)
    , m_shared(std::make_shared<Shared>())
{
	m_shared->code = std::string(query.code, strnlen(query.code, sizeof(query.code)));
	m_shared->password = std::string(query.password, strnlen(query.password, sizeof(query.password)));
	std::thread(&SlippiRoomMember::runNetThread, m_shared, m_identity, query.last_char, query.last_color, nullptr)
	    .detach();
}

SlippiRoomMember::SlippiRoomMember(const Identity &identity, const Reconnect &reconnect)
    : m_identity(identity)
    , m_shared(std::make_shared<Shared>())
{
	m_shared->room = reconnect.room;
	m_shared->code = reconnect.room.Code();
	m_shared->password = reconnect.room.Password();
	m_shared->localMember = reconnect.room.FindMember(identity.connectCode);
	m_shared->status = STATUS_RECONNECTING;
	std::thread(&SlippiRoomMember::runNetThread, m_shared, m_identity, SlippiRoom::CHAR_RANDOM, 0,
	            std::make_shared<Reconnect>(reconnect))
	    .detach();
}

SlippiRoomMember::~SlippiRoomMember()
{
	m_shared->running = false;
}

void SlippiRoomMember::runNetThread(std::shared_ptr<Shared> shared, Identity identity, u8 lastChar, u8 lastColor,
                                    std::shared_ptr<Reconnect> reconnect)
{
	NetThread(std::move(shared), std::move(identity), lastChar, lastColor, std::move(reconnect)).Run();
}

void SlippiRoomMember::HandleLocalAction(u8 action, u8 value0, u8 value1)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	if (m_shared->status != STATUS_JOINED)
		return;

	m_shared->outgoing.push_back({{"type", "action"}, {"action", action}, {"value0", value0}, {"value1", value1}});
}

void SlippiRoomMember::ReportMatchResult(SlippiRoom::MatchResult result)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	m_shared->outgoing.push_back({{"type", "result"}, {"result", result}});
}

void SlippiRoomMember::SetMatchAddress(const std::string &address)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	m_shared->outgoing.push_back({{"type", "matchAddress"}, {"address", address}});
}

void SlippiRoomMember::SetWatchAddress(const std::string &address)
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	m_shared->outgoing.push_back({{"type", "watchAddress"}, {"address", address}});
}

SlippiRoom SlippiRoomMember::CopyRoom()
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	return m_shared->room;
}

SlippiExiTypes::GetRoomStateResponse SlippiRoomMember::GetState()
{
	std::lock_guard<std::mutex> lk(m_shared->lock);

	SlippiExiTypes::GetRoomStateResponse resp = {};
	if (m_shared->status == STATUS_JOINED || m_shared->status == STATUS_RECONNECTING)
		resp = m_shared->room.GetState(m_shared->localMember);

	resp.is_active = 1;
	resp.connection_status = m_shared->status;
	resp.connection_error = m_shared->error;
	strncpy(resp.code, m_shared->code.c_str(), sizeof(resp.code) - 1);
	return resp;
}

void SlippiRoomMember::Leave()
{
	m_shared->leaving = true;
}

// Once this member takes over, the room carries on from its copy. Results waiting to be sent are
// applied to it directly
std::unique_ptr<SlippiRoomSession> SlippiRoomMember::TakeNext()
{
	std::lock_guard<std::mutex> lk(m_shared->lock);
	if (!m_shared->promote)
		return nullptr;

	auto host = std::make_unique<SlippiRoomHost>(m_identity, m_shared->room, m_shared->promoteChange);
	for (const json &msg : m_shared->outgoing)
	{
		if (msg.value("type", "") == "result")
			host->ReportMatchResult(static_cast<SlippiRoom::MatchResult>(msg.value("result", 0)));
	}
	m_shared->promote = false;
	return host;
}
