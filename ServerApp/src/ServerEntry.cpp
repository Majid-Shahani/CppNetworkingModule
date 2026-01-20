#include <CNM.h>
#include <CNM/Buffer.h>

#include <print>
#include <thread>
#include <chrono>

#ifdef CL_Platform_Windows
#include <Windows.h>
#include <mmsystem.h>
#endif

using namespace Carnival;
using namespace utils;
using namespace Network;
using namespace ECS;
using namespace std::chrono_literals;

struct Position {
	float x{}, y{}, z{};

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
		constexpr uint64_t stride = sizeof(float) * 3;
		auto pSrc = static_cast<const Position*>(src);

		auto out = outbuffer.startMessage(sizeof(float) * 3 * count);
		if (!out) return;

		for (uint32_t i{}; i < count; i++) {
			std::byte* dst = out + i * stride;
			std::memcpy(dst, &pSrc[i].x, sizeof(float));
			std::memcpy(dst + sizeof(float), &pSrc[i].y, sizeof(float));
			std::memcpy(dst + (sizeof(float) * 2), &pSrc[i].z, sizeof(float));
		}

		outbuffer.endMessage();
	}
	static void deserialize(void* dest, const MessageBuffer& inBuffer, uint32_t count = 1) {
		
	}
};

void PositionMoverSystem(World& w, float delta) {
	auto query = w.query<QueryPolicy::RW, Position>();
	
	for (auto it = query.begin(); it != query.end(); ++it) {
		it.write().x = it.read().x + delta;
	}
}

int main() {
#ifdef CL_Platform_Windows
	timeBeginPeriod(1);
#endif
	// =========================================== INIT ECS ========================================= //
	std::unique_ptr<World> w{ std::make_unique<World>() };
	w->registerComponents<Position, OnTickNetworkComponent, OnUpdateNetworkComponent>();
	Entity onTickEntity = w->createEntity<Position, OnTickNetworkComponent>();
	Entity onUpdateEntity = w->createEntity<Position, OnUpdateNetworkComponent>();
	w->removeComponentsFromEntity<OnUpdateNetworkComponent>(onUpdateEntity);
	w->addComponentsToEntity<OnUpdateNetworkComponent>(onUpdateEntity);

	// ========================================= INIT NETWORK ======================================= //
	SocketData sockRel{
		.InAddress = 127 << 24 | 1,
		.InPort = 52000,
		.status = SocketStatus::NONBLOCKING,
	};
	SocketData sockURel{
		.InAddress = 127 << 24 | 1,
		.InPort = 52001,
		.status = SocketStatus::NONBLOCKING,
	};

	std::unique_ptr<NetworkManager> netMan{ std::make_unique<NetworkManager>(w.get(),
		sockRel, sockURel, 64) };
	std::jthread netRun{ [&]() {
		netMan->run(512);
	} };

	// =========================================== Main Loop ========================================= //
	// Entities Should be Marked Dirty and Replication Records Submitted IF onUpdate Networked
	w->startUpdate();
	PositionMoverSystem(*w, 1);
	w->endUpdate();
	// ============================================ CLEANUP =========================================== //
	std::this_thread::sleep_for(115s);
	netMan->stop();
	netRun.join();

#ifdef CL_Platform_Windows
	timeEndPeriod(1);
#endif
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