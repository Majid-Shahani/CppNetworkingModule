#pragma once

#include <Stubs/ECS.h>

namespace Carnival::ECS {

	class World {
	public:
		// TODO: Rule of 5
		Entity createEntity(std::span<uint64_t> components);
		void destroyEntity(Entity e);

		template<ECSComponent... Ts>
		bool registerComponent();
		
		template <ECSComponent... Ts>
		void addComponentToEntity(Entity e);
		
		template <ECSComponent... Ts>
		void removeComponentFromEntity(Entity e);

		template <ECSComponent... Ts>
		void* writeComponentOf(Entity entity);

		template<ECSComponent... Ts>
		void* writeAll();

		void startUpdate();
		void endUpdate();

	private:
		ComponentRegistry		m_Registry;
		std::vector<Archetype>	m_onTickArchetypes;
		std::vector<Archetype>	m_onUpdateArchetypes;
		std::vector<Archetype>	m_localArchetypes;
	};
}