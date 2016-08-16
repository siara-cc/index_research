#ifndef UTIL_H
#define UTIL_H
#include <stdint.h>
#include <malloc.h>

typedef unsigned char byte;
#define null 0

// Check windows
#if _WIN32 || _WIN64
   #if _WIN64
     #define ENV64BIT
  #else
    #define ENV32BIT
  #endif
#endif

// Check GCC
#if __GNUC__
  #if __x86_64__ || __ppc64__
    #define ENV64BIT
  #else
    #define ENV32BIT
  #endif
#endif

class util {
public:
    static inline int16_t getInt(byte *pos) {
//        return (int16_t *) pos; // fast endian-dependent
        return ((*pos << 8) | *(pos + 1)); // slow endian independent
    }

    static inline void setInt(byte *pos, int16_t val) {
//        *((int16_t *) pos) = val; // fast endian-dependent
        *pos++ = val >> 8; // slow endian independent
        *pos = val % 256;
    }

    static void ptrToFourBytes(unsigned long addr_num, char *addr) {
        addr[3] = (addr_num & 0xFF);
        addr_num >>= 8;
        addr[2] = (addr_num & 0xFF);
        addr_num >>= 8;
        addr[1] = (addr_num & 0xFF);
        addr_num >>= 8;
        addr[0] = addr_num;
        addr[4] = 0;
    }

    static unsigned long fourBytesToPtr(const byte *addr) {
        unsigned long ret = 0;
        ret = addr[0];
        ret <<= 8;
        ret |= addr[1];
        ret <<= 8;
        ret |= addr[2];
        ret <<= 8;
        ret |= addr[3];
        return ret;
    }

    static int16_t compare(const char *v1, int16_t len1, const char *v2,
            int16_t len2, int k = 0) {
        //register int k = 0;
        register int lim = len1;
        if (len2 < len1)
            lim = len2;
        while (k < lim) {
            byte c1 = v1[k];
            byte c2 = v2[k];
            k++;
            if (c1 < c2)
                return -k;
            else if (c1 > c2)
                return k;
        }
        return len1 - len2;
    }

    static int16_t compare(const byte *v1, int16_t len1, const byte *v2,
            int16_t len2, int k = 0) {
        //register int k = 0;
        register int lim = len1;
        if (len2 < len1)
            lim = len2;
        while (k < lim) {
            byte c1 = v1[k];
            byte c2 = v2[k];
            k++;
            if (c1 < c2)
                return -k;
            else if (c1 > c2)
                return k;
        }
        return len1 - len2;
    }

    static inline int16_t min(int16_t x, int16_t y) {
        if (x > y)
            return y;
        return x;
    }

    static void *alignedAlloc(int16_t blockSize) {
#ifdef _MSC_VER
        return malloc(blockSize);
#elif defined(__MINGW32_VERSION)
        return __mingw_aligned_malloc(blockSize, 64);
#else
        return memalign(64, blockSize);
#endif
    }
};

#endif
