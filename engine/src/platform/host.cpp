#include "ice/platform/host.h"
#include <algorithm>
#include <cmath>
namespace ice::platform {

bool host::set_state(app_state state) {
	if (state_ == app_state::stopped)
		return false;
	if (state == app_state::stopped) {
		stop();
		return true;
	}
	if (state == state_)
		return true;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		accept_input_ = state == app_state::active;
		pending_.clear();
		overflow_ = false;
	}
	if (state != app_state::active)
		cancel_inputs();
	state_       = state;
	clock_valid_ = false;
	client_.on_app_state(state);
	return true;
}
surface_id host::add_surface(surface surface) {
	if (state_ == app_state::stopped || surface.width < 0 || surface.height < 0 ||
	    !std::isfinite(surface.scale) || surface.scale <= 0)
		return 0;
	surface.id            = next_surface_++;
	surfaces_[surface.id] = surface;
	client_.on_surface_added(surface);
	return surface.id;
}
bool host::resize_surface(surface_id id, int width, int height, float scale) {
	auto found = surfaces_.find(id);
	if (found == surfaces_.end() || width < 0 || height < 0 || !std::isfinite(scale) || scale <= 0)
		return false;
	auto &surface = found->second;
	if (surface.width == width && surface.height == height && surface.scale == scale)
		return true;
	surface.width  = width;
	surface.height = height;
	surface.scale  = scale;
	client_.on_surface_changed(surface);
	return true;
}
bool host::set_surface_visible(surface_id id, bool visible) {
	auto found = surfaces_.find(id);
	if (found == surfaces_.end())
		return false;
	if (found->second.visible == visible)
		return true;
	found->second.visible = visible;
	if (!visible)
		cancel_inputs(0, id);
	client_.on_surface_changed(found->second);
	return true;
}
bool host::remove_surface(surface_id id) {
	auto found = surfaces_.find(id);
	if (found == surfaces_.end())
		return false;
	cancel_inputs(0, id);
	client_.on_surface_removed(found->second);
	surfaces_.erase(found);
	return true;
}
device_id host::add_device(std::uint32_t capabilities) {
	if (state_ == app_state::stopped || !capabilities)
		return 0;
	input_device device;
	device.id           = next_device_++;
	device.capabilities = capabilities;
	devices_[device.id] = device;
	client_.on_device_added(device);
	return device.id;
}
bool host::remove_device(device_id id) {
	if (!devices_.count(id))
		return false;
	cancel_inputs(id);
	devices_.erase(id);
	client_.on_device_removed(id);
	return true;
}
bool host::post_input(const device_input &event) {
	if (!std::isfinite(event.seconds) || event.seconds < 0 || !std::isfinite(event.x) ||
	    !std::isfinite(event.y) || !std::isfinite(event.z) || !std::isfinite(event.w))
		return false;
	std::lock_guard<std::mutex> lock(mutex_);
	if (!accept_input_)
		return false;
	if (pending_.size() == 1024) {
		++dropped_;
		overflow_ = true;
		return false;
	}
	pending_.push_back(event);
	return true;
}
void host::cancel_inputs(device_id device, surface_id surface) {
	for (auto i = held_.begin(); i != held_.end();) {
		if ((!device || i->device == device) && (!surface || i->surface == surface)) {
			auto event  = *i;
			event.phase = input_phase::cancel;
			client_.on_input(event);
			i = held_.erase(i);
		} else
			++i;
	}
}
void host::drain_input() {
	std::deque<device_input> pending;
	bool                     overflow;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		pending.swap(pending_);
		overflow  = overflow_;
		overflow_ = false;
	}
	// A dropped release must never leave a key/touch held indefinitely.
	if (overflow) {
		cancel_inputs();
		return;
	}
	for (auto event : pending) {
		auto     device = devices_.find(event.device);
		unsigned kind   = static_cast<unsigned>(event.kind);
		if (kind > static_cast<unsigned>(input_kind::attitude) || device == devices_.end() ||
		    !(device->second.capabilities & input_capability(event.kind)) ||
		    (event.surface &&
		     (!surfaces_.count(event.surface) || !surfaces_.at(event.surface).visible)))
			continue;
		bool held = event.kind == input_kind::pointer || event.kind == input_kind::key;
		auto same = [&](const device_input &other) {
			return other.device == event.device && other.surface == event.surface &&
			       other.contact == event.contact && other.kind == event.kind &&
			       other.key == event.key && other.button == event.button;
		};
		if (held) {
			auto found = std::find_if(held_.begin(), held_.end(), same);
			if (event.phase == input_phase::begin) {
				if (found != held_.end())
					continue;
				held_.push_back(event);
			} else if (event.phase == input_phase::change) {
				if (found == held_.end())
					continue;
				*found = event;
			} else if (event.phase == input_phase::end || event.phase == input_phase::cancel) {
				if (found == held_.end())
					continue;
				held_.erase(found);
			} else
				continue;
		} else if (event.phase != input_phase::sample)
			continue;
		client_.on_input(event);
	}
}
void host::frame(double monotonic_seconds) {
	if (state_ != app_state::active || !std::isfinite(monotonic_seconds) || monotonic_seconds < 0)
		return;
	drain_input();
	double dt = clock_valid_ ? std::max(0.0, std::min(.1, monotonic_seconds - previous_)) : 0;
	// Never move the time baseline backwards if a platform supplies a stale tick.
	if (!clock_valid_ || monotonic_seconds >= previous_)
		previous_ = monotonic_seconds;
	clock_valid_ = true;
	client_.on_update(dt);
	for (const auto &entry : surfaces_)
		if (entry.second.visible && entry.second.width > 0 && entry.second.height > 0)
			client_.on_render(entry.second);
}
void host::stop() {
	if (state_ == app_state::stopped)
		return;
	set_state(app_state::inactive);
	while (!surfaces_.empty())
		remove_surface(surfaces_.begin()->first);
	while (!devices_.empty())
		remove_device(devices_.begin()->first);
	state_ = app_state::stopped;
	client_.on_app_state(state_);
}

} // namespace ice::platform
