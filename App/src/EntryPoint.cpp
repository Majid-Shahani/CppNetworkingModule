#include <CNM.h>

int main() {
	Carnival::Network::Sockets mySock;
	mySock.createSocket(1);
	mySock.bindSocket(1);

	return 0;
}