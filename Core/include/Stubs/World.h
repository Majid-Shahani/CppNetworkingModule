#pragma once

#include <cstdint>
#include <type_traits>
#include <span>
#include <unordered_map>

#include <Stubs/ECS.h>
#include <CNM/macros.h>

namespace Carnival::ECS {

	enum class QueryPolicy : uint8_t {
		ReadOnly = 0,
		ReadWrite,

		RO = ReadOnly,
		RW = ReadWrite,
	};

	class World {
	private:
		template<QueryPolicy P, ECSComponent C>
		struct InnerLocalIter {
			InnerLocalIter(C* base, C* end) : current{ base }, end{ end } {}

			static constexpr bool writable = (P == QueryPolicy::ReadWrite);
			
			const C& read() const noexcept {
				return *current;
			}
			C& write() noexcept requires(writable) {
				return *current;
			}

			decltype(auto) operator*() const noexcept {
				CL_CORE_ASSERT(current < end, "Accessing Iterator On End");
				if constexpr (writable)
					return *current;
				else
					return static_cast<const C&>(*current);
			}
			decltype(auto) operator->() const noexcept {
				CL_CORE_ASSERT(current < end, "Accessing Iterator On End");
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
				CL_CORE_ASSRT(current < end, "Incrementing End Iterator.");
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
			InnerNetworkedIter(C* base, C* end, Archetype& archetype)
				: current{ base }, end{ end }, arch{ archetype }, index{ arch.getEntityCount() - (end - current) } {}

			static constexpr bool writable = (P == QueryPolicy::ReadWrite);

			const C& read() const noexcept {
				return *current;
			}
			C& write() noexcept requires(writable) {
				if (!arch.testAndSetEntityDirty(static_cast<uint32_t>(index))); // Submit Replication
				return *current;
			}

			decltype(auto) operator*() const noexcept {
				CL_CORE_ASSERT(current < end, "Accessing Iterator On End");
				if constexpr (writable) {
					if (!arch.testAndSetEntityDirty(static_cast<uint32_t>(index))); // Submit Replication
					return *current;
				}
				else
					return static_cast<const C&>(*current);
			}
			decltype(auto) operator->() const noexcept {
				CL_CORE_ASSERT(current < end, "Accessing Iterator On End");
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
				CL_CORE_ASSERT(current < end, "Incrementing End Iterator");
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

	public:
		template <QueryPolicy P, ECSComponent C>
		struct OuterIter {
			using LocalIter = InnerLocalIter<P, C>;
			using NetworkedIter = InnerNetworkedIter<P, C>;
			static constexpr bool writable = (P == QueryPolicy::ReadWrite);


			OuterIter(std::span<LocalIter> locals, 
				std::span<NetworkedIter> networks,
				uint64_t chunk = 0,
				bool local = true,
				bool doneFlag = false)
				: localChunks{ locals }, networkedChunks{ networks }, currentChunk{ chunk }, onLocal{ local }, isDone{ doneFlag }
			{
			}

			decltype(auto) operator*() const noexcept {
				CL_CORE_ASSERT(!isDone, "Accessing Iterator On End");
				if (onLocal) return *localChunks[currentChunk];
				else		 return *networkedChunks[currentChunk];
			}
			decltype(auto) operator->() const noexcept {
				CL_CORE_ASSERT(!isDone, "Accessing Iterator On End");
				if (onLocal) return localChunks[currentChunk].operator->();
				else		 return networkedChunks[currentChunk].operator->();
			}

			decltype(auto) read() const noexcept {
				CL_CORE_ASSERT(!isDone, "Accessing Iterator On End");
				if (onLocal) return localChunks[currentChunk].read();
				else		 return networkedChunks[currentChunk].read();
			}
			decltype(auto) write() noexcept requires(writable) {
				CL_CORE_ASSERT(!isDone, "Accessing Iterator On End");
				if (onLocal) return localChunks[currentChunk].write();
				else return networkedChunks[currentChunk].write();
			}

			OuterIter& operator++() noexcept {
				CL_CORE_ASSERT(!isDone, "Incrementing End Iterator.");
				if (onLocal) {
					++localChunks[currentChunk];
					if (localChunks[currentChunk].done()) {
						++currentChunk;
						if (currentChunk >= localChunks.size()) {
							currentChunk = 0;
							onLocal = false;
							if (networkedChunks.size() == 0) isDone = true;
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
			
			bool operator!=(const OuterIter& other) const noexcept {
				if (isDone && other.done()) return false;
				else if (!isDone && !other.done()) {
					if (onLocal)
						return localChunks[currentChunk] != other.localChunks[other.currentChunk];
					else
						return networkedChunks[currentChunk] != other.networkedChunks[other.currentChunk];
				}
				else
					return true;
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

		template <QueryPolicy P, ECSComponent C>
		struct ComponentRange {
			using Iterator = OuterIter<P, C>;

			ComponentRange(std::vector<InnerLocalIter<P, C>>&& local, std::vector<InnerNetworkedIter<P, C>>&& networks)
				: locals{ std::move(local) }, networkeds{ std::move(networks) } {}

			Iterator begin() { 
				return Iterator{ locals, networkeds, 0, !(locals.size() == 0), (locals.size() == 0 && networkeds.size() == 0)}; 
			}
			Iterator end() { return Iterator{ locals, networkeds, UINT64_MAX, false, true };	}
		private:
			std::vector<InnerLocalIter<P, C>> locals;
			std::vector<InnerNetworkedIter<P, C>> networkeds;
		};

	public:
		// TODO: Rule of 5
		Entity createEntity(std::vector<uint64_t> components, NetworkFlags flag = NetworkFlags::LOCAL);
		void destroyEntity(Entity e);

		template<ECSComponent... Ts>
		void registerComponents() {	(m_Registry.registerComponent<Ts>(), ...); }
		
		template <ECSComponent... Ts>
		void addComponentsToEntity(Entity e);
		
		template <ECSComponent... Ts>
		void removeComponentsFromEntity(Entity e);

		template <QueryPolicy P, ECSComponent T>
		ComponentRange<P, T> query();

		void startUpdate();
		void endUpdate();
	private:
		void addCompImpl(Entity e, uint64_t compID);
		void removeCompImpl(Entity e, uint64_t compID);
		void updateEntity(Entity e, uint64_t archID, uint32_t index);
	private:
		EntityManager m_EntityManager;
		std::unordered_map<uint64_t, ArchetypeRecord> m_Archetypes;
		ComponentRegistry m_Registry;
	};

}