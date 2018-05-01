#ifndef bfos_H
#define bfos_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree.h"

using namespace std;

#define BS_MIDDLE_PREFIX 1
#define MAX_PTR_BITMAP_BYTES 0

#define BFOS_NODE_SIZE 768

#define BFOS_HDR_SIZE 7
#define BX_MAX_KEY_LEN buf[6]

#define INSERT_AFTER 1
#define INSERT_BEFORE 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5
#define INSERT_CONVERT 6
#define INSERT_THREAD1 7

class bfos_node_handler: public trie_node_handler {
private:
    inline byte *getLastPtr();
    inline void setPrefixLast(byte key_char, byte *t, byte pfx_rem_len);
    byte *nextPtr(byte *first_key, byte *tp, byte **t_ptr, byte& ctr,
            byte& tc, byte& child, byte& leaf, int16_t brk_idx, byte *new_t);
    void consolidateInitialPrefix(byte *t);
    void markTrieByte(int16_t brk_idx, byte *new_t, byte *t);
    void deleteTrieParts(bfos_node_handler& new_block, byte *last_key, int16_t last_key_len,
            byte *first_key, int16_t first_key_len);
    void setPtrDiff(int16_t diff);
public:
    byte *last_t;
    byte last_child;
    byte last_leaf;
    bfos_node_handler(byte *m);
    inline int16_t locate();
    int16_t traverseToLeaf(byte *node_paths[] = null);
    inline char *getValueAt(int16_t *vlen);
    inline byte *getChildPtr(byte *ptr);
    void setBuf(byte *m);
    void initBuf();
    inline void initVars();
    bool isFull(int16_t kv_lens);
    void addData();
    byte *split(byte *first_key, int16_t *first_len_ptr);
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
