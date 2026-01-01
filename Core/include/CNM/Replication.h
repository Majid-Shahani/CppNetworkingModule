#pragma once
#include <cstdint>
#include <vector>
#include <span>

namespace Carnival::Network {
	struct ReplicationRecord {
		uint32_t entity{};
		uint32_t netID{};

	};
}