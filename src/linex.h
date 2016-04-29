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
    int binarySearchLeaf(const char *key, int key_len);
    int binarySearchNode(const char *key, int key_len);
public:
    byte *buf;
    linex_node(byte *m);
    void setBuf(byte *m);
    void init();
    bool isLeaf();
    void setLeaf(char isLeaf);
    bool isFull(int kv_len);
    int filledSize();
    void setFilledSize(int filledSize);
    int locate(const char *key, int key_len, int level);
    void addData(int idx, const char *key, int key_len, const char *value,
            int value_len);
    byte *getChild(int pos);
    byte *getKey(int pos, int *plen);
    byte *getData(int pos, int *plen);
    void setKVLastPos(int val);
    int getKVLastPos();
    byte *split(int *pbrk_idx);
};

class linex {
private:
    long total_size;
    int numLevels;
    int maxKeyCount;
    int blockCount;
    byte *recursiveSearch(const char *key, int key_len, byte *node_data,
            int lastSearchPos[], byte *node_paths[], int *pIdx);
    void recursiveUpdate(const char *key, int key_len, byte *foundNode, int pos,
            const char *value, int value_len, int lastSearchPos[],
            byte *node_paths[], int level);
public:
    linex_node *root;
    int maxThread;
    linex();
    ~linex();
    char *get(const char *key, int key_len, int *pValueLen);
    void put(const char *key, int key_len, const char *value, int value_len);
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
