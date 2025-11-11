#pragma once
// STL
#include <unordered_map>
//#include <cstdint>
// CNM
#include <CNM/Socket.h>

namespace Carnival::Network {
	template <typename T> // std::deque not included
	concept Buffer = requires (T a, int b) {
		a.push(b);
		a.pop();
		a.size();
		a.empty();
	};

	class NetworkManager {
	public:
		void createSocket(uint8_t sockKey, SocketData& initData);
		
		void sendData (uint8_t sockKey);
		void recvData(uint8_t sockKey);
	private:
		std::unordered_map<uint8_t, Socket> m_Sockets;
	};
}