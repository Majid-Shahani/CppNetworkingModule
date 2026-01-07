#pragma once
//CNM
#include <CNM/CNMtypes.h>

namespace Carnival::Network {

	class Socket {
	public:
		Socket(const SocketData& initData) noexcept;
		~Socket() noexcept;

		//multiple sockets for same IP/Port not allowed
		Socket(const Socket&)				= delete;
		Socket& operator=(const Socket&)	= delete;
		Socket(Socket&& other) noexcept;
		Socket& operator=(Socket&& other) noexcept;

		void openSocket();
		bool closeSocket();

		// SocketData port (if 0) is overwritten after call to bindSocket
		bool bindSocket();
		bool sendPackets(const char* packetData, const int packetSize, const ipv4_addr outAddr, uint16_t port = 0) const;
		bool receivePackets() const; // TODO: should return bytes

		// Status Checking
		bool isOpen() const			{ return (m_Status & SocketStatus::OPEN); }
		bool isBound() const		{ return (m_Status & SocketStatus::BOUND); }
		bool isError() const		{ return (m_Status & SocketStatus::SOCKERROR); }
		bool isNonBlocking() const	{ return (m_Status & SocketStatus::NONBLOCKING); }

		// Set Address & Port, calls to these will cause a reset to the socket if bound
		void setInAddress(const ipv4_addr inAddr);
		void setPort(const uint16_t port);

		uint16_t getPort() const {
			if (isBound()) return m_Port; 
			else return 0; 
		}
		ipv4_addr getAddr() const {
			if (isBound()) return m_InAddress;
			else return ipv4_addr{}; 
		}
	private:
		// Host Byte Order for Address and Port
		uint64_t		m_Handle = UINT64_MAX;
		ipv4_addr		m_InAddress{};
		uint16_t		m_Port = 0;
		SocketStatus	m_Status = SocketStatus::NONE;
		char			m_Padding[1] = {0};
		// 8-bit padding for alignment at the end
	};

}
