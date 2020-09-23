#include "SlippiSpectate.h"
#include "Common/Logging/Log.h"
#include "Common/CommonTypes.h"
#include <Core/ConfigManager.h>
#include "base64.hpp"

// Networking
#ifdef _WIN32
#include <share.h>
#include <ws2tcpip.h>
#else
#include <errno.h>
#endif

// CALLED FROM DOLPHIN MAIN THREAD
SlippiSpectateServer* SlippiSpectateServer::getInstance()
{
    static SlippiSpectateServer instance; // Guaranteed to be destroyed.
                                          // Instantiated on first use.
    return &instance;
}

// CALLED FROM DOLPHIN MAIN THREAD
void SlippiSpectateServer::write(u8 *payload, u32 length)
{
    if(!SConfig::GetInstance().m_enableSpectator)
    {
        return;
    }
    std::string str_payload((char*)payload, length);
    m_event_queue.Push(str_payload);
}

// CALLED FROM DOLPHIN MAIN THREAD
void SlippiSpectateServer::startGame()
{
    if(!SConfig::GetInstance().m_enableSpectator)
    {
        return;
    }
    m_event_queue.Push("START_GAME");
}

// CALLED FROM DOLPHIN MAIN THREAD
void SlippiSpectateServer::endGame()
{
    if(!SConfig::GetInstance().m_enableSpectator)
    {
        return;
    }
    m_event_queue.Push("END_GAME");
}

// CALLED FROM SERVER THREAD
void SlippiSpectateServer::writeEvents(u16 peer_id)
{
    // Send menu events
    if(!m_in_game && (m_sockets[peer_id]->m_menu_cursor != m_menu_cursor))
    {
        ENetPacket *packet = enet_packet_create(m_menu_event.data(),
                                                m_menu_event.length(),
                                                ENET_PACKET_FLAG_RELIABLE);
        // Batch for sending
        enet_peer_send(m_sockets[peer_id]->m_peer, 0, packet);
        // Record for the peer that it was sent
        m_sockets[peer_id]->m_menu_cursor = m_menu_cursor;
    }

    // Send game events
    // Loop through each event that needs to be sent
    //  send all the events starting at their cursor
    // If the client's cursor is beyond the end of the event buffer, then
    //  it's probably left over from an old game. (Or is invalid anyway)
    //  So reset it back to 0
    if(m_sockets[peer_id]->m_cursor > m_event_buffer.size())
    {
        m_sockets[peer_id]->m_cursor = 0;
    }

    for(u64 i = m_sockets[peer_id]->m_cursor; i < m_event_buffer.size(); i++)
    {
        ENetPacket *packet = enet_packet_create(m_event_buffer[i].data(),
                                                m_event_buffer[i].size(),
                                                ENET_PACKET_FLAG_RELIABLE);
        // Batch for sending
        enet_peer_send(m_sockets[peer_id]->m_peer, 0, packet);
        m_sockets[peer_id]->m_cursor++;
    }
}

// CALLED FROM SERVER THREAD
void SlippiSpectateServer::popEvents()
{
    // Loop through the event queue and keep popping off events and handlign them
    while(!m_event_queue.Empty())
    {
        std::string event;
        m_event_queue.Pop(event);
        // These two are meta-events, used to signify the start/end of a game
        //  They are not sent over the wire
        if(event == "END_GAME")
        {
            m_menu_cursor = 0;
            if(m_event_buffer.size() > 0)
            {
                m_cursor_offset += m_event_buffer.size();
            }
            m_menu_event.clear();
            m_in_game = false;
            continue;
        }
        if(event == "START_GAME")
        {
            m_event_buffer.clear();
            m_in_game = true;
            continue;
        }

        // Make json wrapper for game event
        json game_event;
        if(event.empty())
        {
            game_event["payload"] = "";
        }
        else
        {
            game_event["payload"] = base64::Base64::Encode(event);
        }
        if(m_in_game)
        {
            u32 cursor = (u32)m_event_buffer.size() + m_cursor_offset;
            game_event["type"] = "game_event";
            game_event["cursor"] = cursor;
            game_event["next_cursor"] = cursor+1;
            m_event_buffer.push_back(game_event.dump());
        }
        else
        {
            m_menu_cursor += 1;
            game_event["type"] = "menu_event";
            m_menu_event = game_event.dump();
        }
    }
}

// CALLED ONCE EVER, DOLPHIN MAIN THREAD
SlippiSpectateServer::SlippiSpectateServer()
{
    if(!SConfig::GetInstance().m_enableSpectator)
    {
        return;
    }

    m_in_game = false;
    m_menu_cursor = 0;

    // Init some timestamps
    m_last_broadcast_time = std::chrono::system_clock::now();

    // Spawn thread for socket listener
    m_stop_socket_thread = false;
    m_socketThread = std::thread(&SlippiSpectateServer::SlippicommSocketThread, this);
}

// CALLED FROM DOLPHIN MAIN THREAD
SlippiSpectateServer::~SlippiSpectateServer()
{
    // The socket thread will be blocked waiting for input
    // So to wake it up, let's connect to the socket!
    m_stop_socket_thread = true;
    if (m_socketThread.joinable())
    {
        m_socketThread.join();
    }
}

// CALLED FROM SERVER THREAD
void SlippiSpectateServer::writeBroadcast()
{
    sendto(m_broadcast_socket, (char*)&m_broadcast_message, sizeof(m_broadcast_message), 0,
        (struct sockaddr *)&m_broadcastAddr, sizeof(m_broadcastAddr));

    m_last_broadcast_time = std::chrono::system_clock::now();
}

// CALLED FROM SERVER THREAD
void SlippiSpectateServer::handleMessage(u8 *buffer, u32 length, u16 peer_id)
{
    // Unpack the message
    std::string message((char*)buffer, length);
    json json_message = json::parse(message);
    if(!json_message.is_discarded() && (json_message.find("type") != json_message.end()))
    {
        // Check what type of message this is
        if(!json_message["type"].is_string())
        {
            return;
        }

        if(json_message["type"] == "connect_request")
        {
            // Get the requested cursor
            if(json_message.find("cursor") == json_message.end())
            {
                return;
            }
            if(!json_message["cursor"].is_number_integer())
            {
                return;
            }
            u32 requested_cursor = json_message["cursor"];
            u32 sent_cursor = 0;
            // Set the user's cursor position
            if(requested_cursor >= m_cursor_offset)
            {
                // If the requested cursor is past what events we even have, then just tell them to start over
                if(requested_cursor > m_event_buffer.size() + m_cursor_offset)
                {
                    m_sockets[peer_id]->m_cursor = 0;
                }
                // Requested cursor is in the middle of a live match, events that we have
                else
                {
                    m_sockets[peer_id]->m_cursor = requested_cursor - m_cursor_offset;
                }
            }
            else
            {
                // The client requested a cursor that was too low. Bring them up to the present
                m_sockets[peer_id]->m_cursor = 0;
            }

            sent_cursor = (u32)m_sockets[peer_id]->m_cursor + (u32)m_cursor_offset;

            // If someone joins while at the menu, don't catch them up
            //  set their cursor to the end
            if(!m_in_game)
            {
                m_sockets[peer_id]->m_cursor = m_event_buffer.size();
            }

            json reply;
            reply["type"] = "connect_reply";
            reply["nick"] = "Slippi Online";
            reply["version"] = scm_slippi_semver_str;
            reply["cursor"] = sent_cursor;

            std::string packet_buffer = reply.dump();

            ENetPacket *packet = enet_packet_create(packet_buffer.data(),
                          (u32)packet_buffer.length(),
                          ENET_PACKET_FLAG_RELIABLE);

            // Batch for sending
            enet_peer_send(m_sockets[peer_id]->m_peer, 0, packet);
            // Put the client in the right in_game state
            m_sockets[peer_id]->m_shook_hands = true;
        }
    }
}

// CALLED FROM SERVER THREAD
void SlippiSpectateServer::sendHolePunchMsg(ENetHost *host, std::string remoteIp, u16 remotePort)
{
  	ENetAddress addr;
  	enet_address_set_host(&addr, remoteIp.c_str());
  	addr.port = remotePort;

  	auto server = enet_host_connect(host, &addr, 3, 0);
  	if (server == nullptr)
  	{
    		// Failed to connect to server
    		return;
  	}

  	enet_host_flush(host);
  	enet_peer_reset(server);
}

void SlippiSpectateServer::SlippicommSocketThread(void)
{
    // Setup the broadcast advertisement message and socket
    m_broadcast_socket = socket(AF_INET, SOCK_DGRAM, 0);
    int broadcastEnable=1;
    if(setsockopt(m_broadcast_socket, SOL_SOCKET, SO_BROADCAST,
       (char*)&broadcastEnable, sizeof(broadcastEnable)))
    {
        WARN_LOG(SLIPPI, "Failed configuring Slippi braodcast socket");
        return;
    }
    memset(&m_broadcastAddr, 0, sizeof(m_broadcastAddr));
    m_broadcastAddr.sin_family = AF_INET;
    m_broadcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
    m_broadcastAddr.sin_port = htons(20582);

    // Setup the broadcast message
    //  It never changes, so let's just do this once
    const char* nickname = netplay_dolphin_ver.c_str();
    char cmd[] = "SLIP_READY";

    memcpy(m_broadcast_message.cmd, cmd, sizeof(m_broadcast_message.cmd));
    memset(m_broadcast_message.mac_addr, 0, sizeof(m_broadcast_message.mac_addr));
    strncpy(m_broadcast_message.nickname, nickname, sizeof(m_broadcast_message.nickname));

    if (enet_initialize () != 0) {
        WARN_LOG(SLIPPI, "An error occurred while initializing spectator server.");
        return;
    }

    ENetAddress server_address = {0};
    server_address.host = ENET_HOST_ANY;
    server_address.port = SConfig::GetInstance().m_spectator_local_port;

    // Create the spectator server
    // This call can fail if the system is already listening on the specified port
    //  or for some period of time after it closes down. You basically have to just
    //  retry until the OS lets go of the port and we can claim it again
    //  This typically only takes a few seconds
    ENetHost *server = enet_host_create(&server_address, MAX_CLIENTS, 2, 0, 0);
    int tries = 0;
    while(server == nullptr && tries < 20)
    {
        server = enet_host_create(&server_address, MAX_CLIENTS, 2, 0, 0);
        tries += 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    if (server == nullptr) {
        WARN_LOG(SLIPPI, "Could not create spectator server");
        enet_deinitialize();
        return;
    }

    // Main slippicomm server loop
    while(1)
    {
        // If we're told to stop, then quit
        if(m_stop_socket_thread)
        {
            enet_host_destroy(server);
            enet_deinitialize();
            return;
        }

        // Pop off any events in the queue
        popEvents();

        std::map<u16, std::shared_ptr<SlippiSocket>>::iterator it = m_sockets.begin();
        for(; it != m_sockets.end(); it++)
        {
            if(it->second->m_shook_hands)
            {
                writeEvents(it->first);
            }
        }

        // Write any broadcast messages if we haven't in two seconds
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        if(now - std::chrono::seconds(2) > m_last_broadcast_time)
        {
            writeBroadcast();
            // Also take this time to punch a connection out to the spectator
            if(!SConfig::GetInstance().m_spectator_IP.empty())
            {
                sendHolePunchMsg(server,
                  SConfig::GetInstance().m_spectator_IP,
                  SConfig::GetInstance().m_spectator_port);
            }
        }

        ENetEvent event;
        while (enet_host_service (server, &event, 1) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {

                    INFO_LOG(SLIPPI, "A new spectator connected from %x:%u.\n",
                        event.peer -> address.host,
                        event.peer -> address.port);

                    std::shared_ptr<SlippiSocket> newSlippiSocket(new SlippiSocket());
                    newSlippiSocket->m_peer = event.peer;
                    m_sockets[event.peer->incomingPeerID] = newSlippiSocket;
                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    handleMessage(event.packet->data, (u32)event.packet->dataLength, event.peer->incomingPeerID);
                    /* Clean up the packet now that we're done using it. */
                    enet_packet_destroy (event.packet);

                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    INFO_LOG(SLIPPI, "A spectator disconnected from %x:%u.\n",
                        event.peer -> address.host,
                        event.peer -> address.port);

                    // Delete the item in the m_sockets map
                    m_sockets.erase(event.peer->incomingPeerID);
                    /* Reset the peer's client information. */
                    event.peer -> data = NULL;
                    break;
                }
                default:
                {
                    INFO_LOG(SLIPPI, "Spectator sent an unknown ENet event type");
                    break;
                }
            }
        }
    }

    enet_host_destroy(server);
    enet_deinitialize();
}
