#include "ice/render/camera.h"
#include "ice/render/scene_camera.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Camera failed at line %d: %s\n", __LINE__, #condition);          \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
bool close(float first, float second, float tolerance = .0001f) {
	return std::abs(first - second) <= tolerance;
}
bool close(const ice::math::vec3 &first, const ice::math::vec3 &second, float tolerance = .0001f) {
	return glm::all(glm::lessThanEqual(glm::abs(first - second), ice::math::vec3(tolerance)));
}
bool close(const ice::math::mat4 &first, const ice::math::mat4 &second, float tolerance = .0001f) {
	for (int column = 0; column < 4; ++column)
		if (glm::any(glm::greaterThan(glm::abs(first[column] - second[column]),
		                              ice::math::vec4(tolerance))))
			return false;
	return true;
}
float depth_at(const ice::render::camera_view &camera, float distance) {
	const auto clip = camera.projection * ice::math::vec4(0, 0, -distance, 1);
	return clip.z / clip.w;
}
} // namespace

int main() {
	using namespace ice::render;
	using namespace ice::math;
	const float       nan      = std::numeric_limits<float>::quiet_NaN();
	const float       infinity = std::numeric_limits<float>::infinity();
	camera_projection projection;
	projection.vertical_fov = glm::radians(90.f);
	projection.near_plane   = 1;
	projection.far_plane    = 11;
	for (auto depth : {clip_depth::zero_to_one, clip_depth::negative_one_to_one}) {
		auto camera = make_camera_view({}, projection, 200, 100, depth);
		CHECK(camera);
		CHECK(camera->width == 200 && camera->height == 100 && camera->depth == depth);
		CHECK(close(camera->view, mat4(1)) && close(camera->eye, vec3(0)));
		CHECK(close(camera->right, {1, 0, 0}) && close(camera->up, {0, 1, 0}) &&
		      close(camera->forward, {0, 0, -1}));
		CHECK(close(camera->inverse_view_projection * camera->projection * camera->view, mat4(1)));
		CHECK(close(depth_at(*camera, 1), depth == clip_depth::zero_to_one ? 0.f : -1.f));
		CHECK(close(depth_at(*camera, 11), 1));
		auto center = camera_ray(*camera, {.5f, .5f});
		CHECK(center && close(center->origin, {0, 0, -1}) && close(center->direction, {0, 0, -1}) &&
		      close(center->length, 10));
		auto corner = camera_ray(*camera, {0, 0});
		CHECK(corner && close(corner->origin, {-2, 1, -1}));
		CHECK(close(corner->direction, glm::normalize(vec3(-2, 1, -1))));
		CHECK(close(corner->origin + corner->direction * corner->length, {-22, 11, -11}));
		auto bottom = camera_ray(*camera, {1, 1});
		CHECK(bottom && close(bottom->origin, {2, -1, -1}));

		auto orthographic            = projection;
		orthographic.kind            = projection_kind::orthographic;
		orthographic.vertical_extent = 10;
		auto ortho_camera            = make_camera_view({}, orthographic, 200, 100, depth);
		CHECK(ortho_camera);
		CHECK(close(depth_at(*ortho_camera, 1), depth == clip_depth::zero_to_one ? 0.f : -1.f));
		CHECK(close(depth_at(*ortho_camera, 11), 1));
		auto ortho_center = camera_ray(*ortho_camera, {.5f, .5f});
		auto ortho_corner = camera_ray(*ortho_camera, {0, 0});
		CHECK(ortho_center && ortho_corner);
		CHECK(close(ortho_center->origin, {0, 0, -1}) && close(ortho_corner->origin, {-10, 5, -1}));
		CHECK(close(ortho_center->direction, ortho_corner->direction) &&
		      close(ortho_corner->direction, {0, 0, -1}) && close(ortho_corner->length, 10));
	}

	const vec3 eye{7, 5, 10}, target{0, 1, 0};
	auto       pose = look_at_pose(eye, target);
	CHECK(pose && close(pose->position, eye));
	auto camera = make_camera_view(*pose, projection, 1280, 800);
	CHECK(camera && close(camera->view, glm::lookAtRH(eye, target, vec3(0, 1, 0))));
	CHECK(close(camera->forward, glm::normalize(target - eye)));
	auto ray = camera_ray(*camera, {.5f, .5f});
	CHECK(ray && close(ray->direction, camera->forward));
	CHECK(close(ray->origin, eye + camera->forward * projection.near_plane));

	// An explicit quarter turn and a nonunit quaternion exercise camera-to-world handedness.
	camera_pose turned{{3, 4, 5}, glm::angleAxis(glm::radians(90.f), vec3(0, 1, 0)) * 7.f};
	auto        rotated = make_camera_view(turned, projection, 100, 100);
	CHECK(rotated && close(rotated->forward, {-1, 0, 0}) && close(rotated->right, {0, 0, -1}));
	auto rotated_ray = camera_ray(*rotated, {.5f, .5f});
	CHECK(rotated_ray && close(rotated_ray->origin, {2, 4, 5}) &&
	      close(rotated_ray->direction, {-1, 0, 0}));
	for (float scale :
	     {std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::max()}) {
		camera_pose scaled;
		scaled.orientation = {scale, 0, 0, 0};
		auto normalized    = make_camera_view(scaled, projection, 1, 1);
		CHECK(normalized && close(normalized->view, mat4(1)));
	}

	CHECK(!look_at_pose({}, {}));
	CHECK(!look_at_pose({}, {0, 1, 0}));
	CHECK(!look_at_pose({}, {0, 0, -1}, {}));
	CHECK(!look_at_pose({nan, 0, 0}, {0, 0, -1}));
	CHECK(!look_at_pose({}, {infinity, 0, 0}));
	CHECK(!look_at_pose({}, {0, 0, -1}, {nan, 1, 0}));
	CHECK(look_at_pose({}, {0, 0, -std::numeric_limits<float>::denorm_min()}));
	CHECK(!make_camera_view({}, projection, 0, 100));
	CHECK(!make_camera_view({}, projection, 100, -1));
	CHECK(!make_camera_view({}, projection, 100, 100, static_cast<clip_depth>(99)));
	camera_pose bad_pose;
	bad_pose.position.x = infinity;
	CHECK(!make_camera_view(bad_pose, projection, 100, 100));
	bad_pose             = {};
	bad_pose.orientation = {0, 0, 0, 0};
	CHECK(!make_camera_view(bad_pose, projection, 100, 100));
	bad_pose.orientation = {1, nan, 0, 0};
	CHECK(!make_camera_view(bad_pose, projection, 100, 100));
	for (float value : {-1.f, 0.f, glm::pi<float>(), nan, infinity}) {
		auto invalid         = projection;
		invalid.vertical_fov = value;
		CHECK(!make_camera_view({}, invalid, 100, 100));
	}
	for (float value : {-1.f, 0.f, nan, infinity}) {
		auto invalid            = projection;
		invalid.kind            = projection_kind::orthographic;
		invalid.vertical_extent = value;
		CHECK(!make_camera_view({}, invalid, 100, 100));
		invalid            = projection;
		invalid.near_plane = value;
		CHECK(!make_camera_view({}, invalid, 100, 100));
	}
	for (float value : {-1.f, 0.f, 1.f, nan, infinity}) {
		auto invalid      = projection;
		invalid.far_plane = value;
		CHECK(!make_camera_view({}, invalid, 100, 100));
	}
	auto invalid = projection;
	invalid.kind = static_cast<projection_kind>(99);
	CHECK(!make_camera_view({}, invalid, 100, 100));
	invalid                 = projection;
	invalid.vertical_extent = nan;
	CHECK(make_camera_view({}, invalid, 100, 100));
	invalid.kind            = projection_kind::orthographic;
	invalid.vertical_extent = 10;
	invalid.vertical_fov    = nan;
	CHECK(make_camera_view({}, invalid, 100, 100));
	invalid              = projection;
	invalid.vertical_fov = std::numeric_limits<float>::denorm_min();
	CHECK(!make_camera_view({}, invalid, 100, 100));
	invalid           = projection;
	invalid.far_plane = 1.0e20f;
	CHECK(!make_camera_view({}, invalid, 100, 100));

	CHECK(camera);
	for (const vec2 point : {vec2(-.01f, 0), vec2(0, 1.01f), vec2(nan, 0), vec2(0, infinity)})
		CHECK(!camera_ray(*camera, point));
	auto invalid_camera  = *camera;
	invalid_camera.width = 0;
	CHECK(!camera_ray(invalid_camera, {.5f, .5f}));
	invalid_camera       = *camera;
	invalid_camera.depth = static_cast<clip_depth>(99);
	CHECK(!camera_ray(invalid_camera, {.5f, .5f}));
	invalid_camera            = *camera;
	invalid_camera.projection = mat4(0);
	CHECK(!camera_ray(invalid_camera, {.5f, .5f}));
	invalid_camera            = *camera;
	invalid_camera.view[0][0] = nan;
	CHECK(!camera_ray(invalid_camera, {.5f, .5f}));
	invalid_camera                  = *camera;
	invalid_camera.projection[0][0] = infinity;
	CHECK(!camera_ray(invalid_camera, {.5f, .5f}));
	// Ray construction uses current public matrices rather than a possibly stale cached inverse.
	invalid_camera                         = *camera;
	invalid_camera.inverse_view_projection = mat4(0);
	CHECK(camera_ray(invalid_camera, {.5f, .5f}));
	// An asymmetric projection shifts the center ray; the public matrix is authoritative.
	auto asymmetric = make_camera_view({}, projection, 200, 100);
	CHECK(asymmetric);
	asymmetric->projection[2][0] = .25f;
	auto asymmetric_ray          = camera_ray(*asymmetric, {.5f, .5f});
	CHECK(asymmetric_ray && close(asymmetric_ray->origin, {.5f, 0, -1}));

	// The existing orbit helper preserves its framing transform and shares picking metadata.
	scene_camera orbit;
	for (auto depth : {clip_depth::zero_to_one, clip_depth::negative_one_to_one}) {
		auto orbit_view = orbit.view(1280, 720, depth);
		CHECK(orbit_view.depth == depth && close(glm::length(orbit_view.forward), 1));
		auto orbit_ray = camera_ray(orbit_view, {.5f, .5f});
		CHECK(orbit_ray && close(orbit_ray->direction, orbit_view.forward));
		const vec3 expected_eye{3 * std::cos(.12f) * std::sin(.3f), 3 * std::sin(.12f),
		                        3 * std::cos(.12f) * std::cos(.3f)};
		CHECK(close(orbit_view.eye, expected_eye));
		CHECK(close(orbit_view.view, glm::lookAtRH(expected_eye, vec3(0), vec3(0, 1, 0))));
	}
	std::puts("Camera math passed: explicit depth, projection, pose, picking and validation");
}
