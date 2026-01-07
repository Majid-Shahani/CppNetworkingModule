#pragma once

#include <cstdint>
#include <memory>

#include <CNM/macros.h>
#include <ECS/Entity.h>
#include <ECS/Component.h>

namespace Carnival::ECS {
	struct ComponentColumn {
		ComponentMetadata metadata{};
		void* pComponentData{ nullptr };
	};

	class Archetype {
	public:
		// fnv1a 64-bit hash specifically for little-endian systems, not cross-compatible
		static uint64_t hashArchetypeID(std::span<const uint64_t> sortedCompIDs);
	public:
		static std::unique_ptr<Archetype> create(const ComponentRegistry& metadataReg,
			std::span<const uint64_t> sortedComponentIDs, uint64_t archetypeID, void* world, uint32_t initialCapacity = 5);

		std::vector<uint64_t>			getComponentIDs() const {
			std::vector<uint64_t> comps{};
			comps.reserve(m_Components.size());
			for (const auto& comp : m_Components) comps.emplace_back(comp.metadata.componentTypeID);
			return comps;
		}

		uint32_t						addEntity(Entity id);
		uint32_t						addEntity(Entity id, const Archetype& src, uint32_t srcIndex);

		std::pair<uint32_t, uint32_t>	removeEntity(Entity entity);
		std::pair<uint32_t, uint32_t>	removeEntityAt(uint32_t index);
		std::pair<uint32_t, uint32_t>	removeLastEntity();

		inline uint32_t					getEntityCount() const { return m_EntityCount; }
		inline Entity					getEntity(uint32_t index) const { return m_Entities[index]; }

		inline uint32_t					getComponentIndex(uint64_t cID) const {
			for (int i{}; i < m_Components.size(); i++) if (m_Components[i].metadata.componentTypeID == cID) return i;
			return UINT32_MAX;
		}
		inline void* getComponentData(uint64_t cID) {
			for (const auto& c : m_Components) if (c.metadata.componentTypeID == cID) return c.pComponentData;
			return nullptr;
		}
		inline void const* readComponentData(uint32_t index) const {
			if (index >= m_Components.size()) return nullptr;
			return m_Components[index].pComponentData;
		}
		inline void* writeComponentData(uint32_t index) {
			if (index >= m_Components.size()) return nullptr;
			return m_Components[index].pComponentData;
		}
		inline uint64_t					getID() const { return m_ArchetypeID; }

		~Archetype() noexcept;
	private:
		// TODO: Memory Allocator so operator new doesn't throw :)
		Archetype(std::vector<ComponentColumn>&& components,
			uint64_t archetypeID,
			void*	 world,
			uint32_t initialCapacity);

		void ensureCapacity(uint32_t newCapacity);
	private:
		std::vector<ComponentColumn> m_Components{};
		std::vector<Entity> m_Entities{};
		const uint64_t m_ArchetypeID;
		void* m_World;
		uint32_t m_Capacity{};
		uint32_t m_EntityCount{};
	};

	enum class NetworkFlags : uint8_t {
		LOCAL = 0,
		ON_TICK = 1,
		ON_UPDATE = 2,
	};

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