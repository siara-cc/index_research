#ifndef dfox_H
#define dfox_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"

using namespace std;

typedef unsigned char byte;
#define DFOX_NODE_SIZE 512
#define MAX_PTR_BITMAP_BYTES 8
#define IDX_HDR_SIZE (MAX_PTR_BITMAP_BYTES+5)
#define TRIE_PTR_AREA_SIZE (IDX_BLK_SIZE-IDX_HDR_SIZE)
#define MAX_PTRS 63

#define INSERT_MIDDLE1 1
#define INSERT_MIDDLE2 2
#define INSERT_THREAD 3
#define INSERT_LEAF 4

#define DATA_PTR_HIGH_BITS buf
#define IS_LEAF_BYTE buf[MAX_PTR_BITMAP_BYTES]
#define TRIE_LEN buf[MAX_PTR_BITMAP_BYTES+1]
#define FILLED_SIZE buf[MAX_PTR_BITMAP_BYTES+2]
#define LAST_DATA_PTR buf + MAX_PTR_BITMAP_BYTES + 3

class dfox_node_handler {
private:
    const static byte eight = 0x08;
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
    inline void insBit(uint32_t *ui32, int pos, int16_t kv_pos);
    static byte *alignedAlloc();
public:
    byte *buf;
    byte *trie;
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
    int16_t key_at_pos;
    const char *value;
    int16_t value_len;
    uint32_t *bitmap1;
    uint32_t *bitmap2;
    byte skip_list[20];
    dfox_node_handler(byte *m);
    void initBuf();
    void initVars();
    void setBuf(byte *m);
    bool isFull(int16_t kv_lens);
    inline bool isLeaf();
    inline void setLeaf(char isLeaf);
    int16_t filledSize();
    inline void setFilledSize(int16_t filledSize);
    inline int16_t getKVLastPos();
    inline void setKVLastPos(int16_t val);
    void addData(int16_t pos);
    byte *getChild(int16_t pos);
    inline byte *getKey(int16_t pos, int16_t *plen);
    byte *getData(int16_t pos, int16_t *plen);
    byte *split(int16_t *pbrk_idx);
    inline int16_t getPtr(int16_t pos);
    inline void setPtr(int16_t pos, int16_t ptr);
    void insPtr(int16_t pos, int16_t kvIdx);
    int16_t locate(int16_t level);
    void insertCurrent();
};

class dfox {
private:
    long total_size;
    int numLevels;
    int maxKeyCount;
    int blockCount;
    void recursiveSearch(dfox_node_handler *node, int16_t lastSearchPos[],
            byte *node_paths[], int16_t *pIdx);
    void recursiveSearchForGet(dfox_node_handler *node, int16_t *pIdx);
    void recursiveUpdate(dfox_node_handler *node, int16_t pos, int16_t lastSearchPos[],
            byte *node_paths[], int16_t level);
public:
    byte *root_data;
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
    static uint32_t left_mask32[32];
    static uint32_t ryte_mask32[32];
    static uint32_t mask32[32];

};

#endif
