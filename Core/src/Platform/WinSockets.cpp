#include <src/CNMpch.hpp>

#ifdef CL_X64
#include <immintrin.h>
static inline void SpinPause() { _mm_pause(); }
#elif defined(CL_ARM64)
#include <arm_acle.h>
static inline void SpinPause() { __yield(); }
#else
static inline void SpinPause() {}
#endif

#ifdef CL_Platform_Windows
#include <include/CNM/macros.h>
#include <include/CNM/Socket.h>
namespace {
	enum class WSAState : uint8_t {UNINITIALIZED, INITIALIZING, INITIALIZED};
	std::atomic<WSAState> winsockState{WSAState::UNINITIALIZED};
	std::atomic<int> socketRefCount{ 0 };

	WORD winsockVersion = MAKEWORD(2, 2);
	WSADATA wsaData{};

	void initializeWSA() 
	{
		WSAState expected = WSAState::UNINITIALIZED;
		if (winsockState.compare_exchange_strong(expected, WSAState::INITIALIZING, std::memory_order_acq_rel)) {
			if (WSAStartup(winsockVersion, &wsaData) != 0) {
				winsockState.store(WSAState::UNINITIALIZED, std::memory_order_release);
			}
			winsockState.store(WSAState::INITIALIZED, std::memory_order_release);
		}
		else {
			uint8_t loops = 0;
			while (winsockState.load(std::memory_order_acquire) != WSAState::INITIALIZED) {
				SpinPause();
				loops++;
				if (loops > 200) {
					loops = 0;
					std::this_thread::yield();
				}
			}
		}
		// Possible issues : WSAStartup will fail and a thread will busy-wait until another thread attempt a WSAStartup
	}
}

namespace Carnival::Network {
	Socket::Socket(SocketData& initData) noexcept
	{
		if (winsockState.load(std::memory_order_acquire) != WSAState::INITIALIZED) {
			// multiple threads entering this will be handled down the line via compare exchange strong
			initializeWSA();
		}
		socketRefCount.fetch_add(1, std::memory_order_release);

		// Set
		m_Status = (initData.NonBlocking? SocketStatus::NONBLOCKING : SocketStatus::NONE);
		m_InAddress = initData.InAddress;
		m_Port = initData.Port;
		//m_Type = initData.Type;
	}
	Socket::~Socket() noexcept 
	{
		if(isOpen() || isBound()) closeSocket();
		if (socketRefCount.fetch_sub(1, std::memory_order_acquire) == 1)
		{
			WSACleanup();
			winsockState.store(WSAState::UNINITIALIZED, std::memory_order_release);
		}
	}

	void Socket::openSocket()
	{
		CL_CORE_ASSERT(winsockState.load(std::memory_order_acquire) == WSAState::INITIALIZED, "WSA uninitialized");
		
		m_Handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (m_Handle == INVALID_SOCKET)
		{
			std::cerr << "Failed to Open Socket.\n";
			m_Status = SocketStatus::SOCKERROR;
			return;
		}
		m_Status = static_cast<SocketStatus>(SocketStatus::OPEN | m_Status);

		if (m_Status & SocketStatus::NONBLOCKING) {
			unsigned long nb = 1;
			if (ioctlsocket(m_Handle, FIONBIO, &nb) == SOCKET_ERROR)
			{
				std::cerr << "Failed to set Non-Blocking on Socket.\n";
			}
		}
	}
	bool Socket::closeSocket()
	{
		CL_CORE_ASSERT(winsockState.load(std::memory_order_acquire) == WSAState::INITIALIZED, "WSA uninitialized");
		CL_CORE_ASSERT(isOpen(), "Socket must be open before closing.");
		
		if (closesocket(m_Handle) == SOCKET_ERROR) {
			m_Status = SocketStatus::SOCKERROR;
			return false;
		}
		else {
			m_Status = static_cast<SocketStatus>(m_Status & (~SocketStatus::OPEN) & (~SocketStatus::BOUND));
			m_Handle = NULL;
			return true;
		}
	}

	bool Socket::bindSocket()
	{
		CL_CORE_ASSERT(winsockState.load(std::memory_order_acquire) == WSAState::INITIALIZED, "WSA uninitialized");
		CL_CORE_ASSERT(isOpen() && !isError(), "Socket Must be open before binding.");
		
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
	bool Socket::sendPackets(ipv4_addr outAddr, const char* packetData, int packetSize) const
	{
		CL_CORE_ASSERT(winsockState.load(std::memory_order_acquire) == WSAState::INITIALIZED, "WSA uninitialized");
		CL_CORE_ASSERT(isBound() && !isError(), "Socket Must be Bound before Sending Packets.");
		CL_CORE_ASSERT(packetSize != 0, "Packet Size is 0");
		CL_CORE_ASSERT(packetData != nullptr, "Packet Data Poiner is null.");
		
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(outAddr.addr32);
		address.sin_port = htons(m_Port);

		int sent = sendto(m_Handle, packetData, packetSize, 0, (sockaddr*) &address, sizeof(sockaddr_in));
		if (sent != packetSize) return false;
		else return true;
	}
	bool Socket::receivePackets() const
	{
		CL_CORE_ASSERT(winsockState.load(std::memory_order_acquire) == WSAState::INITIALIZED, "WSA uninitialized");
		CL_CORE_ASSERT(isBound() && !isError(), "Socket Must be Bound before Receiving Packets.");
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
			uint32_t fromAddr = ntohl(from.sin_addr.s_addr);
			uint16_t fromPort = ntohs(from.sin_port);

			packetData[(bytes >= (sizeof(packetData) - 1) ? sizeof(packetData) - 1 : bytes)] = '\0';
			std::cout << packetData << '\n';
		}
	}

	void Socket::setInAddress(ipv4_addr inAddr)
	{
		m_InAddress = inAddr;
		// wait for transfers to complete maybe
		// different queue for new packets or they are tagged
		if (isBound()) {
			closeSocket();
			openSocket();
		}
	}
	void Socket::setPort(uint16_t port)
	{
		m_Port = port;
		if (isBound()) {
			closeSocket();
			openSocket();
		}
	}
}
#endif