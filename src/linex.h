#ifndef LINEX_H
#define LINEX_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"
#include "GenTree.h"

using namespace std;

#define LINEX_NODE_SIZE 512
#define BLK_HDR_SIZE 5

class linex_node {
private:
    int16_t binarySearchLeaf(const char *key, int16_t key_len);
    int16_t binarySearchNode(const char *key, int16_t key_len);
public:
    byte *buf;
    linex_node(byte *m);
    inline void setBuf(byte *m);
    inline void init();
    inline bool isLeaf();
    inline void setLeaf(char isLeaf);
    inline void setFilledUpto(int16_t filledUpto);
    inline void setKVLastPos(int16_t val);
    inline int16_t getKVLastPos();
    int16_t filledUpto();
    bool isFull(int16_t kv_len);
    int16_t locate(const char *key, int16_t key_len, int16_t level);
    void addData(int16_t idx, const char *key, int16_t key_len,
            const char *value, int16_t value_len);
    byte *getChild(int16_t pos);
    byte *getKey(int16_t pos, int16_t *plen);
    byte *getData(int16_t pos, int16_t *plen);
    byte *split(int16_t *pbrk_idx);
};

class linex {
private:
    long total_size;
    int numLevels;
    int maxKeyCount;
    int blockCount;
    byte *recursiveSearch(const char *key, int16_t key_len, byte *node_data,
            int16_t lastSearchPos[], byte *node_paths[], int16_t *pIdx);
    byte *recursiveSearchForGet(const char *key, int16_t key_len,
            int16_t *pIdx);
    void recursiveUpdate(const char *key, int16_t key_len, byte *foundNode,
            int16_t pos, const char *value, int16_t value_len,
            int16_t lastSearchPos[], byte *node_paths[], int16_t level);
public:
    linex_node *root;
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
