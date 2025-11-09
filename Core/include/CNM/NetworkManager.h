#pragma once
// STL
#include <unordered_map>
// CNM
#include <CNM/Socket.h>

namespace Carnival::Network {
	class NetworkManager {
	public:
	private:
		std::unordered_map<uint8_t, Socket> m_Sockets;
	};
}