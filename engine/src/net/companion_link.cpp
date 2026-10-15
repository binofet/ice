#include "ice/net/companion_link.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
namespace ice::net {
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
} // namespace ice::net
#include <winsock2.h>
#include <ws2tcpip.h>

// IP_ADAPTER_ADDRESSES requires the Winsock 2 declarations before iphlpapi.h.
#include <iphlpapi.h>
namespace ice::net {
using socket            = SOCKET;
static const socket bad = INVALID_SOCKET;
static void         close_socket(socket s) { closesocket(s); }
#else
} // namespace ice::net
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
namespace ice::net {
using socket            = int;
static const socket bad = -1;
static void         close_socket(socket s) { close(s); }
#endif
namespace {
enum kind { hello = 1, welcome, ready, paired, shot, ack, ping, pong, rejected };
struct packet {
	unsigned      kind    = 0;
	std::uint64_t session = 0, nonce = 0;
	unsigned      seq   = 0;
	int           aim   = 0;
	unsigned      power = 0, source = 0;
};
using wire = std::array<unsigned char, 40>;
void put(unsigned char *p, std::uint64_t v, int n) {
	for (int i = n - 1; i >= 0; --i) {
		p[i] = v & 255;
		v >>= 8;
	}
}
std::uint64_t get(const unsigned char *p, int n) {
	std::uint64_t v = 0;
	while (n--)
		v = (v << 8) | *p++;
	return v;
}
wire encode(const packet &p) {
	wire w{};
	std::memcpy(w.data(), "ICEC", 4);
	w[4] = 1;
	w[5] = p.kind;
	w[7] = 1; // shot-intent capability
	put(w.data() + 8, p.session, 8);
	put(w.data() + 16, p.nonce, 8);
	put(w.data() + 24, p.seq, 4);
	put(w.data() + 28, p.aim + 10000, 2);
	put(w.data() + 30, p.power, 2);
	w[32] = p.source;
	return w;
}
bool decode(const unsigned char *w, int n, packet &p) {
	if (n != 40 || std::memcmp(w, "ICEC", 4) || w[4] != 1 || w[5] < hello || w[5] > rejected ||
	    w[6] || w[7] != 1)
		return false;
	for (int i = 33; i < 40; ++i)
		if (w[i])
			return false;
	p.kind    = w[5];
	p.session = get(w + 8, 8);
	p.nonce   = get(w + 16, 8);
	p.seq     = (unsigned)get(w + 24, 4);
	p.aim     = (int)get(w + 28, 2) - 10000;
	p.power   = (unsigned)get(w + 30, 2);
	p.source  = w[32];
	return p.nonce && p.aim >= -10000 && p.aim <= 10000 && p.power <= 10000 && p.source <= 1;
}
bool same(const sockaddr_in &a, const sockaddr_in &b) {
	return a.sin_port == b.sin_port && a.sin_addr.s_addr == b.sin_addr.s_addr;
}
std::uint64_t random_id() {
	std::random_device r;
	std::uint64_t      v = (std::uint64_t(r()) << 32) ^ r();
	return v ? v : 1;
}
} // namespace
struct companion_link::impl {
	ice::net::socket      socket  = bad;
	companion_link::state state   = state::closed;
	std::string           message = "Local test / disconnected";
	sockaddr_in           peer{};
	bool server = false, accept = false, pending = false, queued = false, ready = false,
	     last_rejected  = false;
	unsigned       code = 0, seq = 0, last_seq = 0, delivered = 0, attempts = 0;
	unsigned short port    = 0;
	std::uint64_t  session = 0, nonce = 0;
	double heard = 0, sent = -100, start = 0, pending_at = 0, queued_at = 0, attempt_window = 0,
	       last_poll = -1;
	packet                  retry;
	ice::input::shot_intent queued_shot;
	bool                    open(const char *address, unsigned short requested) {
#ifdef _WIN32
		static const bool winsock = []() {
			WSADATA data;
			return WSAStartup(MAKEWORD(2, 2), &data) == 0;
		}();
		if (!winsock)
			return false;
#endif
		sockaddr_in local{};
		local.sin_family = AF_INET;
		local.sin_port   = htons(requested);
		if (inet_pton(AF_INET, address, &local.sin_addr) != 1)
			return false;
		socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (socket == bad)
			return false;
#ifdef _WIN32
		u_long nonblocking = 1;
		if (ioctlsocket(socket, FIONBIO, &nonblocking)) {
			close_socket(socket);
			socket = bad;
			return false;
		}
#else
		if (fcntl(socket, F_SETFL, O_NONBLOCK) < 0) {
			close_socket(socket);
			socket = bad;
			return false;
		}
#endif
		if (bind(socket, (sockaddr *)&local, sizeof(local)) < 0) {
			close_socket(socket);
			socket = bad;
			return false;
		}
#ifdef _WIN32
		int length = sizeof(local);
#else
		socklen_t length = sizeof(local);
#endif
		if (getsockname(socket, (sockaddr *)&local, &length) < 0) {
			close_socket(socket);
			socket = bad;
			return false;
		}
		port = ntohs(local.sin_port);
		return true;
	}
	void send(packet p, const sockaddr_in &to) {
		auto w = encode(p);
		sendto(socket, (const char *)w.data(), (int)w.size(), 0, (const sockaddr *)&to, sizeof(to));
	}
	void send(unsigned kind, unsigned number = 0) {
		packet p;
		p.kind    = kind;
		p.session = session;
		p.nonce   = nonce;
		p.seq     = number;
		send(p, peer);
	}
	void reset_peer() {
		session = nonce = 0;
		seq = last_seq = 0;
		pending = queued = ready = false;
		heard                    = 0;
		sent                     = -100;
	}
};
companion_link::companion_link() : m_(new impl) {}
companion_link::~companion_link() {
	if (m_->socket != bad)
		close_socket(m_->socket);
}
void companion_link::close() {
	if (m_->socket != bad)
		close_socket(m_->socket);
	m_.reset(new impl);
}
bool companion_link::listen(unsigned short port, unsigned code, const char *bind_address) {
	close();
	if (code < 100000 || code > 999999 || !m_->open(bind_address, port)) {
		m_->state   = state::failed;
		m_->message = "Could not open companion port";
		return false;
	}
	m_->server  = true;
	m_->code    = code;
	m_->state   = state::listening;
	m_->message = "Waiting for companion";
	return true;
}
bool companion_link::connect(const char *ipv4, unsigned short port, unsigned code, double now) {
	close();
	m_->peer.sin_family = AF_INET;
	m_->peer.sin_port   = htons(port);
	if (!std::isfinite(now) || now < 0 || !port || code < 100000 || code > 999999 ||
	    inet_pton(AF_INET, ipv4, &m_->peer.sin_addr) != 1 || !m_->open("0.0.0.0", 0)) {
		m_->state   = state::failed;
		m_->message = "Enter IPv4 address, port and six-digit code";
		return false;
	}
	m_->nonce = random_id();
	m_->code  = code;
	m_->start = m_->heard = now;
	m_->state             = state::pairing;
	m_->message           = "Pairing with desktop";
	poll(now);
	return true;
}
void companion_link::accept_shots(bool accept) {
	m_->accept = accept;
	if (!accept)
		m_->queued = false;
}
void companion_link::poll(double now) {
	if (m_->socket == bad || !std::isfinite(now) || now < 0)
		return;
	// A paused loop must not consume buffered actions from before suspension.
	if (m_->last_poll >= 0 && (now < m_->last_poll || now - m_->last_poll > 1.0)) {
		if (m_->server) {
			m_->reset_peer();
			m_->state   = state::listening;
			m_->message = "Companion disconnected";
		} else {
			close();
			m_->message = "Connection paused; reconnect";
			return;
		}
	}
	m_->last_poll = now;
	if (m_->queued && now - m_->queued_at > .35)
		m_->queued = false;
	if (m_->session && now - m_->heard > 3) {
		m_->reset_peer();
		m_->state   = m_->server ? state::listening : state::failed;
		m_->message = "Companion timed out; reconnect";
	}
	// Bound packet processing per frame, including malformed datagrams.
	for (int count = 0; count < 64; ++count) {
		unsigned char bytes[512];
		sockaddr_in   from{};
#ifdef _WIN32
		int length = sizeof(from);
#else
		socklen_t length = sizeof(from);
#endif
		int n =
		    (int)recvfrom(m_->socket, (char *)bytes, sizeof(bytes), 0, (sockaddr *)&from, &length);
		if (n < 0)
			break;
		packet p;
		if (!decode(bytes, n, p))
			continue;
		if (m_->server && p.kind == hello) {
			if (now - m_->attempt_window >= 1) {
				m_->attempt_window = now;
				m_->attempts       = 0;
			}
			if (++m_->attempts > 8)
				continue;
			if (p.seq != m_->code || p.session)
				continue;
			if (m_->session) {
				if (!same(from, m_->peer) || p.nonce != m_->nonce)
					continue;
			} else {
				m_->peer    = from;
				m_->nonce   = p.nonce;
				m_->session = random_id();
				m_->heard   = now;
				m_->state   = state::pairing;
			}
			m_->send(welcome);
			continue;
		}
		if (!same(from, m_->peer) || p.nonce != m_->nonce)
			continue;
		if (!m_->server && p.kind == welcome && m_->state == state::pairing && p.session) {
			if (m_->session && m_->session != p.session)
				continue;
			m_->session = p.session;
			m_->heard   = now;
			m_->ready   = true;
			m_->send(ready);
			continue;
		}
		if (!p.session || p.session != m_->session)
			continue;
		if (m_->server && p.kind == ready) {
			m_->state   = state::connected;
			m_->message = "Companion connected";
			m_->heard   = now;
			m_->send(paired);
			continue;
		}
		if (!m_->server && p.kind == paired && m_->state == state::pairing) {
			m_->state   = state::connected;
			m_->message = "Connected to desktop";
			m_->heard   = now;
			continue;
		}
		if (m_->state != state::connected)
			continue;
		if (m_->server && p.kind == shot && p.seq) {
			m_->heard = now;
			if (p.seq <= m_->last_seq) {
				m_->send(m_->last_rejected ? rejected : ack, p.seq);
				continue;
			}
			m_->last_seq      = p.seq;
			m_->last_rejected = !m_->accept || m_->queued;
			if (!m_->accept || m_->queued) {
				m_->send(rejected, p.seq);
				continue;
			}
			m_->queued_shot.aim     = p.aim / 10000.f;
			m_->queued_shot.power   = p.power / 10000.f;
			m_->queued_shot.source  = (ice::input::shot_mode)p.source;
			m_->queued_shot.seconds = now;
			m_->queued              = true;
			m_->queued_at           = now;
			m_->send(ack, p.seq);
			continue;
		}
		if (!m_->server && (p.kind == ack || p.kind == rejected) && m_->pending &&
		    p.seq == m_->retry.seq) {
			m_->heard   = now;
			m_->pending = false;
			if (p.kind == ack) {
				++m_->delivered;
				m_->message = "Shot received by desktop";
			} else
				m_->message = "Desktop not ready for a shot";
			continue;
		}
		if (m_->server && p.kind == ping) {
			m_->heard = now;
			m_->send(pong);
		}
		if (!m_->server && p.kind == pong)
			m_->heard = now;
	}
	if (!m_->server && m_->state == state::pairing) {
		if (now - m_->start > 5) {
			m_->state   = state::failed;
			m_->message = "Pairing timed out; check address and code";
			return;
		}
		if (now - m_->sent >= .2) {
			m_->send(m_->ready ? ready : hello, m_->ready ? 0 : m_->code);
			m_->sent = now;
		}
	}
	if (!m_->server && m_->state == state::connected) {
		if (m_->pending && now - m_->pending_at > .35) {
			m_->pending = false;
			m_->message = "No confirmation; shot may have arrived";
		}
		if (m_->pending && now - m_->sent >= .06) {
			m_->send(m_->retry, m_->peer);
			m_->sent = now;
		} else if (!m_->pending && now - m_->sent >= .5) {
			m_->send(ping);
			m_->sent = now;
		}
	}
}
bool companion_link::send_shot(const ice::input::shot_intent &shot, double now) {
	if (m_->server || m_->state != state::connected || m_->pending || !std::isfinite(now) ||
	    now < 0 || !std::isfinite(shot.aim) || !std::isfinite(shot.power) || shot.aim < -1 ||
	    shot.aim > 1 || shot.power < 0 || shot.power > 1 ||
	    (shot.source != ice::input::shot_mode::touch &&
	     shot.source != ice::input::shot_mode::motion))
		return false;
	if (m_->seq == std::numeric_limits<unsigned>::max()) {
		close();
		return false;
	}
	packet p;
	p.kind         = kind::shot;
	p.session      = m_->session;
	p.nonce        = m_->nonce;
	p.seq          = ++m_->seq;
	p.aim          = (int)std::lround(shot.aim * 10000);
	p.power        = (unsigned)std::lround(shot.power * 10000);
	p.source       = (unsigned)shot.source;
	m_->retry      = p;
	m_->pending    = true;
	m_->pending_at = m_->sent = now;
	m_->send(p, m_->peer);
	return true;
}
bool companion_link::take_shot(ice::input::shot_intent &shot, double now) {
	if (!m_->server || !m_->queued || !std::isfinite(now) || now < m_->queued_at)
		return false;
	if (now - m_->queued_at > .35) {
		m_->queued = false;
		return false;
	}
	shot       = m_->queued_shot;
	m_->queued = false;
	return true;
}
companion_link::state companion_link::status() const { return m_->state; }
const std::string    &companion_link::message() const { return m_->message; }
unsigned short        companion_link::port() const { return m_->port; }
std::uint64_t         companion_link::peer_device() const {
    return m_->state == state::connected ? m_->session : 0;
}
unsigned companion_link::delivered() const { return m_->delivered; }

std::string companion_link::local_addresses() {
	std::string result;
	auto        add = [&](const sockaddr *address) {
        if (!address || address->sa_family != AF_INET)
            return;
        auto    *ip    = (const sockaddr_in *)address;
        unsigned value = ntohl(ip->sin_addr.s_addr);
        if (!value || (value >> 24) == 127 || (value >> 16) == 0xa9fe)
            return;
        char text[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &ip->sin_addr, text, sizeof(text))) {
            if (!result.empty())
                result += " / ";
            result += text;
        }
	};
#ifdef _WIN32
	ULONG                      size = 16384;
	std::vector<unsigned char> buffer(size);
	auto                      *adapters = (IP_ADAPTER_ADDRESSES *)buffer.data();
	if (GetAdaptersAddresses(
	        AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
	        nullptr, adapters, &size) == ERROR_BUFFER_OVERFLOW) {
		buffer.resize(size);
		adapters = (IP_ADAPTER_ADDRESSES *)buffer.data();
	}
	if (GetAdaptersAddresses(
	        AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
	        nullptr, adapters, &size) == NO_ERROR)
		for (auto *a = adapters; a; a = a->Next)
			if (a->OperStatus == IfOperStatusUp)
				for (auto *u = a->FirstUnicastAddress; u; u = u->Next)
					add(u->Address.lpSockaddr);
#else
	ifaddrs *addresses = nullptr;
	if (getifaddrs(&addresses) == 0) {
		for (auto *a = addresses; a; a = a->ifa_next)
			add(a->ifa_addr);
		freeifaddrs(addresses);
	}
#endif
	return result.empty() ? "127.0.0.1" : result;
}

} // namespace ice::net
