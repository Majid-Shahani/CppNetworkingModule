#include <CNM.h>
#include <iostream>

namespace cnm = Carnival::Network;
int main() {
	cnm::ipv4_addr addr{.addr32 = (127 << 24) | 1};
	cnm::SocketData sockData{
		.InAddress = addr,
		.Port = 0,
		.ConnectionOriented = false,
		.NonBlocking = true
	};

	cnm::Socket mySock{sockData};
	std::cout << "Size of mySock: " << sizeof(mySock) << '\n';
	std::cout << "Size of sockData: " << sizeof(sockData) << '\n';

	mySock.openSocket();
	mySock.bindSocket();
	const char msg[] = "Hello";
	mySock.sendPackets(addr, msg, sizeof(msg));
	mySock.receivePackets();
	mySock.receivePackets();

	return 0;
}