#pragma once

#include <cstdint>

#if defined(_MSC_VER)
    #define COMPAT_CONST_FN
#else
    #define COMPAT_CONST_FN __attribute__((const))
#endif

#if defined(_MSC_VER)

#include <intrin.h>

#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanReverse64)
#pragma intrinsic(__popcnt)
#pragma intrinsic(__popcnt64)

static inline int madras_builtin_clz(unsigned int x)
{
    unsigned long index;

    if (_BitScanReverse(&index, x))
        return 31 - static_cast<int>(index);

    return 32;
}

static inline int madras_builtin_clzll(unsigned long long x)
{
#if defined(_M_X64) || defined(_M_ARM64)

    unsigned long index;

    if (_BitScanReverse64(&index, x))
        return 63 - static_cast<int>(index);

    return 64;

#else

    if (x >> 32)
        return madras_builtin_clz(static_cast<unsigned int>(x >> 32));

    return 32 + madras_builtin_clz(static_cast<unsigned int>(x));

#endif
}

static inline int madras_builtin_popcount(unsigned int x)
{
    return static_cast<int>(__popcnt(x));
}

static inline int madras_builtin_popcountll(unsigned long long x)
{
#if defined(_M_X64) || defined(_M_ARM64)
    return static_cast<int>(__popcnt64(x));
#else
    return static_cast<int>(
        __popcnt(static_cast<unsigned int>(x)) +
        __popcnt(static_cast<unsigned int>(x >> 32)));
#endif
}

#define __builtin_clz(x)        madras_builtin_clz(static_cast<unsigned int>(x))
#define __builtin_clzll(x)      madras_builtin_clzll(static_cast<unsigned long long>(x))
#define __builtin_popcount(x)   madras_builtin_popcount(static_cast<unsigned int>(x))
#define __builtin_popcountll(x) madras_builtin_popcountll(static_cast<unsigned long long>(x))

#endif
