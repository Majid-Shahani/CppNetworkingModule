#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <algorithm>
#include <stdexcept>

#include <CNM/utils.h>

namespace Carnival::ECS {
	using Entity = uint32_t;
	
	class Archetype;
	struct EntityEntry {
		Archetype* archetype;
		uint64_t index;
	};

	// Not Thread safe, Change
	class EntityManager {
	public:
		static Entity create(Archetype* archetype, uint64_t index) {
			if (!s_FreeIDs.empty()) {
				Entity id = s_FreeIDs[s_FreeIDs.size() - 1];
				s_FreeIDs.pop_back();
				s_Entries[id] = { archetype, index };
				return id;
			}
			else {
				s_Entries.push_back({ archetype, index });
				return s_NextID++;
			}
		}
		
		static void updateEntity(Entity e, Archetype* archetype, uint64_t index) {
			if (e >= s_NextID) return;
			if (s_Entries[e].archetype == nullptr) {
				auto it = std::find(s_FreeIDs.begin(), s_FreeIDs.end(), e);
				if (it != s_FreeIDs.end()) {
					std::iter_swap(it, s_FreeIDs.end() - 1);
					s_FreeIDs.pop_back();
				}
			}
			s_Entries[e] = { archetype, index };
		}

		static void destroyEntity(Entity e) {
			s_Entries[e] = { nullptr, 0 };
			s_FreeIDs.push_back(e);
		}
		static void destroyEntities(const std::vector<Entity>& e) {
			for (const Entity entity : e) {
				s_Entries[entity] = { nullptr, 0 };
				s_FreeIDs.push_back(entity);
			}
		}
		static void reset() {
			s_FreeIDs.clear();
			s_Entries.clear();
			s_NextID = 1;
		}
	private:
		EntityManager() = default;
		static inline std::vector<Entity> s_FreeIDs{};
		static inline std::vector<EntityEntry> s_Entries{};
		static inline Entity s_NextID{ 1 };
	};

	struct ComponentMetadata {
		uint64_t componentTypeID{ 0xFFFFFFFFFFFFFFFFul };
		uint64_t sizeOfComponent{ 0xFFFFFFFFFFFFFFFFul };

		void (*constructFn)(void* dest, uint64_t numberOfElements) = nullptr; // placement-new elements, please make it noexcept
		void (*destructFn)(void* dest, uint64_t numberOfElements) = nullptr; // destruct elements, must be noexcept
		void (*copyFn)(const void* src, void* dest, uint64_t numberOfElements) = nullptr;
		void (*serializeFn)(const void* src, void* outBuffer) = nullptr;
		void (*deserializeFn)(void* dest, const void* inBuffer) = nullptr;
	};

	struct ComponentColumn {
		void* pComponentData{ nullptr };
		uint16_t componentHandle{};
		uint16_t stride{};
	};

	// Since Indices into the ComponentRegistry metadata are stored as handles,
	// ComponentRegistry must not be cleaned mid-session, Components must not be removed mid-Session
	class ComponentRegistry {
	public:
		// ComponentRegistry Owns MetaData, Changes to user owned metaData will not be Canon, lifetime doesn't matter.
		void registerComponent(const ComponentMetadata& metaData) {
			if (canAdd(metaData)) m_MetaData.push_back(metaData);
		}

		ComponentMetadata getMetadata(uint16_t handle) const {
			if (handle >= m_MetaData.size()) return ComponentMetadata();
			return m_MetaData[handle];
		}

		// TODO: Add Error Returning and Proper Checks, Optionals instead of Sentinel values
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
			else return static_cast<uint16_t>(m_MetaData[handle].sizeOfComponent);
		}
	private:
		bool canAdd(const ComponentMetadata& metaData) const {
			for (const auto& comp : m_MetaData)
				if (comp.componentTypeID == metaData.componentTypeID) {
					if (comp.sizeOfComponent != metaData.sizeOfComponent) throw std::runtime_error("ComponentRegistry Name Hash Collision.");
					else return false;
				}
			return true;
		}
	private:
		std::vector<ComponentMetadata> m_MetaData;
	};

	class Archetype {
	public:
		static std::unique_ptr<Archetype> create(const ComponentRegistry& metadataReg, 
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
				columns.emplace_back(nullptr, handle, metadataReg.getSizeOf(handle));
			}

			return std::unique_ptr<Archetype>(new Archetype(std::move(columns), metadataReg, sortedIDs, initialCapacity));
		}

		std::vector<uint64_t> getComponents() const{
			std::vector<uint64_t> comps{};
			comps.reserve(m_Components.size());
			for (const auto& comp : m_Components) comps.emplace_back(m_Registry.getTypeID(comp.componentHandle));
			return comps;
		}

		Entity addEntity() {
			ensureCapacity(m_EntityCount + 1);

			Entity id = EntityManager::create(this, m_EntityCount);
			m_Entities.push_back(id);

			for (auto& cc : m_Components) {
				const auto metadata = m_Registry.getMetadata(cc.componentHandle);
				metadata.constructFn(static_cast<uint8_t*>(cc.pComponentData) + (m_EntityCount * cc.stride), 1);
			}

			m_EntityCount++;
			return id;
		}

		~Archetype() noexcept {
			for (auto& cc : m_Components) {
				const auto meta = m_Registry.getMetadata(cc.componentHandle);
				if (cc.pComponentData != nullptr) {
					meta.destructFn(static_cast<uint8_t*>(cc.pComponentData), m_EntityCount);
					operator delete(cc.pComponentData);
				}
			}
		}
	private:
		// TODO: Memory Allocator so operator new doesn't throw :)
		Archetype(std::vector<ComponentColumn>&& components,
			const ComponentRegistry& registry,
			const std::vector<uint64_t>& componentIDs,
			uint64_t initialCapacity)
			: m_Components{std::move(components)}, m_ArchetypeID{getArchetypeID(componentIDs)},
			m_Registry{registry}, m_Capacity{initialCapacity}
		{
			for (auto& c : m_Components) {
				c.pComponentData = operator new(c.stride * m_Capacity);
			}
		}

		void ensureCapacity(uint64_t newCapacity) {
			if (newCapacity >= m_Capacity) {
				uint64_t updatedCapacity = static_cast<uint64_t>(m_Capacity * 1.5);
				for (auto& c : m_Components) {
					const auto metadata = m_Registry.getMetadata(c.componentHandle);
					void* newMem = operator new(c.stride * updatedCapacity);
					metadata.copyFn(c.pComponentData, newMem, m_EntityCount);
					metadata.destructFn(c.pComponentData, m_EntityCount);
					operator delete(c.pComponentData);
				}
				m_Capacity = updatedCapacity;
			}
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
		std::vector<Entity> m_Entities{};
		const uint64_t m_ArchetypeID;
		const ComponentRegistry& m_Registry;
		uint64_t m_Capacity{};
		uint64_t m_EntityCount{};

	};

}