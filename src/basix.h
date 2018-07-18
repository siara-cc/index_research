#ifndef BASIX_H
#define BASIX_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree.h"

using namespace std;

#if BPT_9_BIT_PTR == 1
#define BASIX_NODE_SIZE 512
#define BLK_HDR_SIZE 14
#define BITMAP_POS 6
#else
#define BASIX_NODE_SIZE 768
#define BLK_HDR_SIZE 6
#endif

class basix_node_handler : public bplus_tree_node_handler {
private:
    inline int16_t binarySearch(const char *key, int16_t key_len);
public:
    int16_t pos;
    basix_node_handler(byte *m);
    void initBuf();
    void initVars();
    inline void setBuf(byte *m);
    bool isFull(int16_t kv_lens);
    void addData();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    int16_t traverseToLeaf(byte *node_paths[] = null);
    int16_t locate();
    void insertCurrent();
    inline byte *getPtrPos();
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
