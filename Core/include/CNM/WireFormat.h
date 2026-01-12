#pragma once
#include <cstdint>
#include <vector>

namespace Carnival::Network::WireFormat {
	// Alignment doesn't matter, padding is discarded, structs are unused.
	// These are contracts, read field by field
	// redundant info is not sent. general wire format is as follows:

	enum RecordType : uint8_t {
		ENTITY_DATA = 0,
		COMPONENT_DATA,
		ARCHETYPE_DATA,
		ARCHETYPE_SCHEMA,
		SCHEMA,

		SYSTEM_EVENT,
		USER_EVENT,
	};
	enum EventType : uint8_t {
		CreateEntity = 0,
		MoveEntity,
		DestroyEntity,
	};

	struct EntityData {
		uint64_t archetypeID{};
		uint32_t netID{};
		uint8_t componentCount{};
		std::vector<std::byte> componentData;
		// Possible Owner peerID
	};
	struct ArchetypeData {
		uint64_t archetypeID{};
		uint32_t entity_count{};
		// Component Data
		// Array of Components, component Serializer / deserializer will handle
	};
	struct ArchetypeSchema {
		uint64_t archetypeID{};
		uint16_t componentCount{};
		std::vector<uint64_t> compIDs;
		// array of uint64_t componentID written manually
		// bit-packed flags for networked or not flags, starting from LSB
		// maybe an entity count depending on status and ownership
		// relevant EntityData (just net ID) per entity if sent.
	};
	struct SCHEMA_SNAPSHOT {
		uint64_t archetypeCount{};
		// for each archetype:
		//ArchetypeInfo archInfo{};
		//ComponentData components{}; 
	};
}