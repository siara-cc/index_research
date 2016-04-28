#ifndef dfox_H
#define dfox_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"

using namespace std;

typedef unsigned char byte;
#define DFOX_NODE_SIZE 512
#define MAX_PTR_BITMAP_BYTES 4
#define IDX_BLK_SIZE 64
#define IDX_HDR_SIZE (MAX_PTR_BITMAP_BYTES+4)
#define TRIE_PTR_AREA_SIZE (IDX_BLK_SIZE-IDX_HDR_SIZE)
#define MAX_PTRS 30

#define INSERT_MIDDLE1 1
#define INSERT_MIDDLE2 2
#define INSERT_THREAD 3
#define INSERT_LEAF 4

#define IS_LEAF_BYTE buf[MAX_PTR_BITMAP_BYTES-1]
#define DATA_PTR_HIGH_BITS buf
#define TRIE_LEN buf[MAX_PTR_BITMAP_BYTES]
#define FILLED_SIZE buf[MAX_PTR_BITMAP_BYTES+1]
#define LAST_DATA_PTR buf + MAX_PTR_BITMAP_BYTES + 2

class dfox_var {
public:
    byte tc;
    byte mask;
    byte msb5;
    byte keyPos;
    byte children;
    byte leaves;
    byte triePos;
    byte origPos;
    byte need_count;
    byte insertState;
    byte isPut;
    const char *key;
    int key_len;
    char *key_at;
    int key_at_len;
    int lastSearchPos;
    dfox_var() {
        init();
    }
    ~dfox_var() {
    }
    void init();
};

class dfox_node{
private:
    byte *trie;
    static byte left_mask[8];
    static byte ryte_mask[8];
    static byte ryte_incl_mask[8];
    static byte pos_mask[48];
    inline void insAt(byte pos, byte b);
    inline void insAt(byte pos, byte b1, byte b2);
    inline void insAt(byte pos, byte b1, byte b2, byte b3);
    inline void setAt(byte pos, byte b);
    inline void append(byte b);
    inline byte getAt(byte pos);
    inline void delAt(byte pos);
    inline void delAt(byte pos, int count);
    byte recurseSkip(dfox_var *v, byte skip_count, byte skip_size);
    byte processTC(dfox_var *v);
    static byte *alignedAlloc();
public:
    byte *buf;
    int threads;
    dfox_node(byte *m);
    void setBuf(byte *m);
    bool isFull(int kv_len, dfox_var *v);
    inline bool isLeaf();
    inline void setLeaf(char isLeaf);
    int filledSize();
    inline void setFilledSize(int filledSize);
    inline int getKVLastPos();
    inline void setKVLastPos(int val);
    void addData(int idx, const char *value, int value_len, dfox_var *v);
    byte *getChild(int pos);
    byte *getKey(int pos, int *plen);
    byte *getData(int pos, int *plen);
    byte *split(int *pbrk_idx);
    int getPtr(int pos);
    void insPtr(int pos, int kvIdx);
    int locate(dfox_var *v, int level);
    bool recurseTrie(int level, dfox_var *v);
    byte recurseEntireTrie(int level, dfox_var *v, long idx_list[],
            int *pidx_len);
    void insertCurrent(dfox_var *v);
};

class dfox {
private:
    long total_size;
    int numLevels;
    int maxKeyCount;
    int blockCount;
    byte *recursiveSearch(const char *key, int key_len, byte *node_data,
            int lastSearchPos[], byte *node_paths[], int *pIdx, dfox_var *v);
    void recursiveUpdate(const char *key, int key_len, byte *foundNode, int pos,
            const char *value, int value_len, int lastSearchPos[],
            byte *node_paths[], int level, dfox_var *v);
public:
    dfox_node *root;
    int maxThread;
    dfox();
    ~dfox();
    char *get(const char *key, int key_len, int *pValueLen);
    void put(const char *key, int key_len, const char *value,
            int value_len);
    void printMaxKeyCount(long num_entries) {
        std::cout << "Block Count:" << blockCount << std::endl;
        std::cout << "Avg Block Count:" << (num_entries / blockCount)
                << std::endl;
        std::cout << "Avg Max Count:" << (maxKeyCount / blockCount)
                << std::endl;
    }
    void printNumLevels() {
        std::cout << "Level Count:" << numLevels << std::endl;
    }
    long size() {
        return total_size;
    }

};

#endif
