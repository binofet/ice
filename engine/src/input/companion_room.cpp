#include "ice/input/companion_room.h"
#include <algorithm>
#include <cmath>
namespace ice::input {
companion_room::player *companion_room::find(std::uint64_t id) {
	for (auto &p : players_)
		if (p.id == id)
			return &p;
	return nullptr;
}
std::uint64_t companion_room::join(const std::string &name, double now) {
	if (!std::isfinite(now) || now < 0 || name.size() > 20)
		return 0;
	if (players_.size() >= 8) {
		auto old = std::find_if(players_.begin(), players_.end(), [&](const player &p) {
			return !p.online && p.id != shooter_ && p.id != current_;
		});
		if (old == players_.end())
			return 0;
		players_.erase(old);
	}
	player p;
	p.id        = next_++;
	p.seen      = now;
	p.name      = name.empty() ? "Player " + std::to_string(p.id) : name;
	p.forfeited = phase_ == phase::playing;
	players_.push_back(p);
	return p.id;
}
bool companion_room::touch(std::uint64_t id, double now) {
	auto *p = find(id);
	if (!p || !std::isfinite(now) || now < 0)
		return false;
	p->online = true;
	p->seen   = now;
	return true;
}
void companion_room::leave(std::uint64_t id) {
	auto *p = find(id);
	if (!p)
		return;
	p->online = false;
	if (phase_ == phase::playing)
		p->forfeited = true;
	if (!shooter_ && id == current_)
		advance();
}
void companion_room::poll(double now) {
	if (!std::isfinite(now) || now < 0)
		return;
	for (auto &p : players_)
		if (p.online && now - p.seen > 10)
			leave(p.id);
}
std::uint64_t companion_room::leader() const {
	for (const auto &p : players_)
		if (p.online)
			return p.id;
	return 0;
}
bool companion_room::start(std::uint64_t leader) {
	if (!leader || leader != this->leader() || phase_ == phase::playing)
		return false;
	current_ = shooter_ = 0;
	++round_;
	for (auto &p : players_) {
		p.score = p.balls = 0;
		p.forfeited       = !p.online;
		if (p.online && !current_)
			current_ = p.id;
	}
	if (!current_)
		return false;
	phase_ = phase::playing;
	++turn_;
	return true;
}
std::uint64_t companion_room::current() const { return phase_ == phase::playing ? current_ : 0; }
bool          companion_room::can_shoot(std::uint64_t player) const {
    if (phase_ != phase::playing || shooter_ || !player || player != current_)
        return false;
    for (const auto &p : players_)
        if (p.id == player)
            return p.online && !p.forfeited && p.balls < 5;
    return false;
}
bool companion_room::begin_shot(std::uint64_t player) {
	if (!can_shoot(player))
		return false;
	shooter_ = player;
	return true;
}
bool companion_room::finish_shot(std::uint64_t player, unsigned points) {
	if (!player || player != shooter_ || points > 100)
		return false;
	auto *p = find(player);
	if (!p)
		return false;
	p->score += points;
	++p->balls;
	shooter_ = 0;
	advance();
	return true;
}
void companion_room::advance() {
	if (phase_ != phase::playing || shooter_)
		return;
	unsigned start = 0;
	for (unsigned i = 0; i < players_.size(); ++i)
		if (players_[i].id == current_) {
			start = i + 1;
			break;
		}
	for (unsigned i = 0; i < players_.size(); ++i) {
		auto &p = players_[(start + i) % players_.size()];
		if (p.online && !p.forfeited && p.balls < 5) {
			current_ = p.id;
			++turn_;
			return;
		}
	}
	current_ = 0;
	phase_   = phase::results;
}
void companion_room::interrupt() {
	shooter_ = current_ = 0;
	if (phase_ == phase::playing)
		phase_ = phase::results;
	for (auto &p : players_)
		p.online = false;
}

} // namespace ice::input
