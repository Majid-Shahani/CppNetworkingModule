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
		Endpoint pending{
			.addr = addr,
			.port = port,
			.state = ConnectionState::CONNECTING,
		};
		
		m_PacketBuffer.clear();
		writeHeader(PacketFlags::CONNECTION);

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

		auto type = flags & 3;
		if (type == PacketFlags::CONNECTION || type == PacketFlags::HEARTBEAT) return;

		append(seq);
		append(ackf);
		append(lastReceiv);
		append(sessionID);
		if ((flags & PacketFlags::FRAGMENT) != 0)	append(frag);
		return;
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