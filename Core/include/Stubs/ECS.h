#pragma once

#include <concepts>
#include <cstdint>
#include <vector>
#include <span>
#include <memory>
#include <stdexcept>
#include <utility>

#include <CNM/utils.h>
#include <CNM/macros.h>

namespace Carnival::ECS {
	class Archetype;
	using Entity = uint32_t;

	enum class NetworkFlags : uint8_t {
		LOCAL = 0,
		ON_TICK = 1,
		ON_UPDATE = 2,
	};

	struct OnTickNetworkComponent {
		uint32_t networkID{};

		static constexpr uint64_t ID{ utils::fnv1a64("OnTickNetworkComponent") };
		static void construct(void* dest, uint32_t count) noexcept {
			std::memset(dest, 0, sizeof(OnTickNetworkComponent) * count);
		}
		static void destruct(void* dest, uint32_t count) noexcept {
			std::memset(dest, 0, sizeof(OnTickNetworkComponent) * count);
		}
		static void copy(const void* src, void* dest, uint32_t count) {
			memcpy(dest, src, sizeof(OnTickNetworkComponent) * count);
		}
		static void serialize(const void* src, void* out, uint32_t count) {
			// static_cast<buffer*>(out)->put_uint32(static_cast<const OnTickNetworkComponent*>(src)->networkID);
		}
		static void deserialize(void* dest, const void* in, uint32_t count) {
			// static_cast<OnTickNetworkComponent*>(dest)->networkID = static_cast<const buffer*>(in)->read_uint32();
		}
	};
	struct OnUpdateNetworkComponent {
		uint32_t networkID{};
		bool dirty{ false };
		uint8_t padding[3]{};

		static constexpr uint64_t ID{ utils::fnv1a64("OnUpdateNetworkComponent") };
		static void construct(void* dest, uint32_t count) noexcept {
			std::memset(dest, 0, sizeof(OnUpdateNetworkComponent) * count);
		}
		static void destruct(void* dest, uint32_t count) noexcept {
			// Free Network IDs
			std::memset(dest, 0, sizeof(OnUpdateNetworkComponent) * count);
		}
		static void copy(const void* src, void* dest, uint32_t count) {
			memcpy(dest, src, sizeof(OnUpdateNetworkComponent) * count);
		}
		static void serialize(const void* src, void* out, uint32_t count) {
			// static_cast<buffer*>(out)->put_uint32(static_cast<const OnUpdateNetworkComponent*>(src)->networkID);
		}
		static void deserialize(void* dest, const void* in, uint32_t count) {
			// static_cast<OnUpdateNetworkComponent*>(dest)->networkID = static_cast<const buffer*>(in)->read_uint32();
		}
	};

	enum EntityStatus : uint32_t {
		DEAD		= 0,
		ALIVE		= 1 << 0,
		// Reserved
	};
	struct EntityEntry {
		Archetype* pArchetype;
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
		Entity create(Archetype* pArchetype, uint32_t index, EntityStatus status = EntityStatus::DEAD);
		
		const EntityEntry& get(Entity e);

		void updateEntity(Entity e, Archetype* pArchetype, uint32_t index, EntityStatus status);
		void updateEntityLocation(Entity e, Archetype* pArchetype, uint32_t index);

		void destroyEntity(Entity e);
		void destroyEntities(std::span<const Entity> e);
		void reset();
	private:
		std::vector<Entity> m_FreeIDs{};
		std::vector<EntityEntry> m_Entries{};
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
			std::span<const uint64_t> sortedComponentIDs, uint64_t archetypeID, uint32_t initialCapacity = 5);

		std::vector<uint64_t>			getComponentIDs() const;

		uint32_t						addEntity(Entity id);
		uint32_t						addEntity(Entity id, const Archetype& src, uint32_t srcIndex);

		std::pair<uint32_t, uint32_t>	removeEntity(Entity entity);
		std::pair<uint32_t, uint32_t>	removeEntityAt(uint32_t index);
		std::pair<uint32_t, uint32_t>	removeLastEntity();

		inline uint32_t					getEntityCount() const { return m_EntityCount; }
		inline Entity					getEntity(uint32_t index) const { return m_Entities[index]; }

		inline bool						testAndSetEntityDirty(uint32_t index) noexcept {
			CL_CORE_ASSERT(index < m_EntityCount, "Index out of bounds");
			auto comp = (static_cast<OnUpdateNetworkComponent*>(getComponentData(OnUpdateNetworkComponent::ID)) + index);
			bool res = comp->dirty;
			comp->dirty = true;
			return res;
		}
		inline void						clearEntityDirty(uint32_t index) {
			CL_CORE_ASSERT(index < m_EntityCount, "Index out of bounds");
			auto comp = (static_cast<OnUpdateNetworkComponent*>(getComponentData(OnUpdateNetworkComponent::ID)) + index);
			comp->dirty = false;
		}

		inline uint32_t					getComponentIndex(uint64_t cID) const {
			for (int i{}; i < m_Components.size(); i++) if (m_Components[i].metadata.componentTypeID == cID) return i;
			return UINT32_MAX;
		}
		inline void*					getComponentData(uint64_t cID){
			for (const auto& c : m_Components) if (c.metadata.componentTypeID == cID) return c.pComponentData;
			return nullptr;
		}
		inline void const*				readComponentData(uint32_t index) const { 
			if (index >= m_Components.size()) return nullptr;
			return m_Components[index].pComponentData;
		}
		inline void*					writeComponentData(uint32_t index) {
			if (index >= m_Components.size()) return nullptr;
			return m_Components[index].pComponentData;
		}
		inline uint64_t					getID() const { return m_ArchetypeID; }

		// fnv1a 64-bit hash specifically for little-endian systems, not cross-compatible
		static uint64_t hashArchetypeID(std::span<const uint64_t> sortedCompIDs);

		~Archetype() noexcept;
	private:
		// TODO: Memory Allocator so operator new doesn't throw :)
		Archetype(std::vector<ComponentColumn>&& components,
			uint64_t archetypeID,
			uint32_t initialCapacity);

		void ensureCapacity(uint32_t newCapacity);
		
	private:
		std::vector<ComponentColumn> m_Components{};
		std::vector<Entity> m_Entities{};
		const uint64_t m_ArchetypeID;
		uint32_t m_Capacity{};
		uint32_t m_EntityCount{};

	};

	struct ArchetypeRecord {
		ArchetypeRecord(const ComponentRegistry& metadataReg,
			std::span<const uint64_t> IDs, uint64_t ID, NetworkFlags flag, uint32_t initialCapacity = 5)
			: arch{ Archetype::create(metadataReg, IDs, ID, initialCapacity)}, flags{flag} {
		}
		//ArchetypeRecord() : arch{ nullptr }, flags{ NetworkFlags::LOCAL } {}
		// MOVE CONSTRUCTOR NEEDED!

		std::unique_ptr<Archetype> arch;
		NetworkFlags flags;
		// 7 bytes of padding
	};
}