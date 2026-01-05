#include <src/CNMpch.hpp>

#include <Stubs/ECS.h>
#include <CNM/utils.h>
#include <CNM/macros.h>

namespace Carnival::ECS {

	// ================================================================================================== //
	// ==================================== Entity Manager ============================================= //
	// ================================================================================================ //

	Entity EntityManager::create(Archetype* pArchetype, uint32_t index, EntityStatus status) {
		status = static_cast<EntityStatus>(status | ALIVE);
		if (!m_FreeIDs.empty()) {
			Entity id = m_FreeIDs[m_FreeIDs.size() - 1];
			m_FreeIDs.pop_back();
			m_Entries[id] = { pArchetype, index, status };
			return id;
		}
		m_Entries.push_back({ pArchetype, index, status });
		return static_cast<Entity>(m_Entries.size() - 1);
	}

	const EntityEntry& EntityManager::get(Entity e) {
		CL_CORE_ASSERT(e < m_Entries.size(), "Entity access for out of bounds entity requested");
		CL_CORE_ASSERT(m_Entries[e].status & ALIVE, "get called for dead entity");
		return m_Entries[e];
	}

	void EntityManager::updateEntity(Entity e, Archetype* pArchetype, uint32_t index, EntityStatus status) {
		CL_CORE_ASSERT(e < m_Entries.size(), "Entity access for out of bounds entity requested");
		CL_CORE_ASSERT(m_Entries[e].status & ALIVE, "Update called for dead entity");
		m_Entries[e] = { pArchetype, index, status };
	}
	void EntityManager::updateEntityLocation(Entity e, Archetype* pArchetype, uint32_t index) {
		CL_CORE_ASSERT(e < m_Entries.size(), "Entity access for out of bounds entity requested");
		CL_CORE_ASSERT(m_Entries[e].status & ALIVE, "Update Location called for dead entity");
		m_Entries[e].pArchetype = pArchetype;
		m_Entries[e].index = index;
	}

	void EntityManager::destroyEntity(Entity e) {
		CL_CORE_ASSERT(e < m_Entries.size(), "Entity access for out of bounds entity requested");
		CL_CORE_ASSERT(m_Entries[e].status & ALIVE, "Destroy called for dead entity");
		m_Entries[e] = { 0, 0, EntityStatus::DEAD };
		m_FreeIDs.push_back(e);
	}
	void EntityManager::destroyEntities(std::span<const Entity> e) {
		for (const Entity entity : e) {
			CL_CORE_ASSERT(entity < m_Entries.size(), "Entity access for out of bounds entity requested");
			CL_CORE_ASSERT(m_Entries[entity].status & ALIVE, "Destroy called for dead entity");
			m_Entries[entity] = { 0, 0, EntityStatus::DEAD };
			m_FreeIDs.push_back(entity);
		}
	}
	void EntityManager::reset() {
		m_FreeIDs.clear();
		m_Entries.clear();
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

	
	std::unique_ptr<Archetype> Archetype::create(const ComponentRegistry& metadataReg,
		std::span<const uint64_t> sortedComponentIDs, uint64_t pArchetype, uint32_t initialCapacity) {
		// Build Component columns and validate componentIDs
		std::vector<ComponentColumn> columns;
		columns.reserve(sortedComponentIDs.size());
		for (uint64_t compID : sortedComponentIDs) {
			auto metadata = metadataReg.getMetadataByID(compID);
			if (metadata.componentTypeID == 0xFFFFFFFFFFFFFFFFul) return nullptr;
			columns.emplace_back(metadata, nullptr);
		}

		return std::unique_ptr<Archetype>(new Archetype(std::move(columns), pArchetype, initialCapacity));
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

	std::pair<uint32_t, uint32_t> Archetype::removeEntity(Entity entity) {
		for (uint32_t i{}; i < m_EntityCount; i++) {
			if (m_Entities[i] == entity) return removeEntityAt(i);
		}
		return { entity, UINT32_MAX };
	}
	// Possibly Destructors should not be called, if moved / copied entity will be come invalid after destruction.
	// RETURNS INDEX OF entity to be updated in entity manager to the index passed in
	std::pair<uint32_t, uint32_t> Archetype::removeEntityAt(uint32_t index) {
		CL_CORE_ASSERT(index < m_EntityCount, "remove called on out of bounds index");
		// Swap and Destruct Components
		if (index == m_EntityCount - 1) return removeLastEntity();

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
		--m_EntityCount;
		return { m_Entities[index], index };
	}
	std::pair<uint32_t, uint32_t> Archetype::removeLastEntity() {
		for (auto& c : m_Components) {
			c.metadata.destructFn(static_cast<uint8_t*>(c.pComponentData) + ((m_EntityCount - 1) * c.metadata.sizeOfComponent), 1);
		}
		Entity e = m_Entities[m_EntityCount];
		m_Entities.pop_back();
		m_EntityCount--;
		return { e, UINT32_MAX };
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
		uint64_t pArchetype,
		uint32_t initialCapacity)
		: m_Components{ std::move(components) }, m_ArchetypeID{ pArchetype }, m_Capacity{ initialCapacity }
	{
		for (auto& c : m_Components) {
			c.pComponentData = operator new(static_cast<uint64_t>(c.metadata.sizeOfComponent) * m_Capacity, std::align_val_t(c.metadata.alignOfComponent));
		}
	}

	void Archetype::ensureCapacity(uint32_t newCapacity) {
		if (newCapacity > m_Capacity) {
			uint32_t updatedCapacity = static_cast<uint32_t>((m_Capacity + 1) * 1.5);
			for (auto& c : m_Components) {
				void* newMem = operator new(static_cast<uint64_t>(c.metadata.sizeOfComponent) * updatedCapacity, std::align_val_t(c.metadata.alignOfComponent));
				c.metadata.copyFn(c.pComponentData, newMem, m_EntityCount);
				c.metadata.destructFn(c.pComponentData, m_EntityCount);
				operator delete(c.pComponentData, std::align_val_t(c.metadata.alignOfComponent));
				c.pComponentData = newMem;
			}
			m_Capacity = updatedCapacity;
		}
	}

	// fnv1a 64-bit hash specifically for little-endian systems, not cross-compatible
	uint64_t Archetype::hashArchetypeID(std::span<const uint64_t> compIDs) {
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