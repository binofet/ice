#pragma once

#include "ice/debug.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
namespace ice {

#ifdef WIN32
} // namespace ice
#include <windows.h>
namespace ice {
#endif

#ifdef __APPLE__
} // namespace ice
#include <memory.h>
#include <stdlib.h>
namespace ice {
#endif

#ifndef NULL
#define NULL 0
#endif

#define SAFEDEL(x)                                                                                 \
	{                                                                                              \
		delete x;                                                                                  \
		x = NULL;                                                                                  \
	}

/// Check for POSIX support
#ifndef WIN32
#define IC_HAS_POSIX
#endif

#if (!defined(WIN32) && !defined(__APPLE__))
#define X11
#endif

/// Graphix devices
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
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

/// ICRESULTS
typedef int32_t result;

#define ICEFAIL(x) (x < IC_OK)
#define ICEOK(x) (x >= IC_OK)
#define ICEOK_NOWARN(x) (x >= IC_OK && x <= IC_WARN_GEN)

/// OK RETURN VALUES
#define IC_OK (0x00000000)

/// WARNING RETURN VALUES
#define IC_WARN_GEN (0x00008001)
#define IC_WARN_DEL_LOCKED_VB (0x00008002)
#define IC_WARN_NOT_LOCKED_VB (0x00008003)
#define IC_WARN_ALREADY_LOCKED_VB (0x00008004)
#define IC_WARN_DEL_LOCKED_IB (0x00008005)
#define IC_WARN_NOT_LOCKED_IB (0x00008006)
#define IC_WARN_ALREADY_LOCKED_IB (0x00008007)
#define IC_WARN_DEL_LOCKED_TEX (0x00008008)
#define IC_WARN_NOT_LOCKED_TEX (0x00008009)
#define IC_WARN_ALREADY_LOCKED_TEX (0x0000800A)

/// FAILURE RETURN VALUES
#define IC_FAIL_GEN ((ice::result)0x80000001u)
#define IC_FAIL_OUT_OF_MEM ((ice::result)0x80000002u)
#define IC_FAIL_NPE ((ice::result)0x80000003u)
#define IC_FAIL_CREATE_WIND ((ice::result)0x80000004u)
#define IC_FAIL_CREATE_DEVICE ((ice::result)0x80000005u)
#define IC_FAIL_NO_DEVICE ((ice::result)0x80000006u)
#define IC_FAIL_CREATE_VB ((ice::result)0x80000007u)
#define IC_FAIL_LOCK_VB ((ice::result)0x80000008u)
#define IC_FAIL_UNLOCK_VB ((ice::result)0x80000009u)
#define IC_FAIL_CREATE_IB ((ice::result)0x8000000Au)
#define IC_FAIL_LOCK_IB ((ice::result)0x8000000Bu)
#define IC_FAIL_UNLOCK_IB ((ice::result)0x8000000Cu)
#define IC_FAIL_CREATE_TEX ((ice::result)0x8000000Du)
#define IC_FAIL_LOCK_TEX ((ice::result)0x8000000Eu)
#define IC_FAIL_UNLOCK_TEX ((ice::result)0x8000000Fu)
#define IC_FAIL_NO_VB ((ice::result)0x80000010u)
#define IC_FAIL_NO_IB ((ice::result)0x80000011u)
#define IC_FAIL_NO_TEX ((ice::result)0x80000012u)
#define IC_FAIL_NOT_LOCKABLE_TEX ((ice::result)0x80000013u)

#define IC_FAIL_FILE_NOT_SUPPORTED ((ice::result)0x80000014u)

#define IC_FAIL_NO_SCRIPT_CONTEXT ((ice::result)0x80000020u)
#define IC_FAIL_NO_SCRIPT_LOADED ((ice::result)0x80000021u)
#define IC_FAIL_SCRIPT_LOAD_ERROR ((ice::result)0x80000022u)
#define IC_FAIL_SCRIPT_RUNTIME_ERROR ((ice::result)0x80000023u)

#define IC_NOT_IMPL ((ice::result)0xFFFFFFFFu)

/// COMMON TYPEDEFS
typedef unsigned char u8;
/// Unsigned 16-bit engine integer alias.
typedef unsigned short u16;
/// Unsigned 32-bit engine integer alias.
typedef unsigned int u32;

/// Signed 8-bit engine integer alias.
typedef char i8;
/// Signed 16-bit engine integer alias.
typedef short i16;
/// Signed 32-bit engine integer alias.
typedef int i32;

/// Native unsigned char alias, used for byte-oriented legacy data.
typedef unsigned char uchar;
/// Native unsigned int alias used by legacy interfaces.
typedef unsigned int uint;
/// Native unsigned long alias; width follows the platform ABI.
typedef unsigned long ulong;
/// Native unsigned short alias used by legacy interfaces.
typedef unsigned short ushort;

#if (defined(WIN32) && defined(_MSC_VER))
typedef unsigned __int64 u64;
typedef unsigned __int64 uint64;
typedef __int64          i64;
typedef __int64          int64;
#else
/// Unsigned 64-bit engine integer alias.
typedef unsigned long long u64;
/// Unsigned 64-bit legacy engine integer alias.
typedef unsigned long long uint64;
/// Signed 64-bit engine integer alias.
typedef unsigned long long i64;
/// Signed 64-bit legacy engine integer alias.
typedef long int int64;
#endif

#ifdef _WIN64
typedef i64 *int_ptr;
typedef u64 *u_int_ptr;
#else
/// Native signed integer-pointer legacy alias.
typedef i32 *int_ptr;
/// Native unsigned integer-pointer legacy alias.
typedef u32 *u_int_ptr;
#endif
/*

/// Does a type specification contain a pointer?
template <typename TYPE>
struct IsPointer
{
    static bool val = false;
};

/// Specialise for yes
template <typename TYPE>
struct IsPointer<TYPE*>
{
    static bool val = true;
};

/// Exactly the same, except the result is the type without the pointer
template <typename TYPE>
struct StripPointer
{
    typedef TYPE Type;
};

/// Specialise for yes
template <typename TYPE>
struct StripPointer<TYPE*>
{
    typedef TYPE Type;
};

*/

} // namespace ice
