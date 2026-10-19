#include "ice/net/companion_web.h"
#include "ice/net/companion_link.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using TestWebSocket = SOCKET;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
using TestWebSocket = int;
#endif
static void check(bool ok, const char *why) {
	if (!ok) {
		std::fprintf(stderr, "FAIL: %s\n", why);
		std::exit(1);
	}
}
static double now() {
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}
static std::string exchange(unsigned short port, const std::string &request) {
	auto        s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in a{};
	a.sin_family      = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	a.sin_port        = htons(port);
#ifdef _WIN32
	DWORD timeout = 3000;
#else
	timeval timeout{3, 0};
#endif
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
	if (connect(s, (sockaddr *)&a, sizeof(a)) < 0)
		return "";
	// Split requests exercise incremental parsing instead of assuming one recv.
	auto half = request.size() / 2;
	send(s, request.data(), (int)half, 0);
	std::this_thread::sleep_for(std::chrono::milliseconds(3));
	send(s, request.data() + half, (int)(request.size() - half), 0);
	std::string response;
	char        buffer[2048];
	for (;;) {
		int n = recv(s, buffer, sizeof(buffer), 0);
		if (n <= 0)
			break;
		response.append(buffer, n);
	}
#ifdef _WIN32
	closesocket(s);
#else
	close(s);
#endif
	return response;
}
static void freshness_boundaries() {
	ice::net::companion_link host, client;
	check(host.listen(0, 123456, "127.0.0.1"), "freshness host");
	host.accept_shots(true);
	double protocol = 10;
	check(client.connect("127.0.0.1", host.port(), 123456, protocol), "freshness client");
	// Protocol time is monotonic and controlled; wall time only bounds socket progress.
	auto await = [&](auto ready, const char *why) {
		const double started = now();
		while (!ready() && now() - started < 4) {
			client.poll(protocol);
			host.poll(protocol);
			protocol += .005;
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		if (!ready())
			std::fprintf(stderr,
			             "freshness: protocol=%.6f wall_seconds=%.6f host_state=%d "
			             "client_state=%d delivered=%u host_message=%s client_message=%s\n",
			             protocol, now() - started, static_cast<int>(host.status()),
			             static_cast<int>(client.status()), client.delivered(),
			             host.message().c_str(), client.message().c_str());
		check(ready(), why);
	};
	await(
	    [&]() {
		    return host.status() == ice::net::companion_link::state::connected &&
		           client.status() == ice::net::companion_link::state::connected;
	    },
	    "bounded freshness handshake");
	const auto device = host.peer_device();
	for (unsigned sample = 0; sample < 3; ++sample) {
		ice::input::shot_intent sent, received;
		sent.aim                    = .4f;
		sent.power                  = .6f;
		const double   sent_at      = protocol;
		const unsigned acknowledged = client.delivered();
		check(client.send_shot(sent, protocol), "freshness shot queued");
		await([&]() { return client.delivered() == acknowledged + 1; },
		      "bounded accepted freshness receipt");
		// An ACK proves the native host accepted/queued this sequence. Nobody has
		// consumed it, and controlled setup remains inside its freshness window.
		check(protocol - sent_at < .3, "freshness receipt precedes boundary");
		protocol = sample == 0 ? sent_at + .34 : protocol + .36;
		if (sample == 2)
			host.poll(protocol);
		const bool taken = host.take_shot(received, protocol);
		std::printf("freshness sample=%u sent_at=%.6f consumed_at=%.6f accepted=%u taken=%d\n",
		            sample, sent_at, protocol, client.delivered(), taken ? 1 : 0);
		if (sample == 0) {
			check(taken, "fresh queued action survives inside 350ms");
			check(std::fabs(received.aim - sent.aim) < .00011f &&
			          std::fabs(received.power - sent.power) < .00011f,
			      "freshness payload matches");
			check(!host.take_shot(received, protocol), "fresh action consumed once");
		} else {
			check(!taken, sample == 1 ? "late pull rejects expired accepted action"
			                          : "poll expires accepted action before late pull");
		}
		check(host.status() == ice::net::companion_link::state::connected &&
		          host.peer_device() == device,
		      "freshness boundary did not reset authenticated peer");
	}
}
int main() {
	ice::net::companion_link host;
	check(host.listen(0, 123456, "127.0.0.1"), "native host");
	host.accept_shots(true);
	ice::net::companion_web web;
	check(web.start(host.port(), 123456), "web bridge");
	std::string authority = "127.0.0.1:" + std::to_string(web.port());
	struct delivery {
		ice::input::shot_intent shot;
		std::uint64_t           owner;
		double                  consumed_at;
	};
	std::vector<delivery> deliveries;
	double      last_poll = -1, max_poll_gap = 0, request_started = 0, request_finished = 0;
	unsigned    polls        = 0;
	bool        http_pending = false;
	std::string request_line;
	auto        diagnostic = [&]() {
        std::fprintf(stderr,
		                    "companion_web: polls=%u max_poll_gap=%.6f request_seconds=%.6f "
		                           "host_state=%d host_message=%s deliveries=%zu request=%s\n",
		                    polls, max_poll_gap,
		                    (http_pending ? now() : request_finished) - request_started,
		                    static_cast<int>(host.status()), host.message().c_str(), deliveries.size(),
		                    request_line.c_str());
        for (const auto &event : deliveries)
            std::fprintf(stderr,
			                    "delivery: aim=%.6f power=%.6f queued_at=%.6f consumed_at=%.6f "
			                           "queue_age=%.6f owner=%llu\n",
			                    event.shot.aim, event.shot.power, event.shot.seconds, event.consumed_at,
			                    event.consumed_at - event.shot.seconds,
			                    static_cast<unsigned long long>(event.owner));
	};
	auto require = [&](bool ok, const char *why) {
		if (!ok)
			diagnostic();
		check(ok, why);
	};
	auto pump = [&](int count) {
		for (int i = 0; i < count; ++i) {
			double t = now();
			if (last_poll >= 0)
				max_poll_gap = (std::max)(max_poll_gap, t - last_poll);
			last_poll = t;
			++polls;
			web.poll(t);
			host.poll(t);
			ice::input::shot_intent shot;
			if (host.take_shot(shot, t)) {
				require(deliveries.size() < 16, "bounded native delivery observations");
				deliveries.push_back({shot, web.claim_shot_owner(t), t});
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	};
	auto await_delivery = [&](std::size_t count) {
		const double started = now();
		while (deliveries.size() < count && now() - started < 4)
			pump(1);
		require(deliveries.size() >= count, "bounded native intent delivery");
		require(deliveries.size() == count, "no extra native delivery");
	};
	auto raw = [&](const std::string &request) {
		request_line    = request.substr(0, (std::min)(request.find("\r\n"), std::size_t(96)));
		request_started = request_finished = now();
		http_pending                       = true;
		auto result =
		    std::async(std::launch::async, [&]() { return exchange(web.port(), request); });
		double start = now();
		while (result.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready &&
		       now() - start < 4)
			pump(1);
		request_finished = now();
		http_pending     = false;
		require(result.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready,
		        "bounded HTTP response");
		return result.get();
	};
	auto request = [&](const char *method, const char *path, const std::string &body,
	                   const std::string &cookie) {
		return raw(std::string(method) + " " + path + " HTTP/1.1\r\nHost: " + authority +
		           "\r\nOrigin: http://" + authority +
		           "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" +
		           (cookie.empty() ? "" : "Cookie: " + cookie + "\r\n") + "\r\n" + body);
	};
	web.set_game_ready(true);
	auto cookieOf = [](const std::string &r) {
		auto c = r.find("Set-Cookie: ");
		check(c != std::string::npos, "session cookie");
		c += 12;
		return r.substr(c, r.find(';', c) - c);
	};
	auto page = request("GET", "/", "", "");
	check(page.find("HTTP/1.1 200") == 0, "embedded controller served");
	check(request("POST", "/join", "654321", "").find("HTTP/1.1 401") == 0, "wrong code denied");
	auto alice = cookieOf(request("POST", "/join", "123456\nAlice", ""));
	auto bob   = cookieOf(request("POST", "/join", "123456\nBob", ""));
	pump(20);
	check(web.room().players().size() == 2, "independent browser seats");
	check(request("POST", "/start", "", bob).find("HTTP/1.1 409") == 0, "only leader starts");
	check(request("POST", "/start", "", alice).find("HTTP/1.1 200") == 0, "leader starts round");
	auto turn     = web.room().turn();
	auto shotBody = [&](unsigned seq, unsigned generation) {
		return std::to_string(seq) + " " + std::to_string(generation) + " 0.4 0.6";
	};
	check(request("POST", "/shot", shotBody(1, turn), bob).find("HTTP/1.1 409") == 0,
	      "out of turn denied");
	require(deliveries.empty(), "out of turn cannot deliver native intent");
	const auto before_first = deliveries.size();
	check(request("POST", "/shot", shotBody(1, turn), alice).find("HTTP/1.1 200") == 0,
	      "current player queues shot");
	await_delivery(before_first + 1);
	require(std::fabs(deliveries.back().shot.aim - .4f) < .00011f &&
	            std::fabs(deliveries.back().shot.power - .6f) < .00011f,
	        "native intent payload matches");
	auto owner = deliveries.back().owner;
	require(owner == web.room().players()[0].id, "shot ownership retained");
	const auto before_duplicate = deliveries.size();
	check(request("POST", "/shot", shotBody(1, turn), alice).find("HTTP/1.1 200") == 0,
	      "duplicate retains result");
	// Observe every delivery across the 60ms retry and 350ms pending window.
	const double duplicate_started = now();
	while (now() - duplicate_started < .4)
		pump(1);
	require(deliveries.size() == before_duplicate, "duplicate cannot launch twice");
	check(web.finish_shot(owner, 50), "settle owned shot");
	check(web.room().players()[0].score == 50, "score belongs to shooter");
	check(request("POST", "/shot", shotBody(2, turn), bob).find("HTTP/1.1 409") == 0,
	      "stale turn denied");
	require(deliveries.size() == before_duplicate, "stale turn cannot deliver native intent");
	const auto before_second = deliveries.size();
	check(request("POST", "/shot", shotBody(3, web.room().turn()), bob).find("HTTP/1.1 200") == 0,
	      "next player shoots");
	await_delivery(before_second + 1);
	require(std::fabs(deliveries.back().shot.aim - .4f) < .00011f &&
	            std::fabs(deliveries.back().shot.power - .6f) < .00011f,
	        "second native intent payload matches");
	owner = deliveries.back().owner;
	require(owner == web.room().players()[1].id, "second shot ownership retained");
	const auto before_leave = deliveries.size();
	check(request("POST", "/leave", "", bob).find("HTTP/1.1 200") == 0,
	      "player leaves during flight");
	require(deliveries.size() == before_leave, "leave cannot deliver another native intent");
	check(web.finish_shot(owner, 20), "disconnected shooter still scores");
	check(web.room().players()[1].score == 20, "disconnected ownership preserved");
	check(request("POST", "/shot", shotBody(4, web.room().turn()), bob).find("HTTP/1.1 401") == 0,
	      "left session cannot shoot");
	check(raw("POST /join HTTP/1.1\r\nHost: " + authority +
	          "\r\nOrigin: http://unrelated.invalid\r\nContent-Length: 6\r\n\r\n123456")
	              .find("HTTP/1.1 403") == 0,
	      "cross-origin denied");
	check(raw("POST /join HTTP/1.1\r\nHost: " + authority +
	          "\r\nContent-Length: 6\r\nContent-Length: 6\r\n\r\n123456")
	              .find("HTTP/1.1 400") == 0,
	      "ambiguous framing denied");
	web.poll(now() + 2);
	check(request("GET", "/status", "", alice).find("HTTP/1.1 401") == 0,
	      "host suspension invalidates authority");
	require(deliveries.size() == before_leave, "rejected requests cannot deliver native intents");
	freshness_boundaries();
	std::puts("Party HTTP/session/UDP/ownership tests passed");
}
