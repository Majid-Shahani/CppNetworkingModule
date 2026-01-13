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
		writeHeader(PacketFlags::CONNECTION_REQUEST);
		
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

	void NetworkManager::writeHeader(PacketFlags flags,
		uint32_t sessionID, uint32_t seq,
		uint32_t ackf, uint32_t lastReceiv, FragmentLoad frag)
	{
		auto append = [&](const auto& val) {
			const std::byte* p = reinterpret_cast<const std::byte*>(&val);
			m_PacketBuffer.insert(m_PacketBuffer.end(), p, p + sizeof(val));
		};

		append(HEADER_VERSION);
		append(flags);

		auto type{ static_cast<PacketFlags>(flags & TYPE_MASK) };
		if (type == PacketFlags::CONNECTION_REQUEST || 
			type == PacketFlags::CONNECTION_REJECT ||
			type == PacketFlags::CONNECTION_ACCEPT ||
			type == PacketFlags::HEARTBEAT) return;

		append(seq);
		append(ackf);
		append(lastReceiv);
		append(sessionID);
		if ((flags & PacketFlags::FRAGMENT) != 0)	append(frag);
		return;
	}

	bool NetworkManager::handlePacket(PacketInfo info)
	{
		// Check header version.
		int cursor{};
		uint32_t packetPV{};
		std::memcpy(&packetPV, m_PacketBuffer.data(), sizeof(packetPV));
		cursor += sizeof(packetPV);

		if (packetPV != HEADER_VERSION)	return false;
		
		PacketFlags flags{};
		std::memcpy(&flags, m_PacketBuffer.data() + cursor, sizeof(flags));
		cursor += sizeof(flags);

		// maximum one channel
		auto channels{ flags & CHANNEL_MASK };
		if ((channels & (channels - 1)) != 0 || channels == 0) return false;

		// switch on flag
		switch (flags & TYPE_MASK) {
		case PacketFlags::INVALID:
			return false;
			break;
		case PacketFlags::CONNECTION_REQUEST:
		case PacketFlags::CONNECTION_ACCEPT:
		case PacketFlags::CONNECTION_REJECT:
			if (!(flags & PacketFlags::RELIABLE) || flags & PacketFlags::FRAGMENT) return false;
			handleConnection(info);
			break;
		default:
			return false;
		}

		return true;
	}

	void NetworkManager::handleConnection(PacketInfo info)
	{
		// check existing sessions
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

	void NetworkManager::handlePayload(PacketInfo info)
	{
	}

	uint32_t NetworkManager::createSession(const PendingPeer& info)
	{
		return 0;
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