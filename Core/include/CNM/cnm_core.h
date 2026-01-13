#pragma once

#include <cstdint>
#include <CNM/utils.h>

namespace Carnival::Network {

	constexpr uint32_t		HEADER_VERSION	= utils::fnv1a32("CarnivalEngine.Network_UDP_0.0.1");
	static constexpr size_t CHANNELS		= 3;
	static constexpr size_t SOCKET_COUNT	= CHANNELS - 1;

	// Address will be set in host byte order after call to bind / send
	enum SocketStatus : uint8_t {
		NONE			= 0,		// uninitialized or closed
		OPEN			= 1,		// handle created and valid
		BOUND			= 1 << 1,	// bound to port/address
		ACTIVE			= 1 << 2,	// in-use by the program
		NONBLOCKING		= 1 << 3,	// recv & send immediate return
		REUSEADDR		= 1 << 4,
		/*
		For Later Use
		BROADCAST	= 1 << 5,
		*/
		SOCKERROR		= 1 << 7,	// socket-level error
	};
	union ipv4_addr {
		uint32_t	addr32{};
		uint16_t	addr16[2];
		uint8_t		addr8[4];

		bool operator==(const ipv4_addr& other) const {	return (addr32 == other.addr32); }
	};
	struct SocketData {
		ipv4_addr		InAddress{};
		uint16_t		InPort{};
		SocketStatus	status{SocketStatus::NONBLOCKING};
	};

	struct PacketInfo {
		ipv4_addr fromAddr{};
		uint16_t fromPort{};
	};

	//===================================== PACKET HEADER ================================//
	enum PacketFlags : uint8_t {
		// EXCLUSIVE TYPE
		PAYLOAD		= 0,
		HEARTBEAT	= 1,
		CONNECTION	= 2,
		RESERVED	= 3,
		// BIT FLAGS
		  // CHANNEL ID
		UNRELIABLE	= 1 << 2,
		RELIABLE	= 1 << 3,
		SNAPSHOT	= 1 << 4,
		  // FRAGMENT
		FRAGMENT	= 1 << 5,
	};
	struct FragmentLoad {
		uint16_t batchNumber{};
		uint16_t FRAGMENT_COUNT{};
		uint16_t fragmentIndex{};
	};
	struct PeerHeader {
		uint32_t sessionID{};
		uint16_t peerID{};
	};
	struct PacketHeader {
		uint32_t		PROTOCOL_VERSION{ HEADER_VERSION };

		uint32_t		SequenceNumber{}; // Sequence of this packet being sent.
		uint32_t		ACKField{}; // Last 32 packets received
		uint32_t		LastSeqReceived{};

		PacketFlags		Flags{ PacketFlags::HEARTBEAT | PacketFlags::UNRELIABLE};
		uint8_t			padding[3];
		// if flags & Connection != 1, peerID, sessionID
		// If Fragmented: FragmentLoad	fragmentData{};
	};
	//======================================== Peer =============================//
	struct ChannelState {
		uint32_t	receivedACKField{};
		uint32_t	lastSent{};
		uint32_t	lastReceived{};
		uint16_t	batchNumber{};
		uint16_t	FRAGMENT_COUNT{};
	};

	struct Peer {
		std::array<ChannelState, CHANNELS> states; // 0 - Unreliable, 1 - Reliable Unordered, 2 - Snapshot
		uint32_t	sessionID{};
		uint32_t	lastSeenTime{};
		ipv4_addr	addr{};
		uint16_t	port{};
		uint16_t	peerID{};
	};

	//======================================== Connection ================================//
	enum class ConnectionState : uint8_t {
		DISCONNECTED,
		CONNECTING,
		CONNECTED,
		DROPPING,
	};
	struct PendingPeer {
		ipv4_addr	addr{};
		uint16_t	port{};
	};
	struct ReliabilityPolicy { // time in MS
		uint16_t resendDelay	= 350;
		uint16_t maxResendDelay = 3500;
		uint16_t disconnect		= 5500;
		uint16_t heartbeat		= 2350; // Time since last send
		uint16_t maxRetries		= UINT16_MAX;
	};
	//=========================================== DEBUG ===================================//
	struct NetworkStats {
		uint64_t packetsSent{};
		uint64_t packetsReceived{};
		uint64_t packetsDropped{};
		uint64_t bytesSent{};
		uint64_t bytesReceived{};
	};
}