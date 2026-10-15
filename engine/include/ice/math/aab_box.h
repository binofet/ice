#pragma once

#include "ice/math/vec3.h"
namespace ice::math {

/// Reserved region-invalidation argument type; this unfinished legacy 3D bounds type currently
/// stores no usable geometry.
class aab_box {
private:
	vec3 min_;
	vec3 max_;

public:
};

} // namespace ice::math
