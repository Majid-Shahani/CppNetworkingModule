#pragma once
//CNM
#include <CNM/CNMtypes.h>

namespace Carnival::Network {

	class Socket {
	private:
		enum SocketStatus : uint8_t
		{
			NONE = 0,		// uninitialized or closed
			OPEN = 1,	// handle created and valid
			BOUND = 1 << 1,	// bound to port/address
			ACTIVE = 1 << 2,	// in-use by the program
			NONBLOCKING = 1 << 3,	// recv & send immediate return
			/*
			For Later Use
			REUSEADDR	= 1 << 4,
			BROADCAST	= 1 << 5,
			*/
			SOCKERROR = 1 << 7,		// socket-level error
		};
	public:
		Socket(SocketData& initData) noexcept;
		~Socket() noexcept;

		// only move for handle later? multiple sockets for same IP/Port not allowed
		Socket(const Socket&)				= delete;
		Socket& operator=(const Socket&)	= delete;
		Socket(Socket&&)					= delete;

		void openSocket();
		bool closeSocket();

		// pass in an address a.b.c.d as (a << 24) | (b << 16) | (c <<8) | d
		// SocketData port (if 0) is overwritten after call to bindSocket
		bool bindSocket();
		bool sendPackets(ipv4_addr outAddr, const char* packetData, int packetSize) const;
		bool receivePackets() const;

		// Status Checking
		bool isOpen() const			{ return (m_Status & SocketStatus::OPEN); }
		bool isBound() const		{ return (m_Status & SocketStatus::BOUND); }
		bool isError() const		{ return (m_Status & SocketStatus::SOCKERROR); }
		bool isNonBlocking() const	{ return (m_Status & SocketStatus::NONBLOCKING); }
		// Set Flags
		void setNonBlocking(bool is) { m_Status = static_cast<SocketStatus>(m_Status | is << 3); }

		// Set Address & Port, calls to these will cause a reset to the socket if bound
		void setInAddress(ipv4_addr inAddr);
		void setPort(uint16_t port);

	private:
		// Host Byte Order for Address and Port
		uint64_t		m_Handle = 0;
		ipv4_addr		m_InAddress{};
		uint16_t		m_Port = 0;
		SocketStatus	m_Status = SocketStatus::NONE;
		char			m_Padding[1] = {0};
		// 8-bit padding for alignment at the end
	};

}
