#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

#include <CNM/utils.h>

using Entity = uint32_t;
namespace Carnival::ECS {

	struct ComponentMetadata {
		std::string componentName;

		uint64_t componentTypeID;
		uint64_t sizeOfComponent;
		void* serializerFn;
		void* deSerializerFn;
	};

	struct ComponentColumn {
		ComponentMetadata* pMetadata;
		void* pComponentData;
		uint64_t stride; // = pMetadata->sizeOfComponent;
	};

	class Archetype {
	public:
	private:
		const std::vector<ComponentColumn> m_Components;
		uint64_t m_Capacity{};
		const uint64_t m_ArchetypeID{};
		const uint32_t m_EntityCount{};

	};

	class Registry {
	private:
		
	};
}