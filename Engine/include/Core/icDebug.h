#ifndef __IC_DEBUG_H__
#define __IC_DEBUG_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef assert

#if (defined(_DEBUG) || defined(DEBUG))
#define assert(x) { if (!(x)) { icErrorf("%s, line(%i); Assertion Failed", __FILE__, __LINE__); } }
#define icDbgPrintf(x,...) {icPrintf(x,__VA_ARGS__);}
#else
#define assert(x)
#define icDbgPrintf(x)
#endif

#include <stdarg.h>
#include <stdio.h>


const char* icGetStr( long res );

void icErrorf(const char* szFormat, ...);
void icWarningf(const char* szFormat, ...);
void icPrintf(const char* szFormat, ...);

void icError(const char* szFormat);
void icWarning(const char* szFormat);
void icPrint(const char* szFormat);

#ifdef __cplusplus
}
#endif

#endif //__IC_DEBUG_H__

