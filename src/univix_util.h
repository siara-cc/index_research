#ifndef UTIL_H
#define UTIL_H
#include <stdint.h>
#ifdef ARDUINO
#include <HardwareSerial.h>
#else
#include <cstdio>
#include <iostream>
#endif
#include <stdlib.h>

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
    static int bit_count[256];
    static int bit_count2x[256];
    static byte first_bit_mask[256];
    static byte first_bit_offset[256];

    static uint32_t left_mask32[32];
    static uint32_t ryte_mask32[32];
    static uint32_t mask32[32];
    static uint64_t left_mask64[64];
    static uint64_t ryte_mask64[64];
    static uint64_t mask64[64];
    static inline int16_t getInt(byte *pos) {
//        return (int16_t *) pos; // fast endian-dependent
        return ((*pos << 8) | *(pos + 1)); // slow endian independent
    }

    static inline void setInt(byte *pos, int16_t val) {
//        *((int16_t *) pos) = val; // fast endian-dependent
        *pos++ = val >> 8; // slow endian independent
        *pos = val % 256;
    }

    static int ptrToBytes(unsigned long addr_num, byte *addr) {
        int i = 0;
        while (addr_num) {
            addr[i++] = addr_num;
            addr_num >>= 8;
        }
        return i;
    }

    static unsigned long bytesToPtr(const byte *addr) {
        unsigned long ret = 0;
        int len = *addr;
        do {
            ret <<= 8;
            ret |= addr[len];
        } while (--len);
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
#if defined(_MSC_VER)
 return _aligned_malloc (blockSize, 64);
#elif defined(__BORLANDC__) || defined(__GNUC__)
 return malloc ((unsigned int)blockSize);
#else
 void* aPtr;
 if (posix_memalign (&aPtr, 64, blockSize))
 {
     aPtr = NULL;
 }
 return aPtr;
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

    static void generateBitCounts() {
        for (int16_t i = 0; i < 256; i++) {
            bit_count[i] = countSetBits(i);
            bit_count2x[i] = bit_count[i] << 1;
            first_bit_mask[i] = firstBitMask(i);
            first_bit_offset[i] = firstBitOffset(i);
        }
        uint32_t ui32 = 1;
        for (int i = 0; i < 32; i++) {
            mask32[i] = (0x80000000 >> i);
            if (i == 0) {
                ryte_mask32[i] = 0xFFFFFFFF;
                left_mask32[i] = 0;
            } else {
                ryte_mask32[i] = (ui32 << (32 - i));
                ryte_mask32[i]--;
                left_mask32[i] = ~(ryte_mask32[i]);
            }
        }
        uint64_t ui64 = 1;
        for (int i = 0; i < 64; i++) {
            mask64[i] = (0x8000000000000000 >> i);
            if (i == 0) {
                ryte_mask64[i] = 0xFFFFFFFFFFFFFFFF;
                left_mask64[i] = 0;
            } else {
                ryte_mask64[i] = (ui64 << (64 - i));
                ryte_mask64[i]--;
                left_mask64[i] = ~(ryte_mask64[i]);
            }
        }
    }

    // Function to get no of set bits in binary
    // representation of passed binary no.
    inline static byte countSetBits(int16_t n) {
        byte count = 0;
        while (n > 0) {
            n &= (n - 1);
            count++;
        }
        return (byte) count;
    }

    inline static byte firstBitMask(int16_t n) {
        byte mask = 0x80;
        while (0 == (n & mask) && mask) {
            mask >>= 1;
        }
        return mask;
    }

    inline static byte lastBitMask(int16_t n) {
        byte mask = 0x01;
        while (0 == (n & mask) && mask) {
            mask <<= 1;
        }
        return mask;
    }

    inline static byte firstBitOffset(int16_t n) {
        int offset = 0;
        do {
            byte mask = 0x01 << offset;
            if (n & mask)
                return offset;
            offset++;
        } while (offset <= 7);
        return 0x08;
    }

    // SWAR algorithm
    static int bitcount (byte x)  {
        x -= ((x >> 1) & 0x55);
        x = (((x >> 2) & 0x33) + (x & 0x33));
        return (((x >> 4) + x) & 0x0f);
        //x += (x >> 8);
        //x += (x >> 16);
        //return x; //(x & 0x3f);
    }

    static inline uint16_t popcnt(uint16_t a) {
    uint16_t b;
    __asm__ volatile ("POPCNT %1, %0;"
                :"=r"(b)
                :"r"(a)
                :
            );
    return b;
    }

    static inline uint16_t popcnt2(uint16_t a) {
    uint16_t b;
    __asm__ volatile ("POPCNT %1, %0;"
                :"=r"(b)
                :"r"(a)
                :
            );
    return b << 1;
    }

};

#endif
