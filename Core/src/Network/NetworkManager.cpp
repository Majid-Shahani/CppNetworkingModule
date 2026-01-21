#include <src/CNMpch.hpp>

#include <CNM/NetworkManager.h>
#include <ECS/World.h>

using namespace Carnival::Engine;

namespace {
	constexpr uint8_t TYPE_MASK = 0b00000111;
	constexpr uint8_t CHANNEL_MASK = Carnival::Network::UNRELIABLE | 
		Carnival::Network::RELIABLE | 
		Carnival::Network::SNAPSHOT;

	uint32_t generateSessionID() {
		static thread_local std::mt19937 rng{ std::random_device{}()};
		static thread_local std::uniform_int_distribution<uint32_t> dist{1, UINT32_MAX};

		return dist(rng);
	}
}

namespace Carnival::Network {
	NetworkManager::NetworkManager(ECS::World* pWorld,
		const SocketData& relSockData, const SocketData& urelSockData,
		uint16_t maxSessions)
		: m_pWorld{ pWorld }, m_MaxSessions{ maxSessions }
	{
		m_Socks[0].setInAddress(urelSockData.InAddress);
		m_Socks[0].setPort(urelSockData.InPort);
		m_Socks[0].setNonBlocking(urelSockData.status & SocketStatus::NONBLOCKING);
		m_Socks[0].openSocket();
		m_Socks[0].bindSocket();
		m_Socks[1].setInAddress(relSockData.InAddress);
		m_Socks[1].setPort(relSockData.InPort);
		m_Socks[1].setNonBlocking(relSockData.status & SocketStatus::NONBLOCKING);
		m_Socks[1].openSocket();
		m_Socks[1].bindSocket();

		m_PacketBuffer.reserve(PACKET_MTU);
		m_CommandBuffer.reserve(35);
		m_PendingConnections.reserve((m_MaxSessions > 32 ? 32 : m_MaxSessions));
	}

	void NetworkManager::maintainSessions()
	{
		auto now = getTime();

		// Retry Pending Connections
		for (auto it{ m_PendingConnections.begin() }; it != m_PendingConnections.end();) {
			if (now - it->lastSendTime > m_Policy.resendDelay) {
				// Resend Connection Request
				it->retryCount++;
				if (it->retryCount > m_Policy.maxRetries) {
					it = m_PendingConnections.erase(it);
					continue;
				}
				m_CommandBuffer.emplace_back(it->addr, it->port,
					static_cast<PacketFlags>(CONNECTION_REQUEST | RELIABLE));
				it->lastSendTime = now;
			}
			it++;
		}

		// Detect timeout
		// Replicate World
		// Submit Heartbeats
		for (auto& [id, sesh] : m_Sessions) {
			auto handleEndpoint = [&](auto& ep, PacketFlags flag) {
				if (ep.state == ConnectionState::DROPPING || ep.state == ConnectionState::TIMEOUT)
					return;

				// Timeout
				if (now - ep.lastRecvTime > m_Policy.disconnect) {
					ep.state = ConnectionState::TIMEOUT;
					return;
				}

				// Queue payload
				if (flag == RELIABLE) {
					queueReliablePayload(id, sesh);
				}
				// else queueUnreliablePayload(id, sesh);
				 
				// Heartbeat
				if (now - ep.lastSentTime > m_Policy.heartbeat) {
					m_CommandBuffer.emplace_back(&sesh, id, 
						static_cast<PacketFlags>(flag | HEARTBEAT));
				}
			};
			handleEndpoint(sesh.endpoint[EP_RELIABLE], RELIABLE);
			handleEndpoint(sesh.endpoint[EP_UNRELIABLE], UNRELIABLE);
		}
	}
	void NetworkManager::opportunisticReceive()
	{
		while (true) {
			uint64_t now{ getTime() };
			if (now >= m_NextTick) return;
			uint64_t remainingTimeUs = m_NextTick - now;
			// Skip short Waits, reduce jitter
			if (remainingTimeUs <= 1500) return;

			// sleep until timeout or wake up if packet / error on sockets
			PollResult res{ Socket::waitForPackets(static_cast<int32_t>(remainingTimeUs / 1000),
				m_Socks[0].getHandle(), m_Socks[1].getHandle()) };

			if (res == PollResult::Packet) collectIncoming();
			else if (res == PollResult::Error) {
				handleError();
			}
		}
	}
	void NetworkManager::collectIncoming()
	{
		PollResult res{};
		do {
			// Reliable
			res = m_Socks[EP_RELIABLE].poll();
			if (res == PollResult::Packet) {
				m_PacketBuffer.clear();
				PacketInfo info = m_Socks[EP_RELIABLE].receivePacket(m_PacketBuffer);
				if (m_PacketBuffer.size() != 0) {
					m_Stats.packetsReceived++;
					m_Stats.bytesReceived += m_PacketBuffer.size();

					// TODO: Check Against Drop List
					// Drop Packet if Invalid
					if (!handleReliablePacket(info)) m_Stats.packetsDropped++;
				}
			}
			if (res == PollResult::Error) {
				// Handle Error
				handleError(m_Socks[EP_RELIABLE]);
			}

		} while (res != PollResult::None);

		do {
			// Unreliable
			auto res = m_Socks[EP_UNRELIABLE].poll();
			if (res == PollResult::Packet) {
				m_PacketBuffer.clear();
				PacketInfo info = m_Socks[EP_UNRELIABLE].receivePacket(m_PacketBuffer);
				if (m_PacketBuffer.size() != 0) {
					m_Stats.packetsReceived++;
					m_Stats.bytesReceived += m_PacketBuffer.size();
					// Drop Packet if Invalid
					if (!handleUnreliablePacket(info)) m_Stats.packetsDropped++;
				}
			}
			if (res == PollResult::Error) {
				// Socket Error
				handleError(m_Socks[EP_UNRELIABLE]); 
			}
		} while (res != PollResult::None);
	}
	void NetworkManager::queueResends()
	{
		auto now = Engine::getTime();
		for (auto it{ m_ResendBuffer.begin() }; it != m_ResendBuffer.end();) {
			auto& state = it->sesh->states[CH_RELIABLE];
			// Check for Ack
			if (!it->Acked) {
				auto diff = state.lastSent - it->sequenceNum;
				if (diff > 32) {
					it = m_ResendBuffer.erase(it);
					continue;
				}
				it->Acked = ((state.receivedACKField >> diff) & 1);
			}
			if (it->Acked) {
				it = m_ResendBuffer.erase(it);
				continue;
			}

			if ((now - it->lastSendTime) > m_Policy.resendDelay) {
				m_CommandBuffer.emplace_back(&(*it), static_cast<PacketFlags>(STATE_LOAD | RELIABLE));
				it->lastSendTime = now;
			}

			it++;
		}
	}
	void NetworkManager::processCommands()
	{
		for (auto& cmd : m_CommandBuffer) {
			auto channel{ cmd.type & CHANNEL_MASK };
			auto type{ cmd.type & TYPE_MASK };
			if (channel & RELIABLE) {
				switch (type) {
				case CONNECTION_REQUEST:
					CL_CORE_ASSERT(!(cmd.type & FRAGMENT), "Connection request has fragment bit!");
					sendRequest(cmd.ep.endpoint.addr, cmd.ep.endpoint.port);
					break;

				case CONNECTION_ACCEPT:
					CL_CORE_ASSERT(!(cmd.type & FRAGMENT), "Connection accept has fragment bit!");
					sendAccept(cmd.sessionID, *cmd.ep.sesh);
					break;

				case CONNECTION_REJECT:
					CL_CORE_ASSERT(!(cmd.type & FRAGMENT), "Connection reject has fragment bit!");
					sendReject(cmd.ep.endpoint.addr, cmd.ep.endpoint.port);
					break;

				case HEARTBEAT:
					CL_CORE_ASSERT(!(cmd.type & FRAGMENT), "heartbeat has fragment bit!");
					sendHeartbeat(cmd.sessionID, *cmd.ep.sesh, EP_RELIABLE, CH_RELIABLE);
					break;

				case STATE_LOAD:
				case EVENT_LOAD:
					sendReliablePayload(*cmd.ep.descriptor);
					break;
				default:
					CL_CORE_ASSERT(false, "Wrong Packet type and channel combo command!");
					break;
				}
			}
			else if (channel & UNRELIABLE) {
				switch (type) {
				case HEARTBEAT:
					CL_CORE_ASSERT(!(cmd.type & FRAGMENT), "heartbeat has fragment bit!");
					sendHeartbeat(cmd.sessionID, *cmd.ep.sesh, EP_UNRELIABLE, CH_UNRELIABLE);
					break;
				}
			}
			else {
				// snapshot
			}
		}
		m_CommandBuffer.clear();
	}

	void NetworkManager::cleanupSessions()
	{
		uint64_t now{ getTime() };
		for (auto it{ m_Sessions.begin() }; it != m_Sessions.end();) {
			auto& sesh{ it->second };
			if (sesh.endpoint[EP_UNRELIABLE].state == ConnectionState::TIMEOUT
				&& sesh.endpoint[EP_RELIABLE].state == ConnectionState::TIMEOUT) {
				// Starting Dropping timer for timed out session,
				// Reconnect not allowed
				sesh.graceTimer = now;
				sesh.endpoint[EP_RELIABLE].state = ConnectionState::DROPPING;
				sesh.endpoint[EP_UNRELIABLE].state = ConnectionState::DROPPING;
				std::print("Session {} is now Dropping!\n", it->first);
			}
			else if (sesh.endpoint[EP_UNRELIABLE].state == ConnectionState::DROPPING
				&& sesh.endpoint[EP_RELIABLE].state == ConnectionState::DROPPING) {
				if (now - sesh.graceTimer > m_Policy.disconnect) {
					// Grace period over, Destruct session
					std::print("Session {} Disconnected!\n", it->first);
					it = m_Sessions.erase(it);
					continue;
				}
			}
			else sesh.graceTimer = 0;
			it++;
		}
	}

	void NetworkManager::run(uint16_t tickRate)
	{
		CL_CORE_ASSERT(tickRate && ((tickRate & (tickRate - 1)) == 0), "TickRate should be a power of two");
		CL_CORE_ASSERT(tickRate < 1024, "Windows Timer granularity is 1ms, 1024hz breaks sleep");
		m_Running.test_and_set(std::memory_order::release);
		m_Running.notify_all();

		uint32_t tickCounter{};
		// Microseconds per Tick
		const uint32_t tickDiffUs{ 1'000'000ul >> std::countr_zero(tickRate) };
		const uint32_t remainder{ 1'000'000ul & (tickRate - 1)};
		uint32_t frac{};
		m_NextTick = getTime() + tickDiffUs;

		while (!m_ShouldStop.test(std::memory_order::acquire)) {
			
			tickCounter++;
			tickCounter = tickCounter & (tickRate - 1);
			if (tickCounter == 0) { // Run once a second
				std::cout << "\033[2J\033[H" << std::flush;
				std::print("Net Stats:\n  Packets:\n    Sent: {}\n    Received: {}\n    Dropped: {}\n",
					m_Stats.packetsSent, m_Stats.packetsReceived, m_Stats.packetsDropped);
				std::print("  Bytes:\n    Sent: {}\n    Received: {}\n",
					m_Stats.bytesSent, m_Stats.bytesReceived);
				cleanupSessions();
			}

			maintainSessions();
			queueResends();
			collectIncoming();
			processCommands();

			//opportunisticReceive();
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
			port, static_cast<PacketFlags>(CONNECTION_REQUEST | RELIABLE));

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

	// Serialize Packet header into buffer
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

		if ((header.flags & FRAGMENT) != 0)
			append(header.fragLoad);
	}
	uint64_t NetworkManager::writeHeader(void* pData, const HeaderInfo& header)
	{
		uint64_t cursor{};

		auto append = [&](const auto& val) {
			std::memcpy(static_cast<char*>(pData) + cursor, &val, sizeof(val));
			cursor += sizeof(val);
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

		if ((header.flags & FRAGMENT) != 0)
			append(header.fragLoad);

		return cursor;
	}
	// Parse header from buffer, validate
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
		if (info.flags & FRAGMENT) {
			if (size - info.offset < sizeof(FragmentLoad)) return {};
			std::memcpy(&info.fragLoad, data + info.offset, sizeof(info.fragLoad));
			info.offset += sizeof(info.fragLoad);
		}

		return info;
	}

	bool NetworkManager::updateSessionStats(const PacketInfo packet, 
		const HeaderInfo& header,
		Endpoint& ep, ChannelState& state)
	{
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
		// Nat Rebind, update endpoint if peer is validated
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

		if (auto channel{ header.flags & CHANNEL_MASK };
			channel != RELIABLE) return false;

		// switch on flag
		switch (auto type{ header.flags & TYPE_MASK }; type) {
		case INVALID:
			return false;
			break;

		case CONNECTION_REQUEST:
			if (header.flags & FRAGMENT) return false;
			handleConnectionRequest(info, header);
			break;

		case CONNECTION_ACCEPT:
			if (header.flags & FRAGMENT) return false;
			return handleConnectionAccept(info, header);
			break;

		case CONNECTION_REJECT:
			if (header.flags & FRAGMENT) return false;
			return handleConnectionReject(info, header);
			break;

		case HEARTBEAT:
			if (header.flags & FRAGMENT) return false;
			return handleHeartbeat(info, header, CH_RELIABLE, EP_RELIABLE);
			break;

		case EVENT_LOAD:
		case STATE_LOAD:
			return handlePayload(info, header, payloadSize, CH_RELIABLE, EP_RELIABLE);
		default:
			return false;
		}

		return true;
	}
	inline bool NetworkManager::handleUnreliablePacket(const PacketInfo info)
	{
		HeaderInfo header{ parseHeader() };
		uint32_t payloadSize{ static_cast<uint32_t>(m_PacketBuffer.size() - header.offset) };

		if (auto channel{ header.flags & CHANNEL_MASK };
			channel != UNRELIABLE) return false;

		switch (auto type{ header.flags & TYPE_MASK }; type) {
		case INVALID:
			return false;
			break;

		case HEARTBEAT:
			if (header.flags & FRAGMENT) return false;
			return handleHeartbeat(info, header, CH_UNRELIABLE, EP_UNRELIABLE);
			break;

		default:
			return false;
		}

		return true;
	}

	inline bool NetworkManager::handleError()
	{
		for (auto& sock : m_Socks) {
			while (sock.poll() == PollResult::Error) {
				m_PacketBuffer.clear();
				if(!handleError(sock)) return false;
			}
		}
		return true;
	}

	inline bool NetworkManager::handleError(Socket& sock)
	{
		SocketError err = sock.pollError();
		// TODO: Expand

		if (err != SocketError::Fatal) return true;
		return false;
	}

	inline void NetworkManager::handleConnectionRequest(const PacketInfo info,
		const HeaderInfo& header)
	{
		// check existing sessions
		if (header.sessionID != 0) {
			if (auto it{ m_Sessions.find(header.sessionID) }; it != m_Sessions.end()) {
				// Resume Existing Session
				auto& localEndPoint = it->second.endpoint[1];
				auto state = localEndPoint.state;

				if (state == ConnectionState::DROPPING) { 
					rejectConnection(info.fromAddr, info.fromPort); 
					return;
				}
				if (localEndPoint.addr == info.fromAddr 
					&& localEndPoint.port == info.fromPort) {
					localEndPoint.state = ConnectionState::CONNECTED;
					acceptConnection(it->first);
					return;
				}
				if (updateSessionStats(info, header, 
					it->second.endpoint[EP_RELIABLE], it->second.states[CH_RELIABLE]))
					acceptConnection(it->first);
				else rejectConnection(info.fromAddr, info.fromPort);
			}
			else	rejectConnection(info.fromAddr, info.fromPort);
			return;
		}

		// Packet session id == 0, 
		for (auto& [id, sesh] : m_Sessions) {
			if (sesh.endpoint[EP_RELIABLE].addr == info.fromAddr 
				&& sesh.endpoint[EP_RELIABLE].port == info.fromPort) {				

				if (sesh.endpoint[EP_RELIABLE].state == ConnectionState::DROPPING) {
					rejectConnection(info.fromAddr, info.fromPort);
					return;
				}
				sesh.endpoint[EP_RELIABLE].state = ConnectionState::CONNECTED;
				acceptConnection(id);
				return;
			}
		}

		// check max vs curr sessions
		// reject if full
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

		// create new session
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
	inline bool NetworkManager::handleConnectionAccept(const PacketInfo packet,
		const HeaderInfo& header)
	{
		if (!header.sessionID) return false;

		if (auto it = m_Sessions.find(header.sessionID); it != m_Sessions.end()) {
			auto& sesh{ it->second };
			if (sesh.endpoint[EP_RELIABLE].state == ConnectionState::DROPPING) return true;
			return updateSessionStats(packet, header,
				sesh.endpoint[EP_RELIABLE], sesh.states[CH_RELIABLE]);
		}

		for (auto& pending : m_PendingConnections) {
			if (pending.addr == packet.fromAddr && pending.port == packet.fromPort) {
				std::print("Peer found! creating session {}\n", header.sessionID);
				if (createSession(pending, header.sessionID))
					return true;
				else { // ID collision
					std::print("ID Collision when creating session from connectionAccept: {}",
						header.sessionID);
					return true;
				}
			}
		}

		return false;
	}
	inline bool NetworkManager::handleConnectionReject(const PacketInfo packet,
		const HeaderInfo& header)
	{
		for (auto it = m_PendingConnections.begin(); it != m_PendingConnections.end(); it++) {
			if (it->addr == packet.fromAddr && it->port == packet.fromPort) {
				m_PendingConnections.erase(it);
				return true;
			}
		}
		return false;
	}
	// refresh session timing, keep alive
	inline bool NetworkManager::handleHeartbeat(const PacketInfo packet,
		const HeaderInfo& header,
		uint8_t Channel, uint8_t endpoint) noexcept {

		if (header.sessionID == 0) return false;
		if (auto it{ m_Sessions.find(header.sessionID) }; it != m_Sessions.end()) {
			if (it->second.endpoint[endpoint].state == ConnectionState::DROPPING) return true;
			return updateSessionStats(packet, header,
				it->second.endpoint[endpoint], it->second.states[Channel]);
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
		auto& reliable_end = sesh.endpoint[EP_RELIABLE];

		reliable_end.addr = info.addr;
		reliable_end.port = info.port;
		reliable_end.state = ConnectionState::CONNECTED;
		reliable_end.lastRecvTime = getTime();
		reliable_end.lastSentTime = info.lastSendTime;

		sesh.endpoint[EP_UNRELIABLE].state = ConnectionState::CONNECTING;
		sesh.endpoint[EP_UNRELIABLE].lastRecvTime = getTime();

		return true;
	}

	void NetworkManager::queueReliablePayload(uint32_t id, Session& sesh)
	{
		auto& context{ m_pWorld->getShardContext(id) };
		// copy contextData, etc.
		// size of data to be replicated, derive from world later
		uint32_t sizeOfData{ 4 };

		uint32_t sizeofHeader{ 21 }; // packet Header wire size
		if (sizeOfData > PACKET_MTU) sizeofHeader += 6; // FragmentPayloadSize

		auto packetSize{ sizeofHeader + sizeOfData };
		auto packet{ new std::byte[packetSize]() };

		HeaderInfo info{
			.protocol = HEADER_VERSION,
			.seqNum{sesh.states[CH_RELIABLE].lastSent++},
			.ackField{sesh.states[CH_RELIABLE].sendingAckF},
			.lastSeqRecv{sesh.states[CH_RELIABLE].lastReceived},
			.sessionID{id},
			.flags = static_cast<PacketFlags>(STATE_LOAD | RELIABLE),
		};
		uint64_t cursor{ writeHeader(packet, info) };
		
		// Copy Data
		uint32_t data{ sesh.states[CH_RELIABLE].lastSent };
		std::memcpy(packet + cursor, &data, 4);
		cursor += 4;

		m_ResendBuffer.emplace_back(packet, cursor, &sesh, info.seqNum, info.sessionID);
	}

	// Construct header, Send over Correct socket
	inline void NetworkManager::sendRequest(ipv4_addr addr, uint16_t port) noexcept
	{
		m_PacketBuffer.clear();
		HeaderInfo info{
			.protocol = HEADER_VERSION,
			.flags = static_cast<PacketFlags>(CONNECTION_REQUEST | RELIABLE),
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
			.seqNum{sesh.states[CH_RELIABLE].lastSent++},
			.ackField{sesh.states[CH_RELIABLE].sendingAckF},
			.lastSeqRecv{sesh.states[CH_RELIABLE].lastReceived},
			.sessionID{sessionID},
			.flags = static_cast<PacketFlags>(CONNECTION_ACCEPT | RELIABLE),
		};
		writeHeader(info);
		sendReliable(sesh.endpoint[1]);
	}
	inline void NetworkManager::sendReject(ipv4_addr addr, uint16_t port) noexcept
	{
		m_PacketBuffer.clear();
		HeaderInfo info{
			.protocol = HEADER_VERSION,
			.flags = static_cast<PacketFlags>(CONNECTION_REJECT | RELIABLE),
		};
		writeHeader(info);
		sendReliable(addr, port);
	}
	inline void NetworkManager::sendHeartbeat(uint32_t sessionID, Session& sesh,
		uint8_t ep, uint8_t ch) noexcept
	{
		m_PacketBuffer.clear();
		HeaderInfo info{
			.protocol{ HEADER_VERSION },
			.seqNum{sesh.states[ch].lastSent++},
			.ackField{sesh.states[ch].sendingAckF},
			.lastSeqRecv{sesh.states[ch].lastReceived},
			.sessionID{sessionID},
			.flags{ static_cast<PacketFlags>(HEARTBEAT | (1 << (ch + 3))) },
		};
		writeHeader(info);
		// pick socket
		if (ep) sendReliable(sesh.endpoint[ep]);
		else sendUnreliable(sesh.endpoint[ep]);
	}


	inline bool NetworkManager::sendReliable(ipv4_addr addr, uint16_t port) noexcept
	{
		if (auto res{ m_Socks[1].sendPacket(m_PacketBuffer, addr, port) }; res) {
			m_Stats.bytesSent += m_PacketBuffer.size();
			m_Stats.packetsSent++;

			return true;
		}
		return false;
	}
	inline bool NetworkManager::sendReliable(Endpoint& ep) noexcept
	{
		if (auto res{ m_Socks[1].sendPacket(m_PacketBuffer, ep.addr, ep.port) }; res) {
			m_Stats.bytesSent += m_PacketBuffer.size();
			m_Stats.packetsSent++;
			ep.lastSentTime = getTime();
			return true;
		}
		return false;
	}

	inline bool NetworkManager::sendReliablePayload(PacketDescriptor& packet) noexcept {
		CL_CORE_ASSERT(packet.pData.get() && packet.size, "Packet must have data and size");

		auto res{ m_Socks[EP_RELIABLE].sendPacket(packet.pData.get(), packet.size,
			packet.sesh->endpoint[EP_RELIABLE].addr, packet.sesh->endpoint[EP_RELIABLE].port) };
		if (res) {
			m_Stats.bytesSent += packet.size;
			m_Stats.packetsSent++;
			packet.sesh->endpoint[EP_RELIABLE].lastSentTime = Engine::getTime();
			return true;
		}
		return false;
	}

	inline bool NetworkManager::sendUnreliable(ipv4_addr addr, uint16_t port) noexcept
	{
		if (auto res{ m_Socks[0].sendPacket(m_PacketBuffer, addr, port) }; res) {
			m_Stats.bytesSent += m_PacketBuffer.size();
			m_Stats.packetsSent++;

			return true;
		}
		return false;
	}
	inline bool NetworkManager::sendUnreliable(Endpoint& ep) noexcept
	{
		if (auto res{ m_Socks[0].sendPacket(m_PacketBuffer, ep.addr, ep.port) }; res) {
			m_Stats.bytesSent += m_PacketBuffer.size();
			m_Stats.packetsSent++;
			ep.lastSentTime = getTime();
			return true;
		}
		return false;
	}


	bool NetworkManager::handlePayload(PacketInfo info, const HeaderInfo& header,
		const uint64_t payloadSize,
		uint8_t Channel, uint8_t endpoint)
	{
		if (auto it = m_Sessions.find(header.sessionID); it != m_Sessions.end()) {
			updateSessionStats(info, header, it->second.endpoint[endpoint], it->second.states[Channel]);
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