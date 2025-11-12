#pragma once
// STL
#include <unordered_map>
#include <string_view>
//#include <cstdint>
// CNM
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
	public:
		NetworkManager();
		~NetworkManager();

		NetworkManager(const NetworkManager&)				= delete;
		NetworkManager& operator=(const NetworkManager&)	= delete;
		NetworkManager(NetworkManager&&)					= delete;
		NetworkManager& operator=(NetworkManager&&)			= delete;

		void createSocket(uint8_t sockKey, const SocketData& initData, ipv4_addr outAddress = {});
		
		void sendData(uint8_t sockKey);
		void sendData(uint8_t sockKey, ipv4_addr outAddr);
		void recvData(uint8_t sockKey);
	private:
		struct SockBuffers {
			Socket sock;
			void* pOutBuffer = nullptr;
			void* pInBuffer = nullptr;
		};
		struct PacketHeader {
			const uint32_t PROTOCOL_VERSION = HEADER_VERSION;
			uint32_t ACKField;
			uint32_t SequenceNumber;
		};

	private:
		std::unordered_map<uint8_t, SockBuffers> m_Sockets;
	};
}