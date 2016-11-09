#ifndef UTIL_H
#define UTIL_H
#include <stdint.h>
#ifdef ARDUINO
#include <stdlib.h>
#include <HardwareSerial.h>
#else
#include <cstdio>
#include <iostream>
#include <malloc.h>
#endif

using namespace std;

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

    static void ptrToFourBytes(unsigned long addr_num, byte *addr) {
        addr[0] = (addr_num & 0xFF);
        addr_num >>= 8;
        addr[1] = (addr_num & 0xFF);
        addr_num >>= 8;
        addr[2] = (addr_num & 0xFF);
        addr_num >>= 8;
        addr[3] = addr_num;
        //addr[4] = 0;
    }

    static unsigned long fourBytesToPtr(const byte *addr) {
        unsigned long ret = 0;
        ret = addr[3];
        ret <<= 8;
        ret |= addr[2];
        ret <<= 8;
        ret |= addr[1];
        ret <<= 8;
        ret |= addr[0];
        return ret;
    }

    static int16_t compare(const char *v1, int16_t len1, const char *v2,
            int16_t len2, int k = 0) {
        int lim = (len2 < len1 ? len2 : len1);
        while (k < lim) {
            byte c1 = v1[k];
            byte c2 = v2[k];
            k++;
            if (c1 < c2)
                return -k;
            else if (c1 > c2)
                return k;
        }
        if (len1 == len2)
            return 0;
        k++;
        return (len1 < len2 ? -k : k);
    }

    static inline int16_t min16(int16_t x, int16_t y) {
        return (x > y ? y : x);
        //if (x > y)
        //    return y;
        //return x;
    }

    static void *alignedAlloc(int16_t blockSize) {
#ifdef _MSC_VER
        return malloc(blockSize);
#elif defined(ARDUINO)
        return malloc(blockSize);
#elif defined(__MINGW32_VERSION)
        return __mingw_aligned_malloc(blockSize, 64);
#else
        return memalign(64, blockSize);
#endif
    }

    static void print(const char s[]) {
#if defined(ARDUINO)
        Serial.print(s);
#else
        cout << s;
#endif
    }

    static void print(long l) {
#if defined(ARDUINO)
        Serial.print(l);
#else
        cout << l;
#endif
    }

    static void print(int16_t i16) {
#if defined(ARDUINO)
        Serial.print(i16);
#else
        cout << i16;
#endif
    }

    static void endl() {
#if defined(ARDUINO)
        Serial.print("\n");
#else
        cout << std::endl;
#endif
    }

};

#endif
