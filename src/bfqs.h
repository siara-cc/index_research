#ifndef bfqs_H
#define bfqs_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree.h"

using namespace std;

#define BIT_COUNT_LF_CH(x) (BIT_COUNT(x & xAA) + BIT_COUNT2(x & x55))

#define BQ_MIDDLE_PREFIX 1

#define BFQS_NODE_SIZE 768

#define BFQS_HDR_SIZE 8
#define BQ_MAX_PFX_LEN buf[7]

#define INSERT_AFTER 1
#define INSERT_BEFORE 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5
#define INSERT_CONVERT 6
#define INSERT_THREAD1 7

class bfqs_node_handler: public trie_node_handler {
private:
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
    bfqs_node_handler(byte *m);
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

class bfqs : public bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    static int count1, count2;
    bfqs();
    ~bfqs();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
};

#endif
