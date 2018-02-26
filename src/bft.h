#ifndef bft_H
#define bft_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree.h"

using namespace std;

#define BFT_UNIT_SIZE 4

#define BFT_NODE_SIZE 1024

#define BFT_HDR_SIZE 7
#define BFT_TRIE_LEN buf[5]
#define BFT_PREFIX_LEN buf[6]

#define INSERT_AFTER 1
#define INSERT_BEFORE 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5

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

class bft_node_handler : public trie_node_handler {
private:
    int16_t nextPtr(bft_iterator_status& s);
    int16_t getLastPtrOfChild(byte *triePos);
    inline byte *getLastPtr(byte *last_t);
    inline int16_t get9bitPtr(byte *t);
    void set9bitPtr(byte *t, int16_t p);
    int16_t deletePrefix(int16_t prefix_len);
    void appendPtr(int16_t p);
public:
    int16_t last_child_pos;
    bft_node_handler(byte *m);
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

class bft : public bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    static byte split_buf[BFT_NODE_SIZE];
    static int count1, count2;
    bft();
    ~bft();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
};

#endif
