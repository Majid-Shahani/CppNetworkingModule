#pragma once
//STL
#include <unordered_map>
//CNM
#include <CNM/CNMtypes.h>

namespace Carnival::Network {
	struct SocketData {
		ipv4_addr		OutAddress{};
		ipv4_addr		InAddress{};
		uint16_t		Port	= 0;
		SocketType		Type	= CONNECTION_LESS;
		SocketStatus	Status	= SocketStatus::NONE;
	};

	class Socket {
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
		bool sendPackets(const char* packetData, int packetSize) const;
		void receivePackets() const;

		// Status Checking
		bool isOpen() const	{ return (m_Status & SocketStatus::OPEN); }
		bool isBound() const { return (m_Status & SocketStatus::BOUND); }
		SocketStatus getStatus() const { return m_Status; }

		// Set Flags
		void setStatusFlag(SocketStatus flags) { m_Status = flags; }
		void addStatusFlag(SocketStatus flags) { m_Status = static_cast<SocketStatus>(m_Status | flags); }

		// Set Address & Port, calls to these will cause a reset to the socket if bound
		void setOutAddress(ipv4_addr outAddr) { m_OutAddress = outAddr; } // if mid-send what to do? wait?
		void setInAddress(ipv4_addr inAddr);
		void setPort(uint16_t port);

	private:
		// Host Byte Order for Address and Port
		uint64_t		m_Handle = 0;
		ipv4_addr		m_OutAddress{};
		ipv4_addr		m_InAddress{};
		uint16_t		m_Port = 0;
		SocketType		m_Type = CONNECTION_LESS;
		SocketStatus	m_Status = SocketStatus::NONE;
		// 32-bit padding for alignment at the end
	};

}
