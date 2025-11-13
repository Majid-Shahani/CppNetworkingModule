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
		uint16_t	InPort{};
		uint8_t		Reliable{ false };
		uint8_t		NonBlocking{ true };
	};
	struct ManagerData {
		SocketData* pReliableSockData{ nullptr };
		SocketData* pUnreliableSockData{ nullptr };
		SocketData* pSnapshotSockData{ nullptr };
		uint8_t Timeout{10};
		uint8_t	NumberOfPeers{1}; // estimated for construction time reserve
	};
}