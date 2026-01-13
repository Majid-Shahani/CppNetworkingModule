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
		NetworkManager(ECS::World* pWorld, const SocketData& sockData, uint16_t maxSessions);
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
		static uint32_t getTime();
		void sendReliableData();
		void sendUnreliableData();
		void sendSnapshot();

		void writeHeader(PacketFlags flags,	uint32_t sessionID = 0,
			uint32_t seq = 0, uint32_t ackf = 0,
			uint32_t lastReceiv = 0, FragmentLoad frag = {});

		bool handlePacket(PacketInfo);
		void handleConnection(PacketInfo);
		void handlePayload(PacketInfo);
		void createSession(PacketInfo);
	private:
		NetworkStats m_Stats{};

		std::array<Socket, SOCKET_COUNT> m_Socks; // 0 - High Frequency Unreliable, 1 - Reliable, Snapshots
		std::vector<std::byte> m_PacketBuffer;
		ECS::World* m_callback;

		std::vector<PendingPeer> m_PendingConnections;
		std::map<uint32_t, Session> m_Sessions;

		uint16_t m_MaxSessions{ 1 };
		ReliabilityPolicy m_Policy{};
	};
}