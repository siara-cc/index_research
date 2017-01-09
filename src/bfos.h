#ifndef bfos_H
#define bfos_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree.h"

using namespace std;

#define BFOS_INT64MAP 1
#define BFOS_UNIT_SZ_3 1
#define BFOS_9_BIT_PTR 0

#define BFOS_NODE_SIZE 512

#if BFOS_9_BIT_PTR == 1
#define MAX_PTR_BITMAP_BYTES 8
#define MAX_PTRS 63
#else
#define MAX_PTR_BITMAP_BYTES 0
#define MAX_PTRS 240
#endif
#define BFOS_HDR_SIZE (MAX_PTR_BITMAP_BYTES+6)

#define INSERT_AFTER 1
#define INSERT_BEFORE 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5

#define BFOS_MAX_KEY_PREFIX_LEN 60

class bfos_iterator_status {
public:
    byte *t;
    int keyPos;
    byte tc, children, leaves, orig_leaves, orig_children;
    byte tp[BFOS_MAX_KEY_PREFIX_LEN];
    byte offset_a[BFOS_MAX_KEY_PREFIX_LEN];
    bfos_iterator_status(byte *trie) {
        t = trie;
        keyPos = 0;
        offset_a[keyPos] = 0x08;
        tc = children = leaves = orig_leaves = orig_children = 0;
    }
};

class bfos_node_handler: public trie_node_handler {
private:
    static byte left_mask[9];
    static byte left_incl_mask[8];
    static byte ryte_mask[18];
    static byte ryte_incl_mask[8];
    static byte ryte_leaf_mask[18];
    int16_t nextPtr(bfos_iterator_status& s);
    int16_t getLastPtrOfChild(byte *triePos);
    inline byte *getLastPtr(byte *last_t, byte last_off);
    inline int16_t get9bitPtr(byte *t);
    void set9bitPtr(byte *t, int16_t p);
    int16_t deletePrefix(int16_t prefix_len);
public:
    int16_t last_child_pos;
    bfos_node_handler(byte *m);
    void initBuf();
    inline void initVars();
    void setBuf(byte *m);
    bool isFull(int16_t kv_lens);
    void addData();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    int16_t locate();
    void traverseToLeaf(byte *node_paths[] = null);
    inline char *getValueAt(int16_t *vlen);
    inline byte *getChildPtr(byte *ptr);
    int16_t getFirstPtr();
    int16_t insertCurrent();
    void updatePtrs(byte *upto, int diff);
};

class bfos : public bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    static byte split_buf[BFOS_NODE_SIZE];
    static int count1, count2;
    bfos();
    ~bfos();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
};

#endif
