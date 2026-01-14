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
		m_PacketBuffer.reserve(1500);
	}
	void NetworkManager::pollIO()
	{
		m_PacketBuffer.clear();

		const char test[] = "Hello boys!";
		m_PacketBuffer.insert(m_PacketBuffer.end(), reinterpret_cast<const std::byte*>(test),
			reinterpret_cast<const std::byte*>(test + sizeof(test)));
		std::print("Data in packet buffer: {}\n", reinterpret_cast<const char*>(m_PacketBuffer.data()));

		if (m_Socks[0].sendPackets(m_PacketBuffer, m_Socks[1].getAddr(), m_Socks[1].getPort()))
			std::print("Packet Sent to Socket 2\n");
		
		m_PacketBuffer.clear();
		while (!m_Socks[1].hasPacket());
		auto from{ m_Socks[1].receivePacket(m_PacketBuffer) };
		if (from.fromAddr == m_Socks[0].getAddr() && from.fromPort == m_Socks[0].getPort()) {
			std::print("Packet from socket 1 Received!\nPacket: {}", reinterpret_cast<const char*>(m_PacketBuffer.data()));
		}

		attemptConnect(m_Socks[0].getAddr(), m_Socks[1].getPort());
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
				m_Stats.bytesReceived += m_PacketBuffer.size();
				m_Stats.packetsReceived++;

				// TODO: Check Against Drop List

				if (!handlePacket(info)) m_Stats.packetsDropped++;
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
		
		// Need to use a command buffer and return a promise.
		// sockets cannot be used concurrently.
		
		if (auto res{ m_Socks[1].sendPackets(m_PacketBuffer, addr, port) }; res) {
			m_Stats.bytesSent += m_PacketBuffer.size();
			m_Stats.packetsSent++;
			m_PendingConnections.emplace_back(addr, getTime(), port, 1);
			return true;
		}
		return false;
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

		auto type{ static_cast<PacketFlags>(header.flags & TYPE_MASK) };
		if (type == PacketFlags::CONNECTION_REQUEST || 
			type == PacketFlags::CONNECTION_REJECT ||
			type == PacketFlags::CONNECTION_ACCEPT ||
			type == PacketFlags::HEARTBEAT) return;

		append(header.seqNum);
		append(header.ackField);
		append(header.lastSeqRecv);
		append(header.sessionID);
		if ((header.flags & PacketFlags::FRAGMENT) != 0)	append(header.fragLoad);
		return;
	}

	HeaderInfo NetworkManager::parseHeader()
	{
		auto size = m_PacketBuffer.size();
		auto data = m_PacketBuffer.data();

		if (size < (sizeof(HeaderInfo::protocol) + sizeof(HeaderInfo::flags))) 
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
		
		// Ensure packet has fields
		if (size - info.offset < 
			sizeof(PacketHeader::SequenceNumber) +
			sizeof(PacketHeader::ACKField) +
			sizeof(PacketHeader::LastSeqReceived) +
			sizeof(PacketHeader::sessionID)) {
			return info;
		}

		std::memcpy(&info.seqNum, data + info.offset, sizeof(info.seqNum));
		info.offset += sizeof(info.seqNum);
		std::memcpy(&info.ackField, data + info.offset, sizeof(info.ackField));
		info.offset += sizeof(info.ackField);
		std::memcpy(&info.lastSeqRecv, data + info.offset, sizeof(info.lastSeqRecv));
		info.offset += sizeof(info.lastSeqRecv);
		std::memcpy(&info.sessionID, data + info.offset, sizeof(info.sessionID));
		info.offset += sizeof(info.sessionID);

		if (info.flags & PacketFlags::FRAGMENT) {
			if (size - info.offset < sizeof(FragmentLoad)) return {};
			std::memcpy(&info.fragLoad, data + info.offset, sizeof(info.fragLoad));
		}

		return info;
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
		case PacketFlags::CONNECTION_REJECT:
			break;
		default:
			return false;
		}

		return true;
	}

	bool NetworkManager::isValidRebind(const Session& sesh, const HeaderInfo& header) const
	{
		// timeout not elapsed
		if (getTime() - sesh.endpoint[1].lastRecvTime > m_Policy.disconnect)
			return false;

		const auto& rel = sesh.states[1];
		// Sequence Must make sense
		if (header.lastSeqRecv + 32 < rel.lastSent ||
			header.lastSeqRecv > rel.lastSent) return false;
		
		// ACK field validation
		uint32_t diff{ rel.lastSent - header.lastSeqRecv };
		uint32_t mask = (diff == 32) ? UINT32_MAX : ((1 << diff) - 1);
		if ((header.ackField & ~mask) == 0) return false;

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
					acceptConnection(it->first);
					return;
				}
				if (!isValidRebind(it->second, header)) {
					rejectConnection(info.fromAddr, info.fromPort);
					return;
				}
				localEndPoint.addr = info.fromAddr;
				localEndPoint.port = info.fromPort;
				localEndPoint.lastRecvTime = getTime();
				acceptConnection(it->first);
			}
			else	rejectConnection(info.fromAddr, info.fromPort);
			return;
		}

		// Packet session id == 0, 
		for (const auto& [id, sesh] : m_Sessions) {
			if (sesh.endpoint[1].addr == info.fromAddr && sesh.endpoint[1].port == info.fromPort) {
				if (sesh.endpoint[1].state == ConnectionState::DROPPING) {
					rejectConnection(info.fromAddr, info.fromPort);
					return;
				}
				acceptConnection(id);
				return;
			}
		}

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

	void NetworkManager::handlePayload(PacketInfo info, const HeaderInfo& header)
	{
	}

	uint32_t NetworkManager::createSession(const PendingPeer& info)
	{

		auto [it, inserted] = m_Sessions.try_emplace(generateSessionID(), Session{});
		Session& sesh = it->second;
		auto& reliable_end = sesh.endpoint[1];

		reliable_end.addr = info.addr;
		reliable_end.port = info.port;
		reliable_end.state = ConnectionState::CONNECTED;
		reliable_end.lastRecvTime = getTime();
		reliable_end.lastSentTime = info.lastSendTime;

		return it->first;
	}

	void NetworkManager::acceptConnection(uint32_t sessionID)
	{
	}

	void NetworkManager::rejectConnection(ipv4_addr addr, uint16_t port)
	{
	}

	uint32_t NetworkManager::getTime()
	{
		static const auto start = std::chrono::steady_clock::now();
		return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
	}

	void NetworkManager::sendReliableData()
	{
	}
	void NetworkManager::sendUnreliableData()
	{
	}
	void NetworkManager::sendSnapshot()
	{
	}

}