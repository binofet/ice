#pragma once

#include "ice/math/common_math.h"
namespace ice::util {

/// Initialize legacy platform timing support; steady-clock timer objects remain independently
/// owned.
void sys_time_init(void);

/* Provides accurate timing functionality
 **/
/// Monotonic elapsed-time measurement in seconds; reset establishes a new origin.
class timer {
public:
	/// Construct inactive state; initialize explicitly before active use.
	timer(void);

	/// Return seconds since the previous elapsed call and advance the timer baseline.
	ice::math::real elapsed(void);

private:
	ice::uint64 last_;
};

} // namespace ice::util
