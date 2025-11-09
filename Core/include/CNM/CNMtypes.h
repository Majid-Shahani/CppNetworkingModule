#pragma once

#include <cstdint>
namespace Carnival::Network {
	enum SocketType : uint8_t
	{
		CONNECTION_ORIENTED = 0,
		CONNECTION_LESS = 1,
	};
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
	// Address will be set in host byte order after call to bind / send
	union ipv4_addr {
		uint32_t addr32{};
		uint16_t addr16[2];
		uint8_t addr8[4];
	};
}