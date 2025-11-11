#pragma once

#include <cstdint>
namespace Carnival::Network {
	// Address will be set in host byte order after call to bind / send
	union ipv4_addr {
		uint32_t addr32{};
		uint16_t addr16[2];
		uint8_t addr8[4];
	};
	struct SocketData {
		ipv4_addr	InAddress{};
		uint16_t	Port = 0;
		uint8_t		ConnectionOriented = false;
		uint8_t		NonBlocking = true;
	};
}