#pragma once

// Sockets in windows are unsigned
#ifdef _WIN32
#include <winsock2.h>
#else
typedef int SOCKET;
#endif

class SlippicommServer
{
  public:

    // Singleton. Get an instance of the class here
    static SlippicommServer* getInstance();

    // Write the given data to all listening sockets
    //  addToHistory = bool on whether to add this data to the event history.
    //    Not adding the data to the history is useful for non-event messages
    //    such as KEEPALIVE's
    void write(u8 *payload, u32 length, bool addToHistory=true);

    // Clear the game event history buffer. Such as when a game ends.
    //  The slippi server keeps a history of events in a buffer. So that
    //  when a new client connects to the server mid-match, it can recieve all
    //  the game events that have happened so far. This buffer needs to be
    //  cleared when a match ends.
    void clearEventHistory();

    // Don't try to copy the class. Delete those functions
    SlippicommServer(SlippicommServer const&) = delete;
    void operator=(SlippicommServer const&)  = delete;

  private:

    std::mutex m_socket_mutex;
    std::vector<SOCKET> m_sockets;
    bool m_stop_socket_thread;
    std::vector< std::vector<u8> > m_event_buffer;
    std::mutex m_event_buffer_mutex;
    std::thread m_socketThread;

    // Private constructor to avoid making another instance
    SlippicommServer();
    ~SlippicommServer();

    // Server thread. Accepts new incoming connections and goes back to sleep
    void SlippicommSocketThread(void);
    // Helper for closing sockets in a cross-compatible way
    int sockClose(SOCKET sock);

    std::vector<u8> uint32ToVector(u32 num);
    std::vector<u8> uint16ToVector(u16 num);
};
