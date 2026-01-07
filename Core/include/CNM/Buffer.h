#pragma once

#include <new>
#include <atomic>
#include <cstdint>

namespace Carnival {
	// Serialized ring buffer :
	// Drop is fine, write index is auth, if read > write, that's capacity, return false if cant write
	// if write > read, capacity = size - write.
	// packet boundaries, recrod say size and type, it should be complete,
	// write flag will be checked, if true, read will add all to the packet.
	// packet send function cannot wrap, data must be contiguous. 
	// if write flag is false, and read index reaches write index, reset both to 0
	// sound a lot like locks.



	// Replication Buffer :
	// One big array, size = configNum(1024) * sizeof(ReplicationRecord)
	// MPSC, Atomic Write Index. Non-Atomic read index. writes and reads will be done in phases.
	// Writer Reserves a spot, by moving index forward. if tmp index > capacity, return false 
	// or throw error. once write phase ends, read phase begins from first index. serialization is done
	// once read phase ends, both indices are set to 0. buffer is reset.

	// Currently : Preliminary MPMC, could be simpler and more efficient.

	// Size has to be power of 2
	template<uint32_t _size>
	class ReplicationBuffer {
	public:
		ReplicationBuffer() {
			static_assert(std::atomic<uint64_t>::is_always_lock_free, "64-bit uint is not lock free!");
			static_assert((_size & (_size - 1)) == 0, "Size must be a power of 2");
			data = new std::atomic<uint64_t>[_size]();
		}
		~ReplicationBuffer() {
			delete[] data;
		}

		bool unqueue(uint32_t entityID) noexcept { return push(entityID); }
		bool push(uint32_t eID) noexcept {
			while (true) {
				uint32_t idx = writeIndex.load(std::memory_order::relaxed);
				uint64_t val = (data + (idx & (_size - 1)))->load(std::memory_order::relaxed);
				uint32_t seq = getSeq(val);

				if (seq == static_cast<uint32_t>(idx << 1)) {
					uint64_t entryData{ getPack(seq | 1u, eID) };
					if ((data + (idx & (_size - 1)))->compare_exchange_strong(val, entryData,
						std::memory_order::release, std::memory_order::relaxed)) {
						writeIndex.compare_exchange_strong(idx, idx + 1, std::memory_order::release, std::memory_order::relaxed);
						return true;
					}
				}
				else if (seq == static_cast<uint32_t>(idx << 1 | 1u)) {
					writeIndex.compare_exchange_strong(idx, idx + 1, std::memory_order::release, std::memory_order::relaxed);
				}
				else if (static_cast<uint32_t>(seq + (_size << 1))
					== static_cast<uint32_t>((idx << 1) | 1u)) {
					return false;
				}
			}
		}

		bool dequeue(uint32_t& eID) noexcept { return pop(eID); }
		bool pop(uint32_t& eID) noexcept {
			while (true) {
				uint32_t idx = readIndex.load(std::memory_order::relaxed);
				uint64_t e = (data + (idx & (_size - 1)))->load(std::memory_order_acquire);

				if (getSeq(e) == static_cast<uint32_t>((idx << 1) | 1u)) {
					uint64_t empty{ getPack(static_cast<uint32_t>((idx + _size) << 1u), 0) };

					if ((data + (idx & (_size - 1)))->compare_exchange_strong(e, empty,
						std::memory_order::release, std::memory_order::relaxed)) {
						eID = getEntityID(e);
						readIndex.compare_exchange_strong(idx, idx + 1, std::memory_order::release, std::memory_order::relaxed);
						return true;
					}
				}
				else if ((getSeq(e) | 1u) == static_cast<uint32_t>(((idx + _size) << 1) | 1u)) {
					readIndex.compare_exchange_strong(idx, idx + 1, std::memory_order::release, std::memory_order::relaxed);
				}
				else if (getSeq(e) == static_cast<uint32_t>(idx << 1)) {
					return false;
				}
			}
		}
	private:
		static constexpr uint64_t getPack(uint32_t seq, uint32_t eID) { return (static_cast<uint64_t>(seq) << 32) | eID; }
		static constexpr uint32_t getSeq(uint64_t value) { return static_cast<uint32_t>(value >> 32); }
		static constexpr uint32_t getEntityID(uint64_t value) { return static_cast<uint32_t>(value); }
	private:
		alignas(std::hardware_destructive_interference_size) std::atomic<uint32_t> writeIndex{};
		alignas(std::hardware_destructive_interference_size) std::atomic<uint32_t> readIndex{};
		alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t>* data;
	};
}