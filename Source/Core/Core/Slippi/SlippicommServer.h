#pragma once

#include <map>
#include <chrono>
#include <thread>
#include <mutex>

// Sockets in windows are unsigned
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/select.h>
typedef int SOCKET;
#endif

#define HANDSHAKE_MSG_BUF_SIZE 128
#define HANDSHAKE_TYPE 1
#define PAYLOAD_TYPE 2
#define KEEPALIVE_TYPE 3

// Actual socket value is not here since that's the key of the map
class SlippiSocket
{
public:
  // Fragmented data that hasn't yet fully arrived
  std::vector<char> m_incoming_buffer;
  u32 m_cursor = 0;
};

class SlippicommServer
{
public:
    // Singleton. Get an instance of the class here
    static SlippicommServer* getInstance();

    // Write the given game payload data to all listening sockets
    void write(u8 *payload, u32 length);

    // Clear the game event history buffer. Such as when a game ends.
    //  The slippi server keeps a history of events in a buffer. So that
    //  when a new client connects to the server mid-match, it can recieve all
    //  the game events that have happened so far. This buffer needs to be
    //  cleared when a match ends.
    void clearEventHistory();

    // Don't try to copy the class. Delete those functions
    SlippicommServer(SlippicommServer const&) = delete;
    void operator=(SlippicommServer const&)  = delete;

    struct broadcast_msg
    {
    	char	cmd[10];
    	u8		mac_addr[6];	// Wi-Fi interface MAC address
    	char	nickname[32];	// Console nickname
    };

  private:
    std::map<SOCKET, std::shared_ptr<SlippiSocket>> m_sockets;
    bool m_stop_socket_thread;
    std::vector< std::vector<u8> > m_event_buffer;
    std::mutex m_event_buffer_mutex;
    std::thread m_socketThread;
    SOCKET m_server_fd;
    std::mutex m_write_time_mutex;
    std::chrono::system_clock::time_point m_last_write_time;
    std::chrono::system_clock::time_point m_last_broadcast_time;
    SOCKET m_broadcast_socket;
    struct sockaddr_in m_broadcastAddr, m_localhostAddr;

    // Private constructor to avoid making another instance
    SlippicommServer();
    ~SlippicommServer();

    // Server thread. Accepts new incoming connections and goes back to sleep
    void SlippicommSocketThread(void);
    // Helper for closing sockets in a cross-compatible way
    int sockClose(SOCKET socket);
    // Build the set of file descriptors that select() needs
    //  Returns the highest socket value, which is required by select()
    SOCKET buildFDSet(fd_set *read_fds, fd_set *write_fds);
    // Handle an incoming message on a socket
    void handleMessage(SOCKET socket);
    // Send keepalive messages to all clients
    void writeKeepalives();
    // Send broadcast advertisement of the slippi server
    void writeBroadcast();
    // Catch up given socket to the latest events
    //  Does nothing if they're already caught up.
    //  Quits out early if the call would block. So this isn't guaranteed to
    //    actually send the data. Best-effort
    void writeEvents(SOCKET socket);

    std::vector<u8> uint32ToVector(u32 num);
    std::vector<u8> uint16ToVector(u16 num);
};
