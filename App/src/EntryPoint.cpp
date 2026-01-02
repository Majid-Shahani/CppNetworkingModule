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
struct OnTickNetworkComponent {
	uint32_t networkID{};

	static constexpr uint64_t ID{ fnv1a64("OnTickNetworkComponent") };
	static void construct(void* dest, uint32_t count) noexcept {
		std::memset(dest, 0, sizeof(OnTickNetworkComponent) * count);
	}
	static void destruct(void* dest, uint32_t count) noexcept {
		std::memset(dest, 0, sizeof(OnUpdateNetworkComponent) * count);
	}
	static void copy(const void* src, void* dest, uint32_t count) {
		memcpy(dest, src, sizeof(OnTickNetworkComponent) * count);
	}
	static void serialize(const void* src, void* out, uint32_t count) {
		// static_cast<buffer*>(out)->put_uint32(static_cast<const OnTickNetworkComponent*>(src)->networkID);
	}
	static void deserialize(void* dest, const void* in, uint32_t count) {
		// static_cast<OnTickNetworkComponent*>(dest)->networkID = static_cast<const buffer*>(in)->read_uint32();
	}
};
struct OnUpdateNetworkComponent {
	uint32_t networkID{};
	bool dirty{ false };
	uint8_t padding[3]{};

	static constexpr uint64_t ID{ fnv1a64("OnUpdateNetworkComponent") };
	static void construct(void* dest, uint32_t count) noexcept {
		std::memset(dest, 0, sizeof(OnUpdateNetworkComponent) * count);
	}
	static void destruct(void* dest, uint32_t count) noexcept {
		// Free Network IDs
		std::memset(dest, 0, sizeof(OnUpdateNetworkComponent) * count);
	}
	static void copy(const void* src, void* dest, uint32_t count) {
		memcpy(dest, src, sizeof(OnUpdateNetworkComponent) * count);
	}
	static void serialize(const void* src, void* out, uint32_t count) {
		// static_cast<buffer*>(out)->put_uint32(static_cast<const OnUpdateNetworkComponent*>(src)->networkID);
	}
	static void deserialize(void* dest, const void* in, uint32_t count) {
		// static_cast<OnUpdateNetworkComponent*>(dest)->networkID = static_cast<const buffer*>(in)->read_uint32();
	}
};

void PositionMoverSystem(Archetype& arch, float delta) {
	uint32_t index = arch.getComponentIndex(Position::ID);
	Position* pos = static_cast<Position*>(arch.writeComponentData(index));
	uint32_t count = arch.getEntityCount();

	for (uint32_t i{}; i < count; i++) {
		pos[i].x += delta;
		auto e = arch.getEntity(i);
		std::print("Entity: {}, Marked Dirty. Value: {}\n", e, pos[i].x);
	}
}

int main() {
	// ========================================= INIT NETWORK ======================================== //

	// Set Up NetworkManager

	// =========================================== INIT ECS ========================================= //
	ComponentRegistry reg{};
	reg.registerComponent<Position>();
	reg.registerComponent<OnTickNetworkComponent>();
	reg.registerComponent<OnUpdateNetworkComponent>();

	auto arch = Archetype::create(reg,
		std::span<const uint64_t>{
		std::array<uint64_t, 2> {Position::ID, OnUpdateNetworkComponent::ID}},
		8);
	if (!arch) throw std::runtime_error("Failed to Create Archetype");

	for (int i{}; i < 10; i++) {
		Entity e = EntityManager::create(arch.get(), 0, static_cast<EntityStatus>(ALIVE));
		uint32_t index = arch->addEntity(e);
		EntityManager::updateEntityLocation(e, arch.get(), index);
	}

	// ============================================= NETWORK ============================================ //

	// Scan / Wait for Connection Requests

	// Accept / Deny

	// Send Schema

	// =========================================== Main Loop ========================================= //
	// Entities Should be Marked Dirty and Replication Records Submitted IF onUpdate Networked 
	PositionMoverSystem(*arch, 1.0f);


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