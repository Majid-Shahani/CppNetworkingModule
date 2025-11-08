#include <CNM.h>

int main() {
	Carnival::Network::Sockets mySock;
	mySock.createSocket(1);
	Carnival::Network::ipv4_addr addr{.addr32 = (127 << 24) | 1};
	mySock.bindSocket(1, 0, addr);

	return 0;
}