#include "../view_camera.h"
#include "../space_anchor.h"
#include <cmath>
#include <cstdio>
#include <limits>
#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "XR camera check %d: %s\n", __LINE__, #condition);                \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)
namespace {
bool close(const ice::math::mat4 &left, const ice::math::mat4 &right) {
	for (int column = 0; column < 4; ++column)
		for (int row = 0; row < 4; ++row)
			if (std::abs(left[column][row] - right[column][row]) > 1e-4f)
				return false;
	return true;
}
} // namespace
int main() {
	using namespace ice::examples::xr;
	using namespace ice::math;
	const eye_fov fov{-.8f, .7f, -.65f, .9f};
	const auto    reference_from_eye = glm::translate(mat4(1), vec3(.032f, 1.6f, 0));
	auto camera = make_eye_camera(reference_from_eye, mat4(1), fov, 1600, 1440, .05f, 1000);
	CHECK(camera && camera->depth == ice::render::clip_depth::zero_to_one);
	const auto &projection = camera->projection;
	auto        ndc        = [&](vec3 point) {
        auto p = projection * vec4(point, 1);
        return vec3(p) / p.w;
	};
	CHECK(std::abs(ndc({0, 0, -.05f}).z) < 1e-5f && std::abs(ndc({0, 0, -1000}).z - 1) < 1e-5f);
	CHECK(std::abs(ndc({std::tan(fov.left) * .05f, 0, -.05f}).x + 1) < 1e-5f);
	CHECK(std::abs(ndc({std::tan(fov.right) * .05f, 0, -.05f}).x - 1) < 1e-5f);
	CHECK(std::abs(ndc({0, std::tan(fov.down) * .05f, -.05f}).y + 1) < 1e-5f);
	CHECK(std::abs(ndc({0, std::tan(fov.up) * .05f, -.05f}).y - 1) < 1e-5f);
	CHECK(close(camera->inverse_view_projection * (camera->projection * camera->view), mat4(1)));
	auto other =
	    make_eye_camera(glm::translate(mat4(1), vec3(-.032f, 1.6f, 0)), mat4(1), fov, 1600, 1440);
	CHECK(other && !close(other->view, camera->view));
	space_anchor anchor;
	anchor.initialize(glm::translate(mat4(1), vec3(1, 0, -2)) *
	                  glm::rotate(mat4(1), .4f, vec3(0, 1, 0)));
	const auto old_from_new =
	    glm::translate(mat4(1), vec3(2, 0, 1)) * glm::rotate(mat4(1), -.8f, vec3(0, 1, 0));
	auto first = make_eye_camera(reference_from_eye, anchor.transform(), fov, 1600, 1440);
	CHECK(first && anchor.queue_change(10, true, old_from_new));
	CHECK(anchor.advance(10).applied == 1);
	auto after = make_eye_camera(glm::inverse(old_from_new) * reference_from_eye,
	                             anchor.transform(), fov, 1600, 1440);
	CHECK(after && close(after->view, first->view));
	// A synthetic local-reference floor moves native head height into canonical world Y.
	auto local =
	    make_eye_camera(mat4(1), glm::translate(mat4(1), vec3(0, -1.5f, 0)), fov, 1600, 1440);
	CHECK(local && std::abs(local->eye.y - 1.5f) < 1e-5f);
	CHECK(!make_eye_camera(mat4(1), mat4(0), fov, 1600, 1440));
	CHECK(!make_eye_camera(glm::scale(mat4(1), vec3(2)), mat4(1), fov, 1600, 1440));
	CHECK(!make_eye_camera(mat4(1), mat4(1), {0, 0, 0, 0}, 1600, 1440));
	CHECK(!make_eye_camera(mat4(1), mat4(1), fov, 0, 1440));
	CHECK(!make_eye_camera(mat4(1), mat4(1), fov, 1600, 1440, 1, 1));
	auto invalid  = reference_from_eye;
	invalid[0][0] = std::numeric_limits<float>::quiet_NaN();
	CHECK(!make_eye_camera(invalid, mat4(1), fov, 1600, 1440));
	std::puts("XR cameras: asymmetric stereo, zero-to-one clip, retained origin and synthetic "
	          "floor passed");
}
