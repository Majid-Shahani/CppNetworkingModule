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
		uint64_t version{};
		uint64_t size{};
		void* pSerializedData{ nullptr };
	};

	struct ReplicationContext {
		ReplicationContext() = default;
		ReplicationContext(ReplicationContext&&) = default;

		~ReplicationContext() {
			for (auto& [netID, snapshot] : entityTable) {
				if (snapshot.pSerializedData) delete[] snapshot.pSerializedData;
			}
		}
		// Reliable Data
		// TODO: Event Log
		std::map<uint64_t, EntitySnapshot> entityTable{}; // not Thread Safe
		MessageBuffer reliableStagingBuffer{ 256 };
		// Unreliable Data
		std::array<MessageBuffer, 2> sendBuffers{};
		std::array<MessageBuffer, 2> receiveBuffers{};
	};
}