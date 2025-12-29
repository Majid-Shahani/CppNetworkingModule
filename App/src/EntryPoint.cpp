#include <CNM.h>

#include <print>
#include <thread>
#include <chrono>

namespace cnm = Carnival::Network;
namespace ecs = Carnival::ECS;
using namespace std::chrono_literals;

int main() {
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

	// A registery
	// Generate Archetype
	// Archetype ID generated Automatically
	// Make Entities in archetype
	// Entity ID and Network Component ID generated Automatically

	// only pass registery reference and archetype IDs to network manager for automatic updates.
	// the address for a reference must not change

	// make NetworkManager
	// Make Network Schema
	// Make Network Archetype ID -> ArchetypeAddress Map
	// Make Network Entity ID map ?? Maybe a sparse Entity Array with Metadata could be made inside ECS.

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