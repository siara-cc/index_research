#ifndef dfox_H
#define dfox_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"

using namespace std;

typedef unsigned char byte;
#define DFOX_NODE_SIZE 512
#define MAX_PTR_BITMAP_BYTES 6
#define IDX_BLK_SIZE 128
#define IDX_HDR_SIZE (MAX_PTR_BITMAP_BYTES+4)
#define TRIE_PTR_AREA_SIZE (IDX_BLK_SIZE-IDX_HDR_SIZE)
#define MAX_PTRS 46

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
    int16_t key_len;
    const char *key_at;
    int16_t key_at_len;
    int16_t lastSearchPos;
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
    inline void delAt(byte pos, int16_t count);
    byte recurseSkip(dfox_var *v, byte skip_count, byte skip_size);
    byte processTC(dfox_var *v);
    static byte *alignedAlloc();
public:
    byte *buf;
    int16_t threads;
    dfox_node(byte *m);
    void init();
    void setBuf(byte *m);
    bool isFull(int16_t kv_len, dfox_var *v);
    inline bool isLeaf();
    inline void setLeaf(char isLeaf);
    int16_t filledSize();
    inline void setFilledSize(int16_t filledSize);
    inline int16_t getKVLastPos();
    inline void setKVLastPos(int16_t val);
    void addData(int16_t idx, const char *value, int16_t value_len, dfox_var *v);
    byte *getChild(int16_t pos);
    byte *getKey(int16_t pos, int16_t *plen);
    byte *getData(int16_t pos, int16_t *plen);
    byte *split(int16_t *pbrk_idx);
    int16_t getPtr(int16_t pos);
    void insPtr(int16_t pos, int16_t kvIdx);
    int16_t locate(const char *key, int16_t key_len, int16_t level, dfox_var *v);
    bool recurseTrie(int16_t level, dfox_var *v);
    byte recurseEntireTrie(int16_t level, dfox_var *v, long idx_list[],
            int16_t *pidx_len);
    void insertCurrent(dfox_var *v);
};

class dfox {
private:
    long total_size;
    int numLevels;
    int maxKeyCount;
    int blockCount;
    byte *recursiveSearch(const char *key, int16_t key_len, byte *node_data,
            int16_t lastSearchPos[], byte *node_paths[], int16_t *pIdx, dfox_var *v);
    byte *recursiveSearchForGet(const char *key, int16_t key_len, int16_t *pIdx);
    void recursiveUpdate(const char *key, int16_t key_len, byte *foundNode, int16_t pos,
            const char *value, int16_t value_len, int16_t lastSearchPos[],
            byte *node_paths[], int16_t level, dfox_var *v);
public:
    dfox_node *root;
    int maxThread;
    dfox();
    ~dfox();
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
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
