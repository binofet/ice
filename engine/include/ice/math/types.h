#pragma once

/// A single configuration is part of the public ABI; consumers also inherit it from ice::math.
#if defined(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES) || defined(GLM_FORCE_QUAT_DATA_WXYZ)
#error "ICE requires packed GLM vectors and XYZW quaternion storage"
#endif
#if defined(GLM_SETUP_INCLUDED) && !defined(GLM_FORCE_CTOR_INIT)
#error "Include ICE math before GLM, or consume the ice::math CMake target"
#endif
#ifndef GLM_FORCE_CTOR_INIT
#define GLM_FORCE_CTOR_INIT
#endif
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <span>

namespace ice::math {

/// Two-component, single-precision vector; coordinates have no implicit units.
using vec2 = glm::vec2;
/// Three-component, single-precision vector, packed as three scalars.
using vec3 = glm::vec3;
/// Four-component, single-precision vector, packed as four scalars.
using vec4 = glm::vec4;
/// Two-component signed integer vector, used for pixel coordinates.
using ivec2 = glm::ivec2;
/// Column-major 3x3 matrix; indexing is matrix[column][row].
using mat3 = glm::mat3;
/// Column-major 4x4 matrix multiplying column vectors: projection * view * world * position.
using mat4 = glm::mat4;
/// Unit quaternion for rotations; GLM's scalar constructor takes (w, x, y, z).
using quat = glm::quat;
/// Double-precision position for simulations that need a larger coordinate range.
using dvec3 = glm::dvec3;
/// Double-precision transform; conversion to GPU float storage is explicit.
using dmat4 = glm::dmat4;

static_assert(sizeof(vec3) == 3 * sizeof(float), "ICE vertex storage requires packed GLM types");
static_assert(sizeof(mat4) == 16 * sizeof(float), "ICE joint palettes require packed matrices");

/// Transform a position with homogeneous w=1; this does not perform perspective division.
inline vec3 transform_point(const mat4 &matrix, const vec3 &point) {
	return vec3(matrix * vec4(point, 1));
}
/// Transform a direction with w=0, excluding translation; length may change under scale.
inline vec3 transform_direction(const mat4 &matrix, const vec3 &direction) {
	return vec3(matrix * vec4(direction, 0));
}
/// Set the affine translation column, with homogeneous w=1; preserve the linear basis.
inline void set_position(mat4 &matrix, const vec3 &position) { matrix[3] = vec4(position, 1); }
/// Replace the linear basis with a rotation; preserve the translation column.
inline void set_rotation(mat4 &matrix, const mat3 &rotation) {
	for (int column = 0; column < 3; ++column)
		matrix[column] = vec4(rotation[column], 0);
}
/// Read a zero-based scalar in contiguous column-major storage, in [0,16).
inline float &element(mat4 &matrix, std::size_t index) { return glm::value_ptr(matrix)[index]; }
/// Read a zero-based scalar in contiguous column-major storage, in [0,16).
inline const float &element(const mat4 &matrix, std::size_t index) {
	return glm::value_ptr(matrix)[index];
}

/// Borrow all sixteen column-major scalars; invalidated when the matrix is destroyed.
inline std::span<float, 16> elements(mat4 &matrix) {
	return std::span<float, 16>(glm::value_ptr(matrix), 16);
}
/// Borrow read-only column-major scalars; invalidated when the matrix is destroyed.
inline std::span<const float, 16> elements(const mat4 &matrix) {
	return std::span<const float, 16>(glm::value_ptr(matrix), 16);
}
} // namespace ice::math
