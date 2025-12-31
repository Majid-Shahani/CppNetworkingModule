#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <memory>

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
		static Entity create(Archetype* archetype, uint32_t index, EntityStatus status);
		
		static const EntityEntry& get(Entity e);

		static bool markDirty(Entity e);
		static void clearDirty(Entity e);

		static void updateEntity(Entity e, Archetype* archetype, uint32_t index, EntityStatus status);
		static void updateEntityLocation(Entity e, Archetype* archetype, uint32_t index);

		static void destroyEntity(Entity e);
		static void destroyEntities(std::span<const Entity> e);
		static void reset();
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
		void registerComponent(const ComponentMetadata& metaData);

		ComponentMetadata getMetadataByHandle(const uint16_t handle) const;

		ComponentMetadata getMetadataByID(const uint64_t ComponentID) const;
		// TODO: Add Error Returning and Proper Checks, Optionals instead of Sentinel values
		uint16_t getComponentHandle(uint64_t componentTypeID) const;

		uint64_t getTypeID(uint16_t handle) const;
		uint16_t getSizeOf(uint16_t handle) const;
	private:
		bool canAdd(const ComponentMetadata& metaData) const;
	private:
		std::vector<ComponentMetadata> m_MetaData;
	};

	class Archetype {
	public:
		static std::unique_ptr<Archetype> create(const ComponentRegistry& metadataReg, 
			std::span<const uint64_t> componentIDs, uint32_t initialCapacity = 5);

		std::vector<uint64_t> getComponentIDs() const;

		uint32_t addEntity(Entity id);
		uint32_t addEntity(Entity id, const Archetype& src, uint32_t srcIndex);

		void removeEntity(Entity entity);
		void removeEntityAt(uint32_t index);
		void removeLastEntity();

		// TODO: Get Entity Function

		~Archetype() noexcept;
	private:
		// TODO: Memory Allocator so operator new doesn't throw :)
		Archetype(std::vector<ComponentColumn>&& components,
			const std::vector<uint64_t>& componentIDs,
			uint32_t initialCapacity);

		void ensureCapacity(uint32_t newCapacity);
		// fnv1a 64-bit hash specifically for little-endian systems, not cross-compatible
		static uint64_t getArchetypeID(std::span<const uint64_t> compIDs);
	private:
		std::vector<ComponentColumn> m_Components{};
		std::vector<Entity> m_Entities{};
		const uint64_t m_ArchetypeID;
		uint32_t m_Capacity{};
		uint32_t m_EntityCount{};

	};

}