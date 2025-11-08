#include <src/CNMpch.hpp>

#ifdef CL_Platform_Windows

#include <include/CNM/macros.h>
#include <include/CNM/Sockets.h>

static WORD winsockVersion = MAKEWORD(2, 2);
static WSADATA wsaData;

namespace Carnival::Network {
	Sockets::Sockets() noexcept {
		if (!s_Initialized) {
			uint32_t wError = WSAStartup(winsockVersion, &wsaData);
			if (!wError)
			{
				s_Initialized = true;
				std::cout << "Status: " << wsaData.szSystemStatus << '\n';
			}
		}
	}
	Sockets::~Sockets() noexcept {
		if (s_Initialized)
		{
			// Close all Sockets
			bool closed = deleteAllSockets();
			if (!closed) {
				// retry or something
				std::cerr << "Could not close all sockets" << '\n';
				m_Sockets.clear();
			}
			// Shut down WSA
			WSACleanup();
			s_Initialized = false;
		}
	}

	void Sockets::createSocket(uint8_t socketKey, SocketData data)
	{
		CL_CORE_ASSERT(s_Initialized, "WSA uninitialized");

		if (m_Sockets.contains(socketKey))	deleteSocket(socketKey);
		if (data.Status & SocketStatus::SOCKERROR) data.Status = SocketStatus::NONE;
		
		data.Handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (data.Handle == INVALID_SOCKET)
		{
			std::cerr << "Failed to Create Socket: " << socketKey << ' ' << WSAGetLastError() << '\n';
			data.Status = SocketStatus::SOCKERROR;
			return;
		}
		data.Status = static_cast<SocketStatus>(SocketStatus::OPEN | data.Status);

		if (data.Status & SocketStatus::NONBLOCKING) {
			unsigned long nb = 1;
			if (ioctlsocket(data.Handle, FIONBIO, &nb) == SOCKET_ERROR)
			{
				std::cerr << "Failed to set Non-Blocking on Socket: " << socketKey << '\n';
			}
		}
		m_Sockets.emplace(socketKey, std::move(data));
	}
	bool Sockets::deleteSocket(uint8_t socketKey)
	{
		CL_CORE_ASSERT(s_Initialized, "WSA uninitialized");
		CL_CORE_ASSERT(m_Sockets.contains(socketKey), "Map does not include socket key: {}", socketKey);
		SocketData& socket = m_Sockets[socketKey];
		if (closesocket(socket.Handle) == SOCKET_ERROR) {
			socket.Status = SocketStatus::SOCKERROR;
			return false;
		}
		else {

			m_Sockets.erase(socketKey);
			return true;
		}
	}
	bool Sockets::deleteAllSockets()
	{
		CL_CORE_ASSERT(s_Initialized, "WSA uninitialized");
		if (m_Sockets.empty()) return true;
		
		bool success = true;
		for (auto it = m_Sockets.begin(); it != m_Sockets.end();)
		{
			// closesocket returns a code other than 0 when an error occurs
			if (closesocket(it->second.Handle) == SOCKET_ERROR) {
				it->second.Status = SocketStatus::SOCKERROR;
				success = false;
			}
			it = m_Sockets.erase(it);
		}
		return success;
	}

	bool Sockets::bindSocket(uint8_t socketKey, uint16_t port, ipv4_addr addr)
	{
		CL_CORE_ASSERT(s_Initialized, "WSA uninitialized");
		CL_CORE_ASSERT(m_Sockets.contains(socketKey), "Map does not include socket key: {}", socketKey);
		
		SocketData& socket = m_Sockets.at(socketKey);
		sockaddr_in service{};
		service.sin_family = AF_INET;
		service.sin_addr.s_addr = htonl(addr.addr32);
		service.sin_port = htons(port);
		
		int addrLen = sizeof(service);
		if (bind(socket.Handle,	(const sockaddr*)&service, sizeof(sockaddr_in)) == SOCKET_ERROR
			|| getsockname(socket.Handle, (sockaddr*)&service, &addrLen) == SOCKET_ERROR)
			return false;
		// Set Socket Status, Address and Port
		socket.Status = static_cast<SocketStatus>(socket.Status | SocketStatus::BOUND);
		socket.Port = ntohs(service.sin_port);
		socket.InAddress.addr32 = ntohl(service.sin_addr.s_addr);
		printf("%u.%u.%u.%u:%u\n",
			(socket.InAddress.addr32 >> 24) & 0xff,
			(socket.InAddress.addr32 >> 16) & 0xff,
			(socket.InAddress.addr32 >> 8) & 0xff,
			socket.InAddress.addr32 & 0xff, 
			socket.Port);

		return true;
	}
	bool Sockets::sendPackets(uint8_t socketKey, const char* packetData, int packetSize, ipv4_addr addr)
	{
		CL_CORE_ASSERT(s_Initialized, "WSA uninitialized");
		CL_CORE_ASSERT(m_Sockets.contains(socketKey), "Map does not include socket key: {}", socketKey);
		CL_CORE_ASSERT(packetSize != 0, "Packet Size is 0");
		CL_CORE_ASSERT(packetData != nullptr, "Packet Data Poiner is null.");
		
		SocketData& socket = m_Sockets.at(socketKey);
		sockaddr_in address{};
		address.sin_family = AF_INET;

		if (addr.addr32 == 0)	address.sin_addr.s_addr = htonl(socket.OutAddress.addr32);
		else					address.sin_addr.s_addr = htonl(addr.addr32);
		
		address.sin_port = htons(socket.Port);

		int sent = sendto(socket.Handle, packetData, packetSize, 0, (sockaddr*) &address, sizeof(sockaddr_in));
		if (sent != packetSize) return false;
		else return true;
	}
	void Sockets::receivePackets(uint8_t socketKey)
	{
		CL_CORE_ASSERT(s_Initialized, "WSA uninitialized");
		CL_CORE_ASSERT(m_Sockets.contains(socketKey), "Map does not include socket key: {}", socketKey);
		while (true)
		{
			uint8_t packetData[256];
			uint32_t maxPacketSize = sizeof(packetData);
			
			sockaddr_in from{};
			int fromLength = sizeof(from);

			auto& socket = m_Sockets.at(socketKey);

			int64_t bytes = recvfrom(socket.Handle, (char*)packetData, 
				maxPacketSize, 0, (sockaddr*)&from, &fromLength);

			if (bytes <= 0 || bytes == SOCKET_ERROR) {
				return;
			}

			uint32_t fromAddr = ntohl(from.sin_addr.s_addr);
			uint16_t fromPort = ntohs(from.sin_port);

			// process
			packetData[(bytes >= (sizeof(packetData) - 1) ? sizeof(packetData) - 1 : bytes)] = '\0';
			std::cout << packetData << '\n';
		}
	}
}
#endif