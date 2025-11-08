#include <include/CNM.h>

namespace cnm = Carnival::Network;
int main() {
	cnm::Sockets mySock;
	cnm::SocketData sockData{};
	sockData.Status = cnm::NONBLOCKING;
	mySock.createSocket(1, sockData);

	cnm::ipv4_addr addr{.addr32 = (127 << 24) | 1};
	mySock.bindSocket(1, 0, addr);
	
	const char msg[] = "Hello";
	mySock.sendPackets(1, msg, sizeof(msg), addr);
	//mySock.sendPackets(1, msg, strlen(msg), addr);
	mySock.receivePackets(1);
	mySock.receivePackets(1);

	return 0;
}