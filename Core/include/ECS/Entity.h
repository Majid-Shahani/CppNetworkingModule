#pragma once

#include <cstdint>
#include <vector>
#include <span>


namespace Carnival::ECS {
	class Archetype;
	using Entity = uint32_t;

	enum EntityStatus : uint32_t {
		DEAD = 0,
		ALIVE = 1 << 0,
		// Reserved
	};
	// Entity location in archetype storage
	struct EntityEntry {
		Archetype* pArchetype;
		uint32_t index; // index in archetype
		EntityStatus status;
	};

	// Not Thread safe, Change
	// Central Entity Registry
	class EntityManager {
	public:
		Entity create(Archetype* pArchetype, uint32_t index, EntityStatus status = EntityStatus::DEAD);

		const EntityEntry& get(Entity e);
		// update state and location
		void updateEntity(Entity e, Archetype* pArchetype, uint32_t index, EntityStatus status);
		// Update archetype and index only
		void updateEntityLocation(Entity e, Archetype* pArchetype, uint32_t index);
		// Release ID
		void destroyEntity(Entity e);
		void destroyEntities(std::span<const Entity> e);
		void reset();
	private:
		std::vector<Entity> m_FreeIDs{}; // Recycled IDs
		std::vector<EntityEntry> m_Entries{}; // Metadata Table
	};
	// maps Entity -> Stable Net ID
	class NetIDGenerator {
	public:
		uint64_t	createID(Entity entityID);
		Entity		getEntity(uint64_t netID);
		void		destroyID(uint64_t netID);
		void		reset();
	private:
		std::vector<uint32_t> m_FreeIDs{}; // Recycled IDs
		std::vector<uint64_t> m_Entries{}; // Mappings
	};
}