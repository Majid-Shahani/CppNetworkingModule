#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

#include <CNM/utils.h>

using Entity = uint32_t;
namespace Carnival::ECS {

	struct ComponentMetadata {
		uint64_t componentTypeID;
		uint64_t sizeOfComponent;
	};

	struct ComponentColumn {
		ComponentMetadata* pMetadata;
		void* pComponentData;
		uint64_t stride; // = pMetadata->sizeOfComponent;
	};

	class Registry {
	public:
		// Registry Owns MetaData, Changes to user owned metaData will not be Canon, lifetime doesn't matter.
		void registerComponent(const ComponentMetadata& metaData) {
			if (canAdd(metaData)) m_MetaData.push_back(metaData);
		}
	private:
		bool canAdd(const ComponentMetadata& metaData) const {
			for (const auto& comp : m_MetaData)
				if (comp.componentTypeID == metaData.componentTypeID) {
					if (comp.sizeOfComponent != metaData.sizeOfComponent) throw std::runtime_error("Registry Name Hash Collision.");
					else return false;
				}
			return true;
		}
	private:
		std::vector<ComponentMetadata> m_MetaData;
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
		const uint64_t m_ArchetypeID;
		uint64_t m_Capacity{};
		uint32_t m_EntityCount{};

	};
}