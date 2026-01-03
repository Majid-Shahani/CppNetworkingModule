#pragma once

#include <cstdint>
#include <type_traits>
#include <span>

#include <Stubs/ECS.h>

namespace Carnival::ECS {

	enum class QueryPolicy : uint8_t {
		ReadOnly = 0,
		ReadWrite,

		RO = ReadOnly,
		RW = ReadWrite,
	};

	template<QueryPolicy P, ECSComponent C>
	struct InnerLocalIter {
		static constexpr bool writable = (P == QueryPolicy::ReadWrite);

		decltype(auto) operator*() const noexcept {
			if constexpr (writable)
				return *current;
			else
				return static_cast<const C&>(*current);
		}
		decltype(auto) operator->() const noexcept {
			if constexpr (writable)
				return current;
			else
				return static_cast<const C*>(current);
		}

		bool operator!=(const InnerLocalIter& other) const noexcept {
			return current != other.current; // && arch != other.arch;
		}
		bool operator==(const InnerLocalIter& other) const noexcept {
			return current == other.current; // && arch == other.arch;
		}

		InnerLocalIter& operator++() noexcept {
			++current;
			return *this;
		}

		bool done() const noexcept { return current == end; }

	private:
		C* current;
		C* end;
	};

	template<QueryPolicy P, ECSComponent C>
	struct InnerNetworkedIter {
		static constexpr bool writable = (P == QueryPolicy::ReadWrite);

		decltype(auto) operator*() const noexcept {
			if constexpr (writable) {
				if (!arch.testAndSetEntityDirty(static_cast<uint32_t>(index))); // Submit Replication
				return *current;
			}
			else
				return static_cast<const C&>(*current);
		}
		decltype(auto) operator->() const noexcept {
			if constexpr (writable) {
				if (!arch.testAndSetEntityDirty(static_cast<uint32_t>(index))); // Submit Replication
				return current;
			}
			else
				return static_cast<const C*>(current);
		}

		bool operator!=(const InnerNetworkedIter& other) const noexcept {
			return current != other.current; // && arch != other.arch;
		}
		bool operator==(const InnerNetworkedIter& other) const noexcept {
			return current == other.current;// && arch == other.arch;
		}

		InnerNetworkedIter& operator++() noexcept {
			++current;
			++index;
			return *this;
		}

		bool done() const noexcept { return current == end; }

	private:
		C* current;
		C* end;
		Archetype& arch;
		uint64_t index{};
	};

	template <QueryPolicy P, ECSComponent C>
	struct OuterIter {
		using LocalIter = InnerLocalIter<P, C>;
		using NetworkedIter = InnerNetworkedIter<P, C>;

		OuterIter(std::span<LocalIter> locals, std::span<NetworkedIter> networks)
			: localChunks{ locals }, networkedChunks{ networks }, currentChunk{ 0 }, onLocal{true}, done{false}
		{}

		decltype(auto) operator*() const noexcept {
			if (onLocal) return localChunks[currentChunk];
			else		 return networkedChunks[currentChunk];
		}

		OuterIter& operator++() noexcept {
			if (onLocal) {
				++localChunks[currentChunk];
				if (localChunks[currentChunk].done()) {
					++currentChunk;
					if (currentChunk >= localChunks.size()) {
						currentChunk = 0;
						onLocal = false;
					}
				}
			}
			else {
				++networkedChunks[currentChunk];
				if (networkedChunks[currentChunk].done()) {
					++currentChunk;
					if (currentChunk >= networkedChunks.size()) isDone = true;
				}
			}
			return *this;
		}

		bool done() const {
			return isDone;
		}

	private:
		std::span<LocalIter>		localChunks;
		std::span<NetworkedIter>	networkedChunks;
		uint64_t currentChunk{};
		bool onLocal{ true };
		bool isDone{ false };
		// 6 Bytes of Padding
	};

	class World {
	public:
		// TODO: Rule of 5
		Entity createEntity(std::span<uint64_t> components);
		void destroyEntity(Entity e);

		template<ECSComponent... Ts>
		bool registerComponent();
		
		template <ECSComponent... Ts>
		void addComponentToEntity(Entity e);
		
		template <ECSComponent... Ts>
		void removeComponentFromEntity(Entity e);

		template <QueryPolicy P, ECSComponent T>
		outerIter<P, T> query();

		void startUpdate();
		void endUpdate();

	private:
		ComponentRegistry		m_Registry;
		std::vector<Archetype>	m_onTickArchetypes;
		std::vector<Archetype>	m_onUpdateArchetypes;
		std::vector<Archetype>	m_localArchetypes;
	};
}