#pragma once

#include <map>
#include <array>

#include <CNM/Buffer.h>
#include <ECS/Entity.h>

namespace Carnival {

	struct BufferIndex {
		bool writerActive{ false };
		bool writerIndex{ false }; // false = 0, true = 1
		bool readerIndex{ true };
		bool readerActive{ false };
	};

	struct EntitySnapshot {
		~EntitySnapshot() {
			if (pSerializedData) delete[] pSerializedData;
		}

		uint64_t version{};
		uint64_t size{};
		void* pSerializedData{ nullptr };
	};

	struct ReplicationContext {
		static_assert(std::atomic<BufferIndex>::is_always_lock_free, "Buffer index is not lock-free");

		// Reliable Data
		// TODO: Event Log
		std::map<uint64_t, EntitySnapshot> entityTable{}; // not Thread Safe
		MessageBuffer reliableStagingBuffer{ 256 };
		// Unreliable Data
		std::unique_ptr<std::atomic<BufferIndex>> unreliableIndex{ std::make_unique<std::atomic<BufferIndex>>() };
		std::array<MessageBuffer, 2> sendBuffers{};
		std::array<MessageBuffer, 2> receiveBuffers{};
	};
}