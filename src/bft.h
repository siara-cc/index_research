#ifndef bft_H
#define bft_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define BFT_UNIT_SIZE 3

#define BFT_HDR_SIZE 8

#define BFT_MAX_KEY_PREFIX_LEN 60

class bft_iterator_status {
public:
    byte *t;
    int keyPos;
    byte is_next;
    byte is_child_pending;
    byte tp[BFT_MAX_KEY_PREFIX_LEN];
    bft_iterator_status(byte *trie, byte prefix_len) {
        t = trie + 1;
        keyPos = prefix_len;
        is_child_pending = is_next = 0;
    }
};

class bft : public bpt_trie_handler {
private:
    byte *split_buf;
    const static byte need_counts[10];
    void decodeNeedCount();
    int16_t nextPtr(bft_iterator_status& s);
    int16_t getLastPtrOfChild(byte *triePos);
    inline byte *getLastPtr(byte *last_t);
    inline int16_t get9bitPtr(byte *t);
    void set9bitPtr(byte *t, int16_t p);
    int16_t deletePrefix(int16_t prefix_len);
    void appendPtr(int16_t p);
public:
    int16_t last_child_pos;
    bft(int16_t leaf_block_sz = 512, int16_t parent_block_sz = 512) :
        bpt_trie_handler(leaf_block_sz, parent_block_sz) {
        split_buf = (byte *) util::alignedAlloc(leaf_block_size > parent_block_size ?
                leaf_block_size : parent_block_size);
    }
    ~bft() {
        delete split_buf;
    }
    bool isFull();
    inline int16_t searchCurrentBlock();
    void addData(int16_t idx);
    void addFirstData();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    int16_t getFirstPtr();
    int16_t insertCurrent();
    void updatePtrs(byte *upto, int diff);
    inline byte *getChildPtrPos(int16_t idx);
    inline byte *getPtrPos();
    inline int getHeaderSize();
};

#endif
