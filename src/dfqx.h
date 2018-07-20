#ifndef dfqx_H
#define dfqx_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#if BPT_9_BIT_PTR == 1
#define DFQX_NODE_SIZE 512
#define DQ_MAX_PTR_BITMAP_BYTES 8
#define DQ_MAX_PTRS 63
#else
#define DFQX_NODE_SIZE 768
#define DQ_MAX_PTR_BITMAP_BYTES 0
#define DQ_MAX_PTRS 240
#endif
#define DFQX_HDR_SIZE 8
#define DQ_MAX_PFX_LEN buf[7]
//#define MID_KEY_LEN buf[DQ_MAX_PTR_BITMAP_BYTES+6]

#define DFQX_MAX_KEY_PREFIX_LEN 60

class dfqx: public bpt_trie_handler {
private:
    const static byte need_counts[10];
    void decodeNeedCount();
    static byte left_mask[4];
    static byte left_incl_mask[4];
    static byte ryte_mask[4];
    static byte dbl_ryte_mask[4];
    static byte ryte_incl_mask[4];
    static byte first_bit_offset[16];
    static byte bit_count[16];
#if (defined(__AVR__))
    const static PROGMEM uint16_t dbl_bit_count[256];
#else
    const static uint16_t dbl_bit_count[256];
#endif
    //static byte first_bit_offset4[16];
    //static byte bit_count_16[16];
    inline byte *skipChildren(byte *t, uint16_t& count);
    inline void append(byte b);
    inline void insBytesWithPtrs(byte *ptr, int16_t len);
    inline void insAtWithPtrs(byte *ptr, const char *s, byte len);
    inline void insAtWithPtrs(byte *ptr, byte b, const char *s, byte len);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4,
            byte b5);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4,
            byte b5, byte b6);
    void updateSkipLens(byte *loop_upto, byte *covering_upto, int diff);
    byte *nextKey(byte *first_key, byte *tp, byte *t, char& ctr, byte& tc, byte& child_leaf);
    void deleteTrieLastHalf(int16_t brk_key_len, byte *first_key, byte *tp);
    void deleteTrieFirstHalf(int16_t brk_key_len, byte *first_key, byte *tp);
    void movePtrList(byte orig_trie_len);
    int deleteSegment(byte *t, byte *delete_start);
    //inline byte *skipChildren(byte *t, uint16_t count);
public:
    byte pos, key_at_pos;
    inline int16_t searchCurrentBlock();
    void addData(int16_t idx);
    bool isFull(int16_t kv_lens);
    byte *split(byte *first_key, int16_t *first_len_ptr);
    void insertCurrent();
    inline byte *getChildPtrPos(int16_t idx);
    inline byte *getPtrPos();
    inline int getHeaderSize();
};

#endif
