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
		bool sendReliable(ipv4_addr addr, uint16_t port);
		bool sendUnreliable(ipv4_addr addr, uint16_t port);
		// void sendSnapshot(ipv4_addr addr, uint16_t port);

		void writeHeader(const HeaderInfo& header);
		HeaderInfo parseHeader();
		
		bool updateSessionStats(Session& sesh, 
			const PacketInfo packet,
			const HeaderInfo& header,
			const uint16_t endpointIndex,
			const uint16_t channelIndex);

		bool handlePacket(const PacketInfo);

		void handleConnectionRequest(const PacketInfo, const HeaderInfo&);
		bool handleConnectionAccept(const PacketInfo, const HeaderInfo&);
		bool handleConnectionReject(const PacketInfo, const HeaderInfo&);

		void handlePayload(const PacketInfo, const HeaderInfo&);

		uint32_t createSession(const PendingPeer& info);
		bool createSession(const PendingPeer& info, uint32_t Key);

		void acceptConnection(uint32_t sessionID);
		void rejectConnection(ipv4_addr addr, uint16_t port);
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