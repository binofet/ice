#include "ice/motion/pose.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using namespace ice::motion;
constexpr sample_tag tag{17, 23, 0};

void check(bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}
bool near(const ice::math::vec3 &first, const ice::math::vec3 &second, float tolerance = 2.e-5f) {
	return glm::length(first - second) <= tolerance;
}
bool same_rotation(const ice::math::quat &first, const ice::math::quat &second) {
	return std::abs(glm::dot(glm::dquat(first), glm::dquat(second))) > 1 - 2.e-7;
}
bool same_pose(const rigid_pose &first, const rigid_pose &second) {
	return near(first.position, second.position) &&
	       same_rotation(first.orientation, second.orientation);
}
bool exact_pose(const rigid_pose &first, const rigid_pose &second) {
	return first.position == second.position && first.orientation == second.orientation;
}
template <typename T> void failure(const ice::expected<T, diagnostic> &result, error_code code) {
	check(!result && result.error().code == code && !result.error().message.empty(),
	      "Expected structured failure");
}
ice::math::quat turn(float degrees, const ice::math::vec3 &axis) {
	return glm::angleAxis(glm::radians(degrees), axis);
}

void validation() {
	const auto identity = validate_pose({});
	check(identity && exact_pose(*identity, {}), "Default pose is identity");
	const rigid_pose pose{{-0.f, 2, -3}, {-2, 3, -4, 5}};
	const auto       first = validate_pose(pose);
	const auto       other = validate_pose({pose.position, -pose.orientation});
	check(first && other && exact_pose(*first, *other), "q and -q canonicalize identically");
	check(first->orientation.w > 0 && !std::signbit(first->position.x),
	      "Canonical sign and positive position zero");
	check(std::abs(glm::length(glm::dquat(first->orientation)) - 1) < 1.e-7,
	      "Validated quaternion is unit within float rounding");
	const auto half_turn = validate_pose({{}, {-0.f, -1, -0.f, 0.f}});
	check(half_turn && half_turn->orientation.x == 1 && !std::signbit(half_turn->orientation.w) &&
	          !std::signbit(half_turn->orientation.y),
	      "Half-turn canonicalization uses first nonzero component and erases signed zero");
	const float maximum = std::numeric_limits<float>::max();
	const float tiny    = std::numeric_limits<float>::denorm_min();
	for (float magnitude : {maximum, tiny}) {
		const auto result = validate_pose(
		    {{maximum, -maximum, tiny}, {magnitude, -magnitude, magnitude, -magnitude}});
		check(result && std::abs(glm::length(result->orientation) - 1) < 1.e-6f,
		      "Finite extreme quaternions normalize without intermediate overflow/underflow");
		check(result->position.x == maximum && result->position.z == tiny,
		      "Validation does not impose physics coordinate limits");
	}
	const auto sample = validate_sample({pose, tag});
	check(sample && sample->tag == tag && exact_pose(sample->pose, *first),
	      "Sample validation preserves exact tag including frame zero");
}

void blend() {
	const pose_sample first{{{-4, 2, 8}, turn(0, {0, 1, 0})}, tag};
	const pose_sample second{{{8, 6, -4}, turn(120, {0, 1, 0}) * 3.f}, tag};
	const auto        start = blend_samples(first, second, 0);
	const auto        end   = blend_samples(first, second, 1);
	check(start && end && exact_pose(start->pose, *validate_pose(first.pose)) &&
	          exact_pose(end->pose, *validate_pose(second.pose)),
	      "Endpoints equal the canonical input exactly");
	const auto quarter = blend_samples(first, second, .25f);
	check(quarter && near(quarter->pose.position, {-1, 3, 5}) &&
	          same_rotation(quarter->pose.orientation, turn(30, {0, 1, 0})) && quarter->tag == tag,
	      "Linear translation and angular slerp share weight and tag");
	const pose_sample beyond{{{}, turn(170, {0, 1, 0})}, tag};
	const pose_sample before{{{}, turn(-170, {0, 1, 0})}, tag};
	const auto        middle = blend_samples(beyond, before, .5f);
	check(middle && near(middle->pose.orientation * ice::math::vec3(0, 0, 1), {0, 0, -1}),
	      "Short arc crosses 180 degrees instead of rotating through zero");
	for (float weight : {0.f, .25f, .5f, .75f, 1.f}) {
		const pose_sample negative{{beyond.pose.position, -beyond.pose.orientation}, tag};
		const auto        result = blend_samples(beyond, negative, weight);
		check(result && same_rotation(result->pose.orientation, beyond.pose.orientation),
		      "Antipodal quaternion representations remain the same rotation");
	}
	const float maximum = std::numeric_limits<float>::max();
	const auto  distant =
	    blend_samples({{{-maximum, 0, 0}, {}}, tag}, {{{maximum, 0, 0}, {}}, tag}, .5f);
	check(distant && distant->pose.position == ice::math::vec3(0),
	      "Distant finite endpoints interpolate without float subtraction overflow");
	const auto close = blend_samples(first, first, .3f);
	check(close && same_pose(close->pose, first.pose), "Coincident orientations stay finite");
	const pose_sample half_turn{{{}, {0, 0, 1, 0}}, tag};
	const pose_sample negative_half{{{}, -half_turn.pose.orientation}, tag};
	const auto        positive_arc = blend_samples(first, half_turn, .5f);
	const auto        same_arc     = blend_samples(first, negative_half, .5f);
	check(positive_arc && same_arc && exact_pose(positive_arc->pose, same_arc->pose),
	      "Equal-length 180-degree arc choice is invariant under q/-q representation");
}

void compose() {
	const rigid_pose parent{{10, 2, -3}, turn(90, {0, 0, 1})};
	const rigid_pose local{{2, 0, 0}, turn(90, {1, 0, 0})};
	const auto       result = compose_pose(parent, local);
	check(result && near(result->position, {10, 4, -3}),
	      "Local translation rotates by a nonidentity parent before addition");
	check(near(result->orientation * ice::math::vec3(0, 1, 0), {0, 0, 1}),
	      "Orientation multiplication applies local rotation before parent");
	const auto sample = compose_sample({parent, tag}, local);
	check(sample && sample->tag == tag && exact_pose(sample->pose, *result),
	      "Timeless local offset composition preserves the parent sample tag");
	const auto local_again = relative_pose(parent, *result);
	check(local_again && same_pose(*local_again, local), "Relative pose undoes parent composition");
	const auto other_order = compose_pose(local, parent);
	check(other_order && !near(other_order->position, result->position),
	      "Composition is not commutative");
}

void inverse() {
	const rigid_pose pose{{10, 2, -3}, turn(90, {0, 0, 1})};
	const auto       inverted = inverse_pose(pose);
	check(inverted && near(inverted->position, {-2, 10, 3}) &&
	          same_rotation(inverted->orientation, turn(-90, {0, 0, 1})),
	      "Inverse rotates the negative translation");
	const auto left  = compose_pose(*inverted, pose);
	const auto right = compose_pose(pose, *inverted);
	check(left && right && same_pose(*left, {}) && same_pose(*right, {}),
	      "Both inverse composition orders return identity");
	const float      maximum = std::numeric_limits<float>::max();
	const rigid_pose huge{{maximum, maximum, 0}, turn(45, {0, 0, 1})};
	failure(inverse_pose(huge), error_code::overflow);
	const auto same = relative_pose(huge, huge);
	check(same && same_pose(*same, {}),
	      "Relative computation permits representable output despite overflowing raw inverse");
}

void invalid() {
	const float maximum = std::numeric_limits<float>::max();
	const float nan     = std::numeric_limits<float>::quiet_NaN();
	const float inf     = std::numeric_limits<float>::infinity();
	for (float value : {nan, inf, -inf}) {
		for (int component = 0; component < 3; ++component) {
			rigid_pose pose;
			pose.position[component] = value;
			failure(validate_pose(pose), error_code::invalid_position);
			failure(compose_pose({}, pose), error_code::invalid_position);
			failure(compose_pose(pose, {}), error_code::invalid_position);
			failure(inverse_pose(pose), error_code::invalid_position);
			failure(relative_pose(pose, {}), error_code::invalid_position);
			failure(relative_pose({}, pose), error_code::invalid_position);
			failure(blend_samples({{}, tag}, {pose, tag}, 0), error_code::invalid_position);
			failure(blend_samples({pose, tag}, {{}, tag}, 1), error_code::invalid_position);
		}
		for (int component = 0; component < 4; ++component) {
			rigid_pose pose;
			pose.orientation[component] = value;
			failure(validate_pose(pose), error_code::invalid_orientation);
			failure(compose_sample({{}, tag}, pose), error_code::invalid_orientation);
			failure(blend_samples({{}, tag}, {pose, tag}, 0), error_code::invalid_orientation);
		}
	}
	const rigid_pose zero{{}, {0, 0, 0, 0}};
	failure(validate_pose(zero), error_code::invalid_orientation);
	failure(inverse_pose(zero), error_code::invalid_orientation);
	failure(compose_pose(zero, {}), error_code::invalid_orientation);
	failure(relative_pose({}, zero), error_code::invalid_orientation);
	failure(blend_samples({zero, tag}, {{}, tag}, .5f), error_code::invalid_orientation);
	for (float weight : {-1.f, 2.f, nan, inf, -inf})
		failure(blend_samples({{}, tag}, {{}, tag}, weight), error_code::invalid_weight);
	failure(compose_pose({{maximum, 0, 0}, {}}, {{maximum, 0, 0}, {}}), error_code::overflow);
	failure(relative_pose({{-maximum, 0, 0}, {}}, {{maximum, 0, 0}, {}}), error_code::overflow);
}

void tags() {
	for (sample_tag invalid_tag : {sample_tag{}, sample_tag{0, 23, 1}, sample_tag{17, 0, 1}}) {
		failure(validate_sample({{}, invalid_tag}), error_code::invalid_tag);
		failure(compose_sample({{}, invalid_tag}, {}), error_code::invalid_tag);
		failure(blend_samples({{}, invalid_tag}, {{}, tag}, .5f), error_code::invalid_tag);
		failure(blend_samples({{}, tag}, {{}, invalid_tag}, 0), error_code::invalid_tag);
	}
	for (sample_tag different :
	     {sample_tag{18, 23, 0}, sample_tag{17, 24, 0}, sample_tag{17, 23, 1}})
		for (float weight : {0.f, .5f, 1.f})
			failure(blend_samples({{}, tag}, {{}, different}, weight), error_code::sample_mismatch);
	const sample_tag last{std::numeric_limits<std::uint64_t>::max(), 1,
	                      std::numeric_limits<std::uint64_t>::max()};
	const auto       result = blend_samples({{}, last}, {{}, last}, .5f);
	check(result && result->tag == last, "Tags are opaque identities and never incremented here");
}

void consumers() {
	// These are CPU host adapters, not renderer or physics implementations. The latter
	// records a desired target while retaining actual state owned by a simulation.
	struct visual_adapter {
		ice::math::mat4 transform{1};
		sample_tag      tag;
		void            consume(const pose_sample &sample) {
            transform = glm::translate(ice::math::mat4(1), sample.pose.position) *
                        glm::mat4_cast(sample.pose.orientation);
            tag = sample.tag;
		}
	} visual;
	struct desired_target_adapter {
		rigid_pose  actual{{-7, 0, 2}, {1, 0, 0, 0}};
		pose_sample desired;
		void        consume(const pose_sample &sample) { desired = sample; }
	} physics;
	const auto original = physics.actual;
	const auto goal     = blend_samples({{{0, 0, 0}, turn(0, {0, 1, 0})}, tag},
	                                    {{{4, 2, 0}, turn(90, {0, 1, 0})}, tag}, .5f);
	check(goal.has_value(), "Controller produces one valid staged goal");
	visual.consume(*goal);
	physics.consume(*goal);
	check(near(ice::math::transform_point(visual.transform, {0, 0, 0}),
	           physics.desired.pose.position) &&
	          near(ice::math::transform_direction(visual.transform, {0, 0, -1}),
	               physics.desired.pose.orientation * ice::math::vec3(0, 0, -1)) &&
	          visual.tag == physics.desired.tag,
	      "The same output supplies visual transform and desired motor/kinematic target");
	check(exact_pose(physics.actual, original),
	      "Submitting a desired target does not teleport the physics-authoritative actual pose");
}
} // namespace

int main(int argc, char **argv) {
	try {
		check(argc == 2, "Specify one fixture case");
		const std::string_view name(argv[1]);
		if (name == "validation")
			validation();
		else if (name == "blend")
			blend();
		else if (name == "compose")
			compose();
		else if (name == "inverse")
			inverse();
		else if (name == "invalid")
			invalid();
		else if (name == "tags")
			tags();
		else if (name == "consumers")
			consumers();
		else
			throw std::runtime_error("Unknown fixture case");
		std::cout << name << " passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
