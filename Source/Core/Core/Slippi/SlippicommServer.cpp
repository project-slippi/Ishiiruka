#include "SlippicommServer.h"
#include "Common/Logging/Log.h"
#include "nlohmann/json.hpp"
#include "Common/CommonTypes.h"
#include <Core/ConfigManager.h>

// Networking
#ifdef _WIN32
#include <share.h>
#include <ws2tcpip.h>
#else
#include <errno.h>
#endif

SlippicommServer* SlippicommServer::getInstance()
{
    static SlippicommServer instance; // Guaranteed to be destroyed.
                                    // Instantiated on first use.
    return &instance;
}

void SlippicommServer::write(u8 *payload, u32 length)
{
    if(!SConfig::GetInstance().m_slippiNetworkingOutput)
    {
        return;
    }

    // Keep track of the latest time we wrote data.
    //  So that we can know when to send keepalives later
    m_write_time_mutex.lock();
    m_last_write_time = std::chrono::system_clock::now();
    m_write_time_mutex.unlock();

    m_event_buffer_mutex.lock();
    u32 cursor = (u32)m_event_buffer.size();
    u64 offset = m_cursor_offset;
    m_event_buffer_mutex.unlock();

      // Note: This is a bit messy because nlohmann can't be used for this case.
      //  nlohmann needs the contents to be valid JSON first, and then it can
      //  read and write it to UBJSON. But arbitrary binary buffers can't be in
      //  regular JSON, so this doesn't work. :(
    std::vector<u8> ubjson_header({'{', 'i', '\x04', 't', 'y', 'p', 'e', 'U',
        '\x02', 'i', '\x07', 'p', 'a', 'y', 'l', 'o', 'a', 'd', '{',});
    std::vector<u8> cursor_header({'i', '\x03', 'p', 'o', 's', '[','$', 'U', '#', 'U', '\x08'});
    std::vector<u8> cursor_value = uint64ToVector(offset + cursor);
    std::vector<u8> next_cursor_header({'i', '\x07', 'n', 'e', 'x', 't', 'P', 'o', 's', '[','$', 'U', '#', 'U', '\x08'});
    std::vector<u8> next_cursor_value = uint64ToVector(offset + cursor + 1);
    std::vector<u8> data_field_header({'i', '\x04', 'd', 'a', 't', 'a', '[',
        '$', 'U', '#', 'I'});
    std::vector<u8> length_vector = uint16ToVector(length);
    std::vector<u8> ubjson_footer({'}', '}'});

    // Length of the entire TCP event. Not part of the slippi message per-se
    u32 event_length = length +
      (u32)ubjson_header.size() + (u32)cursor_header.size() +
      (u32)cursor_value.size() + (u32)data_field_header.size() +
      (u32)next_cursor_value.size() + (u32)next_cursor_header.size() +
      (u32)length_vector.size() + (u32)ubjson_footer.size();
    std::vector<u8> event_length_vector = uint32ToVector(event_length);

    // Let's assemble the final buffer that gets written
    std::vector<u8> buffer;
    buffer.reserve(event_length);
    buffer.insert(buffer.end(), event_length_vector.begin(), event_length_vector.end());
    buffer.insert(buffer.end(), ubjson_header.begin(), ubjson_header.end());
    buffer.insert(buffer.end(), cursor_header.begin(), cursor_header.end());
    buffer.insert(buffer.end(), cursor_value.begin(), cursor_value.end());
    buffer.insert(buffer.end(), next_cursor_header.begin(), next_cursor_header.end());
    buffer.insert(buffer.end(), next_cursor_value.begin(), next_cursor_value.end());
    buffer.insert(buffer.end(), data_field_header.begin(), data_field_header.end());
    buffer.insert(buffer.end(), length_vector.begin(), length_vector.end());
    buffer.insert(buffer.end(), payload, payload + length);
    buffer.insert(buffer.end(), ubjson_footer.begin(), ubjson_footer.end());

    // Put this message into the event buffer
    //  This will queue the message up to go out for all clients
    m_event_buffer_mutex.lock();
    m_event_buffer.push_back(buffer);
    m_event_buffer_mutex.unlock();
}

void SlippicommServer::writeMenuEvent(u8 *payload, u32 length)
{
  if(!SConfig::GetInstance().m_slippiNetworkingOutput)
  {
      return;
  }

  // Note: This is a bit messy because nlohmann can't be used for this case.
  //  nlohmann needs the contents to be valid JSON first, and then it can
  //  read and write it to UBJSON. But arbitrary binary buffers can't be in
  //  regular JSON, so this doesn't work. :(
  std::vector<u8> ubjson_header({'{', 'i', '\x04', 't', 'y', 'p', 'e', 'U',
      '\x04', 'i', '\x07', 'p', 'a', 'y', 'l', 'o', 'a', 'd', '{',});
  std::vector<u8> data_field_header({'i', '\x04', 'd', 'a', 't', 'a', '[',
      '$', 'U', '#', 'I'});
  std::vector<u8> length_vector = uint16ToVector(length);
  std::vector<u8> ubjson_footer({'}', '}'});

  // Length of the entire TCP event. Not part of the slippi message per-se
  u32 event_length = length +
    (u32)ubjson_header.size() + (u32)data_field_header.size() +
    (u32)length_vector.size() + (u32)ubjson_footer.size();
  std::vector<u8> event_length_vector = uint32ToVector(event_length);

  // Let's assemble the final buffer that gets written
  std::vector<u8> buffer;
  buffer.reserve(event_length);
  buffer.insert(buffer.end(), event_length_vector.begin(), event_length_vector.end());
  buffer.insert(buffer.end(), ubjson_header.begin(), ubjson_header.end());
  buffer.insert(buffer.end(), data_field_header.begin(), data_field_header.end());
  buffer.insert(buffer.end(), length_vector.begin(), length_vector.end());
  buffer.insert(buffer.end(), payload, payload + length);
  buffer.insert(buffer.end(), ubjson_footer.begin(), ubjson_footer.end());

  // Put this message into the menu event buffer
  //  This will queue the message up to go out for all clients
  m_event_buffer_mutex.lock();
  m_menu_event_buffer.push_back(buffer);
  m_event_buffer_mutex.unlock();
}

void SlippicommServer::writeEvents(SOCKET socket)
{
    // Get the cursor for this socket
    u64 cursor = m_sockets[socket]->m_cursor;

    // Loop through each event that needs to be sent
    //  send all the events starting at their cursor
    m_event_buffer_mutex.lock();

    std::vector< std::vector<u8>> *buffer_ptr;

    // Are we pulling from the game events buffer or the menu events buffer?
    if(!m_sockets[socket]->m_in_game)
    {
      buffer_ptr = &m_menu_event_buffer;
    }
    else
    {
      buffer_ptr = &m_event_buffer;
    }

    for(u64 i = cursor; i < (*buffer_ptr).size(); i++)
    {
        u32 fragment_index = m_sockets[socket]->m_outgoing_fragment_index;
        int32_t byteswritten = send(socket,
          (char*)(*buffer_ptr)[i].data() + fragment_index,
          (int)(*buffer_ptr)[i].size() - fragment_index, 0);

        // There are three possible results from a send() call.
        //  1) All the data was sent.
        //      Keep the data coming
        //  2) Partial data was sent, and this would block.
        //      Stop sending data for now. Save the partial fragment
        //  3) The socket is broken
        //      Kill the socket

        // We didn't send all the data. This means result #2 or #3
        if(byteswritten < (int32_t)((*buffer_ptr)[i].size() - fragment_index))
        {
            // Is this just a blocking error?
            if(errno == EWOULDBLOCK || byteswritten >= 0)
            {
                // Update the index to represent the bytes that DID get sent
                m_sockets[socket]->m_outgoing_fragment_index += byteswritten;
            }
            else {
                // Kill the socket
                sockClose(socket);
                m_sockets.erase(socket);
            }
            // In both error cases, we have to return. No more data to send
            m_event_buffer_mutex.unlock();
            return;
        }
        // Result #1. Keep the data coming with a new event
        m_sockets[socket]->m_outgoing_fragment_index = 0;
        m_sockets[socket]->m_cursor++;
    }

    // If the socket is all caught up, sync them up the right in_game status
    if(m_sockets[socket]->m_cursor >= (*buffer_ptr).size())
    {
      m_sockets[socket]->m_in_game = m_in_game;
    }

    m_event_buffer_mutex.unlock();
}

// Helper for closing sockets in a cross-compatible way
int SlippicommServer::sockClose(SOCKET sock)
{
    int status = 0;

    #ifdef _WIN32
        status = shutdown(sock, SD_BOTH);
    #else
        status = close(sock);
    #endif

    return status;
}

SOCKET SlippicommServer::buildFDSet(fd_set *read_fds, fd_set *write_fds)
{
    // Keep track of the currently largest FD
    SOCKET maxFD = m_server_fd;
    FD_ZERO(read_fds);
    FD_ZERO(write_fds);
    FD_SET(m_server_fd, read_fds);

    // Read from the sockets list
    std::map<SOCKET, std::shared_ptr<SlippiSocket>>::iterator it = m_sockets.begin();
    while(it != m_sockets.end())
    {
        FD_SET(it->first, read_fds);
        maxFD = std::max(maxFD, it->first);

        // Only add a socket to the write list if it's behind on events
        m_event_buffer_mutex.lock();
        u32 event_count = (u32)m_event_buffer.size();
        if(!m_sockets[it->first]->m_in_game)
        {
          event_count = (u32)m_menu_event_buffer.size();
        }

        m_event_buffer_mutex.unlock();
        // reset cursor if it's > event buffer size
        //  This will happen when a new game starts
        //  or some weird error. In both cases, starting over is right
        if(m_sockets[it->first]->m_cursor > event_count)
        {
            m_sockets[it->first]->m_cursor = 0;
        }
        if(m_sockets[it->first]->m_cursor < event_count)
        {
            FD_SET(it->first, write_fds);
        }
        it++;
    }

    return maxFD;
}

std::vector<u8> SlippicommServer::uint16ToVector(u16 num)
{
    u8 byte0 = num >> 8;
    u8 byte1 = num & 0xFF;

    return std::vector<u8>({byte0, byte1});
}

std::vector<u8> SlippicommServer::uint64ToVector(u64 num)
{
    u8 byte0 = num >> 56;
    u8 byte1 = (num >> 48) & 0xFF;
    u8 byte2 = (num >> 40) & 0xFF;
    u8 byte3 = (num >> 32) & 0xFF;
    u8 byte4 = (num >> 24) & 0xFF;
    u8 byte5 = (num >> 16) & 0xFF;
    u8 byte6 = (num >> 8) & 0xFF;
    u8 byte7 = num & 0xFF;

    return std::vector<u8>{byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7};
}

std::vector<u8> SlippicommServer::uint32ToVector(u32 num)
{
    u8 byte0 = num >> 24;
    u8 byte1 = (num & 0xFF0000) >> 16;
    u8 byte2 = (num & 0xFF00) >> 8;
    u8 byte3 = num & 0xFF;

    return std::vector<u8>({byte0, byte1, byte2, byte3});
}


// We assume, for the sake of simplicity, that all clients have finished reading
//  from the previous game event buffer by now. At least many seconds will have passed
//  by now, so if a listener is still stuck getting events from the last game,
//  they will get erroneous data.
void SlippicommServer::startGame()
{
    if(!SConfig::GetInstance().m_slippiNetworkingOutput)
    {
        return;
    }

    m_event_buffer_mutex.lock();
    if(m_event_buffer.size() > 0)
    {
        m_cursor_offset += m_event_buffer.size();
    }
    m_event_buffer.clear();
    m_in_game = true;

    // If the socket is all caught up, sync them up the right in_game status
    std::map<SOCKET, std::shared_ptr<SlippiSocket>>::iterator it = m_sockets.begin();
    for(; it != m_sockets.end(); it++)
    {
      if(it->second->m_cursor >= m_menu_event_buffer.size())
      {
        it->second->m_in_game = m_in_game;
        it->second->m_cursor = m_cursor_offset;
      }
    }

    m_event_buffer_mutex.unlock();
}

void SlippicommServer::endGame()
{
    if(!SConfig::GetInstance().m_slippiNetworkingOutput)
    {
        return;
    }

    m_event_buffer_mutex.lock();
    m_menu_event_buffer.clear();
    m_in_game = false;

    // If the socket is all caught up, sync them up the right in_game status
    std::map<SOCKET, std::shared_ptr<SlippiSocket>>::iterator it = m_sockets.begin();
    for(; it != m_sockets.end(); it++)
    {
      if(it->second->m_cursor >= m_event_buffer.size())
      {
        it->second->m_in_game = m_in_game;
      }
    }

    m_event_buffer_mutex.unlock();
}

SlippicommServer::SlippicommServer()
{
    if(!SConfig::GetInstance().m_slippiNetworkingOutput)
    {
        return;
    }

    #ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2,2), &wsa_data);
    #endif

    // Init some timestamps
    m_last_write_time = std::chrono::system_clock::now();
    m_last_broadcast_time = std::chrono::system_clock::now();

    // Spawn thread for socket listener
    m_stop_socket_thread = false;
    m_socketThread = std::thread(&SlippicommServer::SlippicommSocketThread, this);
}

SlippicommServer::~SlippicommServer()
{
    // The socket thread will be blocked waiting for input
    // So to wake it up, let's connect to the socket!
    m_stop_socket_thread = true;

    SOCKET sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        WARN_LOG(SLIPPI, "Failed to shut down Slippi networking thread");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(51441);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
    {
        WARN_LOG(SLIPPI, "Failed to shut down Slippi networking thread");
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        WARN_LOG(SLIPPI, "Failed to shut down Slippi networking thread");
        return;
    }

    m_socketThread.join();
    #ifdef _WIN32
    WSACleanup();
    #endif
}

void SlippicommServer::writeKeepalives()
{
    nlohmann::json keepalive = {{"type", KEEPALIVE_TYPE}};
    std::vector<u8> ubjson_keepalive = nlohmann::json::to_ubjson(keepalive);

    // Write the data to each open socket
    std::map<SOCKET, std::shared_ptr<SlippiSocket>>::iterator it = m_sockets.begin();
    for(; it != m_sockets.end(); it++)
    {
        // Don't send a keepalive in the middle of an event message
        if(it->second->m_outgoing_fragment_index > 0)
        {
            continue;
        }

        // Keepalives only get sent when no other data was sent for two whole seconds
        //  So the chances of the network buffer being full here are pretty low.
        //   It's fine to just loop the send(), effectively blocking on the call
        send(it->first, (char *)&m_keepalive_len, sizeof(m_keepalive_len), 0);

        int32_t byteswritten = 0;
        while ((uint32_t)byteswritten < ubjson_keepalive.size())
        {
            int32_t ret = send(it->first, (char *)ubjson_keepalive.data() + byteswritten,
                                (int)ubjson_keepalive.size() - byteswritten, 0);
            // -1 means the socket is closed
            if (ret == -1)
            {
                m_sockets.erase(it->first);
                break;
            }
            byteswritten += ret;
        }
    }
}

void SlippicommServer::writeBroadcast()
{
    // Broadcast message structure
    struct broadcast_msg broadcast;

    const char* nickname = SConfig::GetInstance().m_slippiConsoleName.c_str();
    char cmd[] = "SLIP_READY";

    memcpy(broadcast.cmd, cmd, sizeof(broadcast.cmd));
    memset(broadcast.mac_addr, 0, sizeof(broadcast.mac_addr));
    strncpy(broadcast.nickname, nickname, sizeof(broadcast.nickname));

    sendto(m_broadcast_socket, (char*)&broadcast, sizeof(broadcast), 0,
        (struct sockaddr *)&m_broadcastAddr, sizeof(m_broadcastAddr));
}

void SlippicommServer::handleMessage(SOCKET socket)
{
    // Read data off of the socket. We expect this to be a handshake event.
    //  But let's make room for a little more than that just in case
    char buffer[HANDSHAKE_MSG_BUF_SIZE*2];
    int32_t bytesread = recv(socket, buffer, sizeof(buffer), 0);
    if(bytesread <= 0)
    {
        // Got an error on this socket. It must be dead. Close it and remove it
        //  from the sockets list
        sockClose(socket);
        m_sockets.erase(socket);
        return;
    }

    // Append this buffer to the socket's current data fragment buffer
    // Most of the time, this will just be the whole message. But it might
    //  get fragmented and come in pieces.
    m_sockets[socket]->m_incoming_buffer.insert(
        m_sockets[socket]->m_incoming_buffer.end(), buffer, buffer+bytesread);

    // Try to read the message itself. See if we have enough data yet
    // The first 4 bytes are the length field. Try reading that.
    if(m_sockets[socket]->m_incoming_buffer.size() < 4)
    {
        // Nothing to do, return and wait for more data later
        return;
    }

    u32 messageLength = 0;
    memcpy(&messageLength, m_sockets[socket]->m_incoming_buffer.data(), 4);
    // Fix endianness
    messageLength = ntohl(messageLength);
    // Sanity check on message length. If it's crazy long, then hang up
    if(messageLength > HANDSHAKE_MSG_BUF_SIZE)
    {
        WARN_LOG(SLIPPI, "Got unreasonably long message from Slippi client. Closing");
        sockClose(socket);
        m_sockets.erase(socket);
        return;
    }

    // Do we have the full message yet?
    //  Again, we usually will. But it might get fragmented in half
    if(messageLength + 4 > m_sockets[socket]->m_incoming_buffer.size())
    {
        // Nothing to do, return and wait for more data later
        return;
    }

    // We're finally here. We can read the whole message!
    std::vector<char>::iterator begin = m_sockets[socket]->m_incoming_buffer.begin() + 4;
    std::vector<char>::iterator end = m_sockets[socket]->m_incoming_buffer.begin()
        + messageLength + 4;

    std::vector<u8> ubjsonblob(begin, end);

    nlohmann::json handshake = nlohmann::json::from_ubjson(ubjsonblob,
        true, false);

    if(handshake.is_discarded())
    {
        // Got a bogus UBJSON event. Hang up on the client
        WARN_LOG(SLIPPI, "Got unparseable UBJSON from Slippi client");
        sockClose(socket);
        m_sockets.erase(socket);
        return;
    }

    u64 cursor = 0;
    // Do we have a "payload" and a "cursor"?
    if(handshake.find("payload") != handshake.end() &&
       handshake.at("payload").find("cursor") != handshake.at("payload").end() &&
       handshake.at("payload").at("cursor").is_array())
    {
        // Additional type checking on cursor. It needs to be an array of length 8
        //  consisting entirely of integers. This is painful
        bool bad_cursor = false;
        if(handshake.at("payload").at("cursor").size() != 8)
        {
            bad_cursor = true;
        }

        for( auto it = handshake.at("payload").at("cursor").begin();
            it != handshake.at("payload").at("cursor").end(); ++it)
        {
            if(!(*it).is_number_integer())
            {
                bad_cursor = true;
            }
        }
        if(bad_cursor)
        {
            // Got a bogus UBJSON event. Hang up on the client
            WARN_LOG(SLIPPI, "Got invalid UBJSON cursor from Slippi client");
            sockClose(socket);
            m_sockets.erase(socket);
            return;
        }

        std::vector<u8> cursor_array = handshake.at("payload").at("cursor");
        // Convert the array to an integer
        for(uint32_t i = 0; i < cursor_array.size(); i++)
        {
            uint32_t index = (uint32_t)cursor_array.size() - i - 1;
            cursor += (cursor_array[i]) << (8*index);
        }
    }
    else
    {
        // Got a bogus UBJSON event. Hang up on the client
        WARN_LOG(SLIPPI, "Got unparseable UBJSON from Slippi client");
        sockClose(socket);
        m_sockets.erase(socket);
        return;
    }

    // Remove data from the buffer
    begin = m_sockets[socket]->m_incoming_buffer.begin();
    end = m_sockets[socket]->m_incoming_buffer.begin()
        + messageLength + 4;
    m_sockets[socket]->m_incoming_buffer = std::vector<char>(begin, end);

    // Set the user's cursor position
    m_event_buffer_mutex.lock();
    if(cursor >= m_cursor_offset)
    {
        // If the requested cursor is past what events we even have, then just tell them to start over
        if(cursor > m_event_buffer.size() + m_cursor_offset)
        {
          m_sockets[socket]->m_cursor = 0;
        }
        // Requested cursor is in the middle of a live match, events that we have
        else
        {
          m_sockets[socket]->m_cursor = cursor - m_cursor_offset;
        }
    }
    else
    {
        // The client requested a cursor that was too low. Bring them up to the present
        m_sockets[socket]->m_cursor = 0;
    }

    // handshake back
    nlohmann::json handshake_back = {
        {"payload", {
        {"nick", SConfig::GetInstance().m_slippiConsoleName},
        {"nintendontVersion", "1.9.0-dev-2"},
        {"clientToken", std::vector<u32>{0, 0, 0, 0}},
        {"pos", uint64ToVector(m_sockets[socket]->m_cursor + m_cursor_offset)}
        }}
    };

    m_event_buffer_mutex.unlock();

    std::vector<u8> ubjson_handshake_back = nlohmann::json::to_ubjson(handshake_back);
    auto it = ubjson_handshake_back.begin() + 1; // we want to insert type at index 1
    ubjson_handshake_back.insert(it, m_handshake_type_vec.begin(), m_handshake_type_vec.end());

    std::vector<u8> handshake_back_len = uint32ToVector((u32)ubjson_handshake_back.size());
    ubjson_handshake_back.insert(ubjson_handshake_back.begin(), handshake_back_len.begin(), handshake_back_len.end());

    // This loop will effectively block until the send() completes.
    //  But it's probably fine. The data is small and only sent infrequently.
    int32_t byteswritten = 0;
    while((uint32_t)byteswritten < ubjson_handshake_back.size())
    {
      byteswritten = send(socket, (char*)ubjson_handshake_back.data() +
        byteswritten, (int)ubjson_handshake_back.size(), 0);
    }

    // Put the client in the right in_game state
    m_sockets[socket]->m_in_game = m_in_game;
    m_sockets[socket]->m_shook_hands = true;
}

void SlippicommServer::SlippicommSocketThread(void)
{
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((m_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
          WARN_LOG(SLIPPI, "Failed to create Slippi streaming socket");
            return;
    }

    if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR,
        (char*)&opt, sizeof(opt)))
    {
        WARN_LOG(SLIPPI, "Failed configuring Slippi streaming socket");
        return;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(51441);

    if (bind(m_server_fd, (struct sockaddr *)&address,
        sizeof(address))<0)
    {
        WARN_LOG(SLIPPI, "Failed binding to Slippi streaming port");
        return;
    }
    if (listen(m_server_fd, 10) < 0)
    {
        WARN_LOG(SLIPPI, "Failed listening to Slippi streaming socket");
        return;
    }

    m_broadcast_socket = socket(AF_INET, SOCK_DGRAM, 0);
    int broadcastEnable=1;
    if(setsockopt(m_broadcast_socket, SOL_SOCKET, SO_BROADCAST,
       (char*)&broadcastEnable, sizeof(broadcastEnable)))
    {
        WARN_LOG(SLIPPI, "Failed configuring Slippi braodcast socket");
        return;
    }

    // Setup some more broadcast port variables
    memset(&m_broadcastAddr, 0, sizeof(m_broadcastAddr));
    m_broadcastAddr.sin_family = AF_INET;
    m_broadcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
    m_broadcastAddr.sin_port = htons(20582);

    // Infinite loop, keep accepting new connections and putting them into the list
    while(1)
    {
        // If we're told to stop, then quit
        if(m_stop_socket_thread)
        {
            std::map<SOCKET, std::shared_ptr<SlippiSocket>>::iterator it = m_sockets.begin();
            while(it != m_sockets.end())
            {
                sockClose(it->first);
                it++;
            }
            sockClose(m_server_fd);
            return;
        }

        fd_set read_fds, write_fds;
        int numActiveSockets = 0;
        // Construct the file descriptor set to select from
        SOCKET maxFD = buildFDSet(&read_fds, &write_fds);

        // Block until activity comes in on a socket or we timeout
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;
        numActiveSockets = select((int)maxFD+1, &read_fds, &write_fds, NULL, &timeout);

        if(numActiveSockets < 0)
        {
            // We got an error. Probably we're shutting down. So let's try to
            //  clean up and then just kill the thread by returning.
            sockClose(m_server_fd);
            WARN_LOG(SLIPPI, "Slippi streaming socket received an error. Closing.");
            return;
        }

        // We timed out. So take this moment to send any keepalives that need sending
        if(numActiveSockets == 0)
        {
            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            m_write_time_mutex.lock();
            std::chrono::system_clock::time_point last_write = m_last_write_time;
            m_write_time_mutex.unlock();
            if(now - std::chrono::seconds(2) > last_write)
            {
                writeKeepalives();
                m_write_time_mutex.lock();
                m_last_write_time = now;
                m_write_time_mutex.unlock();
            }

            // Broadcasts are on their own timer. Send one every 2 seconds-ish
            // In a perfect world, we'd have these setup on a signal-based timer but...
            if(now - std::chrono::seconds(2) > m_last_broadcast_time)
            {
                writeBroadcast();
                m_last_broadcast_time = now;
            }
            continue;
        }

        // For each new read socket that has activity, handle it
        for(int sock = 0; sock <= maxFD; ++sock)
        {
            if(FD_ISSET(sock, &read_fds))
            {
                // If the socket that just got activity is the server FD, then we have a
                // whole new connection ready to come in. accept() it
                if(sock == m_server_fd)
                {
                    SOCKET new_socket;
                    if ((new_socket = accept(m_server_fd,
                                       (struct sockaddr *)&address,
                                       (socklen_t*)&addrlen))<0)
                    {
                        WARN_LOG(SLIPPI, "Failed listening to Slippi streaming socket");
                        return;
                    }

                    #ifdef _WIN32
                    u_long mode = 1;
                    ioctlsocket(new_socket, FIONBIO, &mode);
                    #else
                    fcntl(new_socket, F_SETFL, fcntl(new_socket, F_GETFL, 0) | O_NONBLOCK);
                    #endif

                    // Add the new socket to the list
                    std::shared_ptr<SlippiSocket> newSlippiSocket(new SlippiSocket());
                    m_sockets[new_socket] = newSlippiSocket;
                }
                else
                {
                    // The socket that has activity must be new data coming in on
                    // an existing connection. Handle that message
                    handleMessage(sock);
                }
            }
        }

        // For each write socket that is available, write to it
        for(int sock = 0; sock <= maxFD; ++sock)
        {
            if(FD_ISSET(sock, &write_fds))
            {
                if(m_sockets.find(sock) != m_sockets.end() && m_sockets[sock]->m_shook_hands)
                {
                    writeEvents(sock);
                }
            }
        }
    }
}
