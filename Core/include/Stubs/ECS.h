#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <span>

#include <CNM/utils.h>
#include <CNM/macros.h> // To be moved to translation file later

namespace Carnival::ECS {

	struct ComponentMetadata {
		uint64_t componentTypeID{ 0xFFFFFFFFFFFFFFFFul };
		uint64_t sizeOfComponent{ 0xFFFFFFFFFFFFFFFFul };

		void (*constructFn)(void* dest, uint64_t numberOfElements) noexcept = nullptr; // placement-new elements
		void (*destructFn)(void* dest, uint64_t numberOfElements) noexcept = nullptr; // destruct elements
		// Construct Before copy in case of moving around? Copy function could cast and field by field copy,
		// or memcpy if memory is packed properly. first case seems safer, but construction before copy becomes a requirement
		void (*copyFn)(const void* src, void* dest, uint64_t numberOfElements) = nullptr;
		void (*serializeFn)(const void* src, void* outBuffer) = nullptr;
		void (*deserializeFn)(void* dest, const void* inBuffer) = nullptr;
	};

	struct ComponentColumn {
		ComponentMetadata metadata{};
		void* pComponentData{ nullptr };
	};

	using Entity = uint32_t;

	enum EntityStatus : uint32_t {
		DEAD		= 0,
		ALIVE		= 1 << 0,
		NETWORKED	= 1 << 1,
		DIRTY		= 1 << 2,
		// Reserved
	};

	class Archetype;
	struct EntityEntry {
		Archetype* archetype;
		uint32_t index;
		EntityStatus status;
	};

	// Unused
	struct EntityData {
		uint64_t numberOfComponents{};
		uint16_t* sizeOfComponents{nullptr};
		uint8_t* data{nullptr};
	};

	// Not Thread safe, Change
	class EntityManager {
	public:
		static Entity create(Archetype* archetype, uint32_t index, EntityStatus status) {
			status = static_cast<EntityStatus>(status | ALIVE);
			if (!s_FreeIDs.empty()) {
				Entity id = s_FreeIDs[s_FreeIDs.size() - 1];
				s_FreeIDs.pop_back();
				s_Entries[id] = { archetype, index, status };
				return id;
			}
			s_Entries.push_back({ archetype, index, status });
			return s_NextID++;
		}
		
		static bool markDirty(Entity e) {
			if (e >= s_NextID) return false;
			if (s_Entries[e].status == DEAD
				|| !(s_Entries[e].status & NETWORKED) 
				|| (s_Entries[e].status & DIRTY)) 
				return false;
			s_Entries[e].status = static_cast<EntityStatus>(s_Entries[e].status | DIRTY);
			return true;
		}
		static void clearDirty(Entity e) {
			if (e >= s_NextID) return;
			s_Entries[e].status = static_cast<EntityStatus>(s_Entries[e].status & ~DIRTY);
		}

		static void updateEntity(Entity e, Archetype* archetype, uint32_t index, EntityStatus status) {
			if (e >= s_NextID) return;
			if (s_Entries[e].status == DEAD) {
				auto it = std::find(s_FreeIDs.begin(), s_FreeIDs.end(), e);
				if (it != s_FreeIDs.end()) {
					std::iter_swap(it, s_FreeIDs.end() - 1);
					s_FreeIDs.pop_back();
				}
			}
			status = static_cast<EntityStatus>(status | ALIVE);
			s_Entries[e] = { archetype, index, status };
		}
		static void updateEntityLocation(Entity e, Archetype* archetype, uint32_t index) {
			if (e >= s_NextID) return;
			s_Entries[e].archetype = archetype;
			s_Entries[e].index = index;
		}

		static void destroyEntity(Entity e) {
			if (e >= s_NextID) return;
			s_Entries[e] = { nullptr, 0, EntityStatus::DEAD };
			s_FreeIDs.push_back(e);
		}
		static void destroyEntities(std::span<const Entity> e) {
			for (const Entity entity : e) {
				if (entity >= s_NextID) continue;
				s_Entries[entity] = { nullptr, 0, EntityStatus::DEAD };
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

	// Since Indices into the ComponentRegistry metadata are stored as handles,
	// ComponentRegistry must not be cleaned mid-session, Components must not be removed mid-Session
	class ComponentRegistry {
	public:
		// ComponentRegistry Owns MetaData, Changes to user owned metaData will not be Canon, lifetime doesn't matter.
		void registerComponent(const ComponentMetadata& metaData) {
			if (canAdd(metaData)) m_MetaData.push_back(metaData);
		}

		ComponentMetadata getMetadataByHandle(const uint16_t handle) const {
			if (handle >= m_MetaData.size()) return ComponentMetadata{};
			return m_MetaData[handle];
		}

		ComponentMetadata getMetadataByID(const uint64_t ComponentID) const {
			for (const auto& meta : m_MetaData) {
				if (meta.componentTypeID == ComponentID) {
					ComponentMetadata res = meta;
					return res;
				}
			}
			return ComponentMetadata{};
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
			std::span<const uint64_t> componentIDs, uint32_t initialCapacity = 5) {
			
			// Copy Component ID Array
			std::vector<uint64_t> sortedIDs(componentIDs.begin(), componentIDs.end());

			// Sort ID Array canonically
			std::sort(sortedIDs.begin(), sortedIDs.end());
			
			// Check for duplicate components in Entity
			if (std::adjacent_find(sortedIDs.begin(), sortedIDs.end()) != sortedIDs.end()) return nullptr;

			// Build Component columns and validate componentIDs
			std::vector<ComponentColumn> columns;
			columns.reserve(sortedIDs.size());
			for (uint64_t compID : sortedIDs) {
				auto metadata = metadataReg.getMetadataByID(compID);
				if (metadata.componentTypeID == 0xFFFFFFFFFFFFFFFFul) return nullptr;
				columns.emplace_back(metadata, nullptr);
			}

			return std::unique_ptr<Archetype>(new Archetype(std::move(columns), sortedIDs, initialCapacity));
		}

		std::vector<uint64_t> getComponentIDs() const{
			std::vector<uint64_t> comps{};
			comps.reserve(m_Components.size());
			for (const auto& comp : m_Components) comps.emplace_back(comp.metadata.componentTypeID);
			return comps;
		}

		uint32_t addEntity(Entity id) {
			ensureCapacity(m_EntityCount + 1);

			m_Entities.push_back(id);
			for (auto& cc : m_Components) {
				cc.metadata.constructFn(static_cast<uint8_t*>(cc.pComponentData) + (m_EntityCount * cc.metadata.sizeOfComponent), 1);
			}
			return m_EntityCount++;
		}
		uint32_t addEntity(Entity id, const Archetype& src, uint32_t srcIndex) {
			ensureCapacity(m_EntityCount + 1);

			m_Entities.push_back(id);
			for (auto& dstC : m_Components) {
				bool isInSrc{ false };
				for (auto& srcC : src.m_Components) {
					if (dstC.metadata.componentTypeID == srcC.metadata.componentTypeID) {
						CL_CORE_ASSERT(dstC.metadata.sizeOfComponent == srcC.metadata.sizeOfComponent, "Size of Component mismatch across archetypes.\n");
						dstC.metadata.copyFn(static_cast<uint8_t*>(srcC.pComponentData) + (srcIndex * dstC.metadata.sizeOfComponent),
							static_cast<uint8_t*>(dstC.pComponentData) + (m_EntityCount * dstC.metadata.sizeOfComponent), 1);
						isInSrc = true;
						break;
					}
				}
				if (!isInSrc)
					dstC.metadata.constructFn(
						static_cast<uint8_t*>(dstC.pComponentData) + (m_EntityCount * dstC.metadata.sizeOfComponent),
						1);
			}
			return m_EntityCount++;
		}

		void removeEntity(Entity entity) {
			for (uint32_t i{}; i < m_EntityCount; i++) {
				if (m_Entities[i] == entity) {
					if (i == m_EntityCount - 1) {
						removeLastEntity();
						return;
					}
					else {
						removeEntityAt(i);
						return;
					}
				}
			}
		}
		void removeEntityAt(uint32_t index) {
			if (m_EntityCount == 0 || index >= m_EntityCount) return;
			// Swap and Destruct Components
			if (index == m_EntityCount - 1) {
				removeLastEntity();
				return;
			}

			for (auto& c : m_Components) {
				c.metadata.destructFn(static_cast<uint8_t*>(c.pComponentData) + (index * c.metadata.sizeOfComponent), 1);
				c.metadata.copyFn(static_cast<uint8_t*>(c.pComponentData) + (c.metadata.sizeOfComponent * (m_EntityCount - 1)),
					static_cast<uint8_t*>(c.pComponentData) + (index * c.metadata.sizeOfComponent),
					1);
				c.metadata.destructFn(static_cast<uint8_t*>(c.pComponentData) + ((m_EntityCount - 1) * c.metadata.sizeOfComponent), 1);
			}

			// Update Entity Registry
			m_Entities[index] = m_Entities[m_EntityCount - 1];
			m_Entities.pop_back();
			EntityManager::updateEntityLocation(m_Entities[index], this, index);

			m_EntityCount--;
		}
		void removeLastEntity() {
			for (auto& c : m_Components) {
				c.metadata.destructFn(static_cast<uint8_t*>(c.pComponentData) + ((m_EntityCount - 1) * c.metadata.sizeOfComponent), 1);
			}
			m_Entities.pop_back();
			m_EntityCount--;
		}

		// TODO: Get Entity Function

		~Archetype() noexcept {
			for (auto& cc : m_Components) {
				if (cc.pComponentData != nullptr) {
					cc.metadata.destructFn(static_cast<uint8_t*>(cc.pComponentData), m_EntityCount);
					operator delete(cc.pComponentData);
				}
			}
		}
	private:
		// TODO: Memory Allocator so operator new doesn't throw :)
		Archetype(std::vector<ComponentColumn>&& components,
			const std::vector<uint64_t>& componentIDs,
			uint32_t initialCapacity)
			: m_Components{std::move(components)}, m_ArchetypeID{getArchetypeID(componentIDs)}, m_Capacity{initialCapacity}
		{
			for (auto& c : m_Components) {
				c.pComponentData = operator new(c.metadata.sizeOfComponent * m_Capacity);
			}
		}

		void ensureCapacity(uint32_t newCapacity) {
			if (newCapacity > m_Capacity) {
				uint32_t updatedCapacity = static_cast<uint32_t>((m_Capacity + 1) * 1.5);
				for (auto& c : m_Components) {
					void* newMem = operator new(c.metadata.sizeOfComponent * updatedCapacity);
					c.metadata.copyFn(c.pComponentData, newMem, m_EntityCount);
					c.metadata.destructFn(c.pComponentData, m_EntityCount);
					operator delete(c.pComponentData);
					c.pComponentData = newMem;
				}
				m_Capacity = updatedCapacity;
			}
		}
		// fnv1a 64-bit hash specifically for little-endian systems, not cross-compatible
		static uint64_t getArchetypeID(std::span<const uint64_t> compIDs) {
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
		uint32_t m_Capacity{};
		uint32_t m_EntityCount{};

	};

}