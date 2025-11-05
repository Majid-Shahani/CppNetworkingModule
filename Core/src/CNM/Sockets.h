#pragma once

namespace Carnival::Network {
	enum SocketType : uint8_t
	{
		CONNECTION_ORIENTED = 0,
		CONNECTION_LESS		= 1,
	};

#define BIT(x) (1 << x)
	// This is Defined and undefined here in anticipation
	// of being added to my game engine which has this macro defined
	enum SocketStatus : uint8_t
	{
		NONE		= 0,		// uninitialized or closed
		OPEN		= BIT(0),	// handle created and valid
		BOUND		= BIT(1),	// bound to port/address
		ACTIVE		= BIT(2),	// in-use by the program
		NONBLOCKING	= BIT(3),	// recv & send immediate return
		/*
		For Later Use
		REUSEADDR	= BIT(4),
		BROADCAST	= BIT(5),
		*/
		SOCKERROR	= ~0,		// socket-level error
	};
#undef BIT

	struct SocketData {
		uint64_t		Handle	= 0;
		uint16_t		Port	= 0;
		SocketType		Type	= CONNECTION_LESS;
		SocketStatus	Status	= SocketStatus::NONE;
	};

	class Sockets {
	public:
		Sockets();
		~Sockets();

		Sockets(const Sockets&)				= delete;
		Sockets& operator=(const Sockets&)	= delete;
		Sockets(Sockets&&)					= delete;

		void createSocket(uint8_t socketKey, SocketData Data = {});
		bool deleteSocket(uint8_t socketKey);
		bool deleteAllSockets();

		bool bindSocket(uint8_t socketKey, uint32_t ipv4_addr = 0, uint16_t port = 0);

	private:
		static inline bool s_Initialized{ false };
		std::unordered_map<uint8_t, SocketData> m_Sockets; // need getter or change logic to store outside
	};

}
