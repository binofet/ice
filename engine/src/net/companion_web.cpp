#include "ice/net/companion_web.h"
#include "icCompanionPage.h"
#include "ice/net/companion_link.h"
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <random>
#include <sstream>
namespace ice::net {
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
} // namespace ice::net
#include <winsock2.h>
#include <ws2tcpip.h>
namespace ice::net {
using web_socket                    = SOCKET;
static const web_socket invalid_web = INVALID_SOCKET;
static void             close_web(web_socket s) { closesocket(s); }
static bool             nonblocking(web_socket s) {
    u_long enabled = 1;
    return ioctlsocket(s, FIONBIO, &enabled) == 0;
}
static bool would_block() { return WSAGetLastError() == WSAEWOULDBLOCK; }
#else
} // namespace ice::net
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
namespace ice::net {
using web_socket                    = int;
static const web_socket invalid_web = -1;
static void             close_web(web_socket s) { close(s); }
static bool             nonblocking(web_socket s) { return fcntl(s, F_SETFL, O_NONBLOCK) == 0; }
static bool             would_block() { return errno == EAGAIN || errno == EWOULDBLOCK; }
#endif
namespace {
std::string trim(std::string s) {
	auto a = s.find_first_not_of(" \t");
	auto b = s.find_last_not_of(" \t");
	return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
std::string json_text(const std::string &text) {
	std::string result;
	for (unsigned char ch : text) {
		if (ch == '"' || ch == '\\')
			result += '\\';
		if (ch >= 32)
			result += (char)ch;
	}
	return result;
}
std::string token() {
	std::random_device random;
	std::ostringstream text;
	text << std::hex;
	for (int i = 0; i < 4; ++i) {
		text.width(8);
		text.fill('0');
		text << random();
	}
	return text.str();
}
} // namespace
struct companion_web::impl {
	struct client {
		web_socket  socket = invalid_web;
		std::string input, output;
		std::size_t sent  = 0;
		double      began = 0;
	};
	web_socket            listener = invalid_web;
	std::array<client, 8> clients;
	struct peer {
		std::uint64_t player = 0;
		std::string   session;
		unsigned      last_sequence = 0;
		bool          last_accepted = false, active = false;
	};
	std::array<peer, 8>        peers;
	ice::input::companion_room room;
	companion_link             upstream;
	unsigned short             port = 0, game_port = 0;
	unsigned                   code = 0, attempts = 0;
	double                     last_poll = -1, attempt_window = 0, pending_at = 0;
	bool                       game_ready     = false;
	std::uint64_t              pending_player = 0;
	void                       drop_peer() {
        upstream.close();
        for (auto &p : peers)
            p = peer();
        pending_player = 0;
        room.interrupt();
	}
	peer *known(const std::map<std::string, std::string> &h) {
		auto found = h.find("cookie");
		if (found == h.end())
			return nullptr;
		std::istringstream cookies(found->second);
		std::string        entry;
		while (std::getline(cookies, entry, ';'))
			for (auto &p : peers)
				if (!p.session.empty() &&
				    trim(entry) == "ICE" + std::to_string(port) + "=" + p.session)
					return &p;
		return nullptr;
	}
	std::string snapshot(const peer &peer) {
		const char *phase = room.state() == ice::input::companion_room::phase::playing   ? "playing"
		                    : room.state() == ice::input::companion_room::phase::results ? "results"
		                                                                                 : "lobby";
		const char *connection = upstream.status() == companion_link::state::connected ? "connected"
		                         : upstream.status() == companion_link::state::pairing
		                             ? "pairing"
		                             : "disconnected";
		std::string text =
		    "{\"state\":\"" + std::string(connection) + "\",\"phase\":\"" + phase +
		    "\",\"me\":" + std::to_string(peer.player) +
		    ",\"turn\":" + std::to_string(room.turn()) +
		    ",\"current\":" + std::to_string(room.current()) +
		    ",\"round\":" + std::to_string(room.round()) +
		    ",\"leader\":" + std::to_string(room.leader()) +
		    ",\"nextSequence\":" + std::to_string(peer.last_sequence + 1) + ",\"canStart\":" +
		    (room.leader() == peer.player &&
		             room.state() != ice::input::companion_room::phase::playing
		         ? "true"
		         : "false") +
		    ",\"canShoot\":" +
		    (game_ready && room.can_shoot(peer.player) && !pending_player ? "true" : "false") +
		    ",\"players\":[";
		bool comma = false;
		for (const auto &p : room.players()) {
			if (comma)
				text += ",";
			comma = true;
			text += "{\"id\":" + std::to_string(p.id) + ",\"name\":\"" + json_text(p.name) +
			        "\",\"score\":" + std::to_string(p.score) +
			        ",\"balls\":" + std::to_string(p.balls) +
			        ",\"online\":" + (p.online ? "true" : "false") +
			        ",\"waiting\":" + (p.forfeited ? "true" : "false") + "}";
		}
		return text + "]}";
	}
	void close_client(client &c) {
		if (c.socket != invalid_web)
			close_web(c.socket);
		c = client();
	}
	void respond(client &c, int status, const std::string &body, const std::string &cookie = "",
	             bool html = false) {
		const char *reason = status == 200   ? "OK"
		                     : status == 400 ? "Bad Request"
		                     : status == 401 ? "Unauthorized"
		                     : status == 403 ? "Forbidden"
		                     : status == 404 ? "Not Found"
		                     : status == 409 ? "Conflict"
		                     : status == 413 ? "Content Too Large"
		                                     : "Too Many Requests";
		c.output           = "HTTP/1.1 " + std::to_string(status) + " " + reason +
		           "\r\nContent-Type: " + (html ? "text/html; charset=utf-8" : "application/json") +
		           "\r\nContent-Length: " + std::to_string(body.size()) +
		           "\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: "
		           "nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: "
		           "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
		           "img-src data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'\r\n";
		if (!cookie.empty())
			c.output += "Set-Cookie: ICE" + std::to_string(port) + "=" + cookie +
			            "; Path=/; HttpOnly; SameSite=Strict\r\n";
		c.output += "\r\n" + body;
	}
	void request(client &c, double now) {
		auto end = c.input.find("\r\n\r\n");
		if (end == std::string::npos) {
			if (c.input.size() > 4096)
				respond(c, 413, "{}");
			return;
		}
		if (end > 4096) {
			respond(c, 413, "{}");
			return;
		}
		std::istringstream head(c.input.substr(0, end));
		std::string        first, method, path, version, extra;
		std::getline(head, first);
		std::istringstream line(first);
		line >> method >> path >> version;
		if ((line >> extra) || version != "HTTP/1.1" || (method != "GET" && method != "POST")) {
			respond(c, 400, "{}");
			return;
		}
		std::map<std::string, std::string> headers;
		std::string                        header;
		while (std::getline(head, header)) {
			if (!header.empty() && header.back() == '\r')
				header.pop_back();
			auto colon = header.find(':');
			if (colon == std::string::npos || colon == 0 || header[0] == ' ' || header[0] == '\t') {
				respond(c, 400, "{}");
				return;
			}
			std::string key = header.substr(0, colon);
			for (char &ch : key) {
				if (!std::isalnum((unsigned char)ch) && ch != '-') {
					respond(c, 400, "{}");
					return;
				}
				ch = (char)std::tolower((unsigned char)ch);
			}
			if (headers.count(key)) {
				respond(c, 400, "{}");
				return;
			}
			headers[key] = trim(header.substr(colon + 1));
		}
		if (!headers.count("host") || headers.count("transfer-encoding")) {
			respond(c, 400, "{}");
			return;
		}
		// Numeric Host only, with this listener's port: blocks DNS-rebinding origins.
		const std::string host  = headers["host"];
		auto              colon = host.find(':');
		in_addr           ip{};
		if (colon == std::string::npos || host.substr(colon + 1) != std::to_string(port) ||
		    inet_pton(AF_INET, host.substr(0, colon).c_str(), &ip) != 1) {
			respond(c, 403, "{}");
			return;
		}
		unsigned length = 0;
		if (headers.count("content-length")) {
			const auto &text = headers["content-length"];
			if (text.empty() || text.size() > 3) {
				respond(c, 413, "{}");
				return;
			}
			for (char ch : text) {
				if (ch < '0' || ch > '9') {
					respond(c, 400, "{}");
					return;
				}
				length = length * 10 + ch - '0';
			}
		}
		if (length > 128) {
			respond(c, 413, "{}");
			return;
		}
		if (method == "POST" &&
		    (!headers.count("content-length") || headers["origin"] != "http://" + host)) {
			respond(c, 403, "{}");
			return;
		}
		if (c.input.size() < end + 4 + length)
			return;
		if (c.input.size() != end + 4 + length) {
			respond(c, 400, "{}");
			return;
		}
		std::string body = c.input.substr(end + 4, length);
		if (method == "GET" && path == "/") {
			respond(c, 200, kCompanionPage, "", true);
			return;
		}
		peer *peer = known(headers);
		if (method == "POST" && path == "/join") {
			if (now - attempt_window >= 1) {
				attempt_window = now;
				attempts       = 0;
			}
			if (++attempts > 4) {
				respond(c, 429, "{}");
				return;
			}
			auto        newline  = body.find('\n');
			std::string supplied = body.substr(0, newline),
			            name = newline == std::string::npos ? "" : trim(body.substr(newline + 1));
			if (supplied != std::to_string(code)) {
				respond(c, 401, "{}");
				return;
			}
			if (name.size() > 20) {
				respond(c, 400, "{}");
				return;
			}
			for (unsigned char ch : name)
				if (!std::isalnum(ch) && ch != ' ' && ch != '_' && ch != '-' && ch != '.') {
					respond(c, 400, "{}");
					return;
				}
			if (!peer) {
				auto id = room.join(name, now);
				if (!id) {
					respond(c, 409, "{}");
					return;
				}
				for (auto &slot : peers) {
					bool exists = false;
					for (const auto &p : room.players())
						if (p.id == slot.player)
							exists = true;
					if (!exists)
						slot = impl::peer();
				}
				for (auto &slot : peers)
					if (!slot.player) {
						peer         = &slot;
						peer->player = id;
						break;
					}
			}
			if (!peer) {
				respond(c, 409, "{}");
				return;
			}
			peer->session       = token();
			peer->last_sequence = 0;
			peer->last_accepted = false;
			peer->active        = true;
			room.touch(peer->player, now);
			if (upstream.status() != companion_link::state::connected &&
			    upstream.status() != companion_link::state::pairing)
				upstream.connect("127.0.0.1", game_port, code, now);
			respond(c, 200, snapshot(*peer), peer->session);
			return;
		}
		if (!peer || !peer->active) {
			respond(c, 401, "{}");
			return;
		}
		room.touch(peer->player, now);
		if (method == "GET" && path == "/status") {
			respond(c, 200, snapshot(*peer));
			return;
		}
		if (method == "POST" && path == "/leave") {
			room.leave(peer->player);
			peer->active = false;
			if (pending_player == peer->player)
				pending_player = 0;
			respond(c, 200, "{}");
			return;
		}
		if (method == "POST" && path == "/start") {
			respond(c, room.start(peer->player) ? 200 : 409, "{}");
			return;
		}
		if (method == "POST" && path == "/shot") {
			unsigned                sequence = 0, turn = 0;
			ice::input::shot_intent shot;
			std::istringstream      payload(body);
			payload >> sequence >> turn >> shot.aim >> shot.power;
			if (!payload || payload >> extra || !sequence || !std::isfinite(shot.aim) ||
			    !std::isfinite(shot.power) || shot.aim < -1 || shot.aim > 1 || shot.power < 0 ||
			    shot.power > 1) {
				respond(c, 400, "{}");
				return;
			}
			if (sequence <= peer->last_sequence) {
				respond(c, sequence == peer->last_sequence && peer->last_accepted ? 200 : 409,
				        "{}");
				return;
			}
			peer->last_sequence = sequence;
			peer->last_accepted = false;
			if (!game_ready || pending_player || turn != room.turn() ||
			    !room.can_shoot(peer->player)) {
				respond(c, 409, "{}");
				return;
			}
			shot.seconds        = now;
			peer->last_accepted = upstream.send_shot(shot, now);
			if (peer->last_accepted) {
				pending_player = peer->player;
				pending_at     = now;
			}
			respond(c, peer->last_accepted ? 200 : 409, "{}");
			return;
		}
		respond(c, 404, "{}");
	}
};
companion_web::companion_web() : m_(new impl) {}
companion_web::~companion_web() {
	for (auto &c : m_->clients)
		m_->close_client(c);
	if (m_->listener != invalid_web)
		close_web(m_->listener);
}
void companion_web::close() {
	for (auto &c : m_->clients)
		m_->close_client(c);
	if (m_->listener != invalid_web)
		close_web(m_->listener);
	m_.reset(new impl);
}
bool companion_web::start(unsigned short companion_port, unsigned code) {
	close();
	if (!companion_port || code < 100000 || code > 999999)
		return false;
#ifdef _WIN32
	static const bool winsock = []() {
		WSADATA data;
		return WSAStartup(MAKEWORD(2, 2), &data) == 0;
	}();
	if (!winsock)
		return false;
#endif
	m_->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_->listener == invalid_web)
		return false;
	if (!nonblocking(m_->listener)) {
		close();
		return false;
	}
#ifdef __APPLE__
	int yes = 1;
	setsockopt(m_->listener, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
	sockaddr_in local{};
	local.sin_family      = AF_INET;
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(m_->listener, (sockaddr *)&local, sizeof(local)) < 0 || listen(m_->listener, 8) < 0) {
		close();
		return false;
	}
#ifdef _WIN32
	int length = sizeof(local);
#else
	socklen_t length = sizeof(local);
#endif
	if (getsockname(m_->listener, (sockaddr *)&local, &length) < 0) {
		close();
		return false;
	}
	m_->port      = ntohs(local.sin_port);
	m_->game_port = companion_port;
	m_->code      = code;
	return true;
}
void companion_web::poll(double now) {
	if (m_->listener == invalid_web || !std::isfinite(now) || now < 0)
		return;
	if (m_->last_poll >= 0 && (now < m_->last_poll || now - m_->last_poll > 1)) {
		m_->drop_peer();
		for (auto &c : m_->clients)
			m_->close_client(c);
	}
	m_->last_poll = now;
	m_->room.poll(now);
	bool active = false;
	for (auto &peer : m_->peers) {
		for (const auto &player : m_->room.players())
			if (player.id == peer.player && !player.online)
				peer.active = false;
		active |= peer.active;
	}
	if (!active && m_->upstream.status() != companion_link::state::closed)
		m_->upstream.close();
	if (m_->pending_player && now - m_->pending_at > .35)
		m_->pending_player = 0;
	m_->upstream.poll(now);
	for (int i = 0; i < 8; ++i) {
		auto socket = accept(m_->listener, nullptr, nullptr);
		if (socket == invalid_web)
			break;
		auto found = m_->clients.end();
		for (auto c = m_->clients.begin(); c != m_->clients.end(); ++c)
			if (c->socket == invalid_web) {
				found = c;
				break;
			}
		if (found == m_->clients.end() || !nonblocking(socket)) {
			close_web(socket);
			continue;
		}
#ifdef __APPLE__
		int yes = 1;
		setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
		found->socket = socket;
		found->began  = now;
	}
	for (auto &c : m_->clients) {
		if (c.socket == invalid_web)
			continue;
		if (now - c.began > 1) {
			m_->close_client(c);
			continue;
		}
		if (c.output.empty()) {
			char buffer[2048];
			int  n = (int)recv(c.socket, buffer, sizeof(buffer), 0);
			if (n > 0) {
				c.input.append(buffer, n);
				if (c.input.size() > 4230)
					m_->respond(c, 413, "{}");
				else
					m_->request(c, now);
			} else if (n == 0 || !would_block()) {
				m_->close_client(c);
				continue;
			}
		}
		if (!c.output.empty()) {
#ifdef MSG_NOSIGNAL
			const int flags = MSG_NOSIGNAL;
#else
			const int flags = 0;
#endif
			int n = (int)send(c.socket, c.output.data() + c.sent, (int)(c.output.size() - c.sent),
			                  flags);
			if (n > 0) {
				c.sent += n;
				if (c.sent == c.output.size())
					m_->close_client(c);
			} else if (n == 0 || !would_block())
				m_->close_client(c);
		}
	}
}
unsigned short companion_web::port() const { return m_->port; }
std::string    companion_web::join_url(const std::string &address) const {
    return "http://" + address + ":" + std::to_string(m_->port) + "/#" + std::to_string(m_->code);
}

void companion_web::set_game_ready(bool ready) {
	m_->game_ready = ready;
	if (!ready)
		m_->pending_player = 0;
}
std::uint64_t companion_web::claim_shot_owner(double now) {
	auto id            = m_->pending_player;
	m_->pending_player = 0;
	if (!std::isfinite(now) || now < m_->pending_at || now - m_->pending_at > .35 ||
	    !m_->room.begin_shot(id))
		return 0;
	return id;
}
bool companion_web::finish_shot(std::uint64_t player, unsigned points) {
	return m_->room.finish_shot(player, points);
}
const ice::input::companion_room &companion_web::room() const { return m_->room; }

} // namespace ice::net
