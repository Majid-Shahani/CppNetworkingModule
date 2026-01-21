#pragma once

#include <cstdint>
#include <memory>

#include <CNM/macros.h>
#include <CNM/Buffer.h>
#include <ECS/Entity.h>
#include <ECS/Component.h>

namespace Carnival::ECS {
	struct ComponentColumn { // Storage for one Component Type
		ComponentMetadata metadata{};
		void* pComponentData{ nullptr };
	};

	// Dense Archetype Storage, SoA
	class Archetype {
	public:
		// fnv1a 64-bit hash specifically for little-endian systems, not cross-compatible
		// Hash Component IDs
		static uint64_t hashArchetypeID(std::span<const uint64_t> sortedCompIDs) noexcept;
	public:
		// Create Archetype from Component Set
		static std::unique_ptr<Archetype> create(const ComponentRegistry& metadataReg,
			std::span<const uint64_t> sortedComponentIDs, uint64_t archetypeID, void* world, uint32_t initialCapacity = 5);

		std::vector<uint64_t>	getComponentIDs() const {
			std::vector<uint64_t> comps{};
			comps.reserve(m_Components.size());
			for (const auto& comp : m_Components) comps.emplace_back(comp.metadata.componentTypeID);
			return comps;
		}

		uint32_t				addEntity(Entity id);
		// Move From Another Archetype
		uint32_t				addEntity(Entity id, const Archetype& src, uint32_t srcIndex);

		// Remove Entity, Return Swap Indices
		// returns [movedEntity, newIndex] of entity to be updated in entity manager to the index passed in
		std::pair<uint32_t, uint32_t>	removeEntity(Entity entity) noexcept;
		std::pair<uint32_t, uint32_t>	removeEntityAt(uint32_t index) noexcept;
		std::pair<uint32_t, uint32_t>	removeLastEntity() noexcept;
		
		void			serializeEntity(Entity e, MessageBuffer& staging) const;
		void			serializeIndex(uint32_t index, MessageBuffer& staging) const;
		void			serializeArchetype(MessageBuffer& buff) const;

		inline uint32_t	getEntityCount() const noexcept { return m_EntityCount; }
		inline Entity	getEntity(uint32_t index) const noexcept { return m_Entities[index]; }
		inline Entity*	getEntities() noexcept { return m_Entities.data(); }

		// Linear Lookup
		inline uint32_t	getComponentIndex(uint64_t cID) const noexcept {
			for (int i{}; i < m_Components.size(); i++) if (m_Components[i].metadata.componentTypeID == cID) return i;
			return UINT32_MAX;
		}
		// Raw ColumnData pointer
		inline void*	getComponentData(uint64_t cID) noexcept {
			for (const auto& c : m_Components) if (c.metadata.componentTypeID == cID) return c.pComponentData;
			return nullptr;
		}
		
		inline uint64_t	getID() const noexcept { return m_ArchetypeID; }

		~Archetype() noexcept;
	private:
		// TODO: Memory Allocator so operator new doesn't throw :)
		// Internal construction only
		Archetype(std::vector<ComponentColumn>&& components,
			uint64_t archetypeID,
			void*	 world,
			uint32_t initialCapacity);

		void ensureCapacity(uint32_t newCapacity);
	private:
		std::vector<ComponentColumn> m_Components{};
		std::vector<Entity> m_Entities{}; // Entity List
		const uint64_t m_ArchetypeID;
		[[maybe_unused]]void* m_World;
		uint32_t m_Capacity{};
		uint32_t m_EntityCount{};
	};

	// Replication Mode
	enum class NetworkFlags : uint8_t {
		LOCAL = 0,
		ON_TICK = 1,
		ON_UPDATE = 2,
	};
	// Archetype and its replication metadata
	struct ArchetypeRecord {
		ArchetypeRecord(const ComponentRegistry& metadataReg,
			std::span<const uint64_t> IDs, 
			uint64_t archID,
			void* pWorld,
			NetworkFlags flag, 
			uint32_t initialCapacity = 5)
			: arch{ Archetype::create(metadataReg, IDs, archID, pWorld, initialCapacity) }, flags{ flag } {
		}
		
		ArchetypeRecord(ArchetypeRecord&& other) noexcept 
			: arch{ other.arch.release() }, flags{other.flags} {}
		ArchetypeRecord& operator=(ArchetypeRecord&& other) noexcept
		{
			arch.reset(other.arch.release());
			flags = other.flags;
		}

		ArchetypeRecord(const ArchetypeRecord&) = delete;
		ArchetypeRecord& operator=(const ArchetypeRecord&) = delete;

		~ArchetypeRecord() = default;

		std::unique_ptr<Archetype> arch;
		NetworkFlags flags;
		// 7 bytes of padding
	};
}