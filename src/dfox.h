#ifndef dfox_H
#define dfox_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"

using namespace std;

typedef unsigned char byte;
#define BLK_SIZE 512
#define IDX_BLK_SIZE 64
#define IDX_HDR_SIZE 8
#define MAX_DATA_LEN 127

#define INSERT_MIDDLE1 1
#define INSERT_MIDDLE2 2
#define INSERT_THREAD 3
#define INSERT_LEAF1 4
#define INSERT_LEAF2 5

#define IS_LEAF_BYTE buf[3]
#define DATA_PTR_HIGH_BITS buf
#define TRIE_LEN buf[4]
#define FILLED_SIZE buf[5]
#define LAST_DATA_PTR buf + 6
#define TRIE_PTR_AREA_SIZE 56

class dfox_var {
public:
    byte tc;
    byte mask;
    byte msb5;
    byte csPos;
    byte leaves;
    byte triePos;
    byte origPos;
    byte lastLevel;
    byte need_count;
    byte insertState;
    int lastSearchPos;
    const char *key;
    int key_len;
    char *key_at;
    int key_at_len;
    dfox_var() { init(); };
    void init();
};

class dfox_block {
private:
    byte *trie;
    static byte left_mask[8];
    static byte ryte_mask[8];
    static byte ryte_incl_mask[8];
    inline void insAt(byte pos, byte b);
    inline void setAt(byte pos, byte b);
    inline void append(byte b);
    inline byte getAt(byte pos);
    byte recurseSkip(dfox_var *v);
public:
    byte buf[BLK_SIZE];
    dfox_block();
    bool isLeaf();
    void setLeaf(char isLeaf);
    bool isFull(int kv_len, dfox_var *v);
    int filledSize();
    void setFilledSize(int filledSize);
    void addData(int idx, const char *key, int key_len, const char *value,
            int value_len, dfox_var *v);
    int getPtr(int pos);
    void insPtr(int pos, int kvIdx);
    dfox_block *getChild(int pos);
    byte *getKey(int pos, int *plen);
    byte *getData(int pos, int *plen);
    void setKVLastPos(int val);
    int getKVLastPos();
    dfox_block *split(int *pbrk_idx);
    int locateInTrie(const char *key, int key_len, dfox_var *v);
    bool recurseTrie(int level, dfox_var *v);
    void insertCurrent(dfox_var *v);
};

class dfox {
private:
    dfox_block *root;
    long total_size;
    int numLevels;
    int maxKeyCount;
    dfox_block *recursiveSearch(const char *key, int key_len, dfox_block *node,
            int lastSearchPos[], dfox_block *block_paths[], int *pIdx, dfox_var *v);
    void recursiveUpdate(dfox_block *foundNode, int pos, const char *key,
            int key_len, const char *value, int value_len, int lastSearchPos[],
            dfox_block *block_paths[], int level, dfox_var *v);
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
