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
		
		bool isRunning() { return m_Running.test(std::memory_order_acquire); }
		void run(uint16_t tickRate); // Tickrate should be a power of two
		void stop(); // blocking

		void attemptConnect(ipv4_addr addr, uint16_t port);
	private:
		static uint64_t getTime() noexcept;

		inline bool sendReliable(ipv4_addr addr, uint16_t port) noexcept;
		inline bool sendReliable(Endpoint& ep) noexcept;
		inline bool sendUnreliable(ipv4_addr addr, uint16_t port) noexcept;
		inline bool sendUnreliable(Endpoint& ep) noexcept;
		// void sendSnapshot(ipv4_addr addr, uint16_t port);

		void writeHeader(const HeaderInfo& header);
		HeaderInfo parseHeader();
		
		bool updateSessionStats(Session& sesh, 
			const PacketInfo packet,
			const HeaderInfo& header,
			const uint16_t endpointIndex,
			const uint16_t channelIndex);

		inline bool handleReliablePacket(const PacketInfo);
		inline bool handleUnreliablePacket(const PacketInfo);

		inline void handleConnectionRequest(const PacketInfo, const HeaderInfo&);
		inline bool handleConnectionAccept(const PacketInfo, const HeaderInfo&);
		inline bool handleConnectionReject(const PacketInfo, const HeaderInfo&);

		inline void handlePayload(const PacketInfo, const HeaderInfo&);

		uint32_t createSession(const PendingPeer& info);
		bool createSession(const PendingPeer& info, uint32_t Key);

		inline void acceptConnection(uint32_t sessionID) 
		{
			m_CommandBuffer.emplace_back(&(m_Sessions.at(sessionID)),
				sessionID,
				static_cast<PacketFlags>(PacketFlags::CONNECTION_ACCEPT | PacketFlags::RELIABLE));
		}
		inline void rejectConnection(ipv4_addr addr, uint16_t port)
		{
			m_CommandBuffer.emplace_back(addr, port,
				static_cast<PacketFlags>(PacketFlags::CONNECTION_ACCEPT | PacketFlags::RELIABLE));
		}

		inline void sendRequest(ipv4_addr addr, uint16_t port) noexcept;
		inline void sendAccept(uint32_t sessionID, Session& sesh) noexcept;
		inline void sendReject(ipv4_addr addr, uint16_t port) noexcept;

		void collectIncoming(); // receive waiting packets
		void maintainSessions(); // book keeping and keep connections alive
		void opportunisticReceive(); // Wait until next tick for new packets
		void processCommands(); // send waiting messages
	private:
		NetworkStats m_Stats{};

		std::array<Socket, SOCKET_COUNT> m_Socks; // 0 - High Frequency Unreliable, 1 - Reliable, Snapshots
		std::vector<std::byte> m_PacketBuffer;
		std::vector<NetCommand> m_CommandBuffer;

		std::vector<PendingPeer> m_PendingConnections;
		std::map<uint32_t, Session> m_Sessions;

		ECS::World* m_pWorld;

		uint64_t m_NextTick{ 0 };

		std::atomic_flag m_Running;
		std::atomic_flag m_ShouldStop;

		ReliabilityPolicy m_Policy{};
		uint16_t m_MaxSessions{ 1 };
	};
}