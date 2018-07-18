#ifndef dft_H
#define dft_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree.h"

using namespace std;

#define DFT_UNIT_SIZE 4

#define DFT_NODE_SIZE 512

#define DFT_HDR_SIZE 8
#define DFT_MAX_PFX_LEN buf[7]

#define INSERT_AFTER 1
#define INSERT_BEFORE 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5

class dft_node_handler : public trie_node_handler {
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
    dft_node_handler(byte *m);
    void initBuf();
    inline void initVars();
    void setBuf(byte *m);
    bool isFull(int16_t kv_lens);
    void addData();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    int16_t traverseToLeaf(byte *node_paths[] = null);
    int16_t locate();
    int16_t insertCurrent();
    inline byte insertUnit(byte *t, byte c1, byte s1, int16_t ptr1);
    inline byte insert2Units(byte *t, byte c1, byte s1, int16_t ptr1, byte c2, byte s2, int16_t ptr2);
    void updatePtrs(byte *upto, int diff);
    byte *getPtrPos();
};

class dft : public bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    static int count1, count2;
    dft();
    ~dft();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
};

#endif
