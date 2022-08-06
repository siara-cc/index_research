#ifndef UTIL_H
#define UTIL_H
#include <stdint.h>
#ifdef ARDUINO
#include <HardwareSerial.h>
#if (defined(__AVR__))
#include <avr/pgmspace.h>
#else
#include <pgmspace.h>
#endif
#else
#include <cstdio>
#include <iostream>
#endif
#include <stdlib.h>
#if !defined(__APPLE__) && !defined(ARDUINO)
#include <malloc.h>
#endif

#if defined(ANDROID) || defined(__ANDROID__)
#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))
#endif

using namespace std;

typedef unsigned char byte;
#define null 0

#define USE_POP_CNT 0

#if (defined(__AVR_ATmega328P__))
#define BIT_COUNT(x) pgm_read_byte_near(util::bit_count + (x))
#define BIT_COUNT2(x) pgm_read_byte_near(util::bit_count2x + (x))
#define LEFT_MASK32(x) pgm_read_dword_near(util::left_mask32 + (x))
#define RYTE_MASK32(x) pgm_read_dword_near(util::ryte_mask32 + (x))
#define MASK32(x) pgm_read_dword_near(util::mask32 + (x))
#else
#if USE_POP_CNT == 1
//#define BIT_COUNT(x) __builtin_popcount(x)
#define BIT_COUNT(x) util::bitcount(x)
#define BIT_COUNT2(x) util::popcnt2(x)
#else
#define BIT_COUNT(x) util::bit_count[x]
#define BIT_COUNT2(x) util::bit_count2x[x]
#endif
#define LEFT_MASK32(x) util::left_mask32[x]
#define RYTE_MASK32(x) util::ryte_mask32[x]
#define MASK32(x) util::mask32[x]
#define LEFT_MASK64(x) util::left_mask64[x]
#define RYTE_MASK64(x) util::ryte_mask64[x]
#define MASK64(x) util::mask64[x]
#endif

#define FIRST_BIT_OFFSET_FROM_RIGHT(x) BIT_COUNT(254 & ((x) ^ ((x) - 1)))

class util {
public:
#if (defined(__AVR_ATmega328P__))
    static const byte bit_count[256] PROGMEM;
    static const byte bit_count2x[256] PROGMEM;
    static const byte bit_count_lf_ch[256] PROGMEM;
    static const uint32_t left_mask32[32] PROGMEM;
    static const uint32_t ryte_mask32[32] PROGMEM;
    static const uint32_t mask32[32] PROGMEM;
    static const uint64_t left_mask64[64] PROGMEM;
    static const uint64_t ryte_mask64[64] PROGMEM;
    static const uint64_t mask64[64] PROGMEM;
#else
    static const byte bit_count[256];
    static const byte bit_count2x[256];
    static const byte bit_count_lf_ch[256];
    static const uint32_t left_mask32[32];
    static const uint32_t ryte_mask32[32];
    static const uint32_t mask32[32];
    static const uint64_t left_mask64[64];
    static const uint64_t ryte_mask64[64];
    static const uint64_t mask64[64];

#endif

    static inline uint16_t getInt(byte *pos) {
//        return (uint16_t *) pos; // fast endian-dependent
        uint16_t ret = ((*pos << 8) | *(pos + 1));
        return ret; // slow endian independent
    }

    static inline void setInt(byte *pos, uint16_t val) {
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

    static int compare(const char *v1, byte len1, const char *v2,
            byte len2, int k = 0) {
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

//    static int16_t compare(const char *v1, int16_t len1, const char *v2,
//            int16_t len2, int k = 0) {
//        int lim = (len2 < len1 ? len2 : len1);
//        while (k < lim) {
//            byte c1 = v1[k];
//            byte c2 = v2[k];
//            k++;
//            if (c1 < c2)
//                return -k;
//            else if (c1 > c2)
//                return k;
//        }
//        if (len1 == len2)
//            return 0;
//        k++;
//        return (len1 < len2 ? -k : k);
//    }

    static inline int16_t min_b(byte x, byte y) {
        return (x > y ? y : x);
    }

    static inline int16_t min16(int16_t x, int16_t y) {
        return (x > y ? y : x);
    }

    static int fast_memeq(const void* src1, const void* src2, int len) {
        /* simple optimization */
        if (src1 == src2)
            return 0;
        /* Convert char pointers to 4 byte integers */
        int32_t *src1_as_int = (int32_t*) src1;
        int32_t *src2_as_int = (int32_t*) src2;
        int major_passes = len >> 2; /* Number of passes at 4 bytes at a time */
        int minor_passes = len & 0x3; /* last 0..3 bytes leftover at the end */
        for (int ii = 0; ii < major_passes; ii++) {
            if (*src1_as_int++ != *src2_as_int++)
                return 1; /* compare as ints */
        }
        /* Handle last few bytes, but has to be as characters */
        char* src1_as_char = (char*) src1_as_int;
        char* src2_as_char = (char*) src2_as_int;
        switch (minor_passes) {
        case 3:
            if (*src1_as_char++ != *src2_as_char++)
                return 1;
        case 2:
            if (*src1_as_char++ != *src2_as_char++)
                return 1;
        case 1:
            if (*src1_as_char++ != *src2_as_char++)
                return 1;
        }
        /* If we make it here, all compares succeeded */
        return 0;
    }

    static void *alignedAlloc(unsigned int blockSize) {
#if defined(ARDUINO)
        return malloc ((unsigned int)blockSize);
#elif defined(_MSC_VER)
        return _aligned_malloc (blockSize, 64);
#elif defined(__APPLE__)
        void* aPtr;
        if (posix_memalign (&aPtr, 64, blockSize)) {
             aPtr = NULL;
        }
        return aPtr;
#elif defined(__BORLANDC__) || defined(__GNUC__) || defined(__ANDROID__)
        return memalign(64, blockSize);
#else
        return malloc ((unsigned int)blockSize);
#endif
    }

    static void print(const char s[]) {
#if defined(ARDUINO)
        Serial.print(s);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%s", s);
#else
        cout << s;
#endif
    }

    static void print(long l) {
#if defined(ARDUINO)
        Serial.print(l);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%ld", l);
#else
        cout << l;
#endif
    }

    static void print(int16_t i16) {
#if defined(ARDUINO)
        Serial.print(i16);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%d", i16);
#else
        cout << i16;
#endif
    }

    static void print(uint8_t l) {
#if defined(ARDUINO)
        Serial.print((int)l);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%d", l);
#else
        cout << (int) l;
#endif
    }

    static void print(uint32_t l) {
#if defined(ARDUINO)
        Serial.print((unsigned long) l);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%u", l);
#else
        cout << l;
#endif
    }

    static void print(uint64_t l) {
#if defined(ARDUINO)
        Serial.print((unsigned long) l);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%lu", l);
#else
        cout << l;
#endif
    }

    static void endl() {
#if defined(ARDUINO)
        Serial.print("\n");
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("\n");
#else
        cout << std::endl;
#endif
    }

    static void generateBitCounts() {
        //for (int16_t i = 0; i < 256; i++) {
        //    bit_count[i] = countSetBits(i);
        //    print(bit_count[i]);
        //    print(", ");
        //}
        //endl();
        //for (int16_t i = 0; i < 256; i++) {
        //    bit_count2x[i] = bit_count[i] << 1;
        //    print(bit_count2x[i]);
        //    print(", ");
        //}
        //endl();
        //for (int16_t i = 0; i < 256; i++) {
        //    bit_count_lf_ch[i] = bit_count[i & 0xAA] + bit_count2x[i & 0x55];
        //    print(bit_count_lf_ch[i]);
        //    print(", ");
        //}
        //endl();
        //uint32_t ui32;
        //ui32 = 1;
        //for (int i = 0; i < 32; i++) {
        //    mask32[i] = (0x80000000 >> i);
        //    print(mask32[i]);
        //    print("L, ");
        //}
        //endl();
        //ui32 = 1;
        //for (int i = 0; i < 32; i++) {
        //    if (i == 0) {
        //        ryte_mask32[i] = 0xFFFFFFFF;
        //    } else {
        //        ryte_mask32[i] = (ui32 << (32 - i));
        //        ryte_mask32[i]--;
        //    }
        //    print(ryte_mask32[i]);
        //    print(", ");
        //}
        //endl();
        //ui32 = 1;
        //for (int i = 0; i < 32; i++) {
        //    if (i == 0) {
        //        left_mask32[i] = 0;
        //    } else {
        //        left_mask32[i] = ~(ryte_mask32[i]);
        //    }
        //    print(left_mask32[i]);
        //    print(", ");
        //}
        //endl();
        //uint64_t ui64;
        //ui64 = 1;
        //for (int i = 0; i < 64; i++) {
        //    mask64[i] = (0x8000000000000000 >> i);
        //    print(mask64[i]);
        //    print(", ");
        //}
        //endl();
        //ui64 = 1;
        //for (int i = 0; i < 64; i++) {
        //    if (i == 0) {
        //        ryte_mask64[i] = 0xFFFFFFFFFFFFFFFF;
        //    } else {
        //        ryte_mask64[i] = (ui64 << (64 - i));
        //        ryte_mask64[i]--;
        //    }
        //    print(ryte_mask64[i]);
        //    print(", ");
        //}
        //endl();
        //ui64 = 1;
        //for (int i = 0; i < 64; i++) {
        //    if (i == 0) {
        //        left_mask64[i] = 0;
        //    } else {
        //        left_mask64[i] = ~(ryte_mask64[i]);
        //    }
        //    print(left_mask64[i]);
        //    print(", ");
        //}
        //endl();
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

#if !defined(_MSC_VER) and !defined(__ANDROID__)
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
#endif

};

#endif
