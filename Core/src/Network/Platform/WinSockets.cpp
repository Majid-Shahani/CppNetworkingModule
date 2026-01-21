#include <src/CNMpch.hpp>

#ifdef CL_Platform_Windows
#include <include/CNM/macros.h>
#include <include/CNM/Socket.h>
namespace {
	// Process-Wide State
	enum class WSAState : uint8_t {UNINITIALIZED, INITIALIZED};
	WSAState winsockState{WSAState::UNINITIALIZED};
	uint16_t socketRefCount{ 0 };

	WORD winsockVersion = MAKEWORD(2, 2);
	WSADATA wsaData{};
}

namespace Carnival::Network {
	// Non-Opening Constructor
	Socket::Socket() noexcept {
		if (socketRefCount++ == 0) {
			if (WSAStartup(winsockVersion, &wsaData) != 0) std::terminate();
			winsockState = WSAState::INITIALIZED;
		}
	}
	// Opens Socket immediately
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
	PollResult Socket::waitForPackets(int32_t timeout, uint64_t handle1, uint64_t handle2) noexcept
	{
		CL_CORE_ASSERT(handle1 != INVALID_SOCKET
			&& handle2 != INVALID_SOCKET, "Invalid Sockets!");

		WSAPOLLFD fds[2]{ { handle1, POLLRDNORM, 0 }, { handle2, POLLRDNORM, 0 } };

		int ready = WSAPoll(fds, 2, timeout);
		if (ready == SOCKET_ERROR) {
			std::print("Socket Error: {}", WSAGetLastError());
			// handle error
			return PollResult::Error;
		}
		if (fds[0].revents & POLLERR || fds[1].revents & POLLERR) return PollResult::Error;
		if (fds[0].revents & POLLRDNORM || fds[1].revents & POLLRDNORM) return PollResult::Packet;

		return PollResult::None;
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
			if(isOpen()) closeSocket();
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

	// Create Socket, Apply State
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
		// Enable Non-Blocking IO
		if (m_Status & SocketStatus::NONBLOCKING) {
			unsigned long nb = 1;
			if (ioctlsocket(m_Handle, FIONBIO, &nb) == SOCKET_ERROR)
			{
				std::cerr << "Failed to set Non-Blocking on Socket.\n";
			}
		}
		// Allow Address reuse
		if (m_Status & SocketStatus::REUSEADDR) {
			char reuse = 1;
			if (setsockopt(m_Handle, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
				std::cerr << "Failed to set Reuse on Socket.\n";
			}
		}
	}
	// Free OS Resource
	bool Socket::closeSocket() noexcept
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
	// bind and query port
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
		// Debug
		printf("In Address: %u.%u.%u.%u:%u\n",
			(m_InAddress.addr32 >> 24) & 0xff,
			(m_InAddress.addr32 >> 16) & 0xff,
			(m_InAddress.addr32 >> 8) & 0xff,
			m_InAddress.addr32 & 0xff, 
			m_Port);

		return true;
	}
	// Send To Wrapper using existing memory
	bool Socket::sendPackets(std::span<const std::byte> packet,
		const ipv4_addr outAddr, 
		uint16_t port) const noexcept
	{
		CL_CORE_ASSERT(winsockState == WSAState::INITIALIZED, "WSA uninitialized");
		CL_CORE_ASSERT(isBound() && !isError(), "Socket Must be Bound before Sending Packets.");
		CL_CORE_ASSERT(packet.size() != 0, "Packet Size is 0");
		//CL_CORE_ASSERT(packet.data() != nullptr, "Packet Data Poiner is null.");
		
		if (m_Handle == INVALID_SOCKET) return false;

		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(outAddr.addr32);
		if (port == 0) address.sin_port = htons(m_Port);
		else address.sin_port = htons(port);

		int sent = sendto(m_Handle, reinterpret_cast<const char*>(packet.data()),
			static_cast<int>(packet.size()),
			0, (sockaddr*)&address, sizeof(sockaddr_in));
		if (sent != packet.size()) return false;
		else return true;
	}
	// Non-blocking Poll
	PollResult Socket::poll() const noexcept {
		WSAPOLLFD pfd{};
		pfd.fd = m_Handle;
		pfd.events = POLLRDNORM;
		int ready = WSAPoll(&pfd, 1, 0);

		if (ready <= 0) return PollResult::None;
		if (pfd.revents & POLLERR) return PollResult::Error;
		if (pfd.revents & POLLRDNORM) return PollResult::Packet;

		return PollResult::None;
	}
	PacketInfo Socket::receivePacket(std::vector<std::byte>& packet) noexcept
	{
		CL_CORE_ASSERT(winsockState == WSAState::INITIALIZED, "WSA uninitialized");
		CL_CORE_ASSERT(isBound() && !isError(), "Socket Must be Bound before Receiving Packets.");
		CL_CORE_ASSERT(m_Handle != INVALID_SOCKET, "Socket must be open and bound before receiving packets.");

		sockaddr_in from{};
		int fromLength = sizeof(from);

		packet.resize(PACKET_MTU);
		int64_t bytes = recvfrom(m_Handle, reinterpret_cast<char*>(packet.data()),
			static_cast<int>(PACKET_MTU), 0, (sockaddr*)&from, &fromLength);
		if (bytes >= 0) { // if no errors
			packet.resize(bytes);
			return { ntohl(from.sin_addr.s_addr), ntohs(from.sin_port) };
		}

		// if error
		int err{ WSAGetLastError() };
		switch (err) {
		case 0:
		case WSAEWOULDBLOCK:
		case WSAEINTR:
			packet.clear();
			return {};
			break;

		case WSAECONNREFUSED:
		case WSAECONNRESET:
		case WSAETIMEDOUT:
			packet.clear();
			break;
		default:
			m_Status = SocketStatus::SOCKERROR;
			std::print("Sockerror: {}\n", WSAGetLastError());
			CL_CORE_ASSERT(false, "Socket Error!");
			//throw std::runtime_error("Fatal Socket Error");
		}

		return {};
	}
	// Probe socket error
	SocketError Socket::pollError() noexcept
	{
		char dummy{};
		sockaddr_in from{};
		int fromLen{ sizeof(from) };

		int ret = recvfrom(
			m_Handle,
			&dummy,
			sizeof(dummy),
			0,
			reinterpret_cast<sockaddr*>(&from),
			&fromLen
		);

		if (ret >= 0) return SocketError::None;
		int err{ WSAGetLastError() };

		switch (err) {
		case 0:
		case WSAEWOULDBLOCK:
		case WSAEINTR:
			return SocketError::Transient;
			break;

		case WSAECONNREFUSED:
		case WSAETIMEDOUT:
			return SocketError::Remote;
			break;
		default:
			return SocketError::Fatal;
		}
	}

	void Socket::setNonBlocking(bool nb) {
		CL_CORE_ASSERT(winsockState == WSAState::INITIALIZED, "WSA uninitialized");
		m_Status = static_cast<SocketStatus>(m_Status | SocketStatus::NONBLOCKING);

		if (isBound() && m_Handle != INVALID_SOCKET) {
			closeSocket();
			openSocket();
			bindSocket();
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