#include "ice/input/action_router.h"
#include "action_router/allocations.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
using namespace ice::input;
constexpr action_id         move = 1, jump = 2, look = 3, rate = 4, hold = 5, menu = 6;
constexpr action_channel_id gameplay = 10, camera = 20, ui = 30;
constexpr action_owner      player{1, 1}, first{2, 1}, second{2, 2}, third{3, 1}, interface{4, 1};
void                        check(bool condition, const char *expression, int line) {
    if (!condition)
        throw std::runtime_error("Router check " + std::to_string(line) + ": " + expression);
}
#define CHECK(expression) check(bool(expression), #expression, __LINE__)
template <class T> T take(ice::expected<T, action_diagnostic> value) {
	if (!value)
		throw std::runtime_error(value.error().message);
	return std::move(*value);
}
bool                           close(double a, double b) { return std::abs(a - b) < 1e-12; }
std::vector<action_descriptor> declarations() {
	return {{move, "player.move", action_kind::axis2},
	        {jump, "player.jump"},
	        {look, "camera.delta", action_kind::axis2, action_behavior::delta, false},
	        {rate, "camera.rate", action_kind::axis1, action_behavior::rate, false},
	        {hold, "camera.hold"},
	        {menu, "ui.menu"}};
}
std::vector<action_channel_binding> routes() {
	return {{move, gameplay}, {jump, gameplay}, {look, camera},
	        {rate, camera},   {hold, camera},   {menu, ui}};
}
std::vector<action_binding> bindings() {
	std::vector<action_binding> result;
	for (const auto &item : declarations()) {
		action_binding binding;
		binding.action          = item.id;
		binding.control         = control_kind::virtual_control;
		binding.virtual_control = item.id;
		binding.virtual_kind    = item.kind;
		result.push_back(binding);
	}
	return result;
}
struct fixture {
	action_reducer input = take(action_reducer::create(0, declarations(), bindings()));
	action_router  router;
	action_clock   clock;
	source_id      source = 0;
	std::uint64_t  tick   = 0;
	action_frame   last;
	double         last_seconds = .1;
	std::shared_ptr<const routed_action_frame> cache;
	explicit fixture(action_clock domain = action_clock::simulation)
	    : router(take(action_router::create(declarations(), routes(), domain))), clock(domain) {
		input.set_focused(true);
		CHECK(input.set_context(input_context::player));
		source = take(input.add_source());
		for (const auto &item : declarations())
			CHECK(input.submit(source, {item.id, item.kind, 0, 0, true}));
	}
	void sample(action_id id, double x, double y = 0) {
		const auto items = declarations();
		auto       found = std::find_if(items.begin(), items.end(),
		                                [id](const auto &item) { return item.id == id; });
		CHECK(found != items.end());
		CHECK(input.submit(source, {id, found->kind, x, y, true}));
	}
	action_frame frame(double seconds = .1) { return take(input.consume_tick(tick++, seconds)); }
	action_channel_handoff change(action_channel_id id, action_owner desired) {
		const auto lease = router.lease(id);
		CHECK(lease);
		return {id, lease->epoch, desired};
	}
	void stage(action_channel_id id, action_owner desired) {
		const auto value = change(id, desired);
		CHECK(router.stage_handoffs(std::span(&value, 1)));
	}
	std::shared_ptr<const routed_action_frame> publish(double seconds = .1) {
		last         = frame(seconds);
		last_seconds = seconds;
		auto result  = take(router.publish(last, clock, seconds));
		CHECK(result.advanced);
		cache = result.frame;
		return cache;
	}
	void grant() {
		const std::array changes{change(gameplay, player), change(camera, first),
		                         change(ui, interface)};
		CHECK(router.stage_handoffs(changes));
		publish();
		publish();
	}
};
const routed_action_channel &channel(const routed_action_frame &frame, action_channel_id id,
                                     action_owner owner) {
	const auto *found = frame.find(id, owner, frame.clock);
	CHECK(found);
	return *found;
}
const action_value &value(const routed_action_frame &frame, action_channel_id id,
                          action_owner owner, action_id action) {
	const auto *found = channel(frame, id, owner).find(action);
	CHECK(found);
	return *found;
}
void neutral(const routed_action_channel &channel) {
	for (const auto &item : channel.values)
		CHECK(item.x == 0 && item.y == 0 && !item.down && !item.pressed && !item.released);
}
void profile() {
	fixture f;
	CHECK(f.router.profile().actions == 6 && f.router.profile().channels == 3);
	CHECK(f.router.lease(camera)->owner == action_owner{} && f.router.lease(camera)->epoch == 0);
	f.sample(jump, 1);
	f.sample(hold, 1);
	f.sample(look, 4, -2);
	auto frame = f.publish();
	CHECK(frame->channels.size() == 3 && frame->retirements.empty());
	for (const auto &item : frame->channels) {
		CHECK(item.owner == action_owner{});
		neutral(item);
	}
	CHECK(!frame->find(camera, {}, f.clock) && !f.router.lease(999));
	// A real upstream profile beyond its default 1024 actions must not inherit a new low cap.
	constexpr std::size_t               count = 1536;
	std::vector<action_descriptor>      large;
	std::vector<action_channel_binding> map;
	for (std::size_t i = 0; i < count; ++i) {
		const auto id = action_id(i + 1);
		large.push_back({id, "action." + std::to_string(id)});
		map.push_back({id, action_channel_id(i % 256 + 1)});
	}
	action_limits limits;
	limits.actions = count;
	auto source    = take(action_reducer::create(0, large, {}, limits));
	auto router    = take(action_router::create(large, map, action_clock::presentation));
	CHECK(router.profile().actions == count && router.profile().channels == 256);
	auto routed =
	    take(router.publish(take(source.consume_tick(0, 0)), action_clock::presentation, 0));
	CHECK(routed.advanced && routed.frame->channels.size() == 256);
	map.pop_back();
	CHECK(!action_router::create(large, map, action_clock::simulation));
	auto small      = routes();
	small[0].action = 999;
	CHECK(!action_router::create(declarations(), small, f.clock));
	small           = routes();
	small[0].action = jump;
	CHECK(!action_router::create(declarations(), small, f.clock));
	small            = routes();
	small[0].channel = 0;
	CHECK(!action_router::create(declarations(), small, f.clock));
	CHECK(!action_router::create(declarations(), routes(), static_cast<action_clock>(99)));
}
void held() {
	fixture f;
	f.grant();
	f.sample(jump, 1);
	f.sample(hold, 1);
	f.sample(move, .5, .25);
	auto old = f.publish();
	CHECK(value(*old, camera, first, hold).pressed && value(*old, gameplay, player, jump).pressed);
	const auto epoch = f.router.lease(camera)->epoch;
	f.stage(camera, second);
	CHECK(f.router.lease(camera)->owner == first && f.router.lease(camera)->epoch == epoch);
	auto duplicate = take(f.router.publish(f.last, f.clock, f.last_seconds));
	CHECK(!duplicate.advanced && duplicate.frame == old && f.router.lease(camera)->owner == first);
	auto next = f.publish();
	CHECK(f.router.lease(camera)->owner == second && f.router.lease(camera)->epoch == epoch + 1);
	neutral(channel(*next, camera, second));
	CHECK(channel(*next, camera, second).cancelled && next->retirements.size() == 1);
	const auto &retired = next->retirements[0];
	CHECK(retired.owner == first && retired.lease == epoch && retired.channel == camera &&
	      retired.cancelled);
	CHECK(retired.find(hold)->released && !retired.find(hold)->down &&
	      !retired.find(hold)->pressed);
	CHECK(!retired.find(look)->pressed && retired.find(look)->x == 0 && retired.find(rate)->x == 0);
	CHECK(!channel(*next, gameplay, player).cancelled && value(*next, gameplay, player, jump).down);
	CHECK(!value(*next, gameplay, player, jump).released &&
	      value(*next, gameplay, player, move).x == .5);
	CHECK(f.input.context() == input_context::player);
	CHECK(!next->find(camera, first, f.clock));
	next = f.publish();
	CHECK(!value(*next, camera, second, hold).down && next->retirements.empty());
	f.sample(hold, 0);
	next = f.publish();
	CHECK(!value(*next, camera, second, hold).released);
	f.sample(hold, 1);
	next = f.publish();
	CHECK(value(*next, camera, second, hold).down && value(*next, camera, second, hold).pressed);
}
void deltas() {
	fixture f;
	f.grant();
	f.sample(look, 3, -2);
	f.sample(rate, .5);
	auto frame = f.publish(.2);
	CHECK(value(*frame, camera, first, look).x == 3);
	CHECK(close(value(*frame, camera, first, rate).x, .1));
	f.stage(camera, second);
	f.sample(look, 8, 9);
	frame = f.publish(0);
	CHECK(value(*frame, camera, second, look).x == 0 && value(*frame, camera, second, rate).x == 0);
	frame = f.publish(.2);
	CHECK(value(*frame, camera, second, look).x == 0 && value(*frame, camera, second, rate).x == 0);
	f.sample(rate, 0);
	frame = f.publish(0);
	CHECK(value(*frame, camera, second, rate).x == 0);
	f.sample(rate, .5);
	frame = f.publish(.2);
	CHECK(value(*frame, camera, second, rate).x == 0);
	f.sample(rate, 0);
	f.publish(.2);
	f.sample(rate, 1);
	f.sample(look, 2, 1);
	frame = f.publish(.25);
	CHECK(close(value(*frame, camera, second, rate).x, .25) &&
	      value(*frame, camera, second, look).x == 2);
	const auto repeated = take(f.router.publish(f.last, f.clock, .25));
	CHECK(!repeated.advanced && repeated.frame == frame);
	frame = f.publish(.25);
	CHECK(value(*frame, camera, second, look).x == 0 &&
	      close(value(*frame, camera, second, rate).x, .25));
}
void cancellation() {
	fixture f;
	f.grant();
	f.sample(jump, 1);
	f.sample(hold, 1);
	f.publish();
	const auto camera_epoch = f.router.lease(camera)->epoch,
	           player_epoch = f.router.lease(gameplay)->epoch;
	f.input.set_focused(false);
	auto frame = f.publish();
	CHECK(channel(*frame, camera, first).cancelled && channel(*frame, gameplay, player).cancelled);
	CHECK(value(*frame, camera, first, hold).released && !value(*frame, camera, first, hold).down);
	CHECK(value(*frame, gameplay, player, jump).released &&
	      !value(*frame, gameplay, player, jump).down);
	f.input.set_focused(true);
	f.input.cancel();
	f.sample(jump, 0);
	f.sample(jump, 1);
	f.sample(hold, 0);
	f.sample(hold, 1);
	f.sample(look, 4, 2);
	frame = f.publish();
	CHECK(f.last.cancelled && channel(*frame, camera, first).cancelled);
	CHECK(value(*frame, camera, first, hold).pressed &&
	      value(*frame, gameplay, player, jump).pressed);
	CHECK(value(*frame, camera, first, look).x == 4);
	CHECK(f.router.lease(camera)->epoch == camera_epoch &&
	      f.router.lease(gameplay)->epoch == player_epoch);
}
void generations() {
	fixture f;
	f.grant();
	f.sample(hold, 1);
	f.publish();
	f.stage(camera, second);
	auto frame = f.publish();
	CHECK(frame->retirements[0].owner == first);
	CHECK(!frame->find(camera, first, f.clock) && frame->find(camera, second, f.clock));
	CHECK(!frame->find(camera, {second.identity, 99}, f.clock));
	CHECK(!frame->find(camera, second, action_clock::presentation));
	const auto epoch = f.router.lease(camera)->epoch;
	f.stage(camera, {});
	frame = f.publish();
	CHECK(f.router.lease(camera)->owner == action_owner{} &&
	      f.router.lease(camera)->epoch == epoch + 1);
	CHECK(frame->retirements.size() == 1 && frame->retirements[0].owner == second);
	for (const auto &sample : frame->channels)
		if (sample.channel == camera)
			neutral(sample);
	f.stage(camera, third);
	frame = f.publish();
	neutral(channel(*frame, camera, third));
	f.sample(hold, 0);
	f.publish();
	f.sample(hold, 1);
	frame = f.publish();
	CHECK(value(*frame, camera, third, hold).pressed);
}
void consumers() {
	fixture f;
	f.grant();
	f.sample(jump, 1);
	f.sample(hold, 1);
	f.sample(look, 1, 2);
	auto        frame = f.publish();
	const auto *a = frame->find(camera, first, f.clock), *b = frame->find(camera, first, f.clock);
	CHECK(a && a == b && a->find(hold)->pressed &&
	      channel(*frame, gameplay, player).find(jump)->pressed);
	unsigned dispatches = 1;
	for (unsigned eye_or_reader = 0; eye_or_reader < 16; ++eye_or_reader) {
		auto repeated = take(f.router.publish(f.last, f.clock, f.last_seconds));
		if (repeated.advanced)
			++dispatches;
		CHECK(repeated.frame == frame && !repeated.advanced);
	}
	CHECK(dispatches == 1);
	f.stage(camera, second);
	CHECK(!take(f.router.publish(f.last, f.clock, f.last_seconds)).advanced);
	auto next = f.publish();
	CHECK(next != frame && !next->find(camera, first, f.clock));
	CHECK(frame->find(camera, first, f.clock)
	          ->find(hold)
	          ->pressed); // Historical owned data, no dispatch.
	auto moved = std::move(f.router);
	CHECK(!f.router.lease(camera) && f.router.profile().actions == 0);
	CHECK(!f.router.publish(f.last, f.clock, f.last_seconds));
	CHECK(!take(moved.publish(f.last, f.clock, f.last_seconds)).advanced);
}
void atomic() {
	fixture f;
	f.grant();
	f.stage(camera, second);
	const std::array invalid{f.change(gameplay, third), action_channel_handoff{999, 0, third}};
	CHECK(!f.router.stage_handoffs(invalid));
	const auto bad_owner = action_channel_handoff{camera, 1, {0, 3}};
	CHECK(!f.router.stage_handoffs(std::span(&bad_owner, 1)));
	const std::array duplicate{f.change(camera, third), f.change(camera, third)};
	CHECK(!f.router.stage_handoffs(duplicate));
	const auto stale = action_channel_handoff{camera, 0, third};
	CHECK(!f.router.stage_handoffs(std::span(&stale, 1)));
	const auto original = f.frame();
	const auto old      = f.cache;
	for (unsigned fault = 0; fault < 7; ++fault) {
		auto bad = original;
		if (fault == 0)
			bad.values.pop_back();
		if (fault == 1)
			bad.values[0].x = std::numeric_limits<double>::quiet_NaN();
		if (fault == 2)
			bad.values[0].id = 999;
		if (fault == 3)
			bad.values[1].x = 1; // down=false contradicts a button's published level.
		if (fault == 4)
			bad.values[0].kind = action_kind::button;
		if (fault == 5)
			bad.generation = 0;
		if (fault == 6)
			bad.context = static_cast<input_context>(99);
		CHECK(!f.router.publish(bad, f.clock, .1));
		CHECK(f.router.lease(camera)->owner == first && f.router.lease(camera)->epoch == 1);
		CHECK(take(f.router.publish(f.last, f.clock, f.last_seconds)).frame == old);
	}
	CHECK(!f.router.publish(original, action_clock::presentation, .1));
	CHECK(!f.router.publish(original, f.clock, -1));
	auto result = take(f.router.publish(original, f.clock, .1));
	CHECK(result.advanced);
	CHECK(f.router.lease(camera)->owner == second && f.router.lease(gameplay)->owner == player);
	f.last  = original;
	f.cache = result.frame;
	f.stage(camera, third);
	CHECK(f.router.stage_handoffs({}));
	auto next = f.publish();
	CHECK(f.router.lease(camera)->owner == second && next->retirements.empty());
	f.sample(hold, 0);
	f.publish();
	f.sample(hold, 1);
	f.publish();
	const auto epoch = f.router.lease(camera)->epoch;
	f.stage(camera, second);
	next = f.publish();
	CHECK(f.router.lease(camera)->epoch == epoch && !channel(*next, camera, second).cancelled &&
	      value(*next, camera, second, hold).down);
}
void clocks() {
	for (auto clock : {action_clock::simulation, action_clock::presentation}) {
		fixture f(clock);
		f.grant();
		f.sample(rate, 1);
		auto frame = f.publish(.125);
		CHECK(frame->clock == clock && frame->seconds == .125 &&
		      value(*frame, camera, first, rate).x == .125);
		const auto other = clock == action_clock::simulation ? action_clock::presentation
		                                                     : action_clock::simulation;
		CHECK(!frame->find(camera, first, other));
		CHECK(!f.router.publish(f.last, other, .125));
		auto conflict      = f.last;
		conflict.cancelled = !conflict.cancelled;
		CHECK(!f.router.publish(conflict, clock, .125));
		CHECK(!f.router.publish(f.last, clock, .25));
		CHECK(!take(f.router.publish(f.last, clock, .125)).advanced);
		auto backwards = f.last;
		--backwards.tick;
		CHECK(!f.router.publish(backwards, clock, .125));
	}
}
void allocations() {
	namespace probe      = ice::test::route_allocations;
	bool        complete = false;
	std::size_t failures = 0;
	for (std::ptrdiff_t ordinal = 0; ordinal < 256; ++ordinal) {
		fixture f;
		f.grant();
		f.stage(camera, second);
		const auto                         source = f.frame();
		const auto                         old    = f.cache;
		std::optional<action_route_result> result;
		probe::arm(ordinal);
		try {
			auto routed = f.router.publish(source, f.clock, .1);
			CHECK(routed);
			result = std::move(*routed);
		} catch (const std::bad_alloc &) {
		}
		probe::disarm();
		if (probe::fired()) {
			++failures;
			CHECK(!result && f.router.lease(camera)->owner == first &&
			      f.router.lease(camera)->epoch == 1);
			CHECK(take(f.router.publish(f.last, f.clock, f.last_seconds)).frame == old);
			auto retry = take(f.router.publish(source, f.clock, .1));
			CHECK(retry.advanced && f.router.lease(camera)->owner == second);
		} else {
			CHECK(result && result->advanced && f.router.lease(camera)->owner == second);
			complete = true;
			break;
		}
	}
	CHECK(complete && failures > 0);
	std::printf(
	    "Atomic route publication: %zu allocation failures preserved leases/cache/staging\n",
	    failures);
	complete = false;
	failures = 0;
	for (std::ptrdiff_t ordinal = 0; ordinal < 32; ++ordinal) {
		fixture f;
		f.grant();
		f.stage(camera, second);
		const auto change   = f.change(camera, third);
		bool       accepted = false;
		probe::arm(ordinal);
		try {
			CHECK(f.router.stage_handoffs(std::span(&change, 1)));
			accepted = true;
		} catch (const std::bad_alloc &) {
		}
		probe::disarm();
		if (probe::fired()) {
			++failures;
			CHECK(!accepted);
			f.publish();
			CHECK(f.router.lease(camera)->owner == second);
		} else {
			CHECK(accepted);
			f.publish();
			CHECK(f.router.lease(camera)->owner == third);
			complete = true;
			break;
		}
	}
	CHECK(complete && failures > 0);
	std::printf("Atomic handoff staging: %zu allocation failures preserved pending transaction\n",
	            failures);
}
} // namespace
int main(int argc, char **argv) {
	try {
		CHECK(argc == 2);
		const std::string_view name(argv[1]);
		if (name == "profile")
			profile();
		else if (name == "held")
			held();
		else if (name == "delta")
			deltas();
		else if (name == "cancel")
			cancellation();
		else if (name == "generation")
			generations();
		else if (name == "consumers")
			consumers();
		else if (name == "atomic")
			atomic();
		else if (name == "clocks")
			clocks();
		else if (name == "allocation")
			allocations();
		else
			throw std::runtime_error("Unknown router case");
		std::printf("PASS action router %s\n", argv[1]);
		return 0;
	} catch (const std::exception &error) {
		ice::test::route_allocations::disarm();
		std::fprintf(stderr, "%s\n", error.what());
		return 1;
	}
}
