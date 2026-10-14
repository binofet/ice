#ifndef __IC_GLOBALS_H__
#define __IC_GLOBALS_H__


#include "Core/icDebug.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <stdlib.h>
#include <memory.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL 0
#endif


#define SAFEDEL(x) {delete x; x = NULL;}

// Check for POSIX support
#ifndef WIN32
#define IC_HAS_POSIX
#endif

#if (!defined(WIN32) && !defined(__APPLE__))
#define X11
#endif

// Graphix devices
#if defined(WIN32) && !defined(ICE_OPENGL_ONLY)
#define ICDX11
#define ICDX9
#define D3DX_LATEST 43
#endif

#ifndef IPHONE
#define ICGL
#endif

#ifdef IPHONE
#define ICGLES
#endif


#ifdef _MSC_VER
#define FORCE_INLINE    __forceinline
#else
#define FORCE_INLINE    inline
#endif



// ICRESULTS
typedef int32_t ICRESULT;

#define ICEFAIL(x)        ( x < IC_OK )
#define ICEOK(x)        ( x >= IC_OK )
#define ICEOK_NOWARN(x) ( x >= IC_OK && x <= IC_WARN_GEN)

// OK RETURN VALUES
#define IC_OK            (0x00000000)

// WARNING RETURN VALUES
#define IC_WARN_GEN                      (0x00008001)
#define IC_WARN_DEL_LOCKED_VB            (0x00008002)
#define IC_WARN_NOT_LOCKED_VB            (0x00008003)
#define IC_WARN_ALREADY_LOCKED_VB        (0x00008004)
#define IC_WARN_DEL_LOCKED_IB            (0x00008005)
#define IC_WARN_NOT_LOCKED_IB            (0x00008006)
#define IC_WARN_ALREADY_LOCKED_IB        (0x00008007)
#define IC_WARN_DEL_LOCKED_TEX           (0x00008008)
#define IC_WARN_NOT_LOCKED_TEX           (0x00008009)
#define IC_WARN_ALREADY_LOCKED_TEX       (0x0000800A)

// FAILURE RETURN VALUES
#define IC_FAIL_GEN                      ((ICRESULT)0x80000001u)
#define IC_FAIL_OUT_OF_MEM               ((ICRESULT)0x80000002u)
#define IC_FAIL_NPE                      ((ICRESULT)0x80000003u)
#define IC_FAIL_CREATE_WIND              ((ICRESULT)0x80000004u)
#define IC_FAIL_CREATE_DEVICE            ((ICRESULT)0x80000005u)
#define IC_FAIL_NO_DEVICE                ((ICRESULT)0x80000006u)
#define IC_FAIL_CREATE_VB                ((ICRESULT)0x80000007u)
#define IC_FAIL_LOCK_VB                  ((ICRESULT)0x80000008u)
#define IC_FAIL_UNLOCK_VB                ((ICRESULT)0x80000009u)
#define IC_FAIL_CREATE_IB                ((ICRESULT)0x8000000Au)
#define IC_FAIL_LOCK_IB                  ((ICRESULT)0x8000000Bu)
#define IC_FAIL_UNLOCK_IB                ((ICRESULT)0x8000000Cu)
#define IC_FAIL_CREATE_TEX               ((ICRESULT)0x8000000Du)
#define IC_FAIL_LOCK_TEX                 ((ICRESULT)0x8000000Eu)
#define IC_FAIL_UNLOCK_TEX               ((ICRESULT)0x8000000Fu)
#define IC_FAIL_NO_VB                    ((ICRESULT)0x80000010u)
#define IC_FAIL_NO_IB                    ((ICRESULT)0x80000011u)
#define IC_FAIL_NO_TEX                   ((ICRESULT)0x80000012u)
#define IC_FAIL_NOT_LOCKABLE_TEX         ((ICRESULT)0x80000013u)

#define IC_FAIL_FILE_NOT_SUPPORTED       ((ICRESULT)0x80000014u)


#define IC_FAIL_NO_SCRIPT_CONTEXT        ((ICRESULT)0x80000020u)
#define IC_FAIL_NO_SCRIPT_LOADED         ((ICRESULT)0x80000021u)
#define IC_FAIL_SCRIPT_LOAD_ERROR        ((ICRESULT)0x80000022u)
#define IC_FAIL_SCRIPT_RUNTIME_ERROR     ((ICRESULT)0x80000023u)

#define IC_NOT_IMPL               ((ICRESULT)0xFFFFFFFFu)

// COMMON TYPEDEFS
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef char i8;
typedef short i16;
typedef int i32;

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;

#if (defined(WIN32) && defined(_MSC_VER))
typedef unsigned __int64 u64;
typedef unsigned __int64 uint64;
typedef __int64 i64;
typedef __int64 int64;
#else
typedef unsigned long long u64;
typedef unsigned long long uint64;
typedef unsigned long long i64;
typedef long int int64;
#endif

#ifdef _WIN64
typedef i64* icIntPtr;
typedef u64* icUIntPtr;
#else
typedef i32* icIntPtr;
typedef u32* icUIntPtr;
#endif
/*

// Does a type specification contain a pointer?
template <typename TYPE>
struct IsPointer
{
	static bool val = false;
};

// Specialise for yes
template <typename TYPE>
struct IsPointer<TYPE*>
{
	static bool val = true;
};

// Exactly the same, except the result is the type without the pointer
template <typename TYPE>
struct StripPointer
{
	typedef TYPE Type;
};

// Specialise for yes
template <typename TYPE>
struct StripPointer<TYPE*>
{
	typedef TYPE Type;
};

*/


#ifdef __cplusplus
}
#endif


#endif //__ICE_icGlobals_H__

