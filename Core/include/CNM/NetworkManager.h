#pragma once
// STL
#include <string_view>
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
	
	class NetworkManager {
	private:
		struct Peer {
			ipv4_addr	addr{};
			uint32_t	ACKField{};
			uint16_t	port{};
			uint16_t	lastSeen{};
		};
		struct PacketHeader {
			const uint32_t PROTOCOL_VERSION = HEADER_VERSION;
			uint32_t ACKField;
			uint32_t SequenceNumber; // could change to 16-bit to send less data per packet
			// alignment for this struct doesn't matter since they'll be added manually to the payload
		};
	public:
		NetworkManager(const ManagerData& initData);
		~NetworkManager();

		NetworkManager(const NetworkManager&)				= delete;
		NetworkManager& operator=(const NetworkManager&)	= delete;
		NetworkManager(NetworkManager&&)					= delete;
		NetworkManager& operator=(NetworkManager&&)			= delete;
		
		void pollIO();
		void attemptConnect(ipv4_addr addr, uint16_t port);

	private:
		void sendReliableData();
		void sendUnreliableData();
		void sendSnapshot();

		void pollOnGoing();
		void pollIncoming();

		void addPeer(uint32_t peerID);
		void removePeer(uint32_t peerID);
	private:
		//std::unordered_map<uint32_t, Peer> m_Peers;
		Socket m_Socks[3]; // 0 - Reliable , 1 - High Frequency Unreliable, 2 - Snapshots
		void* pOutBuffer{ nullptr };
		void* pInBuffer{ nullptr };
		uint8_t m_TimeOut{10}; // in Seconds.
	};
}