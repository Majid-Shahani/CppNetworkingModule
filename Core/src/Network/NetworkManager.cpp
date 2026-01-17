#include <src/CNMpch.hpp>

#include <CNM/NetworkManager.h>
#include <ECS/World.h>

namespace {
	constexpr uint8_t TYPE_MASK = 0b00000111;
	constexpr int CHANNEL_MASK = Carnival::Network::PacketFlags::UNRELIABLE | 
		Carnival::Network::PacketFlags::RELIABLE | 
		Carnival::Network::PacketFlags::SNAPSHOT;

	uint32_t generateSessionID() {
		static thread_local std::mt19937 rng{ std::random_device{}()};
		static thread_local std::uniform_int_distribution<uint32_t> dist{1, UINT32_MAX};

		return dist(rng);
	}
}

namespace Carnival::Network {
	NetworkManager::NetworkManager(ECS::World* pWorld, const SocketData& sockData, uint16_t maxSessions)
		: m_callback{ pWorld }, m_MaxSessions{ maxSessions }
	{
		for (auto& sock : m_Socks) {
			sock.setInAddress(sockData.InAddress);
			sock.setPort(sockData.InPort);
			sock.setNonBlocking(sockData.status & SocketStatus::NONBLOCKING);
			sock.openSocket();
			sock.bindSocket();
		}
		m_PacketBuffer.reserve(PACKET_MTU);
	}
	void NetworkManager::pollIO()
	{
		m_PacketBuffer.clear();
		attemptConnect(m_Socks[1].getAddr(), m_Socks[1].getPort());
	}
	void NetworkManager::pollOngoing()
	{
	}
	void NetworkManager::pollIncoming()
	{
		for (auto& sock : m_Socks) {
			while (sock.hasPacket()) {
				m_PacketBuffer.clear();
				PacketInfo info = sock.receivePacket(m_PacketBuffer);
				if (m_PacketBuffer.size() != 0) {
					m_Stats.packetsReceived++;
					m_Stats.bytesReceived += m_PacketBuffer.size();

					// TODO: Check Against Drop List

					if (!handlePacket(info)) m_Stats.packetsDropped++;
				}
			}
		}
	}
	void NetworkManager::pollOutgoing()
	{
	}
	bool NetworkManager::attemptConnect(ipv4_addr addr, uint16_t port)
	{	
		// Prevent duplicate pending attempts
		for (const auto& p : m_PendingConnections) {
			if (p.addr == addr && p.port == port)
				return false;
		}

		m_PacketBuffer.clear();
		HeaderInfo header{
			.protocol = HEADER_VERSION,
			.flags = static_cast<PacketFlags>(PacketFlags::CONNECTION_REQUEST | PacketFlags::RELIABLE),
		};
		writeHeader(header);

		// Need to use a command buffer and return a future.
		// sockets cannot be used concurrently.
		if (bool res{ sendReliable(addr, port) }; res) {
			m_PendingConnections.emplace_back(addr, getTime(), port, 1);
			return true;
		}
		else return false;
	}

	void NetworkManager::writeHeader(const HeaderInfo& header)
	{
		auto append = [&](const auto& val) {
			const std::byte* p = reinterpret_cast<const std::byte*>(&val);
			m_PacketBuffer.insert(m_PacketBuffer.end(), p, p + sizeof(val));
		};
		
		CL_CORE_ASSERT(header.flags, "Header to be written needs flags"); // no validity check
		// TODO: flag Validity Check util
		CL_CORE_ASSERT(header.protocol == HEADER_VERSION, "Mismatch header versions!");

		append(HEADER_VERSION);
		append(header.flags);
		append(header.seqNum);
		append(header.ackField);
		append(header.lastSeqRecv);
		append(header.sessionID);

		if ((header.flags & PacketFlags::FRAGMENT) != 0)
			append(header.fragLoad);

		return;
	}

	HeaderInfo NetworkManager::parseHeader()
	{
		auto size = m_PacketBuffer.size();
		auto data = m_PacketBuffer.data();

		if (size < (sizeof(HeaderInfo::protocol) + sizeof(HeaderInfo::flags) +
			sizeof(PacketHeader::SequenceNumber) + sizeof(PacketHeader::ACKField) +
			sizeof(PacketHeader::LastSeqReceived) + sizeof(PacketHeader::sessionID)))
			return {};

		HeaderInfo info{};
		// Check header version.
		std::memcpy(&info.protocol, data, sizeof(info.protocol));
		info.offset += sizeof(info.protocol);

		if (info.protocol != HEADER_VERSION) return {};

		// Check flags
		std::memcpy(&info.flags, data + info.offset, sizeof(info.flags));
		info.offset += sizeof(info.flags);
		// maximum one channel
		auto channels{ info.flags & CHANNEL_MASK };
		if ((channels & (channels - 1)) != 0 || channels == 0) return {};

		std::memcpy(&info.seqNum, data + info.offset, sizeof(info.seqNum));
		info.offset += sizeof(info.seqNum);
		std::memcpy(&info.ackField, data + info.offset, sizeof(info.ackField));
		info.offset += sizeof(info.ackField);
		std::memcpy(&info.lastSeqRecv, data + info.offset, sizeof(info.lastSeqRecv));
		info.offset += sizeof(info.lastSeqRecv);
		std::memcpy(&info.sessionID, data + info.offset, sizeof(info.sessionID));
		info.offset += sizeof(info.sessionID);

		// Check for fragment bit and copy fragment load
		if (info.flags & PacketFlags::FRAGMENT) {
			if (size - info.offset < sizeof(FragmentLoad)) return {};
			std::memcpy(&info.fragLoad, data + info.offset, sizeof(info.fragLoad));
			info.offset += sizeof(info.fragLoad);
		}

		return info;
	}

	bool NetworkManager::updateSessionStats(Session& sesh,
		const PacketInfo packet,
		const HeaderInfo& header,
		const uint16_t endpointIndex,
		const uint16_t channelIndex)
	{
		auto& state = sesh.states[channelIndex];
		auto& ep = sesh.endpoint[endpointIndex];
		const uint32_t now = getTime();

		if (ep.state == ConnectionState::DISCONNECTED
			|| ep.state == ConnectionState::DROPPING)
			return false;

		// timeout not elapsed
		if (now - ep.lastRecvTime > m_Policy.disconnect)
			return false;

		// Sequence Must make sense
		if (header.lastSeqRecv + 32 < state.lastSent ||
			header.lastSeqRecv > state.lastSent) return false;
		
		// Too old to care
		if (header.seqNum + 32 < state.lastReceived) return false;
		
		// Too far ahead // Desync!
		if (header.seqNum > state.lastReceived + 32) return false;

		uint32_t diff{ state.lastSent - header.lastSeqRecv };
		// Nat Rebind
		if (packet.fromAddr != ep.addr || packet.fromPort != ep.port) {
			// ACK field validation
			uint32_t mask = (diff == 32) ? UINT32_MAX : ((1 << diff) - 1);
			if (header.ackField & ~mask) return false;
			if (state.lastSent != 0 && (header.ackField & mask) == 0) return false;

			ep.addr = packet.fromAddr;
			ep.port = packet.fromPort;
		}
		
		if (header.seqNum >= state.lastReceived) {
			state.sendingAckF <<= header.seqNum - state.lastReceived;
			state.lastReceived = header.seqNum;
			state.sendingAckF |= 1;
		}
		else {
			state.sendingAckF |= 1 << (state.lastReceived - header.seqNum);
		}

		ep.lastRecvTime = now;

		uint32_t peerAckMask = header.ackField << diff;
		peerAckMask |= (1u << diff);
		state.receivedACKField |= peerAckMask;

		return true;
	}

	bool NetworkManager::handlePacket(PacketInfo info)
	{
		HeaderInfo header{ parseHeader() };
		uint32_t payloadSize{ static_cast<uint32_t>(m_PacketBuffer.size() - header.offset) };
		// switch on flag
		switch (auto type{ header.flags & TYPE_MASK }; type) {
		case PacketFlags::INVALID:
			return false;
			break;
		case PacketFlags::CONNECTION_REQUEST:
			if (!(header.flags & PacketFlags::RELIABLE) ||
				header.flags & PacketFlags::FRAGMENT) return false;

			handleConnectionRequest(info, header);
			break;
		case PacketFlags::CONNECTION_ACCEPT:
			if (!(header.flags & PacketFlags::RELIABLE) ||
				header.flags & PacketFlags::FRAGMENT) return false;

			return handleConnectionAccept(info, header);
			break;
		case PacketFlags::CONNECTION_REJECT:
			if (!(header.flags & PacketFlags::RELIABLE) ||
				header.flags & PacketFlags::FRAGMENT) return false;

			return handleConnectionReject(info, header);
			break;
		default:
			return false;
		}

		return true;
	}

	void NetworkManager::handleConnectionRequest(PacketInfo info, const HeaderInfo& header)
	{
		// check existing sessions
		if (header.sessionID != 0) {
			if (auto it{ m_Sessions.find(header.sessionID) }; it != m_Sessions.end()) {
				auto& localEndPoint = it->second.endpoint[1];
				auto state = localEndPoint.state;

				if (state == ConnectionState::DROPPING || state == ConnectionState::DISCONNECTED) { 
					rejectConnection(info.fromAddr, info.fromPort); 
					return;
				}
				if (localEndPoint.addr == info.fromAddr && localEndPoint.port == info.fromPort) {
					localEndPoint.state = ConnectionState::CONNECTED;
					acceptConnection(it->first);
					return;
				}
				if (updateSessionStats(it->second, info, header, 1, 1))	acceptConnection(it->first);
				else rejectConnection(info.fromAddr, info.fromPort);
			}
			else	rejectConnection(info.fromAddr, info.fromPort);
			return;
		}

		// Packet session id == 0, 
#ifdef CL_DEBUG
		for (auto& [id, sesh] : m_Sessions) {
			if (sesh.endpoint[1].addr == info.fromAddr && sesh.endpoint[1].port == info.fromPort) {
				CL_CORE_ASSERT(id == 0, "Wrong Session ID for request connection!");
				// possibly just accept connection and send correct sessionID

				/*
				if (sesh.endpoint[1].state == ConnectionState::DROPPING) {
					rejectConnection(info.fromAddr, info.fromPort);
					return;
				}
				sesh.endpoint[1].state = ConnectionState::CONNECTED;
				acceptConnection(id);
				return;
				*/
			}
		}
#endif

		// check max vs curr sessions
		if (m_Sessions.size() >= m_MaxSessions) {
			rejectConnection(info.fromAddr, info.fromPort);
			return;
		}

		// check pending connections
		for (auto it{ m_PendingConnections.begin() }; it != m_PendingConnections.end(); ++it) {
			if (it->addr == info.fromAddr && it->port == info.fromPort) {
				uint32_t sessionID{ createSession(*it) };
				m_PendingConnections.erase(it);
				acceptConnection(sessionID);
				return;
			}
		}

		// create new connect
		// for now just accept
		PendingPeer peer{
			.addr = info.fromAddr,
			.lastSendTime = getTime(),
			.port = info.fromPort,
			.retryCount = 1,
		};
		uint32_t ID{ createSession(peer) };
		acceptConnection(ID);
	}

	bool NetworkManager::handleConnectionAccept(const PacketInfo packet, const HeaderInfo& header)
	{
		if (!header.sessionID) return false;
		
		if (m_Sessions.contains(header.sessionID)) {
			auto& sesh{ m_Sessions.at(header.sessionID) };
			if (sesh.endpoint[1].state == ConnectionState::DISCONNECTED ||
				sesh.endpoint[1].state == ConnectionState::DROPPING) return true;
			if (updateSessionStats(sesh, packet, header, 1, 1))
				return true;
			else return false;
		}

		for (auto& pending : m_PendingConnections) {
			if (pending.addr == packet.fromAddr && pending.port == packet.fromPort) {
				if (createSession(pending, header.sessionID))
					return true;
				else { // ID collision
					std::print("ID Collision when creating session from connectionAccept: {}", header.sessionID);
					return true;				}
			}
		}

		return false;
	}

	bool NetworkManager::handleConnectionReject(const PacketInfo packet, const HeaderInfo& header)
	{
		for (auto it = m_PendingConnections.begin(); it != m_PendingConnections.end(); it++) {
			if (it->addr == packet.fromAddr && it->port == packet.fromPort) {
				m_PendingConnections.erase(it);
				return true;
			}
		}
		return false;
	}

	void NetworkManager::handlePayload(PacketInfo info, const HeaderInfo& header)
	{
	}

	uint32_t NetworkManager::createSession(const PendingPeer& info)
	{
		while (true) {
			uint32_t id{ generateSessionID() };
			if (!createSession(info, id)) continue;
			return id;
		}
	}
	bool NetworkManager::createSession(const PendingPeer& info, uint32_t key)
	{
		auto [it, inserted] = m_Sessions.try_emplace(key, Session{});
		if (!inserted) return false;
		Session& sesh = it->second;
		auto& reliable_end = sesh.endpoint[1];

		reliable_end.addr = info.addr;
		reliable_end.port = info.port;
		reliable_end.state = ConnectionState::CONNECTED;
		reliable_end.lastRecvTime = getTime();
		reliable_end.lastSentTime = info.lastSendTime;

		sesh.endpoint[0].state = ConnectionState::CONNECTING;

		return true;
	}

	void NetworkManager::acceptConnection(uint32_t sessionID)
	{
		m_PacketBuffer.clear();
		auto& sesh = m_Sessions.at(sessionID);

		CL_CORE_ASSERT(sesh.endpoint[1].state == ConnectionState::CONNECTED, 
			"Endpoint must be connected before sending");
		sesh.states[1].receivedACKField <<= 1;
		HeaderInfo info{
			.protocol{ HEADER_VERSION },
			.seqNum{sesh.states[1].lastSent++},
			.ackField{sesh.states[1].sendingAckF},
			.lastSeqRecv{sesh.states[1].lastReceived},
			.sessionID{sessionID},
			.flags = static_cast<PacketFlags>(PacketFlags::CONNECTION_ACCEPT | PacketFlags::RELIABLE),
		};
		writeHeader(info);
		sendReliable(sesh.endpoint[1].addr, sesh.endpoint[1].port);
	}

	void NetworkManager::rejectConnection(ipv4_addr addr, uint16_t port)
	{
		m_PacketBuffer.clear();
		HeaderInfo info{
			.protocol = HEADER_VERSION,
			.flags = static_cast<PacketFlags>(PacketFlags::CONNECTION_REJECT | PacketFlags::RELIABLE),
		};
		writeHeader(info);
		sendReliable(addr, port);
	}

	uint32_t NetworkManager::getTime()
	{
		static const auto start = std::chrono::steady_clock::now();
		return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
	}

	bool NetworkManager::sendReliable(ipv4_addr addr, uint16_t port)
	{
		if (auto res{ m_Socks[1].sendPackets(m_PacketBuffer, addr, port) }; res) {
			m_Stats.bytesSent += m_PacketBuffer.size();
			m_Stats.packetsSent++;

			return true;
		}
		return false;
	}
	bool NetworkManager::sendUnreliable(ipv4_addr addr, uint16_t port)
	{
		if (auto res{ m_Socks[0].sendPackets(m_PacketBuffer, addr, port) }; res) {
			m_Stats.bytesSent += m_PacketBuffer.size();
			m_Stats.packetsSent++;

			return true;
		}
		return false;
	}
	/*
	void NetworkManager::sendSnapshot(ipv4_addr addr, uint16_t port)
	{
		TO BE IMPLEMENTED
	}
	*/
}