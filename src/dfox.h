#ifndef dfox_H
#define dfox_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"

using namespace std;

typedef unsigned char byte;
#define BLK_SIZE 512
#define BLK_HDR_SIZE 5
#define MAX_DATA_LEN 127

#define INSERT_MIDDLE1 1
#define INSERT_MIDDLE2 2
#define INSERT_THREAD 3
#define INSERT_LEAF1 4
#define INSERT_LEAF2 5

class dfox_block {
private:
    byte tc;
    byte mask;
    byte msb5;
    byte leaves;
    byte origPos;
    byte lastLevel;
    byte insertState;
    byte csPos;
    byte triePos;
    static byte left_mask[] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
    static byte ryte_mask[] = { 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00 };
    void insAt(byte pos, byte b);
    void setAt(byte pos, byte b);
    byte getAt(byte pos);
    byte getLen();
    byte recurseSkip();
public:
    byte buf[BLK_SIZE];
    dfox_block();
    bool isLeaf();
    void setLeaf(char isLeaf);
    bool isFull(int kv_len);
    int filledSize();
    void setFilledSize(char filledSize);
    void addData(int idx, const char *key, int key_len, const char *value,
            int value_len);
    dfox_block *getChild(int pos);
    byte *getKey(int pos, int *plen);
    byte *getData(int pos, int *plen);
    void setKVLastPos(int val);
    int getKVLastPos();
    void locateInTrie();
    bool recurseTrie(int level);
    void insertCurrent();
};

class dfox {
private:
    dfox_block *root;
    long total_size;
    int numLevels;
    int maxKeyCount;
    dfox_block *recursiveSearch(const char *key, int key_len, dfox_block *node,
            int lastSearchPos[], dfox_block *block_paths[], int *pIdx);
    void recursiveUpdate(dfox_block *foundNode, int pos, const char *key,
            int key_len, const char *value, int value_len, int lastSearchPos[],
            dfox_block *block_paths[], int level);
public:
    dfox();
    long size();
    char *get(const char *key, int key_len, int *pValueLen);
    void put(const char *key, int key_len, const char *value, int value_len);
    void printMaxKeyCount() {
        std::cout << "Max Key Count:" << maxKeyCount << std::endl;
    }
    void printNumLevels() {
        std::cout << "Level Count:" << numLevels << std::endl;
    }
};

#endif
