#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <algorithm>
#include <stdexcept>

#include <CNM/utils.h>

using Entity = uint32_t;
namespace Carnival::ECS {

	struct ComponentMetadata {
		uint64_t componentTypeID;
		uint16_t sizeOfComponent;
		// 6 bytes of padding
	};

	struct ComponentColumn {
		void* pComponentData{ nullptr };
		uint16_t componentHandle{};
		uint16_t stride{};
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
		uint16_t getSizeOf(uint16_t handle) const {
			if (handle >= m_MetaData.size()) return 0xFFFF;
			else return m_MetaData[handle].sizeOfComponent;
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
			
			// Copy Component ID Array
			std::vector<uint64_t> sortedIDs(componentIDs);

			// Sort ID Array canonically
			std::sort(sortedIDs.begin(), sortedIDs.end());
			
			// Check for duplicate components in Entity
			if (std::adjacent_find(sortedIDs.begin(), sortedIDs.end()) != sortedIDs.end()) return nullptr;

			// Build Component columns and validate componentIDs
			std::vector<ComponentColumn> columns;
			columns.reserve(sortedIDs.size());
			for (uint64_t compID : sortedIDs) {
				auto handle = metadataReg.getComponentHandle(compID);
				if (handle == 0xFFFF) return nullptr;
				columns.emplace_back(ComponentColumn(nullptr, handle, metadataReg.getSizeOf(handle)));
			}

			return std::unique_ptr<Archetype>(new Archetype(std::move(columns), sortedIDs, initialCapacity));
		}

		std::vector<uint64_t> getComponents(const Registry& componentRegistry) const{
			std::vector<uint64_t> comps{};
			for (const auto& comp : m_Components) comps.emplace_back(componentRegistry.getTypeID(comp.componentHandle));
			return comps;
		}

	private:
		Archetype(std::vector<ComponentColumn>&& components,
			const std::vector<uint64_t>& componentIDs,
			uint64_t initialCapacity)
			: m_Components{std::move(components)}, m_ArchetypeID{getArchetypeID(componentIDs)}, m_Capacity{initialCapacity}
		{

		}

		// fnv1a 64-bit hash specifically for little-endian systems, not cross-compatible
		static uint64_t getArchetypeID(const std::vector<uint64_t>& compIDs) {
			uint64_t hash = utils::FNV64_OFFSET_BASIS;
			for (auto id : compIDs) {
				for (uint64_t i{}; i < 8; i++) {
					uint8_t byte = (id >> (i * 8)) & 0xFF;
					hash ^= byte;
					hash *= utils::FNV64_PRIME;
				}
			}
			return hash;
		}

	private:
		std::vector<ComponentColumn> m_Components{};
		const uint64_t m_ArchetypeID;
		uint64_t m_Capacity{};
		uint32_t m_EntityCount{};

	};
}