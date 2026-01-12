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
	}
	void NetworkManager::pollIO()
	{
	}
	void NetworkManager::pollOngoing()
	{
	}
	void NetworkManager::pollIncoming()
	{
	}
	bool NetworkManager::attemptConnect(ipv4_addr addr, uint16_t port)
	{
		return false;
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
	void NetworkManager::addPeer(uint16_t peerID)
	{
	}
	void NetworkManager::removePeer(uint16_t peerID)
	{
	}
}