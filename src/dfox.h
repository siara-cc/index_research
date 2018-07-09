#ifndef dfox_H
#define dfox_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree.h"

using namespace std;

#define DX_MIDDLE_PREFIX 1
#define DX_INT64MAP 1
#define DX_9_BIT_PTR 0

#define DFOX_NODE_SIZE 768

#define DFOX_HDR_SIZE 8
#define DX_MAX_PFX_LEN buf[7]
//#define MID_KEY_LEN buf[DX_MAX_PTR_BITMAP_BYTES+6]

#define INSERT_AFTER 1
#define INSERT_BEFORE 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5
#define INSERT_CONVERT 6

class dfox_node_handler : public trie_node_handler {
private:
    static byte need_counts[10];
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
    dfox_node_handler(byte *m);
    inline int16_t locate();
    byte *getKey(byte *t, int16_t *plen);
    inline char *getValueAt(int16_t *vlen);
    inline byte *getChildPtr(byte *ptr);
    int16_t traverseToLeaf(byte *node_paths[] = null);
    inline int16_t getPtr(byte *t);
    void initBuf();
    inline void initVars();
    void setBuf(byte *m);
    bool isFull(int16_t kv_lens);
    void addData();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    inline void setPtr(byte *t, int16_t ptr);
    byte *insertCurrent();
    inline void insBytes(byte *ptr, int16_t len);
};

class dfox : public bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    static byte split_buf[DFOX_NODE_SIZE];
    static long count1, count2;
    dfox();
    ~dfox();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
    static void printCounts() {
        cout << "Count1:" << count1 << ", Count2:" << count2 << endl;
    }
};

#endif
