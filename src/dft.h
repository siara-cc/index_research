#ifndef dft_H
#define dft_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define DFT_UNIT_SIZE 4

#define DFT_NODE_SIZE 512

#define DFT_HDR_SIZE 8
#define DFT_MAX_PFX_LEN buf[7]

class dft : public bpt_trie_handler {
private:
    const static byte need_counts[10];
    void decodeNeedCount();
    inline void append(byte b);
    void appendPtr(int16_t p);
    inline int16_t get9bitPtr(byte *t);
    void set9bitPtr(byte *t, int16_t p);
public:
    int16_t last_sibling_pos;
    int16_t pos, key_at_pos;
    inline int16_t searchCurrentBlock();
    void addData(int16_t idx);
    bool isFull(int16_t kv_lens);
    byte *split(byte *first_key, int16_t *first_len_ptr);
    int16_t traverseToLeaf(byte *node_paths[] = null);
    int16_t insertCurrent();
    inline byte insertUnit(byte *t, byte c1, byte s1, int16_t ptr1);
    inline byte insert2Units(byte *t, byte c1, byte s1, int16_t ptr1, byte c2, byte s2, int16_t ptr2);
    void updatePtrs(byte *upto, int diff);
    inline byte *getChildPtrPos(int16_t idx);
    inline byte *getPtrPos();
    inline int getHeaderSize();
};

#endif
