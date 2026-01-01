#pragma once
#include <cstdint>

namespace Carnival::Network {
	struct ComponentRecord {
		uint64_t archetypeID{};
		uint64_t componentID{};
	};
	struct EntityRecord {
		uint32_t entityID{};
		uint32_t netID{};
	};
	namespace WireFormat {
		// Alignment doesn't matter, padding is discarded, structs are unused.
		// These are contracts, read field by field
		// redundant info is not sent. general wire format is as follows:

		enum class RecordType : uint8_t {
			ENTITY_DATA,
			COMPONENT_DATA,
			ARCHETYPE_INFO,
			SCHEMA,
		};
		enum class NetStatus : uint8_t {
			DELETE = 0,
			CREATE,
			UPDATE,
			MOVE,
		};
		struct EntityData {
			uint64_t archetypeID{};
			uint32_t netID{};
			NetStatus status{};
			// if status == update, componentData in canonical order (based on cID)
			uint8_t padding[3];
			// Possible Owner peerID
		};
		struct ComponentData {
			uint64_t archetypeID{};
			uint64_t componentID{};
			uint32_t count{};
			// Array of Components, component Serializer / deserializer will handle
			uint8_t padding[4];
		};
		struct ArchetypeInfo {
			uint64_t archetypeID{};
			uint32_t componentCount{};
			NetStatus status{};
			// array of uint64_t componentID written manually
			// bit-packed flags for networked or not flags, starting from LSB
			// maybe an entity count depending on status and ownership
			// relevant EntityData (just net ID) per entity if sent. 
			uint8_t padding[3];
		};
		struct SCHEMA_SNAPSHOT {
			NetStatus status;
			uint64_t archetypeCount{};
			// for each archetype:
			ArchetypeInfo archInfo{};
			ComponentData components{}; 
		};

	}
}