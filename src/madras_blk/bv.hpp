#ifndef _BV_HPP_
#define _BV_HPP_

#include <vector>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "gen.hpp"

#if defined(__SSSE3__) || defined(__AVX2__)
#include <tmmintrin.h>
#endif

namespace gen {

// Function qualifiers
#ifndef __fq1
#define __fq1
#endif

#ifndef __fq2
#define __fq2
#endif

typedef std::vector<uint8_t> byte_vec;
typedef std::vector<uint8_t *> byte_ptr_vec;

__fq1 __fq2 static size_t get_lkup_tbl_size(size_t count, size_t block_size, size_t entry_size) {
  size_t ret = (count / block_size) + ((count % block_size) == 0 ? 0 : 1);
  ret *= entry_size;
  return ret;
}

__fq1 __fq2 static size_t get_lkup_tbl_size2(size_t count, size_t block_size, size_t entry_size) {
  size_t ret = get_lkup_tbl_size(count, block_size, entry_size) + entry_size;
  return ret;
}

template <class T>
class bit_vector {
  private:
    std::vector<T> bv;
    size_t bit_count;
  public:
    bit_vector() {
      bit_count = 0;
    }
    // bit_no starts from 0
    void set(size_t bit_no, bool val) {
      if (bit_count <= bit_no)
        bit_count = bit_no + 1;
      size_t bit_width = (sizeof(T) * 8);
      size_t pos = bit_no / bit_width;
      while (bv.size() <= pos)
        bv.push_back(0);
      T non_repeats = (T) (T(1) << (bit_no % bit_width));
      if (val)
        bv[pos] |= non_repeats;
      else
        bv[pos] &= ~non_repeats;
    }
    bool operator[](size_t bit_no) const {
      if (bit_no >= bit_count)
        return false;
      size_t bit_width = (sizeof(T) * 8);
      size_t pos = bit_no / bit_width;
      if (pos >= bv.size())
        return false;
      T non_repeats = (T) (T(1) << (bit_no % bit_width));
      return (bv[pos] & non_repeats) != 0;
    }
    size_t get_highest() {
      return bit_count - 1;
    }
    size_t size_bytes() {
      return sizeof(T) * bv.size();
    }
    std::vector<T> *raw_data() {
      return &bv;
    }
    void reset() {
      bv.clear();
      bit_count = 0;
    }
};

class hash_bv {
private:
    std::unordered_map<size_t, uint64_t> bv;
    size_t bit_count;
public:
    hash_bv() : bit_count(0) {}
    void set(size_t bit_no, bool val) {
        if (bit_count <= bit_no)
            bit_count = bit_no + 1;
        size_t bit_width = sizeof(uint64_t) * 8;
        size_t pos = bit_no / bit_width;
        uint64_t non_repeats = uint64_t(1) << (bit_no % bit_width);
        if (val) {
            bv[pos] |= non_repeats;
        } else {
            auto it = bv.find(pos);
            if (it != bv.end()) {
                it->second &= ~non_repeats;
                if (it->second == 0)
                    bv.erase(it);
            }
        }
    }
    bool operator[](size_t bit_no) const {
        if (bit_no >= bit_count)
            return false;
        size_t bit_width = sizeof(uint64_t) * 8;
        size_t pos = bit_no / bit_width;
        auto it = bv.find(pos);
        if (it == bv.end())
          return false;
        uint64_t mask = uint64_t(1) << (bit_no % bit_width);
        return (it->second & mask) != 0;
    }
    void append_valids(size_t valid_bit_count) {
      bit_count += valid_bit_count;
    }
    void append_valids(const uint64_t *valids, size_t valid_bit_count) {
      size_t bit_width = sizeof(uint64_t) * 8; // 64
      size_t current_block_count = (bit_count + bit_width - 1) / bit_width;
      size_t given_valid_blocks = (valid_bit_count + bit_width - 1) / bit_width; // ceil division, fixed
      for (size_t i = 0; i < given_valid_blocks; i++) {
        uint64_t nulls = valids[i];
        nulls = ~nulls;
        if (nulls != 0) bv[current_block_count + i] = nulls;
      }
      bit_count += valid_bit_count;
    }
    size_t get_highest() const {
        return bit_count > 0 ? bit_count - 1 : 0;
    }
    void reset() {
        bv.clear();
        bit_count = 0;
    }
    const std::unordered_map<size_t, uint64_t>* get_bv() const {
      return &bv;
    }
    void set_u64(size_t row_id, uint64_t bm) {
      bv.emplace(row_id, bm);
    }
    void set_bit_count(size_t _bc) {
      bit_count = _bc;
    }
    uint64_t get_u64(size_t row_id) {
      auto it = bv.find(row_id);
      if (it != bv.end())
        return it->second;
      return 0;
    }
    size_t get_cmprsd_size() {
      size_t sz = 0;
      for (const auto& kv : bv) {
        uint64_t value = kv.second;
        if (value == 0) continue;
        sz++;
        if (__builtin_popcountll(value) > 32) value = ~value;
        for (int i = 0; i < 8; i++) {
          uint8_t byte = (value >> (i * 8)) & 0xFF;
          if (byte != 0) sz++;
        }
      }
      return sz;
    }
    static std::pair<uint8_t, bool> compress(uint64_t val, uint8_t* out) {
      bool flipped = false;
      if (__builtin_popcountll(val) > 32) {
          val = ~val;
          flipped = true;
      }
      uint8_t header = 0;
      uint8_t non_zero_bytes[8];
      uint8_t count = 0;
      for (int i = 0; i < 8; i++) {
          uint8_t byte = (val >> (i * 8)) & 0xFF;
          if (byte != 0) {
              header |= (1 << i);
              non_zero_bytes[count++] = byte;
          }
      }
      out[0] = header;
      for (int i = 0; i < count; i++)
          out[i + 1] = non_zero_bytes[i];
      return {static_cast<uint8_t>(1 + count), flipped};
    }

    uint8_t serialize_ranked(std::vector<uint8_t> &out) const {
      size_t num_blocks = (bit_count + 63) / 64;
      size_t rank_bytes = gen::bytes_needed(bit_count + 63);
      uint64_t rank_val = 0;
      for (size_t block = 0; block < num_blocks; ++block) {
        out.insert(out.end(), reinterpret_cast<uint8_t*>(&rank_val),
          reinterpret_cast<uint8_t*>(&rank_val) + rank_bytes);
        std::unordered_map<size_t, uint64_t>::const_iterator it = bv.find(block * 64);
        uint64_t bits = 0;
        if (it != bv.end()) bits = it->second;
        gen::append_uint64(bits, out);
        rank_val += gen::pop_count(bits);
      }
      return (uint8_t) rank_bytes;
    }
};

class bit_vector_reader {
  private:
    size_t word_count;
    bool implicit_is_ones;
    const uint64_t *presence;
    const uint64_t *rank;
    const uint64_t *payload;
  public:
    bit_vector_reader(uint64_t *in) {
      const uint64_t *data = in;
      uint64_t header = data[0];
      implicit_is_ones = (header & 1) != 0;
      word_count = header >> 1;
      size_t presence_words = (word_count + 63) / 64;
      presence = data + 1;
      rank = presence + presence_words;
      payload = rank + presence_words;
    }
    uint64_t word(size_t word_no) const {
      if (word_no >= word_count)
        return implicit_is_ones ? ~uint64_t(0) : 0;
      size_t pw = word_no / 64;
      size_t pb = word_no % 64;
      if (!((presence[pw] >> pb) & 1))
        return implicit_is_ones ? ~uint64_t(0) : 0;
      uint64_t idx = rank[pw];
      uint64_t non_repeats = (pb == 0) ? 0 : ((uint64_t(1) << pb) - 1);
      idx += __builtin_popcountll(presence[pw] & non_repeats);
      return payload[idx];
    }
    bool operator[](size_t bit_no) const {
      size_t word_no = bit_no / 64;
      size_t bit = bit_no % 64;
      uint64_t w = word(word_no);
      return (w >> bit) & 1;
    }
};

template <class T>
class bv_reader {
  private:
    T *bv;
    size_t bv_size;
    // todo: implement size check
  public:
    __fq1 __fq2 bool operator[](size_t bit_no) {
      size_t bit_width = (sizeof(T) * 8);
      size_t pos = bit_no / bit_width;
      if (pos >= bv_size)
        return false;
      T non_repeats = 1ULL << (bit_no % bit_width);
      return (bv[pos] & non_repeats) != 0;
    }
    __fq1 __fq2 void set_bv(T *_bv, size_t _bv_size) {
      bv = _bv;
      bv_size = _bv_size;
    }
};

class int_bit_vector {
private:
    byte_vec* int_ptrs;
    size_t bit_len;
    size_t count;
    size_t sz;
    size_t last_bits_used; // 0..64

    static void append_uint64(uint64_t u64, byte_vec& v) {
      // always little-endian storage
      v.push_back(u64 & 0xFF);
      v.push_back((u64 >> 8) & 0xFF);
      v.push_back((u64 >> 16) & 0xFF);
      v.push_back((u64 >> 24) & 0xFF);
      v.push_back((u64 >> 32) & 0xFF);
      v.push_back((u64 >> 40) & 0xFF);
      v.push_back((u64 >> 48) & 0xFF);
      v.push_back(u64 >> 56);
    }

public:
    int_bit_vector() : int_ptrs(nullptr), bit_len(0), count(0), sz(0), last_bits_used(0) {}

    int_bit_vector(byte_vec* _ptrs, size_t _bit_len, size_t _count) {
      init(_ptrs, _bit_len, _count);
    }

    const uint8_t *data() {
      return int_ptrs->data();
    }

    void init(byte_vec* _ptrs, size_t _bit_len, size_t _count) {
      int_ptrs = _ptrs;
      bit_len = _bit_len;
      count = _count;
      int_ptrs->clear();

      // reserve space (bit_len*count bits + up to 64b slack)
      sz = bit_len * count / 8 + 8;
      int_ptrs->reserve(sz);

      // initial 64-bit slot
      append_uint64(0, *int_ptrs);
      last_bits_used = 0;
    }

    void append(uint32_t v) { append((uint64_t)v); }

    void append(uint64_t v) {
      size_t bits = bit_len;

      while (bits > 0) {
        uint64_t* last_ptr = (uint64_t*)(int_ptrs->data() + int_ptrs->size() - 8);
        size_t space = 64 - last_bits_used;
        size_t take = (bits < space ? bits : space);

        uint64_t chunk = v & ((1ULL << take) - 1);
        *last_ptr |= chunk << last_bits_used;

        v >>= take;
        bits -= take;
        last_bits_used += take;

        if (last_bits_used == 64) {
          append_uint64(0, *int_ptrs);
          last_bits_used = 0;
        }
      }
    }

    size_t size() { return int_ptrs->size(); }
};

class int_bv_reader {
private:
    uint64_t* base;
    size_t bit_len;

public:
    __fq1 __fq2 int_bv_reader() : base(nullptr), bit_len(0) {}
    __fq1 __fq2 void init(const uint8_t* _int_bv, size_t _bit_len) {
        base = (uint64_t *) _int_bv;
        bit_len = _bit_len;
    }
    __fq1 __fq2 inline uint64_t operator[](size_t pos) const noexcept {
      if (bit_len == 0) return 0;
      uint64_t bit = pos * bit_len;
      size_t w = bit >> 6;
      uint32_t s = bit & 63;
      uint64_t lo = base[w] >> s;
      if (s + bit_len <= 64) {
        return lo & ((1ULL << bit_len) - 1);
      }
      uint64_t hi = base[w+1] << (64 - s);
      return (lo | hi) & ((1ULL << bit_len) - 1);
    }
};

// non_repeats-byte + sparse-byte encoder/decoder for uint64
class u64b8 {
public:

  #if defined(__SSSE3__) || defined(__AVX2__)
  static uint8_t shuffle_table[256][16];
  static bool shuffle_init;
  static void init_shuffle() {
    if (shuffle_init) return;
    for (int m = 0; m < 256; m++) {
      int idx = 0;
      for (int i = 0; i < 8; i++) {
        if (m & (1 << i))
          shuffle_table[m][i] = idx++;
        else
          shuffle_table[m][i] = 0x80;
      }
      for (int i = 8; i < 16; i++)
        shuffle_table[m][i] = 0x80;
    }
    shuffle_init = true;
  }
  #endif

  static size_t encode(uint64_t x, uint8_t* out) {
    uint8_t non_repeats = nonzero_non_repeats(x);
    out[0] = non_repeats;
    uint8_t* dst = out + 1;
    uint64_t v = x;
    for (int i = 0; i < 8; i++) {
      if (non_repeats & (1 << i))
        *dst++ = (uint8_t)v;
      v >>= 8;
    }
    return (size_t) (1 + (dst - (out + 1)));
  }

  static uint64_t decode(const uint8_t* in) {
    uint8_t non_repeats = in[0];
    const uint8_t* src = in + 1;

    #if defined(__SSSE3__) || defined(__AVX2__)
      init_shuffle();
      __m128i packed  = _mm_loadu_si64(src);
      __m128i shuffle = _mm_loadu_si128((const __m128i*)shuffle_table[non_repeats]);
      __m128i result  = _mm_shuffle_epi8(packed, shuffle);
      uint64_t out;
      memcpy(&out, &result, 8);
      return out;
    #else
      uint64_t result = 0;
      int idx = 0;
      for (int i = 0; i < 8; i++)
        if (non_repeats & (1 << i))
          result |= (uint64_t)src[idx++] << (i * 8);
      return result;
    #endif
  }

  static inline uint32_t encoded_size(uint64_t x) {
    #if defined(__POPCNT__)
      unsigned zero_bytes = __builtin_popcountll(zero_byte_non_repeats(x));
    #else
      uint64_t v = zero_byte_non_repeats(x);
      v -= (v >> 1) & 0x5555555555555555ULL;
      v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
      v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
      unsigned zero_bytes = (v * 0x0101010101010101ULL) >> 56;
    #endif
    return 1 + (8 - zero_bytes);
  }

private:
  static inline uint64_t zero_byte_non_repeats(uint64_t x) {
    return (x - 0x0101010101010101ULL) & ~x & 0x8080808080808080ULL;
  }
  static inline uint8_t nonzero_non_repeats(uint64_t x) {
    uint64_t tmp = zero_byte_non_repeats(x);
    uint8_t zero_non_repeats =
      ((tmp >> 7)  & 1)  | ((tmp >> 14) & 2)   |
      ((tmp >> 21) & 4)  | ((tmp >> 28) & 8)   |
      ((tmp >> 35) & 16) | ((tmp >> 42) & 32)  |
      ((tmp >> 49) & 64) | ((tmp >> 56) & 128);
    return (uint8_t)~zero_non_repeats;
  }

}; // class u64b8

class BitmapCodecs {
public:
  static uint64_t inflate(const uint8_t* in, bool flipped) {
      uint8_t header = in[0];
      uint64_t val = 0;
      if (header != 0) {
        uint8_t src = 1;
        for (int i = 0; i < 8; i++)
            if (header & (1u << i))
                val |= static_cast<uint64_t>(in[src++]) << (i * 8);
      }
      return flipped ? ~val : val;
  }
  template<typename T>
  static void expand_repeats(T* arr, size_t count, uint64_t repeats, uint64_t nulls) {
    if (count == 0) return;
    size_t rpt_null_count = (size_t) __builtin_popcountll(repeats | nulls);
    size_t last_idx = rpt_null_count + count - 1;
    uint64_t mask = 1ULL << last_idx;
    T cur = arr[--count];
    for (int i = (int) last_idx; i >= 0; i--) {
      if (nulls & mask)
        arr[i] = 0;
      else {
        arr[i] = cur;
        if ((repeats & mask) == 0 && count > 0) cur = arr[--count];
      }
      mask >>= 1;
    }
  }
};

}

#endif
