#ifndef LINEX_H
#define LINEX_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"
#include "GenTree.h"

using namespace std;

#define BLK_SIZE 512
#define BLK_HDR_SIZE 5
#define MAX_DATA_LEN 127

class linex_block {
private:
    int binarySearchLeaf(const char *key, int key_len);
    int binarySearchNode(const char *key, int key_len);
public:
    byte buf[BLK_SIZE];
    linex_block();
    bool isLeaf();
    void setLeaf(char isLeaf);
    bool isFull(int kv_len);
    int filledSize();
    void setFilledSize(char filledSize);
    int binarySearch(const char *key, int key_len);
    void addData(int idx, const char *key, int key_len, const char *value,
            int value_len);
    linex_block *getChild(int pos);
    byte *getKey(int pos, int *plen);
    byte *getData(int pos, int *plen);
    void setKVLastPos(int val);
    int getKVLastPos();
};

class linex {
private:
    linex_block *root;
    long total_size;
    int numLevels;
    int maxKeyCount;
    linex_block *recursiveSearch(const char *key, int key_len,
            linex_block *node, int lastSearchPos[], linex_block *block_paths[],
            int *pIdx);
    void recursiveUpdate(linex_block *foundNode, int pos, const char *key,
            int key_len, const char *value, int value_len, int lastSearchPos[],
            linex_block *block_paths[], int level);
public:
    linex();
    long size();
    char *get(const char *key, int key_len, int *pValueLen);
    void put(const char *key, int key_len, const char *value, int value_len);
    void printMaxKeyCount() { std::cout << "Max Key Count:" << maxKeyCount << std::endl; }
    void printNumLevels() { std::cout << "Level Count:" << numLevels << std::endl; }
};

#endif
