#include <CNM.h>

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
	static void construct(void* dest, uint32_t count = 1) noexcept {
		auto* p = static_cast<Position*>(dest);
		for (uint64_t i{}; i < count; i++)
			p[i] = { 0.f, 0.f, 0.f };
	}
	static void destruct(void* dest, uint32_t count = 1) noexcept {
		// trivial
	}
	static void copy(const void* src, void* dest, uint32_t count = 1) {
		memcpy(dest, src, sizeof(Position) * count);
	}
	static void serialize(const void* src, void* out, uint32_t count = 1) {
		
	}
	static void deserialize(void* dest, const void* in, uint32_t count = 1) {
		
	}
};

void PositionMoverSystem(World& w, float delta) {
	auto query = w.query<QueryPolicy::RW, Position>();
	/*
	for (auto it = query.begin(); it != query.end(); ++it) {
		it.write().x = it.read().x + delta;

		std::print("Marked Dirty. Value: {}\n", it.read().x);
	}
	*/
	int i{};
	for (const auto& c : query) {
		c.x;
	}
}

int main() {
	// ========================================= INIT NETWORK ======================================== //

	// Set Up NetworkManager

	// =========================================== INIT ECS ========================================= //
	World w{};
	w.registerComponents<Position, OnTickNetworkComponent, OnUpdateNetworkComponent>();
	Entity onTickEntity = w.createEntity<Position, OnTickNetworkComponent>();
	Entity onUpdateEntity = w.createEntity<Position, OnUpdateNetworkComponent>();
	w.removeComponentsFromEntity<OnUpdateNetworkComponent>(onUpdateEntity);
	w.addComponentsToEntity< OnUpdateNetworkComponent>(onUpdateEntity);
	// ============================================ NETWORK =========================================== //

	// Scan / Wait for Connection Requests

	// Accept / Deny

	// Send Schema

	// =========================================== Main Loop ========================================= //
	// Entities Should be Marked Dirty and Replication Records Submitted IF onUpdate Networked 
	PositionMoverSystem(w, 1);

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