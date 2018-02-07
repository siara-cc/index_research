#ifndef BASIX_H
#define BASIX_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree.h"

using namespace std;

#define BX_INT64MAP 1
#define BX_9_BIT_PTR 1
#define BASIX_NODE_SIZE 512

#if BX_9_BIT_PTR == 1
#define BLK_HDR_SIZE 13
#define BITMAP_POS 5
#else
#define BLK_HDR_SIZE 5
#endif

class basix_node_handler : public bplus_tree_node_handler {
private:
    int16_t binarySearch(const char *key, int16_t key_len);
public:
    int16_t pos;
#if BX_INT64MAP == 1
    uint64_t *bitmap;
#else
    uint32_t *bitmap1;
    uint32_t *bitmap2;
#endif
    basix_node_handler(byte *m);
    void initBuf();
    void initVars();
    inline void setBuf(byte *m);
    int16_t filledUpto();
    inline void setFilledUpto(int16_t filledUpto);
    bool isFull(int16_t kv_lens);
    void addData();
    byte *getKey(int16_t pos, int16_t *plen);
    byte *split(byte *first_key, int16_t *first_len_ptr);
    inline int16_t getPtr(int16_t pos);
    inline void setPtr(int16_t pos, int16_t ptr);
    inline void insBit(uint32_t *ui32, int pos, int16_t kv_pos);
    inline void insBit(uint64_t *ui64, int pos, int16_t kv_pos);
    void insPtr(int16_t pos, int16_t kvIdx);
    void traverseToLeaf(byte *node_paths[] = null);
    int16_t locate();
    void insertCurrent();
    inline char *getValueAt(int16_t *vlen);
    inline byte *getChildPtr(byte *ptr);
};

class basix : public bplus_tree {
private:
    void recursiveUpdate(basix_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    static int count1, count2;
    basix();
    ~basix();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
};

#endif
