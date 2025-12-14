#include <CNM.h>
#include <print>
#include <thread>
#include <chrono>

namespace cnm = Carnival::Network;
using namespace std::chrono_literals;

int main() {
	cnm::ipv4_addr addr{.addr32 = (127 << 24) | 1};
	cnm::SocketData sockData{
		.InAddress = addr,
		.InPort = 0,
		.Reliable = false,
		.NonBlocking = true
	};
	
	cnm::Socket mySock{sockData};
	mySock.openSocket();
	mySock.bindSocket();
	const char msg[] = "Hello";
	mySock.sendPackets(msg, sizeof(msg), addr);
	mySock.receivePackets();

	cnm::SocketData sockData2{
	.InAddress = addr,
	.InPort = 0,
	.Reliable = false,
	.NonBlocking = true
	};
	std::print("Socket2: ");
	cnm::Socket sock2{ sockData2 };
	sock2.openSocket();
	sock2.bindSocket();

	std::jthread jt{ [&](std::stop_token st) {
		while (!st.stop_requested()) {
			sock2.sendPackets(msg, sizeof(msg), mySock.getAddr(), mySock.getPort() );
			std::this_thread::sleep_for(10ms);

		}}
	};

	std::this_thread::sleep_for(100ms);
	mySock.receivePackets();
	jt.request_stop();
	return 0;
}