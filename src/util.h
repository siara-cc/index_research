#ifndef UTIL_H
#define UTIL_H
#include <stdint.h>
#include <malloc.h>

typedef unsigned char byte;
#define null 0

class util {
public:
    static inline int16_t getInt(byte *pos) {
        int16_t *ptr = (int16_t *) pos;
        return *ptr;
//        int16_t ret = *pos << 8;
//        pos++;
//        ret += *pos;
//        return ret;
    }

    static inline void setInt(byte *pos, int16_t val) {
        int16_t *ptr = (int16_t *) pos;
        *ptr = val;
//        *pos = val >> 8;
//        pos++;
//        *pos = val % 256;
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

    static unsigned long fourBytesToPtr(byte *addr) {
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

    static int16_t compare(const char *v1, int16_t len1, const char *v2, int16_t len2, int k = 0) {
        register int lim = len1;
        if (len2 < len1)
            lim = len2;
        while (k < lim) {
            byte c1 = v1[k];
            byte c2 = v2[k];
            if (c1 < c2) {
                return -1-k;
            } else if (c1 > c2) {
                return k+1;
            }
            k++;
        }
        return len1 - len2;
    }

    static inline int16_t min(int16_t x, int16_t y) {
        return (x > y ? y : x);
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
