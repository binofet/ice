#include "ice/input/action_router.h"
#include "ice/motion/pose.h"
#include <array>
#include <cstdio>

// An ordinary consumer links exported ICE targets, never library implementation sources.
int main() {
	using namespace ice::input;
	constexpr action_id                  action  = 1;
	constexpr action_channel_id          channel = 10;
	constexpr action_owner               owner{23, 1};
	const std::vector<action_descriptor> catalog{{action, "desired.offset", action_kind::axis1}};
	auto                                 input = action_reducer::create(0, catalog, {});
	auto router = action_router::create(catalog, {{action, channel}}, action_clock::simulation);
	if (!input || !router)
		return 1;
	const std::array changes{action_channel_handoff{channel, 0, owner}};
	if (!router->stage_handoffs(changes))
		return 2;
	const auto frame = input->consume_tick(0, .1);
	if (!frame)
		return 3;
	const auto routed = router->publish(*frame, action_clock::simulation, .1);
	if (!routed || !routed->advanced)
		return 4;
	const auto *sample = routed->frame->find(channel, owner, action_clock::simulation);
	if (!sample || !sample->find(action) || sample->find(action)->x != 0)
		return 5;
	const ice::motion::pose_sample pose{{}, {17, 700, routed->frame->tick}};
	const auto                     validated = ice::motion::validate_sample(pose);
	const auto                     blended   = ice::motion::blend_samples(pose, pose, .5f);
	if (!validated || !blended || blended->tag != pose.tag)
		return 6;
	std::puts("motion/action ordinary target consumer passed");
	return 0;
}
