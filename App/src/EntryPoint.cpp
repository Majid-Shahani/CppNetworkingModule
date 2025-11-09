#include <CNM.h>

namespace cnm = Carnival::Network;
int main() {
	cnm::ipv4_addr addr{.addr32 = (127 << 24) | 1};
	
	cnm::SocketData sockData{
		.OutAddress = addr,
		.InAddress = addr,
		.Port = 0,
		.Type = cnm::CONNECTION_LESS,
		.Status = cnm::NONBLOCKING
	};

	cnm::Socket mySock{sockData};
	
	mySock.openSocket();
	mySock.bindSocket();
	const char msg[] = "Hello";
	mySock.sendPackets(msg, sizeof(msg));
	mySock.sendPackets(msg, strlen(msg));
	mySock.receivePackets();
	mySock.receivePackets();

	return 0;
}