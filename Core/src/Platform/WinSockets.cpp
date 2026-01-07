#include <src/CNMpch.hpp>

#ifdef CL_Platform_Windows
#include <include/CNM/macros.h>
#include <include/CNM/Socket.h>
namespace {
	enum class WSAState : uint8_t {UNINITIALIZED, INITIALIZED};
	WSAState winsockState{WSAState::UNINITIALIZED};
	uint16_t socketRefCount{ 0 };

	WORD winsockVersion = MAKEWORD(2, 2);
	WSADATA wsaData{};
}

namespace Carnival::Network {
	Socket::Socket(const SocketData& initData) noexcept
	{
		if (socketRefCount++ == 0) {
			if(WSAStartup(winsockVersion, &wsaData) != 0) std::terminate();
			winsockState = WSAState::INITIALIZED;
		}
		// Set
		m_Status = initData.status;
		m_Status = static_cast<SocketStatus> (m_Status 
			& (~(SocketStatus::NONE 
			| SocketStatus::OPEN 
			| SocketStatus::ACTIVE 
			| SocketStatus::SOCKERROR)));
		
		m_InAddress = initData.InAddress;
		m_Port = initData.InPort;
	}
	Socket::~Socket() noexcept 
	{
		if(isOpen() || isBound() || m_Handle != INVALID_SOCKET) closeSocket();
		if (--socketRefCount == 0)
		{
			WSACleanup();
			winsockState = WSAState::UNINITIALIZED;
		}
	}
	Socket::Socket(Socket&& other) noexcept
		: m_Handle{ other.m_Handle },
		m_InAddress{ other.m_InAddress },
		m_Port{ other.m_Port },
		m_Status{ other.m_Status }
	{
		other.m_Handle = INVALID_SOCKET;
		other.m_InAddress.addr32 = 0;
		other.m_Port = 0;
		other.m_Status = SocketStatus::NONE;

	}
	Socket& Socket::operator=(Socket&& other) noexcept
	{
		if (this != &other) {
			closeSocket();
			m_Handle = other.m_Handle;
			m_InAddress = other.m_InAddress;
			m_Port = other.m_Port;
			m_Status = other.m_Status;

			other.m_Handle = INVALID_SOCKET;
			other.m_InAddress.addr32 = 0;
			other.m_Port = 0;
			other.m_Status = SocketStatus::NONE;
		}
		return *this;
	}

	void Socket::openSocket()
	{
		CL_CORE_ASSERT(winsockState == WSAState::INITIALIZED, "WSA uninitialized");
		if (m_Handle == INVALID_SOCKET) {
			m_Handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (m_Handle == INVALID_SOCKET)
			{
				std::cerr << "Failed to Open Socket.\n";
				m_Status = SocketStatus::SOCKERROR;
				return;
			}
		}
		m_Status = static_cast<SocketStatus>(SocketStatus::OPEN | m_Status);

		if (m_Status & SocketStatus::NONBLOCKING) {
			unsigned long nb = 1;
			if (ioctlsocket(m_Handle, FIONBIO, &nb) == SOCKET_ERROR)
			{
				std::cerr << "Failed to set Non-Blocking on Socket.\n";
			}
		}

		if (m_Status & SocketStatus::REUSEADDR) {
			char reuse = 1;
			if (setsockopt(m_Handle, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
				std::cerr << "Failed to set Reuse on Socket.\n";
			}
		}
	}
	bool Socket::closeSocket()
	{
		CL_CORE_ASSERT(winsockState == WSAState::INITIALIZED, "WSA uninitialized");
		CL_CORE_ASSERT(isOpen(), "Socket must be open before closing.");
		
		if (!(m_Handle == INVALID_SOCKET) && closesocket(m_Handle) == SOCKET_ERROR) {
			m_Status = SocketStatus::SOCKERROR;
			return false;
		}
		else {
			m_Status = static_cast<SocketStatus>(m_Status & (~SocketStatus::OPEN) & (~SocketStatus::BOUND));
			m_Handle = INVALID_SOCKET;
			return true;
		}
	}

	bool Socket::bindSocket()
	{
		CL_CORE_ASSERT(winsockState == WSAState::INITIALIZED, "WSA uninitialized");
		CL_CORE_ASSERT(isOpen() && !isError(), "Socket Must be open before binding.");
		if (m_Handle == INVALID_SOCKET) return false;

		sockaddr_in service{};
		service.sin_family = AF_INET;
		service.sin_addr.s_addr = htonl(m_InAddress.addr32);
		service.sin_port = htons(m_Port);
		
		int addrLen = sizeof(service);
		if (bind(m_Handle,	(const sockaddr*)&service, sizeof(sockaddr_in)) == SOCKET_ERROR
			|| getsockname(m_Handle, (sockaddr*)&service, &addrLen) == SOCKET_ERROR)
			return false;
		// Set Socket Status, Address and Port
		m_Status = static_cast<SocketStatus>(m_Status | SocketStatus::BOUND);
		m_Port = ntohs(service.sin_port);
		m_InAddress.addr32 = ntohl(service.sin_addr.s_addr);
		printf("In Address: %u.%u.%u.%u:%u\n",
			(m_InAddress.addr32 >> 24) & 0xff,
			(m_InAddress.addr32 >> 16) & 0xff,
			(m_InAddress.addr32 >> 8) & 0xff,
			m_InAddress.addr32 & 0xff, 
			m_Port);

		return true;
	}
	bool Socket::sendPackets(const char* packetData, const int packetSize, const ipv4_addr outAddr, uint16_t port) const
	{
		CL_CORE_ASSERT(winsockState == WSAState::INITIALIZED, "WSA uninitialized");
		CL_CORE_ASSERT(isBound() && !isError(), "Socket Must be Bound before Sending Packets.");
		CL_CORE_ASSERT(packetSize != 0, "Packet Size is 0");
		CL_CORE_ASSERT(packetData != nullptr, "Packet Data Poiner is null.");
		
		if (m_Handle == INVALID_SOCKET) return false;

		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(outAddr.addr32);
		if (port == 0) address.sin_port = htons(m_Port);
		else address.sin_port = htons(port);

		int sent = sendto(m_Handle, packetData, packetSize, 0, (sockaddr*) &address, sizeof(sockaddr_in));
		if (sent != packetSize) return false;
		else return true;
	}
	bool Socket::receivePackets() const
	{
		CL_CORE_ASSERT(winsockState == WSAState::INITIALIZED, "WSA uninitialized");
		CL_CORE_ASSERT(isBound() && !isError(), "Socket Must be Bound before Receiving Packets.");

		if (m_Handle == INVALID_SOCKET) return false;

		while (true)
		{
			uint8_t packetData[256] = "";
			uint32_t maxPacketSize = sizeof(packetData);
			
			thread_local sockaddr_in from{};
			thread_local int fromLength = sizeof(from);

			int64_t bytes = recvfrom(m_Handle, (char*)packetData, 
				maxPacketSize, 0, (sockaddr*)&from, &fromLength);

			if (bytes <= 0 || bytes == SOCKET_ERROR) {
				return false;
			}

			// process
			/*
			uint32_t fromAddr = ntohl(from.sin_addr.s_addr);
			uint16_t fromPort = ntohs(from.sin_port);
			*/
			packetData[(bytes >= (sizeof(packetData) - 1) ? sizeof(packetData) - 1 : bytes)] = '\0';
			std::cout << packetData << '\n';
		}
	}

	void Socket::setInAddress(ipv4_addr inAddr)
	{
		CL_CORE_ASSERT(winsockState == WSAState::INITIALIZED, "WSA uninitialized");
		m_InAddress = inAddr;
		// wait for transfers to complete maybe
		// different queue for new packets or they are tagged
		if (isBound() && m_Handle != INVALID_SOCKET) {
			closeSocket();
			openSocket();
			bindSocket();
		}
	}
	void Socket::setPort(uint16_t port)
	{
		CL_CORE_ASSERT(winsockState == WSAState::INITIALIZED, "WSA uninitialized");
		m_Port = port;
		if (isBound() && m_Handle != INVALID_SOCKET) {
			closeSocket();
			openSocket();
			bindSocket();
		}
	}
}
#endif