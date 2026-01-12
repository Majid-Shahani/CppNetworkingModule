#pragma once

#include <cstdint>

#include <CNM/utils.h>
#include <CNM/Buffer.h>

namespace Carnival::ECS {

	// could later have a list of archetypes that have this with component index, for easier queries.
	struct ComponentMetadata {
		uint64_t componentTypeID{ 0xFFFFFFFFFFFFFFFFul };

		using ConstructFn = void (*)(void* dest, void* world, Entity e) noexcept;
		using DestructFn = void (*)(void* dest, void* world, Entity e) noexcept;
		using CopyFn = void (*)(const void* src, void* dest, uint32_t count);
		using SerializeFn = void (*)(const void* src, MessageBuffer& outbuffer, uint32_t count);
		using DeserializeFn = void (*)(void* dest, const MessageBuffer& inBuffer, uint32_t count);

		ConstructFn		constructFn = nullptr; // placement-new elements
		DestructFn		destructFn = nullptr; // destruct elements
		CopyFn			copyFn = nullptr;
		SerializeFn		serializeFn = nullptr;
		DeserializeFn	deserializeFn = nullptr;

		uint32_t sizeOfComponent{ 0xFFFFFFFFu };
		uint32_t alignOfComponent{ 0xFFFFFFFFu };
	};

	// Highly Preferred to be packed and POD
	template<typename T>
	concept ECSComponent =
		requires { { T::ID } -> std::same_as<const uint64_t&>; }
		&& std::same_as<decltype(&T::construct), ComponentMetadata::ConstructFn>
		&& std::same_as<decltype(&T::destruct), ComponentMetadata::DestructFn>
		&& std::same_as<decltype(&T::copy), ComponentMetadata::CopyFn>
		&& std::same_as<decltype(&T::serialize), ComponentMetadata::SerializeFn>
		&& std::same_as<decltype(&T::deserialize), ComponentMetadata::DeserializeFn>;

	// ComponentRegistry must not be cleaned mid-session, Components must not be removed mid-Session
	class ComponentRegistry {
	public:
		// TODO: RULE OF 5!
		ComponentRegistry() = default;

		template<ECSComponent T>
		void registerComponent() {
			ComponentMetadata meta{
				.componentTypeID = T::ID,
				.constructFn = &T::construct,
				.destructFn = &T::destruct,
				.copyFn = &T::copy,
				.serializeFn = &T::serialize,
				.deserializeFn = &T::deserialize,
				.sizeOfComponent = sizeof(T),
				.alignOfComponent = alignof(T),
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

	struct OnUpdateNetworkComponent {
		uint64_t networkID{};
		uint64_t version{};
		bool dirty{};

		static constexpr uint64_t ID{ utils::fnv1a64("OnUpdateNetworkComponent") };
		static void construct(void* dest, void* world, Entity e) noexcept {
		}
		static void destruct(void* dest, void* world, Entity e) noexcept {
		}
		static void copy(const void* src, void* dest, uint32_t count) {
			memcpy(dest, src, sizeof(OnUpdateNetworkComponent) * count);
		}
		static void serialize(const void* src, MessageBuffer& outbuffer, uint32_t count) {

		}
		static void deserialize(void* dest, const MessageBuffer& inBuffer, uint32_t count) {
			// need index or handled internally
		}
	};
	struct OnTickNetworkComponent {
		uint64_t networkID{};

		static constexpr uint64_t ID{ utils::fnv1a64("OnTickNetworkComponent") };
		static void construct(void* dest, void* world, Entity e) noexcept {
			std::memset(dest, 0, sizeof(OnTickNetworkComponent));
		}
		static void destruct(void* dest, void* world, Entity e) noexcept {
		}
		static void copy(const void* src, void* dest, uint32_t count) {
			memcpy(dest, src, sizeof(OnTickNetworkComponent) * count);
		}
		static void serialize(const void* src, MessageBuffer& outbuffer, uint32_t count) {
		}
		static void deserialize(void* dest, const MessageBuffer& inBuffer, uint32_t count) {
			;
		}
	};

}