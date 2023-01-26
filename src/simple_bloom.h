#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>

typedef unsigned int (*hashfunc_t)(const uint8_t *, size_t);
typedef struct
{
    size_t asize;
    uint8_t *a;
    size_t nfuncs;
    hashfunc_t *funcs;
} BloomFilter;

#define SETBIT(a, n) (a[n / CHAR_BIT] |= (1 << (n % CHAR_BIT)))
#define GETBIT(a, n) (a[n / CHAR_BIT] & (1 << (n % CHAR_BIT)))

BloomFilter *bloom_create(size_t size, size_t nfuncs, ...)
{
    BloomFilter *bloom;
    va_list l;
    int n;

    bloom = (BloomFilter *) malloc(sizeof(BloomFilter));
    bloom->a = (uint8_t *) calloc((size + CHAR_BIT - 1) / CHAR_BIT, sizeof(char));
    bloom->funcs = (hashfunc_t *)malloc(nfuncs * sizeof(hashfunc_t));
    va_start(l, nfuncs);
    for (n = 0; n < nfuncs; ++n)
        bloom->funcs[n] = va_arg(l, hashfunc_t);
    va_end(l);

    bloom->nfuncs = nfuncs;
    bloom->asize = size;

    return bloom;
}

int bloom_add(BloomFilter *bloom, const uint8_t *s, size_t len)
{
    size_t n;

    for (n = 0; n < bloom->nfuncs; ++n)
        SETBIT(bloom->a, bloom->funcs[n](s, len) % bloom->asize);

    return 0;
}

int bloom_check(BloomFilter *bloom, const uint8_t *s, size_t len)
{
    size_t n;

    for (n = 0; n < bloom->nfuncs; ++n)
    {
        if (!(GETBIT(bloom->a, bloom->funcs[n](s, len) % bloom->asize)))
            return 0;
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
