#include "ice/net/companion_link.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
static void check(bool ok, const char *reason) {
	if (!ok) {
		std::fprintf(stderr, "FAIL: %s\n", reason);
		std::exit(1);
	}
}
static void pump(ice::net::companion_link &host, ice::net::companion_link &client, double &now,
                 int count = 8) {
	for (int i = 0; i < count; ++i) {
		now += .01;
		client.poll(now);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		host.poll(now);
	}
}
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using TestSocket = SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
using TestSocket = int;
#endif
// A real UDP fault proxy: drops replies and duplicates shot datagrams without
// reaching inside the transport's state or reducer implementation.
struct FaultProxy {
	TestSocket     socket;
	sockaddr_in    host{}, client{};
	unsigned short port;
	bool           dropReplies = false, duplicateShots = false;
	explicit FaultProxy(unsigned short hostPort) {
		socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		sockaddr_in local{};
		local.sin_family      = AF_INET;
		local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		check(bind(socket, (sockaddr *)&local, sizeof(local)) == 0, "proxy bind");
#ifdef _WIN32
		u_long on = 1;
		ioctlsocket(socket, FIONBIO, &on);
		int size = sizeof(local);
#else
		fcntl(socket, F_SETFL, O_NONBLOCK);
		socklen_t size = sizeof(local);
#endif
		getsockname(socket, (sockaddr *)&local, &size);
		port          = ntohs(local.sin_port);
		host          = local;
		host.sin_port = htons(hostPort);
	}
	~FaultProxy() {
#ifdef _WIN32
		closesocket(socket);
#else
		close(socket);
#endif
	}
	void poll() {
		for (int i = 0; i < 64; ++i) {
			unsigned char bytes[512];
			sockaddr_in   from{};
#ifdef _WIN32
			int size = sizeof(from);
#else
			socklen_t size = sizeof(from);
#endif
			int n = recvfrom(socket, (char *)bytes, sizeof(bytes), 0, (sockaddr *)&from, &size);
			if (n < 0)
				return;
			bool reply = from.sin_port == host.sin_port;
			bool shot  = n == 40 && bytes[5] == 5;
			if (reply && dropReplies && n == 40 && (bytes[5] == 6 || bytes[5] == 9))
				continue;
			if (!reply)
				client = from;
			const sockaddr_in &to = reply ? client : host;
			sendto(socket, (const char *)bytes, n, 0, (const sockaddr *)&to, sizeof(to));
			if (shot && duplicateShots)
				sendto(socket, (const char *)bytes, n, 0, (const sockaddr *)&to, sizeof(to));
		}
	}
};
static void pumpFaults(ice::net::companion_link &host, ice::net::companion_link &client,
                       FaultProxy &proxy, double &now, int count = 12) {
	for (int i = 0; i < count; ++i) {
		now += .01;
		client.poll(now);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		proxy.poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		host.poll(now);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		proxy.poll();
	}
}
int main() {
	using State = ice::net::companion_link::state;
	ice::net::companion_link host, client;
	double                   now = 10;
	check(host.listen(0, 123456, "127.0.0.1"), "listen ephemeral");
	host.accept_shots(true);
	check(client.connect("127.0.0.1", host.port(), 123456, now), "connect");
	pump(host, client, now);
	check(host.status() == State::connected && client.status() == State::connected,
	      "version/capability handshake");
	auto firstDevice = host.peer_device();
	check(firstDevice != 0, "remote device identity");
	ice::input::shot_intent shot;
	shot.aim    = -.3518f;
	shot.power  = .7234f;
	shot.source = ice::input::shot_mode::motion;
	check(client.send_shot(shot, now), "send shot");
	check(!client.send_shot(shot, now), "bounded pending shots");
	pump(host, client, now);
	ice::input::shot_intent received;
	check(host.take_shot(received, now), "receive shot");
	check(!host.take_shot(received, now), "consume once");
	check(std::fabs(received.aim - shot.aim) < .00011 &&
	          std::fabs(received.power - shot.power) < .00011 && received.source == shot.source,
	      "portable quantized payload");
	check(client.delivered() == 1, "shot acknowledgement");
	host.accept_shots(false);
	check(client.send_shot(shot, now), "send while game unavailable");
	pump(host, client, now);
	check(!host.take_shot(received, now) && client.delivered() == 1,
	      "game rejects rather than queues a future shot");
	host.accept_shots(true);
	check(client.send_shot(shot, now), "queue shot");
	pump(host, client, now);
	now += .4;
	host.poll(now);
	check(!host.take_shot(received, now), "queued game action expires");
	check(client.send_shot(shot, now), "send before suspension");
	now += 2;
	host.poll(now);
	check(!host.take_shot(received, now) && host.peer_device() == 0,
	      "paused host rejects buffered old session shots");
	client.close();
	pump(host, client, now);
	check(client.connect("127.0.0.1", host.port(), 123456, now), "reconnect");
	pump(host, client, now);
	check(host.peer_device() && host.peer_device() != firstDevice,
	      "new session has new device identity");
	check(!host.take_shot(received, now), "no replay on reconnect");
	client.close();
	host.close();
	check(host.listen(0, 123456, "127.0.0.1"), "listen again");
	check(client.connect("127.0.0.1", host.port(), 654321, now), "wrong-code handshake attempt");
	for (int i = 0; i < 60; ++i) {
		now += .1;
		client.poll(now);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		host.poll(now);
	}
	check(client.status() == State::failed && host.peer_device() == 0, "wrong code cannot connect");
	check(!client.connect("host.invalid", host.port(), 123456, now),
	      "numeric address avoids blocking DNS");
	host.close();
	client.close();
	check(host.listen(0, 123456, "127.0.0.1"), "fault-test host");
	host.accept_shots(true);
	FaultProxy proxy(host.port());
	check(client.connect("127.0.0.1", proxy.port, 123456, now), "proxy connect");
	pumpFaults(host, client, proxy, now);
	check(client.status() == State::connected, "handshake through proxy");
	proxy.dropReplies    = true;
	proxy.duplicateShots = true;
	check(client.send_shot(shot, now), "send across dropped replies");
	pumpFaults(host, client, proxy, now);
	check(host.take_shot(received, now) && !host.take_shot(received, now),
	      "duplicate datagrams produce one action");
	check(client.delivered() == 0, "lost acknowledgement remains unconfirmed");
	proxy.dropReplies = false;
	pumpFaults(host, client, proxy, now);
	check(client.delivered() == 1 && !host.take_shot(received, now),
	      "retransmission acknowledges without launching again");
	host.accept_shots(false);
	proxy.dropReplies = true;
	check(client.send_shot(shot, now), "busy-game dropped rejection");
	pumpFaults(host, client, proxy, now);
	proxy.dropReplies = false;
	pumpFaults(host, client, proxy, now);
	check(client.delivered() == 1 && !host.take_shot(received, now),
	      "duplicate rejected shot stays rejected");
	std::puts("Companion link loopback and packet-loss tests passed");
}
