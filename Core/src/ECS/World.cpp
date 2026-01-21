#include <src/CNMpch.hpp>
#include <ECS/World.h>

#include <ranges>

namespace Carnival::ECS {
	Entity World::createEntity(std::vector<uint64_t> components, NetworkFlags flag)
	{
		uint64_t id{ Archetype::hashArchetypeID(components) };
		
		// insert/fetch archetype, assert no hash collision
		auto [it, inserted] = m_Archetypes.try_emplace(id, m_Registry, components, id, static_cast<void*>(this), flag, 5);
		if (!inserted) {
			CL_CORE_ASSERT(it->second.flags == flag, "Mismatch Network flags on archetype");
			CL_CORE_ASSERT(it->second.arch->getComponentIDs() == components, "Hash Collision");
		}
		
		Entity e = m_EntityManager.create(nullptr, 0);
		uint32_t index = it->second.arch->addEntity(e);
		m_EntityManager.updateEntityLocation(e, it->second.arch.get(), index);
		
		return e;
	}

	void World::updateReliable()
	{
		Entity eID{};
		while (m_ReplicationBuffer.pop(eID)) {
			// Resolve Entity -> Shards
			// single shard only for now
			uint16_t shardIndex = 0;

			auto& currShard = m_Shards[shardIndex];

			const auto rec = m_EntityManager.get(eID);

			currShard.reliableStagingBuffer.reset();
			rec.pArchetype->serializeIndex(rec.index, currShard.reliableStagingBuffer);

			auto pData = static_cast<OnUpdateNetworkComponent*>
				(rec.pArchetype->getComponentData(OnUpdateNetworkComponent::ID));
			auto& comp = pData[rec.index];

			uint64_t NetID = comp.networkID;
			
			auto& snapshot = currShard.entityTable[NetID];

			if (snapshot.version >= comp.version) {
				// version mismatch, deal with it
				comp.dirty = false;
				continue;
			}

			snapshot.version = comp.version;
			snapshot.size = currShard.reliableStagingBuffer.size();

			if (snapshot.pSerializedData) delete[] snapshot.pSerializedData;
			snapshot.pSerializedData = new std::byte[currShard.reliableStagingBuffer.size()]();
			auto toBeCopied = currShard.reliableStagingBuffer.getReadyMessages();
			std::memcpy(snapshot.pSerializedData, toBeCopied.data(), toBeCopied.size());

			comp.dirty = false;
		}
	}
	void World::replicateUnreliable(uint16_t shardIndex)
	{
		auto& currShard = m_Shards[shardIndex];
		auto idx = currShard.unreliableIndex->load(std::memory_order::acquire).writerIndex;
		auto& msgBuffer = currShard.sendBuffers[idx];
		msgBuffer.reset();

		for (auto& [id, rec] : m_Archetypes) {
			if (rec.flags == NetworkFlags::ON_TICK) {
				msgBuffer.putRecordType(Network::WireFormat::RecordType::ARCHETYPE_DATA);
				msgBuffer.putArchetypeData(id, rec.arch->getEntityCount());
				rec.arch->serializeArchetype(msgBuffer);
			}
		}
	}

	void World::destroyEntity(Entity e)
	{
		const auto& rec = m_EntityManager.get(e);
		// remove from archetype
		auto [entity, index] = m_Archetypes.at(rec.pArchetype->getID()).arch->removeEntityAt(rec.index);
		if (e != entity) {
			// update swapped entity location
			m_EntityManager.updateEntityLocation(entity, rec.pArchetype, index);
		}
		m_EntityManager.destroyEntity(e);
	}
	void World::startUpdate()
	{
		m_Phase.store(WorldPhase::EXECUTION, std::memory_order::release);
		// update ecs with replication
	}
	void World::endUpdate()
	{
		// remove empty archetypes, go maintenance
		// Needs Syncing between world and net manager
		m_Phase.store(WorldPhase::MAINTENANCE, std::memory_order::release);
		std::erase_if(m_Archetypes, [](const auto& pair) {
			return pair.second.arch->getEntityCount() == 0;
			});
		m_Phase.store(WorldPhase::STABLE, std::memory_order::release);
		// stage reliable updates
		updateReliable();

		// Temporary 1 shard solution:
		auto& u_idx = m_Shards[0].unreliableIndex;

		// Set Replication Write Active
		auto expected = u_idx->load(std::memory_order::relaxed);
		BufferIndex idx{};
		do {
			idx = expected;
			idx.writerActive = true;
		} while (!u_idx->compare_exchange_strong(expected, idx, std::memory_order::release, std::memory_order::relaxed));

		// Replicate Records
		replicateUnreliable(0);

		// Swap Buffers, Set Replication Inactive
		do {
			expected = u_idx->load(std::memory_order::relaxed);
			idx = expected;
			if (!idx.readerActive) {
				idx.writerIndex ^= 1;
				idx.readerIndex ^= 1;
			}
			idx.writerActive = false;
		} while (!u_idx->compare_exchange_strong(expected, idx, std::memory_order::release, std::memory_order::relaxed));
	}
}