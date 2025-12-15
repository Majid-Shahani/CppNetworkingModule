#pragma once
#include <cstdint>
#include <vector>

#include <Stubs/ECS.h>

namespace Carnival::Network {
	struct ReplicationRecord {
		uint64_t ArchetypeID{};
		Entity networkID{};
		uint32_t Version{};
	};

	struct ArchetypeSchema {
		uint8_t* componentIDs{ nullptr };
		uint16_t archetypeID{};
		uint8_t numberOfComponents{};
	};

	void gatherDirty(uint32_t archetypeID, std::vector<ReplicationRecord>& records) {
		return;
	}
}