#pragma once
#include "ice/platform/input_types.h"
#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <vector>
namespace ice::platform {

/// Host-scoped surface identity; zero denotes no surface/application scope.
using surface_id = std::uint64_t;
/// Host-scoped input device identity; zero denotes no device.
using device_id = std::uint64_t;
/// Application activity state; stopped is terminal for a host instance.
enum class app_state { inactive, active, background, stopped };
/// Native graphics API associated with a render surface; no context is implicitly created.
enum class surface_api { software, metal, vulkan, d3d12 };
/// Borrowed render surface description. Dimensions are drawable pixels; the host owns the
/// registration, not the native handle.
struct surface {
	/// Owner-assigned identity; zero denotes no active binding.
	surface_id id = 0;
	/// Graphics API associated with the native surface handle.
	surface_api api = surface_api::software;
	/// Borrowed; the platform keeps it alive through on_surface_removed.
	void *native_handle = nullptr;
	/// Drawable width/height in pixels; zero suppresses rendering.
	int width = 0, height = 0; // drawable pixels, not UI points
	/// Drawable pixels per logical UI unit, supplied by the platform.
	float scale = 1;
	/// Whether this registered surface participates in rendering.
	bool visible = true;
};
/// Device sample category, determining the interpretation and units of sample components.
enum class input_kind { pointer, key, acceleration, angular_velocity, attitude };
/// Contact/key transition or an instantaneous sensor sample.
enum class input_phase { begin, change, end, cancel, sample };
/// Return the capability bit corresponding to an input-kind enumerator.
inline std::uint32_t input_capability(input_kind kind) { return 1u << static_cast<unsigned>(kind); }
/// Host-assigned device identity and supported input-capability bit mask.
struct input_device {
	/// Owner-assigned identity; zero denotes no active binding.
	device_id id = 0;
	/// Bit mask of supported input_kind categories.
	std::uint32_t capabilities = 0;
};
/// Timestamped device sample. Pointer coordinates are normalized top-left coordinates; motion uses
/// natural device axes and SI units.
struct device_input {
	/// Host-scoped input device identity.
	device_id device = 0;
	/// Target surface identity; zero means application-wide input.
	surface_id surface = 0; // zero is application-wide, e.g. a sensor
	/// Touch identity or key/button code, scoped to the input device.
	std::uint64_t contact = 0; // touch identity or key code
	/// Original monotonic sample timestamp in seconds.
	double seconds = 0; // host monotonic clock, in seconds
	/// Input/event category determining payload interpretation.
	input_kind kind = input_kind::pointer;
	/// Contact/key transition or instantaneous sensor sample.
	input_phase phase = input_phase::sample;
	/// Pointer: normalized top-left x/y (may exceed 0..1 during a drag).
	/// Motion: natural device axes, X right/Y top/Z out; acceleration is specific force
	/// in m/s^2 including gravity (+Z when stationary face-up), gyro is rad/s.
	float x = 0, y = 0, z = 0, w = 0;
	/// Optional shared physical keyboard position; unknown retains an adapter's legacy contact
	/// code.
	physical_key key = physical_key::unknown;
	/// Optional mouse button identity; none retains ordinary touch/contact semantics.
	mouse_button button = mouse_button::none;
};
/// Host-thread callbacks. Surface/device references are borrowed for the call; callbacks must not
/// mutate the host.
class host_client {
public:
	/// Destroy after the borrowing host has stopped invoking callbacks.
	virtual ~host_client() {}
	/// Observe an application-state transition on the host thread; do not mutate the host in this
	/// callback.
	virtual void on_app_state([[maybe_unused]] app_state state) {}
	/// Observe a newly registered borrowed native surface on the host thread.
	virtual void on_surface_added([[maybe_unused]] const surface &surface) {}
	/// Observe changed drawable pixel dimensions/scale/visibility; the description is borrowed for
	/// this callback.
	virtual void on_surface_changed([[maybe_unused]] const surface &surface) {}
	/// Observe removal before the platform invalidates its native handle; do not retain the
	/// description reference.
	virtual void on_surface_removed([[maybe_unused]] const surface &surface) {}
	/// Observe an input-device registration and its capability mask.
	virtual void on_device_added([[maybe_unused]] const input_device &device) {}
	/// Observe retirement of a device after its held input is cancelled.
	virtual void on_device_removed([[maybe_unused]] device_id device) {}
	/// Consume the borrowed input sample on the owning thread; recorded timestamps retain their
	/// original units.
	virtual void on_input([[maybe_unused]] const device_input &event) {}
	/// Advance application state by the supplied elapsed seconds; host thread only.
	virtual void on_update([[maybe_unused]] double delta_seconds) {}
	/// Render the borrowed surface on the host thread using its explicitly identified native API.
	virtual void on_render([[maybe_unused]] const surface &surface) {}
};
/// One explicit application owner; no native window, singleton, renderer, or loop required.
/// All methods/callbacks are host-thread-only except post_input. Do not mutate the host
/// inside callbacks. Stop explicitly while client and native handles are still alive.
class host {
public:
	/// Borrow the client callback object; stop the host while that client and all native handles
	/// are still alive.
	explicit host(host_client &client) : client_(client) {}
	/// Transition application activity; stopped is terminal and suspension cancels held inputs.
	bool set_state(app_state state);
	/// Return the current lifecycle/reducer state without transferring ownership.
	app_state state() const { return state_; }
	/// Register a borrowed native surface and return a host-scoped identity; host thread only.
	surface_id add_surface(surface surface);
	/// Update drawable pixel dimensions/scale for an existing surface; false rejects an invalid
	/// identity/description.
	bool resize_surface(surface_id id, int width, int height, float scale);
	/// Remove a surface registration and cancel input targeting it; native-handle ownership stays
	/// with the platform.
	bool remove_surface(surface_id id);
	/// Set whether an existing surface participates in rendering; host thread only.
	bool set_surface_visible(surface_id id, bool visible);
	/// Register input capabilities and return a host-scoped device identity.
	device_id add_device(std::uint32_t capabilities);
	/// Cancel held input and retire an existing device registration.
	bool remove_device(device_id id);
	/// Thread-safe enqueue of a device sample. False rejects invalid, suspended, or overflowing
	/// input.
	bool post_input(const device_input &event);
	/// Drain queued input, update the client, and render visible surfaces using a monotonic
	/// timestamp in seconds.
	void frame(double monotonic_seconds);
	/// Stop activity and cancel pending input; callers must keep borrowed client/native handles
	/// alive through shutdown.
	void stop();
	/// Return the atomic cumulative rejected/overflowed input count.
	std::uint64_t dropped_input() const { return dropped_.load(); }

private:
	void                              cancel_inputs(device_id device = 0, surface_id surface = 0);
	void                              drain_input();
	host_client                      &client_;
	app_state                         state_ = app_state::inactive;
	std::map<surface_id, surface>     surfaces_;
	std::map<device_id, input_device> devices_;
	std::vector<device_input>         held_;
	std::mutex                        mutex_;
	std::deque<device_input>          pending_;
	std::atomic<bool>                 accept_input_{false};
	std::atomic<std::uint64_t>        dropped_{0};
	bool                              overflow_ = false, clock_valid_ = false;
	double                            previous_     = 0;
	std::uint64_t                     next_surface_ = 1, next_device_ = 1;
};

} // namespace ice::platform
