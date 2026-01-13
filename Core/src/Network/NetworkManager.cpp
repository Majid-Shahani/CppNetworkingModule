#include <src/CNMpch.hpp>

#include <CNM/NetworkManager.h>
#include <ECS/World.h>

namespace Carnival::Network {
	NetworkManager::NetworkManager(ECS::World* pWorld, const SocketData& sockData)
		: m_callback{ pWorld }
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
		auto res = m_Socks[1].sendPackets(m_PacketBuffer, addr, port);
		if (res) m_PendingConnections.emplace_back(addr, getTime(), port, 1);
		return res;
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

		auto type = flags & (PacketFlags::UNRELIABLE - 1);
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

	void NetworkManager::handlePacket()
	{
		// Check header version.
		// switch on flag
	}

	void NetworkManager::handleConnection()
	{
		// check max vs curr sessions
		// check existing sessions
		// check pending connections
		// create new connect (decide policy later)
	}

	void NetworkManager::handlePayload()
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