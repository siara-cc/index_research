#ifndef dfos_H
#define dfos_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree.h"

using namespace std;

#define DS_INT64MAP 1
#define DS_9_BIT_PTR 1

#define DFOS_NODE_SIZE 512

#if DS_9_BIT_PTR == 1
#define DS_MAX_PTR_BITMAP_BYTES 8
#define DS_MAX_PTRS 63
#else
#define DS_MAX_PTR_BITMAP_BYTES 0
#define DS_MAX_PTRS 240
#endif
#define DFOS_HDR_SIZE 6
//#define MID_KEY_LEN buf[DS_MAX_PTR_BITMAP_BYTES+6]

#define INSERT_MIDDLE1 1
#define INSERT_MIDDLE2 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5

#define DFOS_MAX_KEY_PREFIX_LEN 60

class dfos_iterator_status {
public:
    byte *t;
    byte tp[DFOS_MAX_KEY_PREFIX_LEN];
    byte tc_a[DFOS_MAX_KEY_PREFIX_LEN];
    byte child_a[DFOS_MAX_KEY_PREFIX_LEN];
    byte leaf_a[DFOS_MAX_KEY_PREFIX_LEN];
    byte offset_a[DFOS_MAX_KEY_PREFIX_LEN];
};

class dfos_node_handler : public trie_node_handler {
private:
    inline byte insChildAndLeafAt(byte *ptr, byte b1, byte b2);
    inline void append(byte b);
    int16_t findPos(dfos_iterator_status& s, int brk_idx);
    int16_t nextKey(dfos_iterator_status& s);
    void deleteTrieLastHalf(int16_t brk_key_len, dfos_iterator_status& s);
    void deleteTrieFirstHalf(int16_t brk_key_len, dfos_iterator_status& s);
public:
    int16_t pos, key_at_pos;
#if defined(DS_INT64MAP)
    uint64_t *bitmap;
#else
    uint32_t *bitmap1;
    uint32_t *bitmap2;
#endif
    dfos_node_handler(byte *m);
    void initBuf();
    inline void initVars();
    void setBuf(byte *m);
    bool isFull(int16_t kv_lens);
    void addData();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    inline int16_t getPtr(int16_t pos);
    inline void setPtr(int16_t pos, int16_t ptr);
    void insPtr(int16_t pos, int16_t kvIdx);
    void insBit(uint32_t *ui32, int pos, int16_t kv_pos);
    void insBit(uint64_t *ui64, int pos, int16_t kv_pos);
    void traverseToLeaf(byte *node_paths[] = null);
    int16_t locate();
    void insertCurrent();
    byte *getKey(int16_t pos, int16_t *plen);
    inline char *getValueAt(int16_t *vlen);
    inline byte *getChildPtr(byte *ptr);
};

class dfos : public bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    static long count1, count2;
    dfos();
    ~dfos();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
    static void printCounts() {
        cout << "Count1:" << count1 << ", Count2:" << count2 << endl;
    }
};

#endif
