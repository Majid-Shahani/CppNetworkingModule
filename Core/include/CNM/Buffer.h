#pragma once

#ifdef CL_X64
#include <immintrin.h>
static inline void SpinPause() { _mm_pause(); }
#elif defined(CL_ARM64)
#include <arm_acle.h>
static inline void SpinPause() { __yield(); }
#else
static inline void SpinPause() {}
#endif

#include <CNM/macros.h>
#include <CNM/WireFormat.h>

#include <new>
#include <atomic>
#include <cstdint>
#include <vector>
#include <span>

namespace Carnival {
	/*
	*  Serialized ring buffer :
	*  split into messages, account of messages taken, writer reserves size,
	*  once write is complete that message is marked complete.
	*  double buffered across network ticks, used for unreliable messages
	*/
	
	class MessageBuffer {
	public:
		MessageBuffer(uint32_t bufferSize = 3000)
			: m_Data{ nullptr }, m_Capacity{ bufferSize } {
			CL_CORE_ASSERT(m_Capacity != 0, "buffer must have starting size");
			m_Data = new std::byte[m_Capacity];
		}
		~MessageBuffer() noexcept {	delete[] m_Data; }

		MessageBuffer(const MessageBuffer&) = delete;
		MessageBuffer& operator=(const MessageBuffer&) = delete;
		
		MessageBuffer(MessageBuffer&& other) noexcept
			: m_Data{ other.m_Data }, m_Capacity{ other.m_Capacity }, m_Size{ other.m_Size } {
			other.m_Data = nullptr;
			other.m_Capacity = 0;
			other.m_Size = 0;
		}
		MessageBuffer& operator=(MessageBuffer&& other) noexcept {
			if (this != &other) {
				delete[] m_Data;
				m_Data = other.m_Data;
				m_Capacity = other.m_Capacity;
				m_Size = other.m_Size;
				other.m_Data = nullptr;
				other.m_Capacity = 0;
				other.m_Size = 0;
			}
			return *this;
		}

		// ========================================== MESSAGE WRITE ======================================= //
		
		// Returns Address, call markReady once message has been written
		std::byte* startMessage(uint32_t size) {
			if (m_MessageInFlight)	return nullptr;
			m_MessageInFlight = true;

			ensureCapacity(size + m_Size);

			auto addr = m_Data + m_Size;
			m_Size += size;
			return addr;
		}
		void endMessage() noexcept { m_MessageInFlight = false; }
		
		// Write WireFormat
		inline void putRecordType(Network::WireFormat::RecordType type) {
			auto addr = startMessage(1);
			if (addr) {
				std::memcpy(addr, &type, 1);
				endMessage();
			}
		}
		inline void putArchetypeSchema(uint64_t archID, uint16_t compCount, std::span<const uint64_t> compIDs) {
			auto addr = startMessage(static_cast<uint32_t>(10 + (compIDs.size() * 8)));
			if (!addr) return;

			std::memcpy(addr, &archID, 8);
			addr += 8;
			std::memcpy(addr, &compCount, 2);
			addr += 2;
			for (auto id : compIDs) {
				std::memcpy(addr, &id, 8);
				addr += 8;
			}
			endMessage();
		}
		inline void putArchetypeData(uint64_t archID, uint16_t entityCount) {
			auto addr = startMessage(10);
			if (!addr) return;

			std::memcpy(addr, &archID, 8);
			addr += 8;
			std::memcpy(addr, &entityCount, 2);
			addr += 2;
			endMessage();
		}
		// ========================================== MESSAGE READ ======================================= //
		std::span<const std::byte> getReadyMessages() { return { m_Data, m_Data + m_Size }; }

		// =============================================================================================== //
		uint32_t size() const noexcept { return m_Size; }
		void reset() noexcept { m_Size = 0; }
		void shrinkToFitOrSize(uint32_t newCap = 1) {
			if (newCap < m_Size) newCap = m_Size;

			auto tmp = new std::byte[newCap];
			std::memcpy(tmp, m_Data, m_Size);
			delete[] m_Data;

			m_Data = tmp;
			m_Capacity = newCap;
		}
	private:
		void ensureCapacity(uint32_t newCap) {
			if (newCap < m_Capacity) return;
			if (newCap < m_Capacity * 2) newCap = m_Capacity * 2;

			auto tmp = new std::byte[newCap];
			std::memcpy(tmp, m_Data, m_Size);
			delete[] m_Data;

			m_Data = tmp;
			m_Capacity = newCap;
		}

	private:
		std::byte* m_Data;
		uint32_t m_Capacity;
		uint32_t m_Size{};
		bool m_MessageInFlight = false;
	};

	// Replication Buffer :
	// One big array, size = configNum(1024) * 4
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
			static_assert( (_size & (_size - 1)) == 0, "Size must be a power of 2");
			data = new std::atomic<uint64_t>[_size]();
			for (uint64_t i{}; i < _size; i++) {
				data[i].store(getPack(0, static_cast<uint32_t>(i << 1)), std::memory_order_release);
			}
		}
		~ReplicationBuffer() {
			delete[] data;
		}

		ReplicationBuffer(const ReplicationBuffer&) = delete;
		ReplicationBuffer& operator=(const ReplicationBuffer&) = delete;
		ReplicationBuffer(ReplicationBuffer&&) = delete;
		ReplicationBuffer& operator=(ReplicationBuffer&&) = delete;

		bool enqueue(uint32_t entityID) noexcept { return push(entityID); }
		bool push(uint32_t eID) noexcept {
			while (true) {
				uint32_t idx = writeIndex.load(std::memory_order::acquire);
				uint64_t val = data[getIndex(idx)].load(std::memory_order::acquire);
				uint32_t seq = getSeq(val);

				if (seq == static_cast<uint32_t>(idx << 1)) {
					uint64_t entryData{ getPack(eID, seq | 1u) };
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
				SpinPause();
			}
		}

		bool dequeue(uint32_t& eID) noexcept { return pop(eID); }
		bool pop(uint32_t& eID) noexcept {
			while (true) {
				uint32_t idx = readIndex.load(std::memory_order::relaxed);
				uint64_t e = data[getIndex(idx)].load(std::memory_order_acquire);

				if (getSeq(e) == static_cast<uint32_t>((idx << 1) | 1u)) {
					uint64_t empty{ getPack(0, static_cast<uint32_t>((idx + _size) << 1u)) };

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
				SpinPause();
			}
		}
	private:
		static constexpr uint64_t getPack(uint32_t eID, uint32_t seq) { return (static_cast<uint64_t>(eID) << 32) | seq; }
		static constexpr uint32_t getEntityID(uint64_t value) { return static_cast<uint32_t>(value >> 32); }
		static constexpr uint32_t getSeq(uint64_t value) { return static_cast<uint32_t>(value); }
		static constexpr uint32_t getIndex(uint32_t idx) { return idx & (_size - 1); }
	private:
		alignas(std::hardware_destructive_interference_size) std::atomic<uint32_t> writeIndex{};
		std::atomic<uint64_t>* data{ nullptr }; // plenty of real sharing
		std::atomic<uint32_t> readIndex{}; // Single reader, read and writes are phased, no false sharing
	};


	/*
	* Reliable Buffer:
	* 
	* 
	*/
}