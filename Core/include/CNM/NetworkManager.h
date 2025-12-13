#pragma once
// STL
#include <string_view>
#include <array>
// CNM
#include <CNM/CNMtypes.h>
#include <CNM/Socket.h>

namespace Carnival::Network {
	inline consteval uint32_t fnv1a32(std::string_view s) {
		uint32_t hash = 0x811C9DC5u; // offset
		for (unsigned char c : s) {
			hash = hash ^ c;
			hash *= 0x01000193u; // prime number
		}
		return hash;
	}

	constexpr uint32_t HEADER_VERSION = fnv1a32("CarnivalEngine.Network_UDP_0.0.1");
	static constexpr size_t CHANNELS = 3;
	static constexpr size_t SOCKET_COUNT = CHANNELS - 1;

	class NetworkManager {
	private:
		struct ChannelState {
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

		enum class ChannelID : uint8_t {
			UNRELIABLE	= 0,
			RELIABLE	= 1,
			SNAPSHOT	= 2,
		};
		
		enum PacketFlags : uint8_t {
			PAYLOAD		= 1 << 0,
			FRAGMENT	= 1 << 1,
			HEARTBEAT	= 1 << 2,
			CONNECTION	= 1 << 3,
		};

		struct PacketHeader {
			uint32_t	PROTOCOL_VERSION{ HEADER_VERSION };
			uint32_t	SequenceNumber{}; // Sequence of this packet being sent.
			uint32_t	ACKField{}; // Last 32 packets received
			uint32_t	LastSeqReceived{};
			uint16_t	SessionID{};
			ChannelID	Channel{ ChannelID::UNRELIABLE };
			PacketFlags Flags{ PacketFlags::HEARTBEAT };
		};
		
		struct FragmentLoad {
			uint16_t batchNumber{};
			uint16_t fragmentIndex{}; // number of fragment from batch
			uint16_t FRAGMENT_COUNT{};
			uint8_t packetState{}; // 0 - Start, 1 - Middle, 2 - Finish
			uint8_t padding{}; // Compiler inserted padding made explicit
		};

	public:
		NetworkManager(const ManagerData& initData);
		~NetworkManager();

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
		std::array<Socket, SOCKET_COUNT> m_Socks; // 0 - High Frequency Unreliable, 1 - Reliable, Snapshots
		void* pOutBuffer{ nullptr };
		void* pInBuffer{ nullptr };
		uint8_t m_TimeOut{10}; // in Seconds.
	};
}