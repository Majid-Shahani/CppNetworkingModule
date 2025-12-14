#pragma once

#include <cstdint>
namespace Carnival::Network {
	// Address will be set in host byte order after call to bind / send
	enum SocketStatus : uint8_t {
		NONE = 0,		// uninitialized or closed
		OPEN = 1,	// handle created and valid
		BOUND = 1 << 1,	// bound to port/address
		ACTIVE = 1 << 2,	// in-use by the program
		NONBLOCKING = 1 << 3,	// recv & send immediate return
		REUSEADDR = 1 << 4,
		/*
		For Later Use
		BROADCAST	= 1 << 5,
		*/
		SOCKERROR = 1 << 7,		// socket-level error
	};
	union ipv4_addr {
		uint32_t addr32{};
		uint16_t addr16[2];
		uint8_t addr8[4];
	};
	struct SocketData {
		ipv4_addr		InAddress{};
		uint16_t		InPort{};
		SocketStatus	status{SocketStatus::NONBLOCKING};
	};
	struct ManagerData {
		SocketData* pReliableSockData{ nullptr };
		SocketData* pUnreliableSockData{ nullptr };
		SocketData* pSnapshotSockData{ nullptr };
		uint8_t Timeout{10};
		uint8_t	NumberOfPeers{1}; // estimated for construction time reserve
	};
}