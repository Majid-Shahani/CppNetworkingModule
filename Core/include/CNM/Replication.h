#pragma once
#include <cstdint>
#include <vector>
#include <span>

#include <Stubs/ECS.h>

namespace Carnival::Network {
	// ============== ARCHETYPE ================
	void writeArchetypeSchema(uint16_t archetypeID, std::span<const uint8_t> components, void* outBuffer) {
		// outbuffer.write_u16(archetypeID);
		// out.write_u8(components.size());
		// out.write_bytes(components.data(), components.size());
	}

	ECS::NetworkArchetypeSchema readArchetypeSchema(void* inBuffer) {
		ECS::NetworkArchetypeSchema schema{};
		// schema.archetypeID = inBuffer.read_u16();
		// schema.numberOfComponents = inBuffer.ready_u8();
		// schema.componentIDs.resize(schema.numberOfComponents);
		// inBuffer.read_bytes(schema.componentIDs.data(), count);
		return schema;
	}
	// ============= ENTITY =====================
	struct ReplicationRecord {
		Entity networkID{};
		uint16_t ArchetypeID{};
		uint16_t Version{};
	};

	void gatherDirty(uint32_t archetypeID, std::vector<ECS::Archetype>& registery, std::vector<ReplicationRecord>& records) {
		auto& networks = registery[archetypeID].GetNetworks();
		for (int i{}; i < networks.size(); i++) {
			if (networks[i].dirty == true) {
				records.emplace_back(networks[i].netID, archetypeID); // Version this
				networks[i].dirty = false;
			}
		}
	}
}