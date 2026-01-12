#pragma once
// STD
#include <string_view>
#include <array>
// CNM
#include <CNM/CNMtypes.h>
#include <CNM/utils.h>
#include <CNM/Socket.h>
#include <CNM/Replication.h>

namespace Carnival::ECS {
	class World;
}

namespace Carnival::Network {
	constexpr uint32_t HEADER_VERSION = utils::fnv1a32("CarnivalEngine.Network_UDP_0.0.1");
	static constexpr size_t CHANNELS = 3;
	static constexpr size_t SOCKET_COUNT = CHANNELS - 1;

	class NetworkManager {
	public:
		NetworkManager(ECS::World* pWorld, const SocketData& sockData);
		~NetworkManager() = default;

		NetworkManager(const NetworkManager&)				= delete;
		NetworkManager& operator=(const NetworkManager&)	= delete;
		NetworkManager(NetworkManager&&)					= delete;
		NetworkManager& operator=(NetworkManager&&)			= delete;
		
		void pollIO();
		void pollOngoing();
		void pollIncoming();

		bool attemptConnect(ipv4_addr addr, uint16_t port);

	private:
		void sendReliableData();
		void sendUnreliableData();
		void sendSnapshot();

		void addPeer(uint16_t peerID);
		void removePeer(uint16_t peerID);

	private:
		enum class ChannelID : uint8_t {
			UNRELIABLE = 0,
			RELIABLE = 1,
			SNAPSHOT = 2,
		};

		enum PacketFlags : uint8_t {
			PAYLOAD = 1 << 0,
			HEARTBEAT = 1 << 1,
			CONNECTION = 1 << 2,
			SCHEMA = 1 << 3,
		};

		struct FragmentLoad {
			uint16_t batchNumber{};
			uint16_t FRAGMENT_COUNT{};
			uint16_t fragmentIndex{};
		};

		struct ChannelState {
			uint16_t	batchNumber{};
			uint16_t	FRAGMENT_COUNT{};
			uint32_t	lastSent{};
			uint32_t	lastReceived{};
			uint32_t	receivedACK{};
		};

		struct Peer {
			std::array<ChannelState, CHANNELS> states; // 0 - Unreliable, 1 - Reliable Unordered, 2 - Snapshot
			uint32_t	lastSeen{};
			ipv4_addr	addr{};
			uint16_t	port{};
			uint16_t	peerID{};
		};
		struct PacketHeader {
			uint32_t		PROTOCOL_VERSION{ HEADER_VERSION };
			uint32_t		SequenceNumber{}; // Sequence of this packet being sent.
			uint32_t		ACKField{}; // Last 32 packets received
			uint32_t		LastSeqReceived{};
			uint16_t		SessionID{};
			FragmentLoad	fragmentData{};
			ChannelID		Channel{ ChannelID::UNRELIABLE };
			PacketFlags		Flags{ PacketFlags::HEARTBEAT };
		};

	private:
		std::array<Socket, SOCKET_COUNT> m_Socks; // 0 - High Frequency Unreliable, 1 - Reliable, Snapshots
		ECS::World* m_callback;
		//uint8_t m_TimeOut{10}; // in Seconds.
	};
}