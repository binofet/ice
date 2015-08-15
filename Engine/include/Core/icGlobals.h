#ifndef __IC_GLOBALS_H__
#define __IC_GLOBALS_H__


#include "Core/icDebug.h"

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
#ifdef WIN32
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
typedef long ICRESULT;

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
#define IC_FAIL_GEN                      (0x80000001)
#define IC_FAIL_OUT_OF_MEM               (0x80000002)
#define IC_FAIL_NPE                      (0x80000003)
#define IC_FAIL_CREATE_WIND              (0x80000004)
#define IC_FAIL_CREATE_DEVICE            (0x80000005)
#define IC_FAIL_NO_DEVICE                (0x80000006)
#define IC_FAIL_CREATE_VB                (0x80000007)
#define IC_FAIL_LOCK_VB                  (0x80000008)
#define IC_FAIL_UNLOCK_VB                (0x80000009)
#define IC_FAIL_CREATE_IB                (0x8000000A)
#define IC_FAIL_LOCK_IB                  (0x8000000B)
#define IC_FAIL_UNLOCK_IB                (0x8000000C)
#define IC_FAIL_CREATE_TEX               (0x8000000D)
#define IC_FAIL_LOCK_TEX                 (0x8000000E)
#define IC_FAIL_UNLOCK_TEX               (0x8000000F)
#define IC_FAIL_NO_VB                    (0x80000010)
#define IC_FAIL_NO_IB                    (0x80000011)
#define IC_FAIL_NO_TEX                   (0x80000012)
#define IC_FAIL_NOT_LOCKABLE_TEX         (0x80000013)

#define IC_FAIL_FILE_NOT_SUPPORTED       (0x80000014)


#define IC_FAIL_NO_SCRIPT_CONTEXT        (0x80000020)
#define IC_FAIL_NO_SCRIPT_LOADED         (0x80000021)
#define IC_FAIL_SCRIPT_LOAD_ERROR        (0x80000022)
#define IC_FAIL_SCRIPT_RUNTIME_ERROR     (0x80000023)

#define IC_NOT_IMPL               (0xFFFFFFFF)

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

