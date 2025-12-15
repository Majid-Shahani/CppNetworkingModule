#include <CNM.h>
#include <print>
#include <thread>
#include <chrono>

namespace cnm = Carnival::Network;
using namespace std::chrono_literals;

int main() {
	/*
	cnm::ipv4_addr addr{.addr32 = (127 << 24) | 1};
	cnm::SocketStatus status{ cnm::SocketStatus::NONBLOCKING | cnm::SocketStatus::REUSEADDR };
	cnm::SocketData sockData{
		.InAddress = addr,
		.InPort = 0,
		.status = status,
	};
	
	cnm::Socket mySock{sockData};
	mySock.openSocket();
	mySock.bindSocket();

	cnm::Socket sock2{ sockData };
	sock2.openSocket();
	sock2.bindSocket();

	const char msg[] = "Hello";
	std::jthread jt{ [&](std::stop_token st) {
		while (!st.stop_requested()) {
			sock2.sendPackets(msg, sizeof(msg), mySock.getAddr(), mySock.getPort());
			std::this_thread::sleep_for(10ms);
		}}};

	std::this_thread::sleep_for(100ms);
	jt.request_stop();
	mySock.receivePackets();
	*/
	/*
	* server code :
	*   listen for connections
	*   accept / deny
	*   send schema
	*   send all networked identities / snapshot
	*   simulate
	*   send / receive deltas
	*   update ecs
	*   goto simulate unless finished
	*/

	Carnival::ECS::Archetype archA{}; // Generate Archetype
	// Archetype ID generated Automatically

	Entity entt1 = archA.CreateEntity(); // Make Entities in archetype
	// Entity ID and Network Component ID generated Automatically

	/* =============================================================================
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