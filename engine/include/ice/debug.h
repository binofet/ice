#pragma once

#include <stdio.h>
namespace ice {

#undef assert

#if (defined(_DEBUG) || defined(DEBUG))
#define assert(x)                                                                                  \
	{                                                                                              \
		if (!(x)) {                                                                                \
			ice::errorf("%s, line(%i); Assertion Failed", __FILE__, __LINE__);                     \
		}                                                                                          \
	}
#define ICE_DBG_PRINTF(x, ...)                                                                     \
	{ ice::printf(x, __VA_ARGS__); }
#else
#define assert(x)
#define ICE_DBG_PRINTF(x)
#endif
} // namespace ice

#include <stdarg.h>
#include <stdio.h>
namespace ice {

const char *get_str(long result_code);

void errorf(const char *format, ...);
void warningf(const char *format, ...);
void printf(const char *format, ...);

void error(const char *format);
void warning(const char *format);
void print(const char *format);

} // namespace ice
