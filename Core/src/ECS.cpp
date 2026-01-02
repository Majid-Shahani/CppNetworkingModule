#include <src/CNMpch.hpp>

#include <Stubs/ECS.h>
#include <CNM/utils.h>
#include <CNM/macros.h>

namespace Carnival::ECS {

	// ================================================================================================== //
	// ==================================== Entity Manager ============================================= //
	// ================================================================================================ //

	Entity EntityManager::create(Archetype* archetype, uint32_t index, EntityStatus status) {
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

	const EntityEntry& EntityManager::get(Entity e) {
		CL_CORE_ASSERT(e < s_NextID, "Entity access for out of bounds entity requested");
		return s_Entries[e];
	}

	void EntityManager::updateEntity(Entity e, Archetype* archetype, uint32_t index, EntityStatus status) {
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
	void EntityManager::updateEntityLocation(Entity e, Archetype* archetype, uint32_t index) {
		if (e >= s_NextID) return;
		s_Entries[e].archetype = archetype;
		s_Entries[e].index = index;
	}

	void EntityManager::destroyEntity(Entity e) {
		if (e >= s_NextID) return;
		s_Entries[e] = { nullptr, 0, EntityStatus::DEAD };
		s_FreeIDs.push_back(e);
	}
	void EntityManager::destroyEntities(std::span<const Entity> e) {
		for (const Entity entity : e) {
			if (entity >= s_NextID) continue;
			s_Entries[entity] = { nullptr, 0, EntityStatus::DEAD };
			s_FreeIDs.push_back(entity);
		}
	}
	void EntityManager::reset() {
		s_FreeIDs.clear();
		s_Entries.clear();
		s_NextID = 1;
	}

	// ================================================================================================ //
	// ================================== Component Registry ========================================== //
	// ================================================================================================ //

	// ComponentRegistry Owns MetaData, Changes to user owned metaData will not be Canon, lifetime doesn't matter.
	void ComponentRegistry::registerComponent(const ComponentMetadata& metaData) {
		if (canAdd(metaData)) m_MetaData.push_back(metaData);
	}

	ComponentMetadata ComponentRegistry::getMetadataByHandle(const uint16_t handle) const {
		if (handle >= m_MetaData.size()) return ComponentMetadata{};
		return m_MetaData[handle];
	}
	ComponentMetadata ComponentRegistry::getMetadataByID(const uint64_t ComponentID) const {
		for (const auto& meta : m_MetaData) {
			if (meta.componentTypeID == ComponentID) {
				ComponentMetadata res = meta;
				return res;
			}
		}
		return ComponentMetadata{};
	}

	// TODO: Add Error Returning and Proper Checks, Optionals instead of Sentinel values
	uint16_t ComponentRegistry::getComponentHandle(uint64_t componentTypeID) const {
		uint16_t res{};
		for (; res < m_MetaData.size(); res++) if (m_MetaData[res].componentTypeID == componentTypeID) break;
		return (res == m_MetaData.size() ? 0xFFFF : res);
	}
	uint64_t ComponentRegistry::getTypeID(uint16_t handle) const {
		if (handle >= m_MetaData.size()) return 0xFFFFFFFFFFFFul;
		else return m_MetaData[handle].componentTypeID;
	}
	uint16_t ComponentRegistry::getSizeOf(uint16_t handle) const {
		if (handle >= m_MetaData.size()) return 0xFFFF;
		else return static_cast<uint16_t>(m_MetaData[handle].sizeOfComponent);
	}

	bool ComponentRegistry::canAdd(const ComponentMetadata& metaData) const {
		for (const auto& comp : m_MetaData)
			if (comp.componentTypeID == metaData.componentTypeID) {
				if (comp.sizeOfComponent != metaData.sizeOfComponent) throw std::runtime_error("ComponentRegistry Name Hash Collision.");
				else return false;
			}
		return true;
	}

	// ================================================================================================ //
	// ======================================= Archetype ============================================== //
	// ================================================================================================ //

	
	std::unique_ptr<Archetype> Archetype::create(
		const ComponentRegistry& metadataReg,
		std::span<const uint64_t> componentIDs,
		uint32_t initialCapacity) {

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

	std::vector<uint64_t> Archetype::getComponentIDs() const {
		std::vector<uint64_t> comps{};
		comps.reserve(m_Components.size());
		for (const auto& comp : m_Components) comps.emplace_back(comp.metadata.componentTypeID);
		return comps;
	}

	uint32_t Archetype::addEntity(Entity id) {
		ensureCapacity(m_EntityCount + 1);

		m_Entities.push_back(id);
		for (auto& cc : m_Components) {
			cc.metadata.constructFn(static_cast<uint8_t*>(cc.pComponentData) + (m_EntityCount * cc.metadata.sizeOfComponent), 1);
		}
		return m_EntityCount++;
	}
	uint32_t Archetype::addEntity(Entity id, const Archetype& src, uint32_t srcIndex) {
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

	void Archetype::removeEntity(Entity entity) {
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
	// Possibly Destructors should not be called, if moved / copied entity will be come invalid after destruction.
	void Archetype::removeEntityAt(uint32_t index) {
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
	void Archetype::removeLastEntity() {
		for (auto& c : m_Components) {
			c.metadata.destructFn(static_cast<uint8_t*>(c.pComponentData) + ((m_EntityCount - 1) * c.metadata.sizeOfComponent), 1);
		}
		m_Entities.pop_back();
		m_EntityCount--;
	}

	Archetype::~Archetype() noexcept {
		for (auto& cc : m_Components) {
			if (cc.pComponentData != nullptr) {
				cc.metadata.destructFn(static_cast<uint8_t*>(cc.pComponentData), m_EntityCount);
				operator delete(cc.pComponentData, std::align_val_t(cc.metadata.alignOfComponent));
			}
		}
	}
	Archetype::Archetype(std::vector<ComponentColumn>&& components,
		const std::vector<uint64_t>& componentIDs,
		uint32_t initialCapacity)
		: m_Components{ std::move(components) }, m_ArchetypeID{ getArchetypeID(componentIDs) }, m_Capacity{ initialCapacity }
	{
		m_DirtyFlags.reserve(m_Components.size());
		for (int i{}; i < m_Components.size(); i++) {
			m_DirtyFlags.emplace_back(false);
		}
		for (auto& c : m_Components) {
			c.pComponentData = operator new(c.metadata.sizeOfComponent * m_Capacity, std::align_val_t(c.metadata.alignOfComponent));
		}
	}

	void Archetype::ensureCapacity(uint32_t newCapacity) {
		if (newCapacity > m_Capacity) {
			uint32_t updatedCapacity = static_cast<uint32_t>((m_Capacity + 1) * 1.5);
			for (auto& c : m_Components) {
				void* newMem = operator new(c.metadata.sizeOfComponent * updatedCapacity, std::align_val_t(c.metadata.alignOfComponent));
				c.metadata.copyFn(c.pComponentData, newMem, m_EntityCount);
				c.metadata.destructFn(c.pComponentData, m_EntityCount);
				operator delete(c.pComponentData, std::align_val_t(c.metadata.alignOfComponent));
				c.pComponentData = newMem;
			}
			m_Capacity = updatedCapacity;
		}
	}

	// fnv1a 64-bit hash specifically for little-endian systems, not cross-compatible
	uint64_t Archetype::getArchetypeID(std::span<const uint64_t> compIDs) {
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

}