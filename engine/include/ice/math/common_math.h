#pragma once

#include "ice/globals.h"
namespace ice::math {

/// Default rendering scalar; use explicit double-precision types for large-world simulations.
using real = float;
#define ICEPSILON (0.00001f)
#define IC_2PI (6.2831853071f)
#define IC_PI (3.1415926535f)
#define IC_PI_HALF (1.5707963267f)
#define ICREAL_MIN (1.17549e-38f)
#define ICREAL_MAX (3.40282e+38f)

#define ICFLOAT_MIN (1.17549e-38f)
#define ICFLOAT_MAX (3.40282e+38f)
#define ICDOUBLE_MIN (2.22507e-308)
#define ICDOUBLE_MAX (1.79769e+308)
#define ICINT_MIN (-2147483648)
#define ICINT_MAX (2147483647)
#define ICSHORT_MIN (-32768)
#define ICSHORT_MAX (32767)

} // namespace ice::math

#include <algorithm>
#include <cmath>
#include <utility>
namespace ice::math {

using std::acos;
using std::asin;
using std::atan;
using std::atan2;
using std::clamp;
using std::cos;
using std::exp;
using std::fabs;
using std::lerp;
using std::log;
using std::max;
using std::min;
using std::pow;
using std::sin;
using std::sqrt;
using std::swap;
using std::tan;

/// Reciprocal square root; x must be strictly positive, using standard floating-point math.
inline real inv_sqrt(real x) { return 1.f / std::sqrt(x); }
/// Test scalar magnitude against the engine's single-precision absolute tolerance.
inline bool is_zero(real x) { return std::fabs(x) < ICEPSILON; }

} // namespace ice::math
