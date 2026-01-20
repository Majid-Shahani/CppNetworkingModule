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
	NetworkManager::NetworkManager(ECS::World* pWorld,
		const SocketData& sockData, uint16_t maxSessions)
		: m_pWorld{ pWorld }, m_MaxSessions{ maxSessions }
	{
		for (auto& sock : m_Socks) {
			sock.setInAddress(sockData.InAddress);
			sock.setPort(sockData.InPort);
			sock.setNonBlocking(sockData.status & SocketStatus::NONBLOCKING);
			sock.openSocket();
			sock.bindSocket();
		}
		m_PacketBuffer.reserve(PACKET_MTU);
		m_CommandBuffer.reserve(35);
		m_PendingConnections.reserve((m_MaxSessions > 32 ? 32 : m_MaxSessions));
	}

	uint64_t NetworkManager::getTime() noexcept
	{
		static const auto start = std::chrono::steady_clock::now();
		return (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
	}

	void NetworkManager::maintainSessions()
	{
		auto now = getTime();

		// Retry Pending Connections
		for (auto it{ m_PendingConnections.begin() }; it != m_PendingConnections.end();) {
			if (now - it->lastSendTime > m_Policy.resendDelay) {
				it->retryCount++;
				if (it->retryCount > m_Policy.maxRetries) {
					it = m_PendingConnections.erase(it);
					continue;
				}
				m_CommandBuffer.emplace_back(it->addr, it->port,
					static_cast<PacketFlags>(PacketFlags::CONNECTION_REQUEST | PacketFlags::RELIABLE));
				it->lastSendTime = now;
			}
			it++;
		}

		// Submit Heartbeats and Detect timeout
		for (auto& [id, sesh] : m_Sessions) {
			for (int i{}; i < sesh.endpoint.size(); i++) {
				auto& ep = sesh.endpoint[i];
				if (!(ep.state == ConnectionState::CONNECTED)) continue;

				if (now - ep.lastRecvTime > m_Policy.disconnect) {
					ep.state = ConnectionState::TIMEOUT;
					break;
				}

				if (now - ep.lastSentTime > m_Policy.heartbeat) {
					m_CommandBuffer.emplace_back(&sesh, id,
						static_cast<PacketFlags>(PacketFlags::HEARTBEAT | (1 << (3 + i)) ));
				}
			}
		}
	}
	void NetworkManager::opportunisticReceive()
	{
		while (true) {
			uint64_t now{ getTime() };
			if (now >= m_NextTick) return;
			uint64_t remainingTimeUs = m_NextTick - now;
			if (remainingTimeUs <= 1500) return;

			uint8_t res{ Socket::waitForPackets(static_cast<int32_t>(remainingTimeUs / 1000),
				m_Socks[0].getHandle(), m_Socks[1].getHandle()) };

			if (res && !(res & (1 << 3))) collectIncoming();
		}
	}
	void NetworkManager::collectIncoming()
	{
		// Reliable
		while (m_Socks[1].hasPacket()) {
			m_PacketBuffer.clear();
			PacketInfo info = m_Socks[1].receivePacket(m_PacketBuffer);
			if (m_PacketBuffer.size() != 0) {
				m_Stats.packetsReceived++;
				m_Stats.bytesReceived += m_PacketBuffer.size();

				// TODO: Check Against Drop List

				if (!handleReliablePacket(info)) m_Stats.packetsDropped++;
			}
		}
		// Unreliable
		while (m_Socks[0].hasPacket()) {
			m_PacketBuffer.clear();
			PacketInfo info = m_Socks[0].receivePacket(m_PacketBuffer);
			if (m_PacketBuffer.size() != 0) {
				m_Stats.packetsReceived++;
				m_Stats.bytesReceived += m_PacketBuffer.size();

				if (!handleUnreliablePacket(info)) m_Stats.packetsDropped++;
			}
		}
	}
	void NetworkManager::processCommands()
	{
		for (auto& cmd : m_CommandBuffer) {
			auto channel{ cmd.type & CHANNEL_MASK };
			auto type{ cmd.type & TYPE_MASK };

			if (channel & PacketFlags::RELIABLE) {
				switch (type) {
				case PacketFlags::CONNECTION_REQUEST:
					if (cmd.type & PacketFlags::FRAGMENT) break;
					sendRequest(cmd.ep.endpoint.addr, cmd.ep.endpoint.port);
					break;

				case PacketFlags::CONNECTION_ACCEPT:
					if (cmd.type & PacketFlags::FRAGMENT) break;
					sendAccept(cmd.sessionID, *cmd.ep.sesh);
					break;

				case PacketFlags::CONNECTION_REJECT:
					if (cmd.type & PacketFlags::FRAGMENT) break;
					sendReject(cmd.ep.endpoint.addr, cmd.ep.endpoint.port);
					break;

				default:
					break;
				}
			}
			else if (channel & PacketFlags::UNRELIABLE) {

			}
			else {
				// snapshot
			}
		}
		m_CommandBuffer.clear();
	}

	void NetworkManager::run(uint16_t tickRate)
	{
		// temp
		attemptConnect({ (127 << 24) | 1 }, m_Socks[1].getPort());

		//
		CL_CORE_ASSERT(tickRate && ((tickRate & (tickRate - 1)) == 0), "TickRate should be a power of two");
		CL_CORE_ASSERT(tickRate < 1024, "Windows Timer granularity is 1ms, 1024hz breaks sleep");
		m_Running.test_and_set(std::memory_order::release);
		m_Running.notify_all();

		uint32_t tickCounter{};
		const uint32_t tickDiffUs{ 1'000'000ul >> std::countr_zero(tickRate) };
		const uint32_t remainder{ 1'000'000ul & (tickRate - 1)};
		uint32_t frac{};
		m_NextTick = getTime() + tickDiffUs;

		while (!m_ShouldStop.test(std::memory_order::acquire)) {
			
			tickCounter++;
			tickCounter = tickCounter & (tickRate - 1);
			if (tickCounter == 0) { // Run once a second
				// cleanupDropping();

				std::cout << "\033[2J\033[H" << std::flush;
				std::print("Net Stats:\n  Packets:\n    Sent: {}\n    Received: {}\n    Dropped: {}\n",
					m_Stats.packetsSent, m_Stats.packetsReceived, m_Stats.packetsDropped);
				std::print("  Bytes:\n    Sent: {}\n    Received: {}\n",
					m_Stats.bytesSent, m_Stats.bytesReceived);
			}
			
			maintainSessions();
			collectIncoming();
			opportunisticReceive();
			processCommands();

			// Less than 1ms Busy wait
			uint64_t now = getTime();
			while (now < m_NextTick) {
				SpinPause();
				now = getTime();
			}

			// Advance Tick
			m_NextTick += tickDiffUs;
			frac += remainder;
			while (now >= m_NextTick) {
				m_NextTick += tickDiffUs;
				frac += remainder;
			}
			while (frac >= tickRate) {
				m_NextTick++;
				frac -= tickRate;
			}
		}
		m_NextTick = 0;
		m_Running.clear(std::memory_order::release);
		m_Running.notify_all();
	}
	void NetworkManager::stop()
	{
		m_Running.wait(false, std::memory_order::acquire);
		m_ShouldStop.test_and_set(std::memory_order::release);
		m_Running.wait(true, std::memory_order::acquire);
		m_ShouldStop.clear(std::memory_order::release);
	}

	void NetworkManager::attemptConnect(ipv4_addr addr, uint16_t port)
	{	
		m_CommandBuffer.emplace_back(addr,
			port, static_cast<PacketFlags>(PacketFlags::CONNECTION_REQUEST | PacketFlags::RELIABLE));

		// Prevent duplicate peer creation
		for (auto& p : m_PendingConnections) {
			if (p.addr == addr && p.port == port) {
				p.retryCount++;
				p.lastSendTime = getTime(); // not actually the time sent but last time requested
				return;
			}
		}
		m_PendingConnections.emplace_back(getTime(), addr, port, 0);
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
		const uint64_t now = getTime();

		if (ep.state == ConnectionState::DROPPING)
			return false;

		// timeout not elapsed
		if (ep.state == ConnectionState::CONNECTED 
			&& now - ep.lastRecvTime > m_Policy.disconnect)
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
			ep.state = ConnectionState::CONNECTED;
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

	inline bool NetworkManager::handleReliablePacket(const PacketInfo info)
	{
		HeaderInfo header{ parseHeader() };
		uint32_t payloadSize{ static_cast<uint32_t>(m_PacketBuffer.size() - header.offset) };

		if (auto channel{ header.flags & CHANNEL_MASK }; channel != PacketFlags::RELIABLE)
			return false;

		// switch on flag
		switch (auto type{ header.flags & TYPE_MASK }; type) {
		case PacketFlags::INVALID:
			return false;
			break;

		case PacketFlags::CONNECTION_REQUEST:
			if (header.flags & PacketFlags::FRAGMENT) return false;
			handleConnectionRequest(info, header);
			break;

		case PacketFlags::CONNECTION_ACCEPT:
			if (header.flags & PacketFlags::FRAGMENT) return false;
			return handleConnectionAccept(info, header);
			break;

		case PacketFlags::CONNECTION_REJECT:
			if (header.flags & PacketFlags::FRAGMENT) return false;
			return handleConnectionReject(info, header);
			break;

		default:
			return false;
		}

		return true;
	}
	inline bool NetworkManager::handleUnreliablePacket(const PacketInfo info)
	{
		HeaderInfo header{ parseHeader() };
		uint32_t payloadSize{ static_cast<uint32_t>(m_PacketBuffer.size() - header.offset) };

		if (auto channel{ header.flags & CHANNEL_MASK }; channel != PacketFlags::UNRELIABLE)
			return false;

		switch (auto type{ header.flags & TYPE_MASK }; type) {
		case PacketFlags::INVALID:
			return false;
			break;

		default:
			return false;
		}

		return true;
	}

	inline void NetworkManager::handleConnectionRequest(PacketInfo info, const HeaderInfo& header)
	{
		// check existing sessions
		if (header.sessionID != 0) {
			if (auto it{ m_Sessions.find(header.sessionID) }; it != m_Sessions.end()) {
				auto& localEndPoint = it->second.endpoint[1];
				auto state = localEndPoint.state;

				if (state == ConnectionState::DROPPING) { 
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
			.lastSendTime = getTime(),
			.addr = info.fromAddr,
			.port = info.fromPort,
			.retryCount = 1,
		};
		uint32_t ID{ createSession(peer) };
		acceptConnection(ID);
	}
	inline bool NetworkManager::handleConnectionAccept(const PacketInfo packet, const HeaderInfo& header)
	{
		if (!header.sessionID) return false;
		
		if (m_Sessions.contains(header.sessionID)) {
			std::print("Session {} Found.\n", header.sessionID);
			auto& sesh{ m_Sessions.at(header.sessionID) };
			if (sesh.endpoint[1].state == ConnectionState::DROPPING) return true;
			if (updateSessionStats(sesh, packet, header, 1, 1))
				return true;
			else return false;
		}

		for (auto& pending : m_PendingConnections) {
			if (pending.addr == packet.fromAddr && pending.port == packet.fromPort) {
				std::print("Peer found! creating session {}\n", header.sessionID);
				if (createSession(pending, header.sessionID))
					return true;
				else { // ID collision
					std::print("ID Collision when creating session from connectionAccept: {}", header.sessionID);
					return true;
				}
			}
		}

		return false;
	}
	inline bool NetworkManager::handleConnectionReject(const PacketInfo packet, const HeaderInfo& header)
	{
		for (auto it = m_PendingConnections.begin(); it != m_PendingConnections.end(); it++) {
			if (it->addr == packet.fromAddr && it->port == packet.fromPort) {
				m_PendingConnections.erase(it);
				return true;
			}
		}
		return false;
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

	inline void NetworkManager::sendRequest(ipv4_addr addr, uint16_t port) noexcept
	{
		m_PacketBuffer.clear();
		HeaderInfo info{
			.protocol = HEADER_VERSION,
			.flags = static_cast<PacketFlags>(PacketFlags::CONNECTION_REQUEST | PacketFlags::RELIABLE),
		};
		writeHeader(info);
		sendReliable(addr, port);
	}
	inline void NetworkManager::sendAccept(uint32_t sessionID, Session& sesh) noexcept
	{
		m_PacketBuffer.clear();

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
		sendReliable(sesh.endpoint[1]);
	}
	inline void NetworkManager::sendReject(ipv4_addr addr, uint16_t port) noexcept
	{
		m_PacketBuffer.clear();
		HeaderInfo info{
			.protocol = HEADER_VERSION,
			.flags = static_cast<PacketFlags>(PacketFlags::CONNECTION_REJECT | PacketFlags::RELIABLE),
		};
		writeHeader(info);
		sendReliable(addr, port);
	}

	inline bool NetworkManager::sendReliable(ipv4_addr addr, uint16_t port) noexcept
	{
		if (auto res{ m_Socks[1].sendPackets(m_PacketBuffer, addr, port) }; res) {
			m_Stats.bytesSent += m_PacketBuffer.size();
			m_Stats.packetsSent++;

			return true;
		}
		return false;
	}
	inline bool NetworkManager::sendReliable(Endpoint& ep) noexcept
	{
		if (auto res{ m_Socks[1].sendPackets(m_PacketBuffer, ep.addr, ep.port) }; res) {
			m_Stats.bytesSent += m_PacketBuffer.size();
			m_Stats.packetsSent++;
			ep.lastSentTime = getTime();
			return true;
		}
		return false;
	}

	inline bool NetworkManager::sendUnreliable(ipv4_addr addr, uint16_t port) noexcept
	{
		if (auto res{ m_Socks[0].sendPackets(m_PacketBuffer, addr, port) }; res) {
			m_Stats.bytesSent += m_PacketBuffer.size();
			m_Stats.packetsSent++;

			return true;
		}
		return false;
	}
	inline bool NetworkManager::sendUnreliable(Endpoint& ep) noexcept
	{
		if (auto res{ m_Socks[0].sendPackets(m_PacketBuffer, ep.addr, ep.port) }; res) {
			m_Stats.bytesSent += m_PacketBuffer.size();
			m_Stats.packetsSent++;
			ep.lastSentTime = getTime();
			return true;
		}
		return false;
	}


	void NetworkManager::handlePayload(PacketInfo info, const HeaderInfo& header)
	{
	}

	/*
	void NetworkManager::sendSnapshot(ipv4_addr addr, uint16_t port)
	{
		TO BE IMPLEMENTED
	}
	*/
}