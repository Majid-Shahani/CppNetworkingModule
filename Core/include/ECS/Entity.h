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
	struct EntityEntry {
		Archetype* pArchetype;
		uint32_t index;
		EntityStatus status;
	};

	struct NetEntityInfo {
		Entity entityID{};
	};

	// Not Thread safe, Change
	class EntityManager {
	public:
		Entity create(Archetype* pArchetype, uint32_t index, EntityStatus status = EntityStatus::DEAD);

		const EntityEntry& get(Entity e);

		void updateEntity(Entity e, Archetype* pArchetype, uint32_t index, EntityStatus status);
		void updateEntityLocation(Entity e, Archetype* pArchetype, uint32_t index);

		void destroyEntity(Entity e);
		void destroyEntities(std::span<const Entity> e);
		void reset();
	private:
		std::vector<Entity> m_FreeIDs{};
		std::vector<EntityEntry> m_Entries{};
	};
	class NetIDGenerator {
	public:
		uint32_t	createID(Entity entityID);
		Entity		getEntity(uint32_t netID);
		void		destroyID(uint32_t netID);
		void		reset();
	private:
		std::vector<Entity> m_FreeIDs{};
		std::vector<NetEntityInfo> m_Entries{};
	};
}