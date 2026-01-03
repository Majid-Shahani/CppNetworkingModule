#pragma once

#include <concepts>
#include <cstdint>
#include <vector>
#include <span>
#include <stdexcept>
#include <memory>

namespace Carnival::ECS {
	using Entity = uint32_t;

	enum EntityStatus : uint32_t {
		DEAD		= 0,
		ALIVE		= 1 << 0,
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

		static void updateEntity(Entity e, Archetype* archetype, uint32_t index, EntityStatus status);
		static void updateEntityLocation(Entity e, Archetype* archetype, uint32_t index);

		static void destroyEntity(Entity e);
		static void destroyEntities(std::span<const Entity> e);
		static void reset();
	private:
		EntityManager() = default;
		static inline std::vector<Entity> s_FreeIDs{};
		static inline std::vector<EntityEntry> s_Entries{};
		static inline Entity s_NextID{};
	};

	struct ComponentMetadata {
		uint64_t componentTypeID{ 0xFFFFFFFFFFFFFFFFul };

		using ConstructFn	= void (*)(void* dest, uint32_t count) noexcept;
		using DestructFn	= void (*)(void* dest, uint32_t count) noexcept;
		using CopyFn		= void (*)(const void* src, void* dest, uint32_t count);
		using SerializeFn	= void (*)(const void* src, void* outBuffer, uint32_t count);
		using DeserializeFn = void (*)(void* dest, const void* inBuffer, uint32_t count);

		ConstructFn		constructFn		= nullptr; // placement-new elements
		DestructFn		destructFn		= nullptr; // destruct elements
		CopyFn			copyFn			= nullptr;
		SerializeFn		serializeFn		= nullptr;
		DeserializeFn	deserializeFn	= nullptr;

		uint32_t sizeOfComponent{ 0xFFFFFFFFu };
		uint32_t alignOfComponent{ 0xFFFFFFFFu };
	};

	struct ComponentColumn {
		ComponentMetadata metadata{};
		void* pComponentData{ nullptr };
	};

	// Highly Preferred to be packed and POD
	template<typename T>
	concept ECSComponent =
		requires { { T::ID } -> std::same_as<const uint64_t&>; }
		&& std::same_as<decltype(&T::construct),	ComponentMetadata::ConstructFn>
		&& std::same_as<decltype(&T::destruct),		ComponentMetadata::DestructFn>
		&& std::same_as<decltype(&T::copy),			ComponentMetadata::CopyFn>
		&& std::same_as<decltype(&T::serialize),	ComponentMetadata::SerializeFn>
		&& std::same_as<decltype(&T::deserialize),	ComponentMetadata::DeserializeFn>;

	// ComponentRegistry must not be cleaned mid-session, Components must not be removed mid-Session
	class ComponentRegistry {
	public:
		// TODO: RULE OF 5!
		ComponentRegistry() = default;

		template<ECSComponent T>
		void registerComponent() {
			ComponentMetadata meta{
				.componentTypeID	= T::ID,
				.constructFn		= &T::construct,
				.destructFn			= &T::destruct,
				.copyFn				= &T::copy,
				.serializeFn		= &T::serialize,
				.deserializeFn		= &T::deserialize,
				.sizeOfComponent	= sizeof(T),
				.alignOfComponent	= alignof(T),
			};
			registerComponent(meta);
		}

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
		// TODO: RULE OF 5!

		static std::unique_ptr<Archetype> create(const ComponentRegistry& metadataReg, 
			std::span<const uint64_t> componentIDs, uint32_t initialCapacity = 5);

		std::vector<uint64_t>	getComponentIDs() const;

		uint32_t				addEntity(Entity id);
		uint32_t				addEntity(Entity id, const Archetype& src, uint32_t srcIndex);

		void					removeEntity(Entity entity);
		void					removeEntityAt(uint32_t index);
		void					removeLastEntity();

		inline uint32_t			getEntityCount() const { return m_EntityCount; }
		inline Entity			getEntity(uint32_t index) const { return m_Entities[index]; }

		inline bool				testAndSetEntityDirty(uint32_t index) noexcept {
			//if (index >= m_Entities.size()) throw std::runtime_error("Index out of bounds.");
			return true;
		}
		inline void				clearEntityDirty(uint32_t index) {
			if (index >= m_Entities.size()) throw std::runtime_error("Index out of bounds.");
		}

		inline uint32_t			getComponentIndex(uint64_t cID) const {
			for (int i{}; i < m_Components.size(); i++) if (m_Components[i].metadata.componentTypeID == cID) return i;
			return UINT32_MAX;
		}
		inline void const*		readComponentData(uint32_t index) const { 
			if (index >= m_Components.size()) return nullptr;
			return m_Components[index].pComponentData;
		}
		inline void*			writeComponentData(uint32_t index) {
			if (index >= m_Components.size()) return nullptr;
			return m_Components[index].pComponentData;
		}

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