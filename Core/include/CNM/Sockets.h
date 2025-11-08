#pragma once

namespace Carnival::Network {
	enum SocketType : uint8_t
	{
		CONNECTION_ORIENTED = 0,
		CONNECTION_LESS		= 1,
	};

	enum SocketStatus : uint8_t
	{
		NONE		= 0,		// uninitialized or closed
		OPEN		= 1,	// handle created and valid
		BOUND		= 1 << 1,	// bound to port/address
		ACTIVE		= 1 << 2,	// in-use by the program
		NONBLOCKING	= 1 << 3,	// recv & send immediate return
		/*
		For Later Use
		REUSEADDR	= 1 << 4,
		BROADCAST	= 1 << 5,
		*/
		SOCKERROR	= 1 << 7,		// socket-level error
	};

	// Address will be set in host byte order after call to bind / send
	union ipv4_addr {
		uint32_t addr32{};
		uint16_t addr16[2];
		uint8_t addr8[4];
	};

	struct SocketData {
		uint64_t		Handle	= 0;
		ipv4_addr		OutAddress{};
		ipv4_addr		InAddress{};
		uint16_t		Port	= 0;
		SocketType		Type	= CONNECTION_LESS;
		SocketStatus	Status	= SocketStatus::NONE;
	};

	class Sockets {
	public:
		Sockets() noexcept;
		~Sockets() noexcept;

		Sockets(const Sockets&)				= delete;
		Sockets& operator=(const Sockets&)	= delete;
		Sockets(Sockets&&)					= delete;

		void createSocket(uint8_t socketKey, SocketData Data = {});
		bool deleteSocket(uint8_t socketKey);
		bool deleteAllSockets();

		// pass in an address a.b.c.d as (a << 24) | (b << 16) | (c <<8) | d
		// SocketData InAddress is overwritten after call to bind
		bool bindSocket(uint8_t socketKey, uint16_t port = 0, ipv4_addr inAddr = {});
		bool sendPackets(uint8_t socketKey, const char* packetData, int packetSize, ipv4_addr outAddr = {});
		void receivePackets(uint8_t socketKey);
	private:
		static inline bool s_Initialized{ false };
		std::unordered_map<uint8_t, SocketData> m_Sockets;
	};

}
