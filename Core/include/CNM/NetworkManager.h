#pragma once
// STD
#include <string_view>
#include <array>
#include <vector>
// CNM
#include <CNM/cnm_core.h>
#include <CNM/utils.h>
#include <CNM/Socket.h>
#include <CNM/Replication.h>

namespace Carnival::ECS {
	class World;
}

namespace Carnival::Network {
	class NetworkManager {
	public:
		NetworkManager(ECS::World* pWorld, const SocketData& sockData);
		~NetworkManager() = default;

		NetworkManager(const NetworkManager&)				= delete;
		NetworkManager& operator=(const NetworkManager&)	= delete;
		NetworkManager(NetworkManager&&)					= delete;
		NetworkManager& operator=(NetworkManager&&)			= delete;
		
		void pollIO();
		void pollOngoing(); // book keeping and keep connections alive
		void pollIncoming(); // receive waiting packets
		void pollOutgoing(); // send waiting messages

		bool attemptConnect(ipv4_addr addr, uint16_t port);

	private:
		void sendReliableData();
		void sendUnreliableData();
		void sendSnapshot();

		void addPeer(uint16_t peerID);
		void removePeer(uint16_t peerID);

	private:
		std::array<Socket, SOCKET_COUNT> m_Socks; // 0 - High Frequency Unreliable, 1 - Reliable, Snapshots
		std::vector<std::byte> m_PacketBuffer;
		ECS::World* m_callback;

		NetworkStats m_Stats;
		ReliabilityPolicy m_Policy{};
	};
}