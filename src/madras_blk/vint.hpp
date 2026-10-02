#ifndef _VINT_HPP_
#define _VINT_HPP_

#include <stdint.h>
#include <stdlib.h>
#include <vector>

#include "compat.hpp"

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

#define SVINT60_MIN -0xFFFFFFFFFFFFFFFLL
#define SVINT60_MAX 0xFFFFFFFFFFFFFFFLL

static size_t copy_rvint32(uint8_t *out, uint32_t u32) {
  size_t len = (size_t) (u32 < 64 ? 0 : (u32 < 4096 ? 1 : (u32 < 1048576 ? 2 : 3)));
  *out = (uint8_t) (len << 6);
  switch (len) {
    case 0:
      *out |= u32;
      return 1;
    case 1:
      out[1] = *out | (u32 & 0x3F);
      *out |= (u32 >> 6);
      return 2;
    case 2:
      out[2] = *out | (u32 & 0x3F);
      out[1] = (u32 >> 6) & 0xFF;
      *out |= (u32 >> 14);
      return 3;
  }
  out[3] = *out | (u32 & 0x3F);
  out[2] = (u32 >> 6) & 0xFF;
  out[1] = (u32 >> 14) & 0xFF;
  *out |= (u32 >> 22);
  return 4;
}
static uint32_t read_rvint32(const uint8_t *ptr, size_t& len) {
  uint32_t ret = (uint32_t) (*ptr & 0x3F);
  len = (size_t) (*ptr-- >> 6);
  len++;
  switch (len) {
    case 2:
      ret |= ((uint32_t) *ptr << 6);
      break;
    case 3:
      ret |= ((uint32_t) *ptr-- << 14);
      ret |= ((uint32_t) *ptr << 6);
      break;
    case 4:
      ret |= ((uint32_t) *ptr-- << 22);
      ret |= ((uint32_t) *ptr-- << 14);
      ret |= ((uint32_t) *ptr << 6);
      break;
  }
  return ret;
}
static size_t append_fvint64(byte_vec& vec, uint64_t u64) {
  size_t len = 0;
  do {
    uint8_t b = u64 & 0x7F;
    u64 >>= 7;
    if (u64 > 0)
      b |= 0x80;
    vec.push_back(b);
    len++;
  } while (u64 > 0);
  return len;
}
__fq1 __fq2 static size_t copy_fvint64(uint8_t *ptr, uint64_t u64) {
  size_t len = 0;
  do {
    uint8_t b = u64 & 0x7F;
    u64 >>= 7;
    if (u64 > 0)
      b |= 0x80;
    *ptr++ = b;
    len++;
  } while (u64 > 0);
  return len;
}
static size_t append_fvint64s(byte_vec& vec, int64_t i64) {
  bool is_neg = (i64 < 0);
  if (is_neg)
    i64 = -i64;
  i64 <<= 1;
  if (is_neg)
    i64 |= 1;
  return append_fvint64(vec, (uint64_t) i64);
}
static size_t append_fvint32(byte_vec& vec, uint32_t u32) {
  return append_fvint64(vec, (uint64_t) u32);
}
static size_t copy_fvint32(uint8_t *ptr, uint32_t u32) {
  size_t len = 0;
  do {
    uint8_t b = u32 & 0x7F;
    u32 >>= 7;
    if (u32 > 0)
      b |= 0x80;
    *ptr++ = b;
    len++;
  } while (u32 > 0);
  return len;
}
__fq1 __fq2 static size_t read_fvint(const uint8_t *ptr, size_t& len) {
  len = 0;
  size_t ret = 0;
  do {
    size_t bval = (size_t) (*ptr & 0x7F);
    ret += (bval << (7 * len));
    len++;
  } while (*ptr++ & 0x80);
  return ret;
}
static size_t get_vlen_of_uint32(uint32_t vint) {
  return (size_t) (vint < (1 << 7) ? 1 : (vint < (1 << 14) ? 2 : (vint < (1 << 21) ? 3 :
          (vint < (1 << 28) ? 4 : 5))));
}
static size_t append_vint32(byte_vec& vec, uint32_t vint, size_t len) {
  for (size_t i = len - 1; i > 0; i--)
    vec.push_back(0x80 + ((vint >> (7 * i)) & 0x7F));
  vec.push_back(vint & 0x7F);
  return len;
}
static size_t append_vint32(byte_vec& vec, uint32_t vint) {
  size_t len = get_vlen_of_uint32(vint);
  return append_vint32(vec, vint, len);
}
static size_t append_ovint(byte_vec& vec, size_t vint, size_t offset_bits, uint8_t or_mask) {
  size_t mask_bits = 8 - offset_bits;
  uint8_t mask = (uint8_t) ((1 << mask_bits) - 1);
  size_t len = 0;
  uint8_t b = or_mask;
  do {
    b |= (vint & mask);
    vint >>= mask_bits;
    if (vint > 0)
      b |= (mask + 1);
    vec.push_back(b);
    b = 0;
    mask = '\x7F';
    mask_bits = 7;
    len++;
  } while (vint > 0);
  return len;
}
static size_t read_ovint(const uint8_t *ptr, size_t& len, size_t offset_bits) {
  size_t mask_bits = 7 - offset_bits;
  uint8_t mask = (uint8_t) ((1 << mask_bits) - 1);
  size_t ret = (size_t) (*ptr & mask);
  len = 1;
  while (*ptr++ & (mask + 1)) {
    uint32_t b = (uint32_t) (*ptr & '\x7F');
    ret += (b << mask_bits);
    mask = '\x7F';
    mask_bits += 7;
    len++;
  }
  return ret;
}

static inline size_t first_bit_set(uint64_t num) {
#ifdef _MSC_VER
    unsigned long index;
    if (num == 0 || !_BitScanReverse64(&index, num))
        return 0;
    return index;
#else
    return num == 0 ? 0 : 63 - __builtin_clzll(num);
#endif
}
#if defined(_MSC_VER)
    #include <intrin.h>
    #define bswap64(x) _byteswap_uint64(x)
#else
    #define bswap64(x) __builtin_bswap64(x)
#endif

static inline void encode_int64_sortable(int64_t value, uint8_t out[8]) {
  uint64_t u;
  memcpy(&u, &value, sizeof(u));
  u ^= 0x8000000000000000ULL;
  u = bswap64(u);
  memcpy(out, &u, sizeof(u));
}

static inline int64_t decode_int64_sortable(const uint8_t in[8]) {
  uint64_t u;
  memcpy(&u, in, sizeof(u));
  u = bswap64(u);
  u ^= 0x8000000000000000ULL;
  int64_t value;
  memcpy(&value, &u, sizeof(value));
  return value;
}

static inline void encode_double_sortable(double value, uint8_t out[8]) {
    uint64_t u;
    memcpy(&u, &value, sizeof(u));
    if (u & 0x8000000000000000ULL)
        u = ~u;                         // negative: flip all bits to reverse order
    else
        u ^= 0x8000000000000000ULL;     // positive: flip only sign bit
    u = bswap64(u);
    memcpy(out, &u, sizeof(u));
}

static inline double decode_double_sortable(const uint8_t in[8]) {
    uint64_t u;
    memcpy(&u, in, sizeof(u));
    u = bswap64(u);
    if (u & 0x8000000000000000ULL)
        u ^= 0x8000000000000000ULL;     // positive: undo sign bit flip
    else
        u = ~u;                         // negative: undo full flip
    double value;
    memcpy(&value, &u, sizeof(value));
    return value;
}

const static uint64_t int64_len_map[] = {1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9};
static constexpr int64_t SVINT60_CEIL_TABLE[9] = {
    0,
    (1LL << 4) - 1,   // vlen=1
    (1LL << 12) - 1,  // vlen=2
    (1LL << 20) - 1,  // vlen=3
    (1LL << 28) - 1,  // vlen=4
    (1LL << 36) - 1,  // vlen=5
    (1LL << 44) - 1,  // vlen=6
    (1LL << 52) - 1,  // vlen=7
    (1LL << 60) - 1   // vlen=8
};
static size_t get_svint60_len(int64_t vint) {
    uint64_t abs_val = (uint64_t) std::abs(vint);
    if (abs_val < (1 << 4)) return 1;
    
    // Count leading zeros to find the position of highest bit
    int bits_needed = 64 - __builtin_clzll(abs_val);
    return (size_t) ((bits_needed + 3) / 8 + 1);  // Equivalent to your formula
}
static inline int64_t get_svint60_ceil(size_t vlen) {
    return SVINT60_CEIL_TABLE[vlen];
}
static inline size_t read_svint60_len(const uint8_t *ptr) {
    uint8_t byte = *ptr;
    if (byte == 0) return 1;
    
    size_t len_bits = (size_t) ((byte >> 4) & 0x07);
    return 1 + (byte & 0x80 ? len_bits : 7 - len_bits);
}
static void copy_svint60(int64_t input, uint8_t *out, size_t vlen) {
  const bool is_negative = input < 0;
  uint64_t lng = (uint64_t) (is_negative ? -input : input);

  if (is_negative) {
    lng = SVINT60_CEIL_TABLE[vlen] - lng;
  }

  const size_t shift_amount = (vlen - 1) * 8;
  const uint8_t header = (uint8_t) (((lng >> shift_amount) & 0x0F) |
                         (is_negative ? ((7 - (vlen - 1)) << 4) : (0x80 | ((vlen - 1) << 4))));
  *out++ = header;

  // Unroll for common cases
  switch (vlen - 1) {
    case 7: *out++ = (lng >> 48) & 0xFF; // fallthrough
    case 6: *out++ = (lng >> 40) & 0xFF; // fallthrough
    case 5: *out++ = (lng >> 32) & 0xFF; // fallthrough
    case 4: *out++ = (lng >> 24) & 0xFF; // fallthrough
    case 3: *out++ = (lng >> 16) & 0xFF; // fallthrough
    case 2: *out++ = (lng >> 8) & 0xFF; // fallthrough
    case 1: *out++ = lng & 0xFF; // fallthrough
    case 0: break;
  }
}
static void append_svint60(byte_vec& out, int64_t input) {
    const size_t vlen = get_svint60_len(input);
    const size_t old_size = out.size();
    
    // Pre-allocate space
    out.resize(old_size + vlen);
    
    // Use the existing copy function
    copy_svint60(input, &out[old_size], vlen);
}
static int64_t read_svint60(const uint8_t *ptr) {
    const uint8_t first_byte = *ptr;
    if (first_byte == 0) return 0;
    
    const bool is_negative = !(first_byte & 0x80);
    const size_t len = read_svint60_len(ptr);
    
    uint64_t ret = (uint64_t) (first_byte & 0x0F);
    
    // Unroll for common cases or use simple loop
    for (size_t i = 1; i < len; ++i) {
        ret = (ret << 8) | ptr[i];
    }
    
    return is_negative ? static_cast<int64_t>(ret - SVINT60_CEIL_TABLE[len]) 
                        : static_cast<int64_t>(ret);
}
// svint61: 5 bits in first byte, supports up to 61 bits (8 bytes max)
static inline size_t get_svint61_len(uint64_t vint) {
    if (vint < (1 << 5)) return 1;
    return (size_t) (((63 - __builtin_clzll(vint)) / 8) + 1);
}

static inline size_t read_svint61_len(const uint8_t *ptr) {
    return (size_t) (1 + (*ptr >> 5));
}

static void copy_svint61(uint64_t input, uint8_t *out, size_t vlen) {
  const size_t data_bytes = vlen - 1;
  const size_t shift = data_bytes * 8;

  *out++ = (uint8_t) (((input >> shift) & 0x1F) | (data_bytes << 5));

  switch (data_bytes) {
    case 7: *out++ = (input >> 48) & 0xFF; // fallthrough
    case 6: *out++ = (input >> 40) & 0xFF; // fallthrough
    case 5: *out++ = (input >> 32) & 0xFF; // fallthrough
    case 4: *out++ = (input >> 24) & 0xFF; // fallthrough
    case 3: *out++ = (input >> 16) & 0xFF; // fallthrough
    case 2: *out++ = (input >> 8) & 0xFF; // fallthrough
    case 1: *out++ = input & 0xFF; // fallthrough
    case 0: break;
  }
}

static void append_svint61(byte_vec& out, uint64_t input) {
    const size_t vlen = get_svint61_len(input);
    const size_t old_size = out.size();
    
    out.resize(old_size + vlen);
    copy_svint61(input, &out[old_size], vlen);
}

static uint64_t read_svint61(const uint8_t *ptr) {
  uint64_t ret = (uint64_t) (*ptr & 0x1F);
  const size_t len = (size_t) (*ptr >> 5);
  ++ptr;

  // Unrolled: if len=7, reads bytes 1-7; if len=3, reads bytes 1-3, etc.
  switch (len) {
    case 7: ret = (ret << 8) | ptr[6]; // fallthrough
    case 6: ret = (ret << 8) | ptr[5]; // fallthrough
    case 5: ret = (ret << 8) | ptr[4]; // fallthrough
    case 4: ret = (ret << 8) | ptr[3]; // fallthrough
    case 3: ret = (ret << 8) | ptr[2]; // fallthrough
    case 2: ret = (ret << 8) | ptr[1]; // fallthrough
    case 1: ret = (ret << 8) | ptr[0]; // fallthrough
    case 0: break;
  }

  return ret;
}

// svint15: 7 bits in first byte, supports up to 15 bits (2 bytes max)
static inline size_t get_svint15_len(uint64_t vint) {
    return (size_t) (vint < (1 << 7) ? 1 : 2);
}

static inline size_t read_svint15_len(const uint8_t *ptr) {
    return 1 + (*ptr >> 7);
}

static void copy_svint15(uint64_t input, uint8_t *out, size_t vlen) {
    if (vlen == 1) {
        *out = input & 0x7F;
    } else {
        *out++ = ((input >> 8) & 0x7F) | 0x80;
        *out = input & 0xFF;
    }
}

static void append_svint15(byte_vec& out, uint64_t input) {
    if (input < (1 << 7)) {
        out.push_back(input & 0x7F);
    } else {
        out.push_back(((input >> 8) & 0x7F) | 0x80);
        out.push_back(input & 0xFF);
    }
}

static uint64_t read_svint15(const uint8_t *ptr) {
    uint64_t ret = (uint64_t) (*ptr & 0x7F);
    if (*ptr & 0x80) {
        ret = (ret << 8) | ptr[1];
    }
    return ret;
}
    // void copy_tvint(uint32_t val, uint8_t *ptr) {
    //   if (val < 15) {
    //     *ptr++ = val;
    //     return;
    //   }
    //   val -= 15;
    //   uint32_t var_len = (val < 16 ? 2 : (val < 2048 ? 3 : (val < 262144 ? 4 : 5)));
    //   if (vec != NULL) {
    //     *vec++ = 15;
    //     int bit7s = var_len - 2;
    //     for (int i = bit7s - 1; i >= 0; i--)
    //       *vec++ = (0x80 + ((val >> (i * 7 + 4)) & 0x7F));
    //     *vec++ = (0x10 + (val & 0x0F));
    //   }
    // }
    // uint32_t read_tvint(uint8_t *ptr) {
    // }

}

#endif
