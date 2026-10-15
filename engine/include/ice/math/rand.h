#pragma once

#include "ice/math/common_math.h"
namespace ice::math {

/// Initialize the legacy process-global random sequence and return its seed; this generator is not
/// thread safe or cryptographic.
int rand_auto_seed(void);
/// Reset the legacy process-global random sequence to the supplied seed; synchronize concurrent
/// callers.
void rand_seed(int seed);

/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
int rand_i(void);
/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
int rand_i(int min, int max);

/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
ice::uint rand_ui(void);
/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
ice::uint rand_ui(ice::uint min, ice::uint max);

/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
ice::int64 rand_i64(void);
/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
ice::int64 rand_i64(ice::int64 min, ice::int64 max);

/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
ice::uint64 rand_ui64(void);
/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
ice::uint64 rand_ui64(ice::uint64 min, ice::uint64 max);

/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
float rand_f(void);
/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
float rand_f(float min, float max);

/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
double rand_d(void);
/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
double rand_d(double min, double max);

/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
ice::math::real rand_r(void);
/// Draw from the legacy process-global pseudorandom sequence; range overloads require min < max.
/// Not thread safe.
ice::math::real rand_r(ice::math::real min, ice::math::real max);
} // namespace ice::math

#include "ice/math/rand.inl"
namespace ice::math {} // namespace ice::math
