#include "view_camera.h"
#include <cmath>

namespace ice::examples::xr {
namespace {
bool rigid(const ice::math::mat4 &matrix) {
	for (float value : ice::math::elements(matrix))
		if (!std::isfinite(value))
			return false;
	if (std::abs(glm::length(ice::math::vec3(matrix[0])) - 1) > 1e-4f ||
	    std::abs(glm::length(ice::math::vec3(matrix[1])) - 1) > 1e-4f ||
	    std::abs(glm::length(ice::math::vec3(matrix[2])) - 1) > 1e-4f)
		return false;
	return std::abs(matrix[0][3]) < 1e-5f && std::abs(matrix[1][3]) < 1e-5f &&
	       std::abs(matrix[2][3]) < 1e-5f && std::abs(matrix[3][3] - 1) < 1e-5f &&
	       std::abs(glm::determinant(ice::math::mat3(matrix)) - 1) < 1e-4f &&
	       std::abs(glm::dot(ice::math::vec3(matrix[0]), ice::math::vec3(matrix[1]))) < 1e-4f &&
	       std::abs(glm::dot(ice::math::vec3(matrix[0]), ice::math::vec3(matrix[2]))) < 1e-4f &&
	       std::abs(glm::dot(ice::math::vec3(matrix[1]), ice::math::vec3(matrix[2]))) < 1e-4f;
}
} // namespace
ice::expected<ice::render::camera_view, ice::render::diagnostic>
make_eye_camera(const ice::math::mat4 &reference_from_eye,
                const ice::math::mat4 &reference_from_world, eye_fov fov, int width, int height,
                float near_plane, float far_plane) {
	auto reject = [] {
		return ice::unexpected(ice::render::diagnostic{"Invalid tracked eye pose/FOV/clip extent"});
	};
	if (!rigid(reference_from_eye) || !rigid(reference_from_world) || width < 1 || height < 1 ||
	    !std::isfinite(near_plane) || !std::isfinite(far_plane) || near_plane <= 0 ||
	    far_plane <= near_plane)
		return reject();
	for (float angle : {fov.left, fov.right, fov.down, fov.up})
		if (!std::isfinite(angle) || std::abs(angle) >= 1.5707963f)
			return reject();
	const float left = std::tan(fov.left), right = std::tan(fov.right), down = std::tan(fov.down),
	            up = std::tan(fov.up);
	if (right - left <= 1e-5f || up - down <= 1e-5f)
		return reject();
	const auto world_from_eye = glm::inverse(reference_from_world) * reference_from_eye;
	ice::render::camera_view result;
	result.width                   = width;
	result.height                  = height;
	result.eye                     = ice::math::vec3(world_from_eye[3]);
	result.right                   = ice::math::vec3(world_from_eye[0]);
	result.up                      = ice::math::vec3(world_from_eye[1]);
	result.forward                 = -ice::math::vec3(world_from_eye[2]);
	result.view                    = glm::inverse(world_from_eye);
	result.projection              = ice::math::mat4(0);
	result.projection[0][0]        = 2 / (right - left);
	result.projection[1][1]        = 2 / (up - down);
	result.projection[2][0]        = (right + left) / (right - left);
	result.projection[2][1]        = (up + down) / (up - down);
	result.projection[2][2]        = -far_plane / (far_plane - near_plane);
	result.projection[2][3]        = -1;
	result.projection[3][2]        = -far_plane * near_plane / (far_plane - near_plane);
	result.inverse_view_projection = glm::inverse(result.projection * result.view);
	for (const auto *matrix : {&result.projection, &result.view, &result.inverse_view_projection})
		for (float value : ice::math::elements(*matrix))
			if (!std::isfinite(value))
				return reject();
	if (result.projection[2][2] == -1)
		return reject();
	return result;
}
} // namespace ice::examples::xr
