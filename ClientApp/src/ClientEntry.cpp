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

void PositionReaderSystem(World& w, float delta) {
	auto query = w.query<QueryPolicy::RO, Position>();
	for (auto& P : query) {
		std::print("X: {}, Y: {}, Z: {}\n", P.x, P.y, P.z);
	}
}

int main() {
#ifdef CL_Platform_Windows
	timeBeginPeriod(1);
#endif
	// =========================================== INIT ECS ========================================= //
	std::unique_ptr<World> w{ std::make_unique<World>() };
	w->registerComponents<Position, OnTickNetworkComponent, OnUpdateNetworkComponent>();

	// ========================================= INIT NETWORK ======================================= //
	SocketData sock{
		.InAddress = 127 << 24 | 1,
		.InPort = 0,
		.status = SocketStatus::NONBLOCKING,
	};
	SocketData sock2{
		.InAddress = 127 << 24 | 1,
		.InPort = 0,
		.status = SocketStatus::NONBLOCKING,
	};
	std::unique_ptr<NetworkManager> netMan{ std::make_unique<NetworkManager>(w.get(),
		sock, sock2, 1) };
	std::jthread netRun{ [&]() {
		netMan->attemptConnect({ (127 << 24) | 1 }, 52000);
		//netMan->attemptConnect({ (127 << 24) | 1 }, 52001);
		netMan->run(64);
	} };

	// =========================================== Main Loop ========================================= //
	// Entities Should be Marked Dirty and Replication Records Submitted IF onUpdate Networked
	w->startUpdate();
	PositionReaderSystem(*w, 1);
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