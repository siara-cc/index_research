#ifndef dfox_H
#define dfox_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define DX_MIDDLE_PREFIX 1

#define DFOX_NODE_SIZE 768

#define DFOX_HDR_SIZE 8
#define DX_MAX_PFX_LEN buf[7]
//#define MID_KEY_LEN buf[DX_MAX_PTR_BITMAP_BYTES+6]

class dfox : public bpt_trie_handler {
private:
    const static byte need_counts[10];
    void decodeNeedCount();
    inline byte *skipChildren(byte *t, int16_t count);
    void updateSkipLens(byte *loop_upto, byte *covering_upto, int diff);
    byte *nextKey(byte *first_key, byte *tp, byte *t, char& ctr, byte& tc, byte& child, byte& leaf);
    byte *nextPtr(byte *t);
    void deleteTrieLastHalf(int16_t brk_key_len, byte *first_key, byte *tp);
    void deleteTrieFirstHalf(int16_t brk_key_len, byte *first_key, byte *tp);
    void updatePrefix();
    int deleteSegment(byte *t, byte *delete_start);
public:
    byte *last_t;
    bool isFull(int16_t kv_lens);
    inline int16_t searchCurrentBlock();
    void addData(int16_t idx);
    byte *split(byte *first_key, int16_t *first_len_ptr);
    void setPtr(byte *t, int16_t ptr);
    byte *insertCurrent();
    inline void insBytes(byte *ptr, int16_t len);
    inline byte *getChildPtrPos(int16_t idx);
    inline byte *getPtrPos();
    inline int getHeaderSize();
};

#endif
