#ifndef UTIL_H
#define UTIL_H
#include <stdint.h>
#ifdef ARDUINO
#include <Hardware_serial.h>
#if (defined(__AVR__))
#include <avr/pgmspace.h>
#else
#include <pgmspace.h>
#endif
#else
#include <cstdio>
#include <cstring>
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

typedef unsigned char uint8_t;
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
    static const uint8_t bit_count[256] PROGMEM;
    static const uint8_t bit_count2x[256] PROGMEM;
    static const uint8_t bit_count_lf_ch[256] PROGMEM;
    static const uint32_t left_mask32[32] PROGMEM;
    static const uint32_t ryte_mask32[32] PROGMEM;
    static const uint32_t mask32[32] PROGMEM;
    static const uint64_t left_mask64[64] PROGMEM;
    static const uint64_t ryte_mask64[64] PROGMEM;
    static const uint64_t mask64[64] PROGMEM;
#else
    static const uint8_t bit_count[256];
    static const uint8_t bit_count2x[256];
    static const uint8_t bit_count_lf_ch[256];
    static const uint32_t left_mask32[32];
    static const uint32_t ryte_mask32[32];
    static const uint32_t mask32[32];
    static const uint64_t left_mask64[64];
    static const uint64_t ryte_mask64[64];
    static const uint64_t mask64[64];

#endif

    // Returns how many bytes the given integer will
    // occupy if stored as a variable integer
    static int8_t get_vlen_of_uint16(uint16_t vint) {
        return vint > 16383 ? 3 : (vint > 127 ? 2 : 1);
    }

    // Returns how many bytes the given integer will
    // occupy if stored as a variable integer
    static int8_t get_vlen_of_uint32(uint32_t vint) {
        return vint > ((1 << 28) - 1) ? 5
            : (vint > ((1 << 21) - 1) ? 4 
            : (vint > ((1 << 14) - 1) ? 3
            : (vint > ((1 << 7) - 1) ? 2 : 1)));
    }

    // Returns how many bytes the given integer will
    // occupy if stored as a variable integer
    static int8_t get_vlen_of_uint64(uint64_t vint) {
        return vint > ((1ULL << 56) - 1) ? 9
            : (vint > ((1ULL << 49) - 1) ? 8
            : (vint > ((1ULL << 42) - 1) ? 7
            : (vint > ((1ULL << 35) - 1) ? 6
            : (vint > ((1ULL << 28) - 1) ? 5
            : (vint > ((1ULL << 21) - 1) ? 4 
            : (vint > ((1ULL << 14) - 1) ? 3
            : (vint > ((1ULL <<  7) - 1) ? 2 : 1)))))));
    }

    // Stores the given uint8_t in the given location
    // in big-endian sequence
    static void write_uint8(uint8_t *ptr, uint8_t input) {
        ptr[0] = input;
    }

    // Stores the given uint16_t in the given location
    // in big-endian sequence
    static void write_uint16(uint8_t *ptr, uint16_t input) {
        ptr[0] = input >> 8;
        ptr[1] = input & 0xFF;
    }

    // Stores the given int24_t in the given location
    // in big-endian sequence
    static void write_int24(uint8_t *ptr, uint32_t input) {
        int i = 3;
        ptr[1] = ptr[2] = 0;
        *ptr = (input >> 24) & 0x80;
        while (i--)
            *ptr++ |= ((input >> (8 * i)) & 0xFF);
    }

    // Stores the given uint32_t in the given location
    // in big-endian sequence
    static void write_uint32(uint8_t *ptr, uint32_t input) {
        int i = 4;
        while (i--)
            *ptr++ = (input >> (8 * i)) & 0xFF;
    }

    // Stores the given int64_t in the given location
    // in big-endian sequence
    static void write_int48(uint8_t *ptr, uint64_t input) {
        int i = 7;
        memset(ptr + 1, '\0', 7);
        *ptr = (input >> 56) & 0x80;
        while (i--)
            *ptr++ |= ((input >> (8 * i)) & 0xFF);
    }

    // Stores the given uint64_t in the given location
    // in big-endian sequence
    static void write_uint64(uint8_t *ptr, uint64_t input) {
        int i = 8;
        while (i--)
            *ptr++ = (input >> (8 * i)) & 0xFF;
    }

    // Stores the given uint16_t in the given location
    // in variable integer format
    static int write_vint16(uint8_t *ptr, uint16_t vint) {
        int len = get_vlen_of_uint16(vint);
        for (int i = len - 1; i > 0; i--)
            *ptr++ = 0x80 + ((vint >> (7 * i)) & 0x7F);
        *ptr = vint & 0x7F;
        return len;
    }

    // Stores the given uint32_t in the given location
    // in variable integer format
    static int write_vint32(uint8_t *ptr, uint32_t vint) {
        int len = get_vlen_of_uint32(vint);
        for (int i = len - 1; i > 0; i--)
            *ptr++ = 0x80 + ((vint >> (7 * i)) & 0x7F);
        *ptr = vint & 0x7F;
        return len;
    }

    // Stores the given uint64_t in the given location
    // in variable integer format
    static int write_vint64(uint8_t *ptr, uint64_t vint) {
        int len = get_vlen_of_uint64(vint);
        for (int i = len - 1; i > 0; i--) {
            if (i == 8)
                *ptr++ = vint >> 56;
            else
                *ptr++ = 0x80 + ((vint >> (7 * i)) & 0x7F);
        }
        *ptr = vint & 0x7F;
        return len;
    }

    // Reads and returns big-endian uint8_t
    // at a given memory location
    static uint8_t read_uint8(const uint8_t *ptr) {
        return *ptr;
    }

    // Reads and returns big-endian uint16_t
    // at a given memory location
    static uint16_t read_uint16(const uint8_t *ptr) {
        return (*ptr << 8) + ptr[1];
    }

    // Reads and returns big-endian int24_t
    // at a given memory location
    static int32_t read_int24(const uint8_t *ptr) {
        uint32_t ret;
        ret = ((uint32_t)(*ptr & 0x80)) << 24;
        ret |= ((uint32_t)(*ptr++ & 0x7F)) << 16;
        ret |= ((uint32_t)*ptr++) << 8;
        ret += *ptr;
        return ret;
    }

    // Reads and returns big-endian uint24_t
    // at a given memory location
    static uint32_t read_uint24(const uint8_t *ptr) {
        uint32_t ret;
        ret = ((uint32_t)*ptr++) << 16;
        ret += ((uint32_t)*ptr++) << 8;
        ret += *ptr;
        return ret;
    }

    // Reads and returns big-endian uint32_t
    // at a given memory location
    static uint32_t read_uint32(const uint8_t *ptr) {
        uint32_t ret;
        ret = ((uint32_t)*ptr++) << 24;
        ret += ((uint32_t)*ptr++) << 16;
        ret += ((uint32_t)*ptr++) << 8;
        ret += *ptr;
        return ret;
    }

    // Reads and returns big-endian int48_t
    // at a given memory location
    static int64_t read_int48(const uint8_t *ptr) {
        uint64_t ret;
        ret = ((uint64_t)(*ptr & 0x80)) << 56;
        ret |= ((uint64_t)(*ptr++ & 0x7F)) << 48;
        ret |= ((uint64_t)*ptr++) << 32;
        ret |= ((uint64_t)*ptr++) << 24;
        ret |= ((uint64_t)*ptr++) << 16;
        ret |= ((uint64_t)*ptr++) << 8;
        ret += *ptr;
        return ret;
    }

    // Reads and returns big-endian uint48_t :)
    // at a given memory location
    static uint64_t read_uint48(const uint8_t *ptr) {
        uint64_t ret = 0;
        int len = 6;
        while (len--)
            ret += (*ptr++ << (8 * len));
        return ret;
    }

    // Reads and returns big-endian uint64_t
    // at a given memory location
    static uint64_t read_uint64(const uint8_t *ptr) {
        uint64_t ret = 0;
        int len = 8;
        while (len--)
            ret += (*ptr++ << (8 * len));
        return ret;
    }

    // Reads and returns variable integer
    // from given location as uint16_t
    // Also returns the length of the varint
    static uint16_t read_vint16(const uint8_t *ptr, int8_t *vlen) {
        uint16_t ret = 0;
        int8_t len = 3; // read max 3 bytes
        do {
            ret <<= 7;
            ret += *ptr & 0x7F;
            len--;
        } while ((*ptr++ & 0x80) == 0x80 && len);
        if (vlen)
            *vlen = 3 - len;
        return ret;
    }

    // Reads and returns variable integer
    // from given location as uint32_t
    // Also returns the length of the varint
    static uint32_t read_vint32(const uint8_t *ptr, int8_t *vlen) {
        uint32_t ret = 0;
        int8_t len = 5; // read max 5 bytes
        do {
            ret <<= 7;
            ret += *ptr & 0x7F;
            len--;
        } while ((*ptr++ & 0x80) == 0x80 && len);
        if (vlen)
            *vlen = 5 - len;
        return ret;
    }

    // Converts float to Sqlite's Big-endian double
    static int64_t float_to_double(const void *val) {
        uint32_t bytes = *((uint32_t *) val);
        uint8_t exp8 = (bytes >> 23) & 0xFF;
        uint16_t exp11 = exp8;
        if (exp11 != 0) {
            if (exp11 < 127)
            exp11 = 1023 - (127 - exp11);
            else
            exp11 = 1023 + (exp11 - 127);
        }
        return ((int64_t)(bytes >> 31) << 63) 
            | ((int64_t)exp11 << 52)
            | ((int64_t)(bytes & 0x7FFFFF) << (52-23) );
    }

    static double read_double(const uint8_t *data) {
        uint64_t value;
        memcpy(&value, data, sizeof(uint64_t)); // read 8 bytes from data pointer
        // SQLite stores 64-bit reals as big-endian integers
        value = ((value & 0xff00000000000000ull) >> 56) | // byte 1 -> byte 8
                ((value & 0x00ff000000000000ull) >> 40) | // byte 2 -> byte 7
                ((value & 0x0000ff0000000000ull) >> 24) | // byte 3 -> byte 6
                ((value & 0x000000ff00000000ull) >> 8)  | // byte 4 -> byte 5
                ((value & 0x00000000ff000000ull) << 8)  | // byte 5 -> byte 4
                ((value & 0x0000000000ff0000ull) << 24) | // byte 6 -> byte 3
                ((value & 0x000000000000ff00ull) << 40) | // byte 7 -> byte 2
                ((value & 0x00000000000000ffull) << 56);  // byte 8 -> byte 1
        double result;
        memcpy(&result, &value, sizeof(double)); // convert the integer to a double
        return result;
    }

    static inline uint16_t get_int(uint8_t *pos) {
//        return (uint16_t *) pos; // fast endian-dependent
        uint16_t ret = ((*pos << 8) | *(pos + 1));
        return ret; // slow endian independent
    }

    static inline void set_int(uint8_t *pos, uint16_t val) {
//        *((int16_t *) pos) = val; // fast endian-dependent
        *pos++ = val >> 8; // slow endian independent
        *pos = val % 256;
    }

    static inline uint32_t get_int3(uint8_t *pos) {
        uint32_t ret = ((*pos << 16) + (pos[1] << 8) + pos[2]);
        return ret; // slow endian independent
    }

    static inline void set_int3(uint8_t *pos, uint32_t val) {
        *pos++ = val >> 16; // slow endian independent
        *pos++ = (val >> 8) & 0xFF;
        *pos = val & 0xFF;
    }

    static int ptr_to_bytes(unsigned long addr_num, uint8_t *addr) {
        int l = 0;
        while (addr_num) {
            addr[l++] = addr_num;
            addr_num >>= 8;
        }
        for (int i = 0; i < l / 2; i++) {
          uint8_t b = addr[i];
          addr[i] = addr[l - i - 1];
          addr[l - i - 1] = b;
        }
        return l;
    }

    static unsigned long bytes_to_ptr(const uint8_t *addr, int len) {
        unsigned long ret = 0;
        addr++;
        do {
            ret <<= 8;
            ret |= *addr++;
        } while (--len);
        return ret;
    }

    static unsigned long bytes_to_ptr(const uint8_t *addr) {
        unsigned long ret = 0;
        int len = *addr++;
        do {
            ret <<= 8;
            ret |= *addr++;
        } while (--len);
        return ret;
    }

    static int compare(const uint8_t *v1, int len1, const uint8_t *v2,
            int len2, int k = 0) {
        int lim = (len2 < len1 ? len2 : len1);
        while (k < lim) {
            uint8_t c1 = v1[k];
            uint8_t c2 = v2[k];
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
//            uint8_t c1 = v1[k];
//            uint8_t c2 = v2[k];
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

    static inline int16_t min_b(uint8_t x, uint8_t y) {
        return (x > y ? y : x);
    }

    static inline int16_t min16(int16_t x, int16_t y) {
        return (x > y ? y : x);
    }

    static int fast_memeq(const void* src1, const void* src2, int len) {
        /* simple optimization */
        if (src1 == src2)
            return 0;
        /* Convert char pointers to 4 uint8_t integers */
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

    static void *aligned_alloc(size_t block_size) {
#if defined(ARDUINO)
        return malloc ((unsigned int)block_size);
#elif defined(_MSC_VER)
        return _aligned_malloc (block_size, 64);
#elif defined(__APPLE__)
        void* a_ptr;
        if (posix_memalign (&a_ptr, 64, block_size)) {
             a_ptr = NULL;
        }
        return a_ptr;
#elif defined(__BORLANDC__) || defined(__GNUC__) || defined(__ANDROID__)
        return memalign(64, block_size);
#else
        return malloc ((unsigned int)block_size);
#endif
    }

    static void print(const char s[]) {
#if defined(ARDUINO)
        Serial.print(s);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%s", s);
#else
        std::cout << s;
#endif
    }

    static void print(long l) {
#if defined(ARDUINO)
        Serial.print(l);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%ld", l);
#else
        std::cout << l;
#endif
    }

    static void print(int16_t i16) {
#if defined(ARDUINO)
        Serial.print(i16);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%d", i16);
#else
        std::cout << i16;
#endif
    }

    static void print(uint8_t l) {
#if defined(ARDUINO)
        Serial.print((int)l);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%d", l);
#else
        std::cout << (int) l;
#endif
    }

    static void print(uint32_t l) {
#if defined(ARDUINO)
        Serial.print((unsigned long) l);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%u", l);
#else
        std::cout << l;
#endif
    }

    static void print(uint64_t l) {
#if defined(ARDUINO)
        Serial.print((unsigned long) l);
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("%lu", l);
#else
        std::cout << l;
#endif
    }

    static void endl() {
#if defined(ARDUINO)
        Serial.print("\n");
#elif defined(ANDROID) || defined(__ANDROID__)
        LOGI("\n");
#else
        std::cout << std::endl;
#endif
    }

    static void generate_bit_counts() {
        //for (int16_t i = 0; i < 256; i++) {
        //    bit_count[i] = count_set_bits(i);
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
    inline static uint8_t count_set_bits(int16_t n) {
        uint8_t count = 0;
        while (n > 0) {
            n &= (n - 1);
            count++;
        }
        return (uint8_t) count;
    }

    inline static uint8_t first_bit_mask(int16_t n) {
        uint8_t mask = 0x80;
        while (0 == (n & mask) && mask) {
            mask >>= 1;
        }
        return mask;
    }

    inline static uint8_t last_bit_mask(int16_t n) {
        uint8_t mask = 0x01;
        while (0 == (n & mask) && mask) {
            mask <<= 1;
        }
        return mask;
    }

    inline static uint8_t first_bit_offset(int16_t n) {
        int offset = 0;
        do {
            uint8_t mask = 0x01 << offset;
            if (n & mask)
                return offset;
            offset++;
        } while (offset <= 7);
        return 0x08;
    }

    // SWAR algorithm
    static int bitcount (uint8_t x)  {
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
