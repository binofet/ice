
#include "Math/icCommonMath.h"

#include <math.h>

#ifdef IC_MATH_USE_DOUBLE
FORCE_INLINE icReal icSqrt( const icReal x )
{
    return sqrt(x);
}

FORCE_INLINE icReal icInvSqrt    ( const icReal x )
{
    return 1.0/sqrt(x);
}
FORCE_INLINE icReal icFabs( const icReal x ) { return fabs(x); }
FORCE_INLINE icReal icCos( const icReal x ) { return cos(x); }
FORCE_INLINE icReal icSin( const icReal x ) { return sin(x); }
FORCE_INLINE icReal icTan( const icReal x ) { return tan(x); }
FORCE_INLINE icReal icAcos( const icReal x ) { return acos(x); }
FORCE_INLINE icReal icAsin( const icReal x ) { return asin(x); }
FORCE_INLINE icReal icAtan( const icReal x ) { return atan(x); }
FORCE_INLINE icReal icAtan2( const icReal x, icReal y)
{
    return atan2(x, y);
}
FORCE_INLINE icReal icExp( const icReal x ) { return exp(x); }
FORCE_INLINE icReal icLog( const icReal x ) { return log(x); }
FORCE_INLINE icReal icPow( const icReal x, icReal y)
{
    return pow(x,y);
}
#else
FORCE_INLINE icReal icSqrt( const icReal x )    { return sqrtf(x); }
#ifdef CARMACKS_INVSQRT
FORCE_INLINE icReal icInvSqrt    ( const icReal x )
{
    // this is taken from the quake 3 engine
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = x * 0.5F;
    y  = x;
    i  = * ( long * ) &y;            // evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 );   // what the fuck?
    y  = * ( float * ) &i;
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
    //    y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration

    return y;
}
#else
FORCE_INLINE icReal icInvSqrt( const icReal x )    { return 1.0f/sqrtf(x); }
#endif
FORCE_INLINE icReal icFabs( const icReal x ) { return fabsf(x); }
FORCE_INLINE icReal icCos( const icReal x ) { return cosf(x); }
FORCE_INLINE icReal icSin( const icReal x ) { return sinf(x); }
FORCE_INLINE icReal icTan( const icReal x ) { return tanf(x); }
FORCE_INLINE icReal icAcos( const icReal x ) { return acosf(x); }
FORCE_INLINE icReal icAsin( const icReal x ) { return asinf(x); }
FORCE_INLINE icReal icAtan( const icReal x ) { return atanf(x); }
FORCE_INLINE icReal icAtan2( const icReal x, icReal y)
{
    return atan2f(x, y);
}
FORCE_INLINE icReal icExp( const icReal x ) { return expf(x); }
FORCE_INLINE icReal icLog( const icReal x ) { return logf(x); }
FORCE_INLINE icReal icPow( const icReal x, icReal y)
{
    return pow(x,y);
}
#endif

FORCE_INLINE bool icIsZero ( const icReal x )
{
    return icFabs(x) < ICEPSILON;
}

template<class T>
FORCE_INLINE void icSwap(T& _L, T& _R)
{
    T temp(_L);
    _L = _R;
    _R = temp;
};

template<class T>
FORCE_INLINE T icClamp(const T& in, const T& min, const T& max)
{
    T ret = (in > max) ? max : in;
    ret = (ret < min) ? min :ret;
    return ret;
};


template<class T>
FORCE_INLINE T icMax(const T& a, const T&b)
{
    return (a > b) ? a : b;
};

template<class T>
FORCE_INLINE T icMin(const T& a, const T&b)
{
    return (a < b) ? a : b;
};


template<class T>
FORCE_INLINE icReal icLerp(const T& start, const T& end,
                           const icReal fDeltaTime)
{
    T ret;
    ret = start + (fDeltaTime * (end - start));
    return ret;
};