#pragma once
#include "ice/math/rand.h"
namespace ice::math {

/// Shared legacy random-generator state; use rand_seed rather than modifying directly.
extern int ic_rand_seed;

#ifdef WIN32
} // namespace ice::math
#include "windows.h"
namespace ice::math {
#endif

FORCE_INLINE int rand_auto_seed(void) {
#ifdef WIN32
	LARGE_INTEGER tim, freq;
	double        seconds;

	QueryPerformanceCounter(&tim);
	QueryPerformanceFrequency(&freq);
	seconds      = (double)tim.QuadPart / (double)freq.QuadPart;
	ic_rand_seed = (int)((seconds - floor(seconds)) * 10000000.0);
#else
	ic_rand_seed = 0xb16b00b5;
#endif
	return ic_rand_seed;
}

FORCE_INLINE void rand_seed(int seed) { ic_rand_seed = seed; }

FORCE_INLINE int rand_i(void) {
	ic_rand_seed = (ic_rand_seed * 69069 + 1);
	return ic_rand_seed;
}

FORCE_INLINE int rand_i(int min, int max) {
	assert(max > min);
	if (min >= 0)
		return (int)(rand_ui() % ((ice::uint)max - (ice::uint)min)) + min;
	else
		return rand_i() % (max - min) + min;
}

FORCE_INLINE ice::uint rand_ui(void) {
	ic_rand_seed = (ic_rand_seed * 69069 + 1);
	return (ice::uint)ic_rand_seed;
}

FORCE_INLINE ice::uint rand_ui(ice::uint min, ice::uint max) {
	return rand_ui() % (max - min) + min;
}

FORCE_INLINE float rand_f() { return (float)(rand_i() & 0xFFFF) / (float)0x10000; }

FORCE_INLINE float rand_f(float min, float max) { return ((max - min) * rand_f()) + min; }

FORCE_INLINE double rand_d() { return (double)(rand_i() & 0xFFFF) / (double)0x10000; }

FORCE_INLINE double rand_d(double min, double max) { return ((max - min) * rand_d()) + min; }

FORCE_INLINE ice::math::real rand_r() {
	return (ice::math::real)(rand_i() & 0xFFFF) / (ice::math::real)0x10000;
}

FORCE_INLINE ice::math::real rand_r(ice::math::real min, ice::math::real max) {
	return ((max - min) * rand_r()) + min;
}
} // namespace ice::math
