#include "ice/math/types.h"
#include <cmath>
#include <cstdio>
#include <type_traits>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Math failed at line %d: %s\n", __LINE__, #condition);            \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

static bool close(float a, float b) { return std::fabs(a - b) < .00001f; }
static bool close(ice::math::vec3 a, ice::math::vec3 b) { return glm::length(a - b) < .00001f; }

int main() {
	using namespace ice::math;
	static_assert(std::is_same_v<vec3, glm::vec3>);
	static_assert(std::is_same_v<mat4, glm::mat4>);
	static_assert(std::is_same_v<quat, glm::quat>);
	CHECK(vec3() == vec3(0));
	CHECK(mat4() == mat4(1));
	CHECK(quat() == quat(1, 0, 0, 0));
	const auto rotation  = glm::angleAxis(glm::half_pi<float>(), vec3(0, 0, 1));
	const auto transform = glm::translate(mat4(1), vec3(4, 5, 6)) * glm::mat4_cast(rotation) *
	                       glm::scale(mat4(1), vec3(2, 3, 4));
	CHECK(close(transform_point(transform, vec3(1, 0, 0)), vec3(4, 7, 6)));
	CHECK(close(transform_direction(transform, vec3(1, 0, 0)), vec3(0, 2, 0)));
	CHECK(close(transform_point(glm::inverse(transform), transform_point(transform, vec3(1, 2, 3))),
	            vec3(1, 2, 3)));
	CHECK(close(glm::value_ptr(transform)[12], 4));
	CHECK(close(element(transform, 13), 5));
	mat4 transposed = glm::transpose(transform);
	CHECK(close(transposed[0][3], 4));
	CHECK(close(transposed[3][0], transform[0][3]));
	const float xyzw[]   = {0, 0, std::sqrt(.5f), std::sqrt(.5f)};
	const quat  imported = glm::make_quat(xyzw);
	CHECK(close(imported * vec3(1, 0, 0), vec3(0, 1, 0)));
	CHECK(close(glm::value_ptr(imported)[3], xyzw[3]));
	CHECK(close(glm::quat_cast(glm::mat3_cast(imported)) * vec3(1, 0, 0), vec3(0, 1, 0)));
	const quat opposite(-imported.w, -imported.x, -imported.y, -imported.z);
	CHECK(close(glm::slerp(imported, opposite, .5f) * vec3(1, 0, 0), vec3(0, 1, 0)));
	const vec3 eye(2, 3, 8), target(2, 3, 0), up(0, 1, 0);
	const auto view_rh = glm::lookAtRH(eye, target, up);
	CHECK(close(transform_point(view_rh, eye), vec3(0)));
	CHECK(close(transform_point(view_rh, target), vec3(0, 0, -8)));
	const auto pose_lh = glm::inverse(glm::lookAtLH(eye, target, up));
	CHECK(close(transform_point(glm::inverse(pose_lh), eye), vec3(0)));
	CHECK(close(transform_point(glm::inverse(pose_lh), target), vec3(0, 0, 8)));
	const auto rh_no = glm::perspectiveRH_NO(glm::half_pi<float>(), 1.f, 1.f, 10.f);
	const auto rh_zo = glm::perspectiveRH_ZO(glm::half_pi<float>(), 1.f, 1.f, 10.f);
	const auto lh_zo = glm::perspectiveLH_ZO(glm::half_pi<float>(), 1.f, 1.f, 10.f);
	auto       depth = [](const mat4 &m, float z) {
        vec4 p = m * vec4(0, 0, z, 1);
        return p.z / p.w;
	};
	CHECK(close(depth(rh_no, -1), -1) && close(depth(rh_no, -10), 1));
	CHECK(close(depth(rh_zo, -1), 0) && close(depth(rh_zo, -10), 1));
	CHECK(close(depth(lh_zo, 1), 0) && close(depth(lh_zo, 10), 1));
	mat4 edited = transform;
	set_position(edited, vec3(10, 20, 30));
	CHECK(edited[0] == transform[0]);
	CHECK(close(transform_point(edited, vec3(0)), vec3(10, 20, 30)));
	set_rotation(edited, mat3(1));
	CHECK(close(transform_point(edited, vec3(1)), vec3(11, 21, 31)));
	std::puts("Math passed: GLM aliases, packed storage, TRS order, inverse/transpose, XYZW "
	          "quaternions, shortest-path slerp, RH/LH cameras and NO/ZO clip depth");
}
