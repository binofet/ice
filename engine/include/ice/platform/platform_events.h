#pragma once
#include "ice/platform/input_types.h"
#include <cstddef>
#include <cstdint>
#include <string>
namespace ice::platform {

/// Process-scoped window identity; zero denotes application scope.
using window_id = std::uint64_t;
/// Zero denotes application scope. A native root window is not required.
constexpr window_id application_scope = 0;

/// Window/application lifecycle, native text/editing, physical controls and cancellation
/// categories.
enum class platform_event_type {
	window_registered,
	window_retired,
	window_resized,
	close_requested,
	focus_gained,
	focus_lost,
	application_quit_requested,
	input_activity,
	text_committed,
	text_composition,
	edit_command,
	input_cancelled,
	physical_key,
	pointer_button,
	pointer_move,
	scroll
};

/// Native interpreted editing action; composition navigation does not produce these actions.
enum class edit_command {
	none,
	accept_line,
	cancel,
	tab,
	backspace,
	delete_forward,
	move_left,
	move_right,
	move_up,
	move_down,
	line_start,
	line_end,
	page_up,
	page_down,
	word_left,
	word_right,
	delete_word_backward,
	delete_word_forward,
	delete_to_line_start,
	delete_to_line_end,
	select_all,
	copy,
	cut,
	paste
};
/// Keyboard modifier bits reported with a native input action.
enum class key_modifier : std::uint8_t {
	shift     = 1,
	control   = 2,
	alt       = 4,
	super     = 8,
	caps_lock = 16
};
/// Bit mask of key_modifier values; the super key means Command on macOS and Windows on Win32.
using key_modifiers = std::uint8_t;
/// Return the mask bit for a keyboard modifier.
constexpr key_modifiers modifier_flag(key_modifier modifier) {
	return static_cast<key_modifiers>(modifier);
}
/// Test whether the given native modifier mask contains a modifier.
constexpr bool has_modifier(key_modifiers modifiers, key_modifier modifier) {
	return (modifiers & modifier_flag(modifier)) != 0;
}

/// Application event ordered by sequence number; window zero denotes application scope.
struct platform_event {
	/// Event category determining which payload fields are meaningful.
	platform_event_type type = platform_event_type::window_registered;
	/// Target window identity; zero denotes application scope.
	window_id window = application_scope;
	/// Monotonically ordered event-stream sequence number.
	std::uint64_t sequence = 0;
	/// Owning UTF-8 commit/preedit payload, at most 4096 bytes; never synthesize it from key codes.
	std::string text{};
	/// Interpreted action, meaningful only for edit_command events.
	ice::platform::edit_command command = ice::platform::edit_command::none;
	/// Modifier state for keyboard, text/editing, pointer and scroll events.
	key_modifiers modifiers = 0;
	/// True for an OS keyboard-repeat action.
	bool repeat = false;
	/// Selection within preedit text, measured in UTF-8 bytes on code-point boundaries.
	std::size_t selection_start = 0, selection_length = 0;
	/// Physical keyboard position for physical_key events; independent of text/IME activation.
	ice::platform::physical_key key = ice::platform::physical_key::unknown;
	/// Button for pointer_button events, independent of native button numbering.
	mouse_button button = mouse_button::none;
	/// Down/up transition for physical_key and pointer_button events.
	bool pressed = false;
	/// Pointer position in logical client units from top-left; not changed by relative-mode warps.
	double x = 0, y = 0;
	/// Relative pointer displacement, positive right/down. First normal sample is zero.
	double delta_x = 0, delta_y = 0;
	/// Logical UI units normally; native raw relative input may explicitly report device counts.
	pointer_delta_unit delta_units = pointer_delta_unit::logical;
	/// Scroll intent, positive right/up; nominal wheel steps or precise logical UI units below.
	double scroll_x = 0, scroll_y = 0;
	/// True means precise logical UI units (AppKit points); false means nominal wheel steps.
	bool precise_scroll = false;
	/// Scope for input_cancelled; text-only cancellation does not reset physical controls/capture.
	input_cancellation cancellation = input_cancellation::all;
};

/// A bounded application event stream. Poll once, then route by window ID.
/// Host-thread clients route lifecycle notifications independently of native handles.
class platform_events {
public:
	/// Allocate a process-local nonzero window identity; a native root window is not required.
	static window_id allocate_window_id();
	/// Append an event to the bounded application stream; route by window ID after polling.
	static void post(platform_event_type type, window_id window = application_scope);
	/// Append a native/injected action, validating UTF-8, bounds, physical identities and finite
	/// pointer payloads. Physical actions require a nonzero window routing identity.
	/// The stream holds at most 1024 events and 256 KiB of text. Overflow clears queued
	/// actions and delivers application-scope input_cancelled before accepting new input.
	/// False denotes rejection and schedules all-input cancellation. Native adapters must release
	/// capture/discard composition after rejection; UI pumping or the effective pointer query also
	/// consumes cancellation independently of whether an application polls the notification.
	static bool post(platform_event event);
	/// Consume one queued lifecycle notification without blocking.
	static bool poll(platform_event &event);
	/// Return the cumulative count of events discarded by the bounded stream.
	static std::size_t dropped_events();
};

} // namespace ice::platform
