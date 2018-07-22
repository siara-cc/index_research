#ifndef dfos_H
#define dfos_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define DS_MIDDLE_PREFIX 1

#if BPT_9_BIT_PTR == 1
#define DS_MAX_PTR_BITMAP_BYTES 8
#define DS_MAX_PTRS 63
#else
#define DS_MAX_PTR_BITMAP_BYTES 0
#define DS_MAX_PTRS 240
#endif
#define DFOS_HDR_SIZE 8
#define DS_MAX_PFX_LEN buf[7]
//#define MID_KEY_LEN buf[DS_MAX_PTR_BITMAP_BYTES+6]

class dfos : public bpt_trie_handler {
private:
    const static byte need_counts[10];
    const static byte switch_map[8];
    void decodeNeedCount();
    inline byte *skipChildren(byte *t, byte count);
    inline void insAtWithPtrs(byte *ptr, const char *s, byte len);
    inline void insAtWithPtrs(byte *ptr, byte b, const char *s, byte len);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4,
            byte b5);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4,
            byte b5, byte b6);
    inline void insBytesWithPtrs(byte *ptr, int16_t len);
    inline byte insChildAndLeafAt(byte *ptr, byte b1, byte b2);
    inline void append(byte b);
    void updateSkipLens(byte *loop_upto, byte *covering_upto, int diff);
    byte *nextKey(byte *first_key, byte *tp, byte *t, char& ctr, byte& tc, byte& child, byte& leaf);
    void deleteTrieLastHalf(int16_t brk_key_len, byte *first_key, byte *tp);
    void deleteTrieFirstHalf(int16_t brk_key_len, byte *first_key, byte *tp);
    void consolidateInitialPrefix(byte *t);
    void movePtrList(byte orig_trie_len);
    int deleteSegment(byte *t, byte *delete_start);
public:
    byte pos, key_at_pos;
    bool isFull();
    inline int16_t searchCurrentBlock();
    void addData(int16_t idx);
    void addFirstData();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    void insertCurrent();
    inline byte *getChildPtrPos(int16_t idx);
    inline byte *getPtrPos();
    inline int getHeaderSize();
};

#endif
