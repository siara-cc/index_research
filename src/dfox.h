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

#define DFOX_NODE_SIZE 1024

#if DX_9_BIT_PTR == 1
#define DX_MAX_PTR_BITMAP_BYTES 8
#define DX_MAX_PTRS 63
#else
#define DX_MAX_PTR_BITMAP_BYTES 0
#define DX_MAX_PTRS 240
#endif
#define DFOX_HDR_SIZE 6
//#define MID_KEY_LEN buf[DX_MAX_PTR_BITMAP_BYTES+6]

#define INSERT_AFTER 1
#define INSERT_BEFORE 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5
#define INSERT_CONVERT 6

#define DFOX_MAX_KEY_PREFIX_LEN 72

class dfox_node_handler : public trie_node_handler {
private:
    inline void insAtWithPtrs(byte *ptr, const char *s, byte len);
    inline void insAtWithPtrs(byte *ptr, byte b, const char *s, byte len);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4,
            byte b5);
    inline byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4,
            byte b5, byte b6);
    inline byte insChildAndLeafAt(byte *ptr, byte b1, byte b2);
    inline void append(byte b);
    void updatePtrs(byte *loop_upto, byte *covering_upto, int diff);
    byte *nextKey(byte *first_key, byte *tp, byte *t, char& ctr, byte& tc, byte& child, byte& leaf);
    void deleteTrieLastHalf(int16_t brk_key_len, byte *first_key, byte *tp);
    void deleteTrieFirstHalf(int16_t brk_key_len, byte *first_key, byte *tp);
    void movePtrList(byte orig_trie_len);
    int deleteSegment(byte *t, byte *delete_start);
    inline byte *skipChildren(byte *t, int16_t count);
public:
    int16_t pos, key_at_pos;
#if DX_INT64MAP == 1
    uint64_t *bitmap;
#else
    uint32_t *bitmap1;
    uint32_t *bitmap2;
#endif
    dfox_node_handler(byte *m);
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

class dfox : public bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
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
