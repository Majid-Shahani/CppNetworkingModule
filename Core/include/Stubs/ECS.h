#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <algorithm>

#include <CNM/utils.h>

using Entity = uint32_t;
namespace Carnival::ECS {

	struct ComponentMetadata {
		uint64_t componentTypeID;
		uint64_t sizeOfComponent; // could be uint16_t
	};

	struct ComponentColumn {
		void* pComponentData;
		uint16_t componentHandle;
		uint16_t stride;
	};

	// Since Indices into the Registry metadata are stored as handles,
	// Registry must not be cleaned mid-session, Components must not be removed mid-Session
	class Registry {
	public:
		// Registry Owns MetaData, Changes to user owned metaData will not be Canon, lifetime doesn't matter.
		void registerComponent(const ComponentMetadata& metaData) {
			if (canAdd(metaData)) m_MetaData.push_back(metaData);
		}
		// TODO: Add Error Returning and Proper Checks
		uint16_t getComponentHandle(uint64_t componentTypeID) const {
			uint16_t res{};
			for (; res < m_MetaData.size(); res++) if (m_MetaData[res].componentTypeID == componentTypeID) break;
			return (res == m_MetaData.size() ? 0xFFFF : res);
		}

		uint64_t getTypeID(uint16_t handle) const {
			if (handle >= m_MetaData.size()) return 0xFFFFFFFFFFFFul;
			else return m_MetaData[handle].componentTypeID;
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
		static std::unique_ptr<Archetype> create(const Registry& metadataReg, 
			const std::vector<uint64_t>& componentIDs, uint64_t initialCapacity = 5) {
			if (!validComponentIDs(metadataReg, componentIDs)) return nullptr;
			
			std::vector<uint64_t> sortedIDs(componentIDs);
			std::sort(sortedIDs.begin(), sortedIDs.end());
			if (std::adjacent_find(sortedIDs.begin(), sortedIDs.end()) != sortedIDs.end()) return nullptr;

			return std::unique_ptr<Archetype>(new Archetype(metadataReg, sortedIDs, initialCapacity));
		}

		std::vector<uint64_t> getComponents(const Registry& componentRegistry) const{
			std::vector<uint64_t> comps{};
			for (const auto& comp : m_Components) comps.emplace_back(componentRegistry.getTypeID(comp.componentHandle));
			return comps;
		}

	private:
		Archetype(const Registry& metadataReg,
			const std::vector<uint64_t>& componentIDs,
			uint64_t initialCapacity)
			: m_ArchetypeID{ getArchetypeID(componentIDs) }, m_Capacity{ initialCapacity }
		{

		}

		static uint64_t getArchetypeID(const std::vector<uint64_t>& compIDs) { return 0; }

		static bool validComponentIDs(const Registry& reg, const std::vector<uint64_t>& componentIDs) {
			for (const auto& id : componentIDs) {
				if (reg.getComponentHandle(id) == 0xFFFF) return false;
			}
			return true;
		}

	private:
		std::vector<ComponentColumn> m_Components{};
		const uint64_t m_ArchetypeID;
		uint64_t m_Capacity{};
		uint32_t m_EntityCount{};

	};
}