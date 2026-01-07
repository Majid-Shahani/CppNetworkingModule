#pragma once

#include <new>
#include <atomic>
#include <cstdint>
#include <span>

namespace Carnival {
	// Serialized ring buffer :
	// Drop is fine, write index is auth, if read > write, that's capacity, return false if cant write
	// if write > read, capacity = size - write.
	// packet boundaries, recrod say size and type, it should be complete,
	// write flag will be checked, if true, read will add all to the packet.
	// packet send function cannot wrap, data must be contiguous. 
	// if write flag is false, and read index reaches write index, reset both to 0
	// sound a lot like locks.

	class IOBuffer {
	public:
		IOBuffer(uint64_t size)
			: m_Data{nullptr}, m_Capacity{size}, m_Size{0} 
		{
			if (size) m_Data = new std::byte[size];
		}
		~IOBuffer() noexcept
		{
			if (m_Data) delete[] m_Data;
		}

		IOBuffer(const IOBuffer&) = delete;
		IOBuffer& operator=(const IOBuffer&) = delete;

		IOBuffer(IOBuffer&& other) noexcept
			: m_Data{other.m_Data}, m_Capacity{other.m_Capacity}, m_Size{other.m_Size} {
			other.m_Data = nullptr;
			other.m_Capacity = 0;
			other.m_Size = 0;
		}
		IOBuffer& operator=(IOBuffer&& other) noexcept {
			if (this != &other) {
				if (m_Data) delete[] m_Data;
				m_Data = other.m_Data;
				m_Capacity = other.m_Capacity;
				m_Size = other.m_Size;
				other.m_Data = nullptr;
				other.m_Capacity = 0;
				other.m_Size = 0;
			}
			return *this;
		}

		void write(std::span<const std::byte> data) {
			ensureCapacity(m_Size + data.size());
			std::memcpy(m_Data + m_Size, data.data(), data.size());
			m_Size += data.size();
		}
		void write(const void* pData, uint64_t size) {
			ensureCapacity(m_Size + size);
			std::memcpy(m_Data + m_Size, pData, size);
			m_Size += size;
		}

		std::span<const std::byte> read() const noexcept {
			return { m_Data, m_Size };
		}
		std::span<const std::byte> readFromIndex(uint64_t index) const noexcept {
			if (m_Data && index < m_Size) {
				return { m_Data + index, m_Size - index };
			}
			return {};
		}

		std::byte* reserveWrite(uint64_t size) {
			ensureCapacity(m_Size + size);
			return m_Data + m_Size;
		}
		void commitWrite(uint64_t size) {
			m_Size += size;
		}

		uint64_t getSize() const noexcept { return m_Size; }

		void clear() noexcept {
			m_Size = 0;
		}
		bool empty() const noexcept { return m_Size == 0; }
	private:
		void ensureCapacity(uint64_t newCap) {
			if (newCap <= m_Capacity) return;
			if (m_Capacity) {
				if (newCap < 2 * m_Capacity) newCap = 2 * m_Capacity;
			}
			if (!m_Data) {
				m_Data = new std::byte[newCap];
				m_Capacity = newCap;
			}
			else {
				auto tmp = new std::byte[newCap];
				std::memcpy(tmp, m_Data, m_Size);
				std::memset(tmp + m_Size, 0, newCap - m_Size);
				delete[] m_Data;
				m_Data = tmp;
				m_Capacity = newCap;
			}
		}
	private:
		std::byte* m_Data;
		uint64_t m_Capacity{};
		uint64_t m_Size{};
	};

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
			static_assert( _size % 2 == 0, "Size must be a power of 2");
			data = new std::atomic<uint64_t>[_size]();
			for (uint64_t i{}; i < _size; i++) {
				data[i].store(getPack(static_cast<uint32_t>(i << 1), 0), std::memory_order_release);
			}
		}
		~ReplicationBuffer() {
			delete[] data;
		}

		ReplicationBuffer(const ReplicationBuffer&) = delete;
		ReplicationBuffer& operator=(const ReplicationBuffer&) = delete;
		ReplicationBuffer(ReplicationBuffer&&) = delete;
		ReplicationBuffer& operator=(ReplicationBuffer&&) = delete;

		bool unqueue(uint32_t entityID) noexcept { return push(entityID); }
		bool push(uint32_t eID) noexcept {
			while (true) {
				uint32_t idx = writeIndex.load(std::memory_order::relaxed);
				uint64_t val = data[getIndex(idx)].load(std::memory_order::relaxed);
				uint32_t seq = getSeq(val);

				if (seq == static_cast<uint32_t>(idx << 1)) {
					uint64_t entryData{ getPack(seq | 1u, eID) };
					if (data[getIndex(idx)].compare_exchange_strong(val, entryData,
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
				uint64_t e = data[getIndex(idx)].load(std::memory_order_acquire);

				if (getSeq(e) == static_cast<uint32_t>((idx << 1) | 1u)) {
					uint64_t empty{ getPack(static_cast<uint32_t>((idx + _size) << 1u), 0) };

					if (data[getIndex(idx)].compare_exchange_strong(e, empty,
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
		static constexpr uint32_t getIndex(uint32_t idx) {
			return idx & (_size - 1);
		}
	private:
		alignas(std::hardware_destructive_interference_size) std::atomic<uint32_t> writeIndex{};
		alignas(std::hardware_destructive_interference_size) std::atomic<uint32_t> readIndex{};
		alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t>* data;
	};
}