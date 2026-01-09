#include <CNM.h>
#include <CNM/Buffer.h>

#include <print>
#include <thread>
#include <chrono>

using namespace Carnival;
using namespace utils;
using namespace Network;
using namespace ECS;
using namespace std::chrono_literals;

struct Position {
	float x, y, z;

	static constexpr uint64_t ID{ fnv1a64("PositionComponent") };
	static void construct(void* dest, void* world, Entity e) noexcept {
		auto* p = static_cast<Position*>(dest);
		*p = { 0.f, 0.f, 0.f };
	}
	static void destruct(void* dest, void* world, Entity e) noexcept {
		// trivial
	}
	static void copy(const void* src, void* dest, uint32_t count = 1) {
		memcpy(dest, src, sizeof(Position) * count);
	}
	static void serialize(const void* src, MessageBuffer& outbuffer, uint32_t count = 1) {
		
	}
	static void deserialize(void* dest, const MessageBuffer& inBuffer, uint32_t count = 1) {
		
	}
};

void PositionMoverSystem(World& w, float delta) {
	auto query = w.query<QueryPolicy::RW, Position>();
	
	for (auto it = query.begin(); it != query.end(); ++it) {
		it.write().x = it.read().x + delta;

		std::print("Value: {}\n", it.read().x);
	}
}

int main() {
	// ========================================= INIT NETWORK ======================================= //
	Replicator rep{};
	// =========================================== INIT ECS ========================================= //
	World w{};
	w.startUpdate();
	w.registerComponents<Position, World::OnTickNetworkComponent, World::OnUpdateNetworkComponent>();
	Entity onTickEntity = w.createEntity<Position, World::OnTickNetworkComponent>();
	Entity onUpdateEntity = w.createEntity<Position, World::OnUpdateNetworkComponent>();
	w.removeComponentsFromEntity<World::OnUpdateNetworkComponent>(onUpdateEntity);
	w.addComponentsToEntity<World::OnUpdateNetworkComponent>(onUpdateEntity);
	w.endUpdate();
	// ============================================ NETWORK =========================================== //

	// Scan / Wait for Connection Requests

	// Accept / Deny

	// Send Schema

	// =========================================== Main Loop ========================================= //
	// Entities Should be Marked Dirty and Replication Records Submitted IF onUpdate Networked 
	w.startUpdate();
	PositionMoverSystem(w, 1);
	w.endUpdate();
	// ============================================ CLEANUP =========================================== //
	/*
	* client code :
	*   request connection
	*   wait for schema
	*   make archetypes
	*   fill in entities
	*   simulate
	*   send action deltas / receive state delta
	*   update ecs
	*   goto simulate until finished
	*/
	return 0;
}