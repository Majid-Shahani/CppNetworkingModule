#include <CNM.h>
#include <print>
#include <thread>
#include <chrono>

namespace cnm = Carnival::Network;
using namespace std::chrono_literals;

int main() {
	cnm::ipv4_addr addr{.addr32 = (127 << 24) | 1};
	cnm::SocketStatus status{ cnm::SocketStatus::NONBLOCKING | cnm::SocketStatus::REUSEADDR };
	cnm::SocketData sockData{
		.InAddress = addr,
		.InPort = 0,
		.status = status,
	};
	
	cnm::Socket mySock{sockData};
	mySock.openSocket();
	mySock.bindSocket();
	const char msg[] = "Hello";
	mySock.sendPackets(msg, sizeof(msg), addr);
	mySock.receivePackets();

	std::print("Socket2: ");
	cnm::Socket sock2{ sockData };
	sock2.openSocket();
	sock2.bindSocket();

	std::jthread jt{ [&](std::stop_token st) {
		while (!st.stop_requested()) {
			sock2.sendPackets(msg, sizeof(msg), mySock.getAddr(), mySock.getPort());
			std::this_thread::sleep_for(10ms);
		}}};

	std::this_thread::sleep_for(100ms);
	jt.request_stop();
	mySock.receivePackets();

	return 0;
}