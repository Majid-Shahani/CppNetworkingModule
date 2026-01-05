#include <src/CNMpch.hpp>
#include <Stubs/World.h>

#include <ranges>

namespace Carnival::ECS {
	Entity World::createEntity(std::vector<uint64_t> components, NetworkFlags flag)
	{
		std::sort(components.begin(), components.end());
		uint64_t id{ Archetype::hashArchetypeID(components) };
		
		auto [it, inserted] = m_Archetypes.try_emplace(id, m_Registry, components, id, flag, 5);
		if (!inserted) {
			CL_CORE_ASSERT(it->second.flags == flag, "Mismatch Network flags on archetype");
			CL_CORE_ASSERT(it->second.arch->getComponentIDs() == components, "Hash Collision");
		}
		
		Entity e = m_EntityManager.create(id, 0);
		uint32_t index = it->second.arch->addEntity(e);
		m_EntityManager.updateEntityLocation(e, id, index);
		
		return e;
	}
	void World::destroyEntity(Entity e)
	{
		const auto& rec = m_EntityManager.get(e);
		

	}
	void World::startUpdate()
	{
	}
	void World::endUpdate()
	{
	}
}