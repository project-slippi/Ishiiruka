#include "SlippicommServer.h"
#include "Common/Logging/Log.h"

// Networking
#ifdef _WIN32
#include <share.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

SlippicommServer* SlippicommServer::getInstance()
{
  static SlippicommServer instance; // Guaranteed to be destroyed.
                                    // Instantiated on first use.
  return &instance;
}

void SlippicommServer::write(u8 *payload, u32 length, bool addToHistory)
{
  std::vector<u8> ubjson_header({'{', 'i', '\x04', 't', 'y', 'p', 'e', 'U',
		'\x02', 'i', '\x07', 'p', 'a', 'y', 'l', 'o', 'a', 'd', '{', 'i', '\x04',
		'd', 'a', 't', 'a', '[', '$', 'U', '#', 'I'});
	std::vector<u8> length_vector = uint16ToVector(length);
	std::vector<u8> ubjson_footer({'}', '}'});

	// Length of the entire TCP event. Not part of the slippi message per-se
	std::vector<u8> event_length_vector = uint32ToVector(length +
			(u32)ubjson_header.size() + (u32)length_vector.size() + (u32)ubjson_footer.size());

	// Let's assemble the final buffer that gets written
	std::vector<u8> buffer;
	buffer.reserve(4 + length + ubjson_header.size() + length_vector.size() +
		ubjson_footer.size());
	buffer.insert(buffer.end(), event_length_vector.begin(), event_length_vector.end());
	buffer.insert(buffer.end(), ubjson_header.begin(), ubjson_header.end());
	buffer.insert(buffer.end(), length_vector.begin(), length_vector.end());
	buffer.insert(buffer.end(), payload, payload + length);
	buffer.insert(buffer.end(), ubjson_footer.begin(), ubjson_footer.end());

	if (addToHistory)
  {
		// Put this message into the event buffer
		//	This is for future connections that come in and need the history
		m_event_buffer_mutex.lock();
		m_event_buffer.push_back(buffer);
		m_event_buffer_mutex.unlock();
	}

	// Write the data to each open socket
	m_socket_mutex.lock();
	for(uint32_t i=0; i < m_sockets.size(); i++)
	{
		int32_t byteswritten = 0;
		while((uint32_t)byteswritten < buffer.size())
		{
			byteswritten = send(m_sockets[i], (char*)buffer.data() + byteswritten, (int)buffer.size(), 0);
		}
	}
	m_socket_mutex.unlock();
}

// Helper for closing sockets in a cross-compatible way
int SlippicommServer::sockClose(SOCKET sock)
{
  int status = 0;

  #ifdef _WIN32
    status = closesocket(sock);
  #else
     status = close(sock);
  #endif

  return status;
}

std::vector<u8> SlippicommServer::uint16ToVector(u16 num)
{
	u8 byte0 = num >> 8;
	u8 byte1 = num & 0xFF;

	return std::vector<u8>({byte0, byte1});
}

std::vector<u8> SlippicommServer::uint32ToVector(u32 num)
{
	u8 byte0 = num >> 24;
	u8 byte1 = (num & 0xFF0000) >> 16;
	u8 byte2 = (num & 0xFF00) >> 8;
	u8 byte3 = num & 0xFF;

	return std::vector<u8>({byte0, byte1, byte2, byte3});
}

void SlippicommServer::clearEventHistory()
{
  m_event_buffer_mutex.lock();
  m_event_buffer.clear();
  m_event_buffer_mutex.unlock();
}

SlippicommServer::SlippicommServer()
{
	#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2,2), &wsa_data);
	#endif

	// Spawn thread for socket listener
	m_stop_socket_thread = false;
	m_socketThread = std::thread(&SlippicommServer::SlippicommSocketThread, this);
}

SlippicommServer::~SlippicommServer()
{
  // The socket thread will be blocked waiting for input
	//	So to wake it up, let's connect to the socket!
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

void SlippicommServer::SlippicommSocketThread(void)
{
	SOCKET server_fd, new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		  WARN_LOG(SLIPPI, "Failed to create Slippi streaming socket");
			return;
	}

	if (setsockopt(server_fd,
								 SOL_SOCKET,
								 SO_REUSEADDR,
								 (char*)&opt,
								 sizeof(opt)))
	{
		WARN_LOG(SLIPPI, "Failed configuring Slippi streaming socket");
		return;
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(51441);

	if (bind(server_fd,
					 (struct sockaddr *)&address,
					 sizeof(address))<0)
	{
		WARN_LOG(SLIPPI, "Failed binding to Slippi streaming port");
		return;
	}
	if (listen(server_fd, 3) < 0)
	{
		WARN_LOG(SLIPPI, "Failed listening to Slippi streaming socket");
		return;
	}

	// Infinite loop, keep accepting new connections and putting them into the list
	while(1)
	{
		// If we're told to stop, then quit
		if(m_stop_socket_thread)
		{
			m_socket_mutex.lock();
			for(uint32_t i=0; i < m_sockets.size(); i++)
			{
				sockClose(m_sockets[i]);
			}
			sockClose(server_fd);
			m_socket_mutex.unlock();
			return;
		}

		if ((new_socket = accept(server_fd,
														 (struct sockaddr *)&address,
											 		 	 (socklen_t*)&addrlen))<0)
		{
			WARN_LOG(SLIPPI, "Failed listening to Slippi streaming socket");
			return;
		}

		// When we get a new socket, send the event buffer over
		m_event_buffer_mutex.lock();
		for(uint32_t i=0; i < m_event_buffer.size(); i++)
		{
			int32_t byteswritten = 0;
			while((uint32_t)byteswritten < m_event_buffer[i].size())
			{
				byteswritten = send(new_socket, (char*)m_event_buffer[i].data() + byteswritten,
					(int)m_event_buffer[i].size(), 0);
			}
		}
		m_event_buffer_mutex.unlock();

		m_socket_mutex.lock();
		m_sockets.push_back(new_socket);
		m_socket_mutex.unlock();
	}
}
