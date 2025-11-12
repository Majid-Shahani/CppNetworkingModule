#include <CNM.h>
#include <iostream>

namespace cnm = Carnival::Network;
int main() {
	cnm::ipv4_addr addr{.addr32 = (127 << 24) | 1};
	cnm::SocketData sockData{
		.InAddress = addr,
		.Port = 0,
		.Reliable = false,
		.NonBlocking = true
	};

	cnm::Socket mySock{sockData};
	mySock.openSocket();
	mySock.bindSocket();
	const char msg[] = "Hello";
	mySock.sendPackets(msg, sizeof(msg), addr);
	mySock.receivePackets();

	std::cout << "Socket2:\n";
	cnm::Socket mySock2 = std::move(mySock);
	mySock2.openSocket();
	mySock2.bindSocket();
	mySock2.sendPackets(msg, sizeof(msg), addr);
	mySock2.receivePackets();

	// Test
	struct packHeader {
		const uint32_t ProtocolVersion{cnm::HEADER_VERSION};
		std::atomic<uint32_t> ackField{ 0 };
		std::atomic<uint32_t> SeqNumber{ 0 };
	} myHeader;
	std::cout << "Packet header size: " << sizeof(myHeader) << '\n';

	return 0;
}