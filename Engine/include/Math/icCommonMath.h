#ifndef __IC_COMMON_MATH_H__
#define __IC_COMMON_MATH_H__

#include "Core/icGlobals.h"


// Un-Comment this for double precision
//#define IC_MATH_USE_DOUBLE


#ifdef IC_MATH_USE_DOUBLE
typedef double icReal;
#define ICEPSILON       (0.000000001f)
#define IC_2PI          (6.283185307179586476925286766559)
#define IC_PI           (3.1415926535897932384626433832795)
#define IC_PI_HALF      (1.5707963267948966192313216916398)
#define ICREAL_MIN      (2.22507e-308)
#define ICREAL_MAX      (1.79769e+308)
#else
typedef float icReal;
#define ICEPSILON       (0.00001f)
#define IC_2PI          (6.2831853071f)
#define IC_PI           (3.1415926535f)
#define IC_PI_HALF      (1.5707963267f)
#define ICREAL_MIN      (1.17549e-38)
#define ICREAL_MAX      (3.40282e+38)
#endif


#define ICFLOAT_MIN     (1.17549e-38f)
#define ICFLOAT_MAX     (3.40282e+38f)
#define ICDOUBLE_MIN    (2.22507e-308)
#define ICDOUBLE_MAX    (1.79769e+308)
#define ICINT_MIN       (-2147483648)
#define ICINT_MAX       (2147483647)
#define ICSHORT_MIN     (-32768)
#define ICSHORT_MAX     (32767)


FORCE_INLINE icReal icSqrt      ( const icReal x );
FORCE_INLINE icReal icInvSqrt   ( const icReal x );
FORCE_INLINE icReal icFabs      ( const icReal x );
FORCE_INLINE icReal icCos       ( const icReal x );
FORCE_INLINE icReal icSin       ( const icReal x );
FORCE_INLINE icReal icTan       ( const icReal x );
FORCE_INLINE icReal icAcos      ( const icReal x );
FORCE_INLINE icReal icAsin      ( const icReal x );
FORCE_INLINE icReal icAtan      ( const icReal x );
FORCE_INLINE icReal icAtan2     ( const icReal x, icReal y );
FORCE_INLINE icReal icExp       ( const icReal x );
FORCE_INLINE icReal icLog       ( const icReal x );
FORCE_INLINE icReal icPow       ( const icReal x, icReal y );

FORCE_INLINE bool icIsZero      ( const icReal x );

template<class T>
FORCE_INLINE icReal icLerp      (const T& start, const T& end,
                                 const icReal fDeltaTime); 

template<class T>
FORCE_INLINE void icSwap            (T& _L, T& _R);


#include "Math/icCommonMath.inl"
#endif //__IC_COMMON_MATH_H__

