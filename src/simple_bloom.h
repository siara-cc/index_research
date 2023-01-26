#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>

// The FALLTHROUGH_INTENDED macro can be used to annotate implicit fall-through
// between switch labels. The real definition should be provided externally.
// This one is a fallback version for unsupported compilers.
#ifndef FALLTHROUGH_INTENDED
#define FALLTHROUGH_INTENDED \
  do {                       \
  } while (0)
#endif

inline uint32_t DecodeFixed32(const uint8_t* ptr) {
  const uint8_t* const buffer = reinterpret_cast<const uint8_t*>(ptr);

  // Recent clang and gcc optimize this to a single mov / ldr instruction.
  return (static_cast<uint32_t>(buffer[0])) |
         (static_cast<uint32_t>(buffer[1]) << 8) |
         (static_cast<uint32_t>(buffer[2]) << 16) |
         (static_cast<uint32_t>(buffer[3]) << 24);
}

uint32_t Hash(const uint8_t* data, size_t n, uint32_t seed) {
  // Similar to murmur hash
  const uint32_t m = 0xc6a4a793;
  const uint32_t r = 24;
  const uint8_t* limit = data + n;
  uint32_t h = seed ^ (n * m);

  // Pick up four bytes at a time
  while (data + 4 <= limit) {
    uint32_t w = DecodeFixed32(data);
    data += 4;
    h += w;
    h *= m;
    h ^= (h >> 16);
  }

  // Pick up remaining bytes
  switch (limit - data) {
    case 3:
      h += static_cast<uint8_t>(data[2]) << 16;
      FALLTHROUGH_INTENDED;
    case 2:
      h += static_cast<uint8_t>(data[1]) << 8;
      FALLTHROUGH_INTENDED;
    case 1:
      h += static_cast<uint8_t>(data[0]);
      h *= m;
      h ^= (h >> r);
      break;
  }
  return h;
}

static uint32_t BloomHash(const uint8_t *key, size_t sz) {
  return Hash(key, sz, 0xbc9f1d34);
}

typedef unsigned int (*hashfunc_t)(const uint8_t *, size_t);
typedef struct
{
    size_t asize;
    uint8_t *a;
    size_t nfuncs;
    size_t k_;
    size_t bits_;
    hashfunc_t *funcs;
} BloomFilter;

#define SETBIT(a, n) (a[n / CHAR_BIT] |= (1 << (n % CHAR_BIT)))
#define GETBIT(a, n) (a[n / CHAR_BIT] & (1 << (n % CHAR_BIT)))

BloomFilter *bloom_create(size_t sz, size_t bits_per_key_)
{
    BloomFilter *bloom;
    va_list l;

    bloom = (BloomFilter *) malloc(sizeof(BloomFilter));

    // Compute bloom filter size (in both bits and bytes)
    size_t bits = sz * bits_per_key_;
    // For small n, we can see a very high false positive rate.  Fix it
    // by enforcing a minimum bloom filter length.
    if (bits < 64) bits = 64;

    // We intentionally round down to reduce probing cost a little bit
    bloom->k_ = static_cast<size_t>(bits_per_key_ * 0.69);  // 0.69 =~ ln(2)
    if (bloom->k_ < 1) bloom->k_ = 1;
    if (bloom->k_ > 30) bloom->k_ = 30;

    size_t bytes = (bits + 7) / 8;
    bits = bytes * 8;
    bloom->bits_ = bits;
    bloom->asize = bytes;

    bloom->a = (uint8_t *) calloc((bytes + CHAR_BIT - 1) / CHAR_BIT, sizeof(char));

    return bloom;
}

int bloom_add(BloomFilter *bloom, const uint8_t *s, size_t len)
{
    uint32_t h = BloomHash(s, len);
    const uint32_t delta = (h >> 17) | (h << 15);  // Rotate right 17 bits
    for (size_t j = 0; j < bloom->k_; j++) {
        const uint32_t bitpos = h % bloom->bits_;
        bloom->a[bitpos / 8] |= (1 << (bitpos % 8));
        h += delta;
    }
    return 0;
}

int bloom_check(BloomFilter *bloom, const uint8_t *s, size_t len)
{
    const uint8_t* array = bloom->a;
    const size_t bits = bloom->bits_;

    uint32_t h = BloomHash(s, len);
    const uint32_t delta = (h >> 17) | (h << 15);  // Rotate right 17 bits
    for (size_t j = 0; j < bloom->k_; j++) {
      const uint32_t bitpos = h % bits;
      if ((bloom->a[bitpos / 8] & (1 << (bitpos % 8))) == 0) return 0;
      h += delta;
    }
    return 1;
}

unsigned int sax_hash(const uint8_t *key, size_t len)
{
    unsigned int h = 0;

    while (len--)
        h ^= (h << 5) + (h >> 2) + (unsigned char)*key++;

    return h;
}

unsigned int sdbm_hash(const uint8_t *key, size_t len)
{
    unsigned int h = 0;
    while (len--)
        h = (unsigned char)*key++ + (h << 6) + (h << 16) - h;
    return h;
}
