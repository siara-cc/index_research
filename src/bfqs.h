#ifndef bfqs_H
#define bfqs_H
#ifdef ARDUINO
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
#include "bplus_tree_handler.h"

using namespace std;

#if defined(ARDUINO)
#define BIT_COUNT_LF_CH(x) pgm_read_byte_near(util::bit_count_lf_ch + (x))
#else
#define BIT_COUNT_LF_CH(x) util::bit_count_lf_ch[x]
//#define BIT_COUNT_LF_CH(x) (BIT_COUNT(x & xAA) + BIT_COUNT2(x & x55))
#endif

#define BQ_MIDDLE_PREFIX 1

#define BFQS_HDR_SIZE 8
#define BQ_MAX_PFX_LEN buf[7]

class bfqs: public bpt_trie_handler {
private:
    const static byte need_counts[10];
    void decodeNeedCount();
    const static byte switch_map[8];
    const static byte shift_mask[8];
    inline byte *getLastPtr();
    inline void setPrefixLast(byte key_char, byte *t, byte pfx_rem_len);
    void setPtrDiff(int16_t diff);
    byte copyKary(byte *t, byte *dest, int lvl, byte *tp, byte *brk_key, int16_t brk_key_len, byte whichHalf);
    byte copyTrieHalf(byte *tp, byte *brk_key, int16_t brk_key_len, byte *dest, byte whichHalf);
    void consolidateInitialPrefix(byte *t);
    int16_t insertAfter();
    int16_t insertBefore();
    int16_t insertLeaf();
    int16_t insertConvert();
    int16_t insertThread();
    int16_t insertEmpty();
public:
    byte *last_t;
    byte last_leaf_child;
    inline int16_t searchCurrentBlock();
    void addData(int16_t idx);
    void addFirstData();
    bool isFull();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    int16_t insertCurrent();
    void updatePtrs(byte *upto, int diff);
    inline byte *getChildPtrPos(int16_t idx);
    inline byte *getPtrPos();
    inline int getHeaderSize();
};

#endif
