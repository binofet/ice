#include "lease_state.h"
#include <exception>
#include <utility>

namespace ice::render {
acquisition_lease::acquisition_lease() noexcept = default;
acquisition_lease::acquisition_lease(std::shared_ptr<detail::acquisition_state> state) noexcept
    : state_(std::move(state)) {}
acquisition_lease::acquisition_lease(acquisition_lease &&other) noexcept = default;
acquisition_lease &acquisition_lease::operator=(acquisition_lease &&other) noexcept {
	if (this != &other) {
		expire();
		state_ = std::move(other.state_);
	}
	return *this;
}
acquisition_lease::~acquisition_lease() { expire(); }
void acquisition_lease::expire() noexcept {
	if (state_)
		state_->active = false;
}
bool acquisition_lease::active() const noexcept {
	return state_ && state_->active && !state_->owner.expired();
}
bool          acquisition_lease::waited() const noexcept { return active() && state_->waited; }
std::uint64_t acquisition_lease::generation() const noexcept {
	return state_ ? state_->generation : 0;
}
ice::expected<void, diagnostic> acquisition_lease::mark_waited() {
	if (!active())
		return ice::unexpected(diagnostic{"Image acquisition lease has expired"});
	state_->waited = true;
	return {};
}
ice::expected<acquisition_lease, diagnostic>
detail::lease_source::acquire(std::uint64_t generation) {
	if (!generation || generation <= latest_)
		return ice::unexpected(
		    diagnostic{"Image acquisition generation must increase and be nonzero"});
	if (auto existing = active_.lock(); existing && existing->active)
		return ice::unexpected(
		    diagnostic{"Retain the existing acquired image across wait timeouts"});
	try {
		auto state        = std::make_shared<acquisition_state>();
		state->owner      = owner_;
		state->generation = generation;
		active_           = state;
		latest_           = generation;
		return acquisition_lease(std::move(state));
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
}
ice::expected<void, diagnostic>
detail::lease_source::validate(const acquisition_lease &lease) const {
	if (!lease.waited() || lease.state_->owner.lock() != owner_ || lease.generation() != latest_)
		return ice::unexpected(diagnostic{"Foreign, unwaited or expired image acquisition lease"});
	return {};
}
} // namespace ice::render
