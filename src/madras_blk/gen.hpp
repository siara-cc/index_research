#ifndef _DS_COMMON_HPP_
#define _DS_COMMON_HPP_

#include <stdarg.h>
#include <stdint.h>
#include <cstring>
#include <vector>
#include <cmath>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <numeric>
#include <unordered_map>

#include "compat.hpp"

#ifndef UINTXX_WIDTH
#define UINTXX_WIDTH 64
#endif

#if UINTXX_WIDTH == 32
#define uintxx_t uint32_t
#define PRIuXX PRIu32
#define UINTXX_MAX UINT32_MAX
#else
#define uintxx_t uint64_t
#define PRIuXX PRIu64
#define UINTXX_MAX UINT64_MAX
#endif

#if defined(_WIN32) || defined(_WIN64)
static const void* memmem(const void* haystack, size_t haystack_len,
                   const void* needle, size_t needle_len) {
    if (needle_len == 0)
        return haystack;

    if (haystack_len < needle_len)
        return nullptr;

    const auto* h = static_cast<const unsigned char*>(haystack);
    const auto* n = static_cast<const unsigned char*>(needle);

    for (size_t i = 0; i <= haystack_len - needle_len; ++i) {
        if (std::memcmp(h + i, n, needle_len) == 0)
            return h + i;
    }

    return nullptr;
}
#endif

namespace gen {

// Function qualifiers
#ifndef __fq1
#define __fq1
#endif

#ifndef __fq2
#define __fq2
#endif

// Attribute qualifiers
#ifndef __gq1
#define __gq1
#endif

#ifndef __gq2
#define __gq2
#endif

typedef std::vector<uint8_t> byte_vec;
typedef std::vector<uint8_t *> byte_ptr_vec;

__gq1 __gq2 static bool is_gen_print_enabled = false;
__fq1 __fq2 static void gen_printf(const char* format, ...) {
  if (!is_gen_print_enabled)
    return;
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}

static constexpr double dbl_div[] = {1.000000000000001, 10.000000000000001, 100.000000000000001, 1000.000000000000001, 10000.000000000000001, 100000.000000000000001, 1000000.000000000000001, 10000000.000000000000001, 100000000.000000000000001, 1000000000.000000000000001};
static int count_decimal_digits(double d) {
  int counter = 0;
  while (d != (int) d) {
    d *= 10;
    counter++;
    if (counter > 9)
      break;
  }
  return counter;
}
static bool is_negative_zero(double x) {
  return x == 0.0 && std::signbit(x);
}
static uint32_t base_offset_bits(size_t bit_width) {
  return (1u << bit_width) - 2;
}
static uint8_t bits_needed(size_t num) {
  if (num == 0)
    return 1;
  return static_cast<uint8_t>(ceil(log2(num + 1)));
}
static uint8_t bytes_needed(size_t num) {
  uint8_t bits = bits_needed(num);
  return (uint8_t) ((bits + 7) / 8);
}
__fq1 __fq2 static inline size_t first_bit_pos64(uint64_t num) {
#ifdef _MSC_VER
    unsigned long index;
    if (num == 0 || !_BitScanReverse64(&index, num))
        return 0;
    return index;
#else
    return num == 0 ? 0 : 63 - __builtin_clzll(num);
#endif
}
__fq1 __fq2 static inline size_t first_bit_pos32(uint64_t num) {
#ifdef _MSC_VER
    unsigned long index;
    if (num == 0 || !_BitScanReverse(&index, (unsigned long)num))
        return 0;
    return index;
#else
    return num == 0 ? 0 : 31 - __builtin_clz((unsigned int)num);
#endif
}
static inline size_t bit_width64(uint64_t num) {
  if (num == 0) return 1;

#ifdef _MSC_VER
  unsigned long index;
  _BitScanReverse64(&index, num);
  return (size_t)index + 1;
#else
  return 64 - __builtin_clzll(num);
#endif
}
static double pow10(int p) {
  return dbl_div[p];
}
static uint32_t log4(uint32_t val) {
  return (uint32_t) (val < 4 ? 1 : (val < 16 ? 2 : (val < 64 ? 3 : (val < 256 ? 4 : (val < 1024 ? 5 : (val < 4096 ? 6 : (val < 16384 ? 7 : (val < 65536 ? 8 : 9))))))));
}
static uint32_t log5(uint32_t val) {
  return (uint32_t) (val < 5 ? 1 : (val < 25 ? 2 : (val < 125 ? 3 : (val < 625 ? 4 : (val < 3125 ? 5 : (val < 15625 ? 6 : (val < 78125 ? 7 : (val < 390625 ? 8 : 9))))))));
}
static uint32_t log8(uint32_t val) {
  return (uint32_t) (val < 8 ? 1 : (val < 64 ? 2 : (val < 512 ? 3 : (val < 4096 ? 4 : (val < 32768 ? 5 : (val < 262144 ? 6 : (val < 2097152 ? 7 : 8)))))));
}
static uint32_t lg10(uint32_t val) {
  return (uint32_t) (val < 10 ? 1 : (val < 100 ? 2 : (val < 1000 ? 3 : (val < 10000 ? 4 : (val < 100000 ? 5 : (val < 1000000 ? 6 : (val < 10000000 ? 7 : 8)))))));
}
static uint32_t log12(uint32_t val) {
  return (uint32_t) (val < 12 ? 1 : (val < 96 ? 2 : (val < 768 ? 3 : (val < 6144 ? 4 : (val < 49152 ? 5 : (val < 393216 ? 6 : (val < 3145728 ? 7 : 8)))))));
}
static uint32_t log16(uint32_t val) {
  return (uint32_t) (val < 16 ? 1 : (val < 256 ? 2 : (val < 4096 ? 3 : (val < 65536 ? 4 : (val < 1048576 ? 5 : (val < 16777216 ? 6 : (val < 268435456 ? 7 : 8)))))));
}
static uint32_t log256(uint32_t val) {
  return (uint32_t) (val < 256 ? 1 : (val < 65536 ? 2 : (val < 16777216 ? 3 : 4)));
}
static size_t max(size_t v1, size_t v2) {
  return v1 > v2 ? v1 : v2;
}
static size_t min(size_t v1, size_t v2) {
  return v1 < v2 ? v1 : v2;
}
static size_t size_align8(size_t input) {
  if ((input % 8) == 0)
    return input;
  return input + (8 - input % 8);
}
__fq1 __fq2 static clock_t print_time_taken(clock_t t, const char *msg) {
  t = clock() - t;
  double time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds
  gen_printf("%s%.5f\n", msg, time_taken);
  return clock();
}
static int compare(const uint8_t *v1, const uint8_t *v2) {
  int k = 0;
  uint8_t c1, c2;
  do {
    c1 = v1[k];
    c2 = v2[k];
    if (c1 == 0 && c2 == 0)
      return 0;
    k++;
  } while (c1 == c2);
  return (c1 < c2 ? -k : k);
}
static int compare(const uint8_t *v1, int len1, const uint8_t *v2) {
  if (len1 == 0 && v2[0] == 0)
    return 0;
  int k = 0;
  uint8_t c1, c2;
  c1 = c2 = 0;
  while (k < len1 && c1 == c2) {
    c1 = v1[k];
    c2 = v2[k];
    k++;
    if (k == len1 && c2 == 0)
      return 0;
  }
  return (c1 < c2 ? -k : k);
}
static int compare(const uint8_t *v1, const uint8_t *v2, int len2) {
  if (len2 == 0 && v1[0] == 0)
    return 0;
  int k = 0;
  uint8_t c1, c2;
  c1 = c2 = 0;
  while (k < len2 && c1 == c2) {
    c1 = v1[k];
    c2 = v2[k];
    k++;
    if (k == len2 && c1 == 0)
      return 0;
  }
  return (c1 < c2 ? -k : k);
}
static int compare(const uint8_t *v1, int len1, const uint8_t *v2, int len2) {
    int lim = (len2 < len1 ? len2 : len1);
    int k = 0;
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
static int compare_rev(const uint8_t *v1, int len1, const uint8_t *v2,
        int len2) {
    int lim = (len2 < len1 ? len2 : len1);
    int k = 0;
      while (k++ < lim) {
        uint8_t c1 = v1[len1 - k];
        uint8_t c2 = v2[len2 - k];
        if (c1 < c2)
            return -k;
        else if (c1 > c2)
            return k;
    }
    if (len1 == len2)
        return 0;
    return (len1 < len2 ? -k : k);
}
__fq1 __fq2 static void copy_uint16(uint16_t input, uint8_t *out) {
  *out++ = (uint8_t) (input & 0xFF);
  *out = (uint8_t) (input >> 8);
}
__fq1 __fq2 static void copy_uint24(uint32_t input, uint8_t *out) {
  *out++ = input & 0xFF;
  *out++ = (input >> 8) & 0xFF;
  *out = (uint8_t) (input >> 16);
}
__fq1 __fq2 static void copy_uint32(uint32_t input, uint8_t *out) {
  *out++ = input & 0xFF;
  *out++ = (input >> 8) & 0xFF;
  *out++ = (input >> 16) & 0xFF;
  *out = (input >> 24);
}
__fq1 __fq2 static void copy_uint64(uint64_t input, uint8_t *out) {
  *out++ = input & 0xFF;
  *out++ = (input >> 8) & 0xFF;
  *out++ = (input >> 16) & 0xFF;
  *out++ = (input >> 24) & 0xFF;
  *out++ = (input >> 32) & 0xFF;
  *out++ = (input >> 40) & 0xFF;
  *out++ = (input >> 48) & 0xFF;
  *out = (input >> 56);
}
static void copy_vint32(uint32_t input, uint8_t *out, size_t vlen) {
  for (int i = (int) vlen - 1; i > 0; i--)
    *out++ = 0x80 + ((input >> (7 * i)) & 0x7F);
  *out = input & 0x7F;
}
static uint32_t read_vint32(const uint8_t *ptr, size_t *vlen = NULL) {
  uint32_t ret = 0;
  size_t len = 5; // read max 5 bytes
  do {
    ret <<= 7;
    ret += *ptr & 0x7F;
    len--;
  } while ((*ptr++ >> 7) && len);
  if (vlen != NULL)
    *vlen = 5 - len;
  return ret;
}
static void write_uint16(uint32_t input, FILE *fp) {
  fputc((int) (input & 0xFF), fp);
  fputc((int) (input >> 8), fp);
}
static void write_uint24(uint32_t input, FILE *fp) {
  fputc((int) (input & 0xFF), fp);
  fputc((int) ((input >> 8) & 0xFF), fp);
  fputc((int) (input >> 16), fp);
}
static void write_uint32(uint32_t input, FILE *fp) {
  fputc((int) (input & 0xFF), fp);
  fputc((int) ((input >> 8) & 0xFF), fp);
  fputc((int) ((input >> 16) & 0xFF), fp);
  fputc((int) (input >> 24), fp);
}
static void write_uint64(uint64_t input, FILE *fp) {
  fputc((int) (input & 0xFF), fp);
  fputc((int) ((input >> 8) & 0xFF), fp);
  fputc((int) ((input >> 16) & 0xFF), fp);
  fputc((int) ((input >> 24) & 0xFF), fp);
  fputc((int) ((input >> 32) & 0xFF), fp);
  fputc((int) ((input >> 40) & 0xFF), fp);
  fputc((int) ((input >> 48) & 0xFF), fp);
  fputc((int) (input >> 56), fp);
}
static void append_uint16(uint16_t u16, byte_vec& v) {
  v.push_back((uint8_t) (u16 & 0xFF));
  v.push_back((uint8_t) (u16 >> 8));
}
static void append_uint24(uint32_t u24, byte_vec& v) {
  v.push_back(u24 & 0xFF);
  v.push_back((u24 >> 8) & 0xFF);
  v.push_back((uint8_t) (u24 >> 16));
}
static void append_uint32(uint32_t u32, byte_vec& v) {
  v.push_back(u32 & 0xFF);
  v.push_back((u32 >> 8) & 0xFF);
  v.push_back((u32 >> 16) & 0xFF);
  v.push_back(u32 >> 24);
}
static void append_uint64(uint64_t u64, byte_vec& v) {
  v.push_back(u64 & 0xFF);
  v.push_back((u64 >> 8) & 0xFF);
  v.push_back((u64 >> 16) & 0xFF);
  v.push_back((u64 >> 24) & 0xFF);
  v.push_back((u64 >> 32) & 0xFF);
  v.push_back((u64 >> 40) & 0xFF);
  v.push_back((u64 >> 48) & 0xFF);
  v.push_back(u64 >> 56);
}
static float fastlog2(float x) {
    union {
        float f;
        uint32_t i;
    } vx = {x};
    float y = static_cast<float>((vx.i >> 23) - 127); // integer part
    vx.i = (vx.i & 0x7FFFFF) | (127 << 23);    // normalize mantissa
    y += vx.f - 1.0f;                          // simple approximation
    return y;
}
static double mean(const std::vector<double>& v) {
    double sum = std::accumulate(v.begin(), v.end(), 0.0);
    return sum / (double) v.size();
}
static double stdev(const std::vector<double>& v) {
  double m = mean(v);
  double accum = 0.0;
  for (double x : v) accum += (x - m) * (x - m);
  return std::sqrt(accum / (double) v.size());   // population stddev
}
static void append_byte_vec(byte_vec& vec, const uint8_t *val, size_t val_len) {
  for (size_t i = 0; i < val_len; i++)
    vec.push_back(val[i]);
}
static inline uint64_t read_fast_vuint64(const uint8_t* src, size_t& used_bytes) {
  uint8_t len = (uint8_t) (src[0] & 0x0F);
  used_bytes = len;
  uint64_t packed = 0;
  memcpy(&packed, src, len);
  uint64_t zz = packed >> 4;
  return zz;
}
static inline void copy_fast_vuint64(uint64_t input, uint8_t *out, size_t vlen) {
  uint64_t value = (input << 4) | (uint64_t)vlen;
  memcpy(out, &value, vlen > 8 ? 8 : vlen);
  if (vlen > 8) out[8] = (uint8_t)(input >> 60);
}
static inline int64_t read_fast_vint64(const uint8_t* src, size_t& used_bytes) {
  uint64_t zz = read_fast_vuint64(src, used_bytes);
  return int64_t((zz >> 1) ^ (~(zz & 1) + 1));
}
static inline uint8_t fast_vuint32_size(uint32_t zz) {
  zz |= 1;
  uint32_t msb = (uint32_t) (31 - __builtin_clz(zz));
  return uint8_t((msb + 4) / 8 + 1);
}
static inline uint8_t fast_vuint64_size(uint64_t zz) {
  zz |= 1ULL;
  uint64_t msb = (uint64_t) (63 - __builtin_clzll(zz));
  return uint8_t((msb + 4) / 8 + 1);
}
static inline uint32_t read_fast_vuint32(const uint8_t* src, size_t& used_bytes) {
  uint8_t len = (uint8_t) (src[0] & 0x0F);
  used_bytes = len;
  uint32_t packed = 0;
  memcpy(&packed, src, len);
  uint32_t zz = packed >> 4;
  return zz;
}
static inline void copy_fast_vuint32(uint32_t input, uint8_t *out, size_t vlen) {
  uint64_t packed = ((uint64_t) input << 4) | vlen;
  memcpy(out, &packed, vlen);
}
static inline void append_fast_vuint32(byte_vec &vec, uint32_t input) {
  size_t vlen = fast_vuint32_size(input);
  for (size_t i = 0; i < vlen; i++) vec.push_back(' ');
  copy_fast_vuint32(input, vec.data() + vec.size() - vlen, vlen);
}
static inline void append_fast_vuint64(byte_vec &vec, uint64_t input) {
  size_t vlen = fast_vuint64_size(input);
  for (size_t i = 0; i < vlen; i++) vec.push_back(' ');
  copy_fast_vuint64(input, vec.data() + vec.size() - vlen, vlen);
}
static inline int32_t read_fast_vint32(const uint8_t* src, size_t& used_bytes) {
  uint32_t zz = read_fast_vuint32(src, used_bytes);
  return int32_t((zz >> 1) ^ (~(zz & 1) + 1));
}
static uint32_t read_uint16(byte_vec& v, size_t pos) {
  uint32_t ret = v[pos++];
  ret |= (v[pos] << 8);
  return ret;
}
static inline uint32_t read_uint16(const uint8_t *ptr) {
#ifndef __CUDA_ARCH__
  uint16_t tmp;
  memcpy(&tmp, ptr, sizeof(tmp));
  return tmp; // faster endian dependent
#else
  uint32_t ret = *ptr++;
  ret |= (*ptr << 8);
  return ret;
#endif
}

static uint32_t read_uint24(byte_vec& v, size_t pos) {
  uint32_t ret = v[pos++];
  ret |= (uint32_t(v[pos++]) << 8);
  ret |= (uint32_t(v[pos]) << 16);
  return ret;
}

static inline uint32_t read_uint24(const uint8_t *ptr) {
#ifndef __CUDA_ARCH__
  uint32_t tmp = 0;
  memcpy(&tmp, ptr, sizeof(tmp));
  return tmp & 0x00FFFFFF; // faster endian dependent
#else
  uint32_t ret = *ptr++;
  ret |= (*ptr++ << 8);
  ret |= (*ptr << 16);
  return ret;
#endif
}

static inline uint32_t read_uint32(uint8_t *ptr) {
#ifndef __CUDA_ARCH__
  uint32_t tmp;
  memcpy(&tmp, ptr, sizeof(tmp));
  return tmp;
#else
  uint32_t ret = *ptr++;
  ret |= (*ptr++ << 8);
  ret |= (*ptr++ << 16);
  ret |= (*ptr << 24);
  return ret;
#endif
}

static uint64_t read_uint40(uint8_t *ptr) {
#ifndef __CUDA_ARCH__
  uint32_t lo;
  memcpy(&lo, ptr, sizeof(lo));
  uint64_t ret = lo;
  return ret | ((uint64_t) ptr[4] << 32);
#else
  uint64_t ret = *ptr++;
  ret |= ((uint64_t)*ptr++ << 8);
  ret |= ((uint64_t)*ptr++ << 16);
  ret |= ((uint64_t)*ptr++ << 24);
  ret |= ((uint64_t)*ptr << 32);
  return ret;
#endif
}

__fq1 __fq2 static inline uint64_t read_uint64(const uint8_t *t) {
#ifndef __CUDA_ARCH__
  uint64_t tmp;
  memcpy(&tmp, t, sizeof(tmp));
  return tmp;
#else
  uint64_t ret = *t++;
  ret |= ((uint64_t)*t++ << 8);
  ret |= ((uint64_t)*t++ << 16);
  ret |= ((uint64_t)*t++ << 24);
  ret |= ((uint64_t)*t++ << 32);
  ret |= ((uint64_t)*t++ << 40);
  ret |= ((uint64_t)*t++ << 48);
  ret |= ((uint64_t)*t << 56);
  return ret;
#endif
}

static uint8_t *read_uint64(uint8_t *t, uint64_t& u64) {
#ifndef __CUDA_ARCH__
  memcpy(&u64, t, sizeof(u64)); // faster endian dependent
  return t + 8;
#else
  u64 = *t++;
  u64 |= ((uint64_t)*t++ << 8);
  u64 |= ((uint64_t)*t++ << 16);
  u64 |= ((uint64_t)*t++ << 24);
  u64 |= ((uint64_t)*t++ << 32);
  u64 |= ((uint64_t)*t++ << 40);
  u64 |= ((uint64_t)*t++ << 48);
  u64 |= ((uint64_t)*t << 56);
  return t;
#endif
  // u64 = 0;
  // for (int v = 0; v < 8; v++) {
  //   u64 <<= 8;
  //   u64 |= *t++;
  // }
  // return t;
}

static uintxx_t read_uintx(const uint8_t *ptr, uintxx_t mask) {
  uintxx_t tmp;
  memcpy(&tmp, ptr, sizeof(tmp));
  return tmp & mask; // faster endian dependent
}
static uint8_t *extract_line(uint8_t *last_line, size_t& last_line_len, size_t remaining) {
  if (remaining == 0)
    return nullptr;
  last_line += last_line_len;
  while (*last_line == '\r' || *last_line == '\n' || *last_line == '\0') {
    last_line++;
    remaining--;
    if (remaining == 0)
      return nullptr;
  }
  uint8_t *cr_pos = (uint8_t *) memchr(last_line, '\n', remaining);
  if (cr_pos != nullptr) {
    if (*(cr_pos - 1) == '\r')
      cr_pos--;
    *cr_pos = 0;
    last_line_len = (size_t) (cr_pos - last_line);
  } else
    last_line_len = remaining;
  return last_line;
}

static uint64_t pop_count(uint64_t bm) {
  return static_cast<uint64_t>(__builtin_popcountll(bm));
}

class byte_str {
  size_t max_len;
  size_t len;
  uint8_t *buf;
  public:
    __fq1 __fq2 byte_str() {
    }
    __fq1 __fq2 inline byte_str(uint8_t *_buf, size_t _max_len) {
      set_buf_max_len(_buf, _max_len);
    }
    __fq1 __fq2 inline void set_buf_max_len(uint8_t *_buf, size_t _max_len) {
      len = 0;
      buf = _buf;
      max_len = _max_len;
    }
    __fq1 __fq2 inline void append(const uint8_t b) {
      if (len < max_len) buf[len++] = b;
    }
    __fq1 __fq2 inline void append(const uint8_t *b, size_t blen) {
      size_t limit = len + blen;
      if (limit > max_len) limit = max_len;
      while (len < limit) buf[len++] = *b++;
    }
    __fq1 __fq2 inline uint8_t *data() {
      return buf;
    }
    __fq1 __fq2 uint8_t operator[](size_t idx) const {
      return buf[idx];
    }
    __fq1 __fq2 inline size_t length() {
      return len;
    }
    __fq1 __fq2 void set_length(size_t _len) {
      len = _len;
    }
    __fq1 __fq2 size_t get_limit() {
      return max_len;
    }
    __fq1 __fq2 void clear() {
      len = 0;
    }
};

}
#endif
