#ifndef dfqx_H
#define dfqx_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree.h"

using namespace std;

#define DQ_INT64MAP 1
#define DQ_9_BIT_PTR 0

#define DFQX_NODE_SIZE 1024

#if DQ_9_BIT_PTR == 1
#define DQ_MAX_PTR_BITMAP_BYTES 8
#define DQ_MAX_PTRS 63
#else
#define DQ_MAX_PTR_BITMAP_BYTES 0
#define DQ_MAX_PTRS 240
#endif
#define DFQX_HDR_SIZE 6
//#define MID_KEY_LEN buf[DQ_MAX_PTR_BITMAP_BYTES+6]

#define INSERT_AFTER 1
#define INSERT_BEFORE 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5

#define DFQX_MAX_KEY_PREFIX_LEN 60

class dfqx_node_handler: public trie_node_handler {
private:
    static byte left_mask[4];
    static byte left_incl_mask[4];
    static byte ryte_mask[4];
    static byte dbl_ryte_mask[4];
    static byte ryte_incl_mask[4];
    static byte first_bit_offset[16];
    static byte bit_count[16];
    static uint16_t dbl_bit_count[256];
    //static byte first_bit_offset4[16];
    //static byte bit_count_16[16];
    inline void append(byte b);
    byte *nextKey(byte *first_key, byte *tp, byte *t, char& ctr, byte& tc, byte& child_leaf);
    void deleteTrieLastHalf(int16_t brk_key_len, byte *first_key, byte *tp);
    void deleteTrieFirstHalf(int16_t brk_key_len, byte *first_key, byte *tp);
    int deleteSegment(byte *t, byte *delete_start);
public:
    int16_t pos, key_at_pos;
#if DQ_INT64MAP == 1
    uint64_t *bitmap;
#else
    uint32_t *bitmap1;
    uint32_t *bitmap2;
#endif
    dfqx_node_handler(byte *m);
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
    void updatePtrs(byte *upto, int diff);
    void insertCurrent();
    byte *getKey(int16_t pos, int16_t *plen);
    inline char *getValueAt(int16_t *vlen);
    inline byte *getChildPtr(byte *ptr);
};

class dfqx: public bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    static long count1, count2;
    dfqx();
    ~dfqx();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
    static void printCounts() {
        cout << "Count1:" << count1 << ", Count2:" << count2 << endl;
    }
};

#endif
