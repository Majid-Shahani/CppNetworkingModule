#pragma once

#include <cstdint>
#include <CNM/utils.h>

namespace Carnival::Network {

	// Maximum packet payload size chosen to avoid IP fragmentation on typical networks
	constexpr static uint32_t PACKET_MTU{ 1200 };

	// Protocol identifier hashed at compile time to reject incompatible clients
	static constexpr uint32_t	HEADER_VERSION	= utils::fnv1a32("CarnivalEngine.Network_UDP_0.0.1");
	static constexpr uint8_t	CHANNELS		= 3; // Logical channels mapped to different reliability/ordering guarantees
	static constexpr uint8_t	SOCKET_COUNT	= CHANNELS - 1; // 0 - High Frequency Unreliable, 1 - Reliable, Snapshots

	// Logical packet channels
	enum CH : uint8_t {
		CH_UNRELIABLE = 0,
		CH_RELIABLE = 1,
		CH_SNAPSHOT = 2,
	};
	// Physical socket endpoints
	enum EP : uint8_t {
		EP_UNRELIABLE = 0,
		EP_RELIABLE = 1,
	};

	// current socket configuration and lifecycle state
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

	// the sender of a received packet
	struct PacketInfo {
		ipv4_addr fromAddr{};
		uint16_t fromPort{};
	};
	enum class PollResult : uint8_t {
		None,
		Packet,
		Error,
	};
	enum class SocketError : uint8_t {
		None,
		Transient,
		Remote,
		Fatal
	};
	//===================================== PACKET HEADER ================================//

	enum PacketFlags : uint8_t {
		// EXCLUSIVE TYPE
		INVALID				= 0,
		HEARTBEAT			= 1,
		CONNECTION_REQUEST	= 2,
		CONNECTION_ACCEPT	= 3,
		CONNECTION_REJECT	= 4,
		STATE_LOAD			= 5,
		EVENT_LOAD			= 6,
		ACKNOWLEDGEMENT		= 7,
		// CHANNEL ID
		UNRELIABLE	= 1 << 3,
		RELIABLE	= 1 << 4,
		SNAPSHOT	= 1 << 5,
		  // FRAGMENT
		FRAGMENT	= 1 << 6,
	};
	struct FragmentLoad {
		uint16_t batchNumber{};
		uint16_t FRAGMENT_COUNT{};
		uint16_t fragmentIndex{};
	};
	// Wire Format
	struct PacketHeader {
		uint32_t	PROTOCOL_VERSION{ HEADER_VERSION };
		PacketFlags	Flags{ PacketFlags::HEARTBEAT | PacketFlags::UNRELIABLE};
		uint32_t	SequenceNumber{}; // Sequence of this packet being sent.
		uint32_t	ACKField{}; // Last 32 packets received
		uint32_t	LastSeqReceived{};
		uint32_t	sessionID{};
		// If Fragmented: 
		FragmentLoad fragmentData{};
	};

	// Local Format
	struct HeaderInfo {
		uint32_t protocol{};
		uint32_t seqNum{};
		uint32_t ackField{};
		uint32_t lastSeqRecv{};
		uint32_t sessionID{};
		FragmentLoad fragLoad{};
		PacketFlags flags{ INVALID };
		uint8_t offset{}; // Byte offset to payload
	};

	//======================================== Connection ================================//
	
	// Connection Lifecycle
	enum class ConnectionState : uint8_t {
		CONNECTING, // Handshake not complete
		CONNECTED, // Alive
		DROPPING, // Explicit Teardown, Cannot Revive!
		TIMEOUT, // Can Revive!
	};
	// Temporary State in handshakes
	struct PendingPeer {
		uint64_t	lastSendTime{}; // in MicroSecond
		ipv4_addr	addr{};
		uint16_t	port{};
		uint16_t	retryCount{};
	};

	struct ReliabilityPolicy { // time in MicroSeconds
		uint32_t resendDelay	= 350'000;
		uint32_t maxResendDelay = 350'000'0;
		uint32_t disconnect		= 5'500'000;
		uint32_t heartbeat		= 2'350'000; // Time since last send
		uint32_t maxRetries		= 10;
	};
	//======================================== Session =============================//

	struct ChannelState {
		uint32_t	receivedACKField{}; // Acks of sent messages
		uint32_t	lastSent{}; // last sent sequence number
		uint32_t	sendingAckF{};
		uint32_t	lastReceived{}; // last received sequence number
		uint16_t	batchNumber{};
		uint16_t	FRAGMENT_COUNT{};
	};
	// Remote endpoint activity and data
	struct Endpoint {
		uint64_t	lastRecvTime{}; // in MicroSecond
		uint64_t	lastSentTime{}; // in MicroSecond
		ipv4_addr	addr{};
		uint16_t	port{};
		ConnectionState state{ ConnectionState::CONNECTING };
	};
	// Logical Connection
	struct Session {
		std::array<Endpoint, SOCKET_COUNT>	endpoint; // 0 - High Frequency Unreliable, 1 - Reliable, Snapshots
		std::array<ChannelState, CHANNELS>	states; // 0 - Unreliable, 1 - Reliable Unordered, 2 - Snapshot

		uint64_t graceTimer{};
	};

	//=========================================== Command ===================================//

	// Descriptors in resend Arena 
	struct PacketDescriptor {
		std::unique_ptr<std::byte[]> pData; // serialized data buffer
		uint64_t size{}; // must be less than MTU
		Session* sesh{ nullptr };
		uint64_t lastSendTime{};
		uint32_t sequenceNum{};
		uint32_t sessionID{};
		uint16_t resendCount{};
		bool Acked{ false };

		PacketDescriptor(std::byte* data,
			uint64_t size, Session* s, uint32_t seq, uint32_t ID)
			:size{ size }, sesh{ s }, sequenceNum{ seq },
			sessionID{ ID },
			resendCount{ 0 }, Acked{ false },
			lastSendTime{ 0 } {
			pData = std::move(std::unique_ptr<std::byte[]>(data));
		}

		PacketDescriptor(const PacketDescriptor&) = delete;
		PacketDescriptor& operator=(const PacketDescriptor&) = delete;
		PacketDescriptor(PacketDescriptor&& other) = default;
		PacketDescriptor& operator=(PacketDescriptor&& other) = default;
	};

	struct NetCommand {
		NetCommand(Session* s, uint32_t id, PacketFlags t)
			: ep{ .sesh = s }, sessionID{ id }, type{ t } {}
		
		NetCommand(PacketDescriptor* data, PacketFlags t)
			: ep{ .descriptor{data} }, type{ t } {}
		
		NetCommand(ipv4_addr addr, uint16_t port, PacketFlags t)
			: ep{ .endpoint{.addr = addr, .port = port} }, type{ t } {}

		union { // Can be active session or raw endpoint
			Session* sesh{ nullptr };
			PacketDescriptor* descriptor;
			struct {
				ipv4_addr addr{};
				uint16_t port{};
				uint16_t padding{};
			} endpoint;
		} ep;
		uint32_t sessionID{};
		PacketFlags type{};
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