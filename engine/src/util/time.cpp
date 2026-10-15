#include "ice/util/time.h"
#include <chrono>
namespace ice::util {

#ifdef WIN32
#ifdef WIN32
} // namespace ice::util
#include <windows.h>
namespace ice::util {
#else
} // namespace ice::util
#include <Xtl.h>
namespace ice::util {
#endif
LARGE_INTEGER __ic_freq;
void          sys_time_init(void) { QueryPerformanceFrequency(&__ic_freq); }
#else
} // namespace ice::util
#include <sys/time.h>
namespace ice::util {
void sys_time_init(void) {}
#endif

/*! c'tor
 **/
static ice::uint64 now_nanoseconds() {
	return (ice::uint64)std::chrono::duration_cast<std::chrono::nanoseconds>(
	           std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}
timer::timer() : last_(now_nanoseconds()) {}
ice::math::real timer::elapsed() {
	ice::uint64     now     = now_nanoseconds();
	ice::math::real elapsed = (ice::math::real)((now - last_) / 1000000000.0);
	last_                   = now;
	return elapsed;
}

} // namespace ice::util
