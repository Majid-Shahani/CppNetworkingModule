#include <CNMpch.hpp>

#ifdef CL_Platform_Windows
#include <CNM/Sockets.h>
#include <CNM/macros.h>

static WORD winsockVersion = MAKEWORD(2, 2);
static WSADATA wsaData;

namespace Carnival::Network {
	Sockets::Sockets() {
		if (!s_Initialized) {
			uint32_t wError = WSAStartup(winsockVersion, &wsaData);
			if (!wError)
			{
				s_Initialized = true;
				std::cout << "Status: " << wsaData.szSystemStatus << '\n';
			}
		}
	}
	Sockets::~Sockets() {
		if (s_Initialized)
		{
			// Close all Sockets
			bool closed = deleteAllSockets();
			if (!closed) {
				// retry or something
				m_Sockets.clear();
			}
			// Shut down WSA
			WSACleanup();
			s_Initialized = false;
		}
	}

	void Sockets::createSocket(uint8_t socketKey, SocketData data)
	{
		CL_CORE_ASSERT(s_Initialized, "WSA uninitialized on call to create socket: {}", socketKey);

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
				std::cerr << "Failed to set Non-Blocing on Socket: " << socketKey << '\n';
			}
		}

		m_Sockets.insert_or_assign(socketKey, data);
	}

	bool Sockets::deleteSocket(uint8_t socketKey)
	{
		CL_CORE_ASSERT(s_Initialized, "WSA uninitialized on call to close socket: {}", socketKey);
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
		CL_CORE_ASSERT(s_Initialized, "WSA uninitialized on call to close all sockets.");
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

	bool Sockets::bindSocket(uint8_t socketKey, uint32_t addr, uint16_t port)
	{
		CL_CORE_ASSERT(s_Initialized, "WSA uninitialized on call to bind socket: {}", socketKey);
		SocketData& socket = m_Sockets[socketKey];

		sockaddr_in service{};
		service.sin_family = AF_INET;
		service.sin_addr.s_addr = addr;
		service.sin_port = htons(port);

		if (bind(socket.Handle,	(const sockaddr*)&service, sizeof(sockaddr_in)) == SOCKET_ERROR)
		{
			return false;
		}
		socket.Status = static_cast<SocketStatus>(socket.Status | SocketStatus::BOUND);
		return true;
	}
}
#endif