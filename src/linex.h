#ifndef LINEX_H
#define LINEX_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"
#include "GenTree.h"

using namespace std;

#define LX_9_BIT_PTR 0
#define LINEX_NODE_SIZE 512

#ifdef LX_9_BIT_PTR
#define BLK_HDR_SIZE 13
#define BITMAP_POS 5
#else
#define BLK_HDR_SIZE 5
#endif

//#define LX_INT64MAP 1

class linex_node_handler {
private:
    int16_t binarySearch(const char *key, int16_t key_len);
public:
    byte *buf;
#if defined(LX_INT64MAP)
    uint64_t *bitmap;
#else
    uint32_t *bitmap1;
    uint32_t *bitmap2;
#endif
    byte isPut;
    const char *key;
    int16_t key_len;
    const char *key_at;
    int16_t key_at_len;
    const char *value;
    int16_t value_len;
    linex_node_handler(byte *m);
    void initBuf();
    void initVars();
    inline void setBuf(byte *m);
    bool isFull(int16_t kv_lens);
    inline bool isLeaf();
    inline void setLeaf(char isLeaf);
    inline int16_t filledUpto();
    inline void setFilledUpto(int16_t filledUpto);
    inline int16_t getKVLastPos();
    inline void setKVLastPos(int16_t val);
    void addData(int16_t pos);
    byte *getChild(int16_t pos);
    byte *getKey(int16_t pos, int16_t *plen);
    byte *getData(int16_t pos, int16_t *plen);
    byte *split(int16_t *pbrk_idx);
    inline int16_t getPtr(int16_t pos);
    inline void setPtr(int16_t pos, int16_t ptr);
    inline void insBit(uint32_t *ui32, int pos, int16_t kv_pos);
    inline void insBit(uint64_t *ui64, int pos, int16_t kv_pos);
    void insPtr(int16_t pos, int16_t kvIdx);
    int16_t locate(int16_t level);
    void insertCurrent();
};

class linex {
private:
    long total_size;
    int numLevels;
    int maxKeyCount;
    int blockCount;
    void recursiveSearch(linex_node_handler *node, int16_t lastSearchPos[],
            byte *node_paths[], int16_t *pIdx);
    void recursiveSearchForGet(linex_node_handler *node, int16_t *pIdx);
    void recursiveUpdate(linex_node_handler *node, int16_t pos, int16_t lastSearchPos[],
            byte *node_paths[], int16_t level);
public:
    byte *root_data;
    linex();
    ~linex();
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
