#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

#include <CNM/utils.h>

using Entity = uint32_t;
namespace Carnival::ECS {

	struct ComponentMetadata {
		const char* componentName;

		uint64_t componentTypeID;
		uint64_t sizeOfComponent;
	};

	struct ComponentColumn {
		ComponentMetadata* pMetadata;
		void* pComponentData;
		uint64_t stride; // = pMetadata->sizeOfComponent;
	};

	class Archetype {
	public:
		Archetype() {

		}

		auto getComponents() const{
			std::vector<uint64_t> comps{};
			for (const auto& comp : m_Components) comps.emplace_back(comp.pMetadata->componentTypeID);
			return comps;
		}
	private:
		std::vector<ComponentColumn> m_Components;
		uint64_t m_Capacity{};
		const uint64_t m_ArchetypeID{};
		const uint32_t m_EntityCount{};

	};

	class Registry {
	private:
		
	};
}