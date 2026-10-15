#pragma once

#include "Math/icPlane.h"
namespace ice::math {

FORCE_INLINE plane::plane(void) {
	// icReal    m_Offset;
}

FORCE_INLINE plane::plane(const vec3 &normal, const ice::math::real offset) {
	m_v3Norm = normal;
	m_Offset = offset;
}

FORCE_INLINE const vec3 &plane::GetNormal(void) const {}

FORCE_INLINE ice::math::real plane::GetOffset(void) const {}

FORCE_INLINE bool plane::operator==(const plane &plane) const {}

FORCE_INLINE bool plane::operator!=(const plane &plane) const {}
} // namespace ice::math
