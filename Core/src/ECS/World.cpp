#include <src/CNMpch.hpp>
#include <Stubs/World.h>

namespace Carnival::ECS {
	template<ECSComponent ...Ts>
	bool World::registerComponent()
	{
		return false;
	}
	template<ECSComponent ...Ts>
	void World::addComponentToEntity(Entity e)
	{
	}
	template<ECSComponent ...Ts>
	void World::removeComponentFromEntity(Entity e)
	{
	}
	template<QueryPolicy P, ECSComponent T>
	World::ComponentRange<P, T> World::query()
	{
		return ComponentRange<P, T>();
	}

	Entity World::createEntity(std::span<uint64_t> components)
	{
		return Entity();
	}
	void World::destroyEntity(Entity e)
	{
	}
	void World::startUpdate()
	{
	}
	void World::endUpdate()
	{
	}
}