#ifndef LINEX_H
#define LINEX_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "GenTree.h"
#include "bplus_tree.h"

using namespace std;

#define LINEX_NODE_SIZE 768

#define LX_BLK_HDR_SIZE 6
#define LX_PREFIX_CODING 1
#define LX_DATA_AREA 0

class linex_node_handler : public bplus_tree_node_handler {
private:
    int16_t linearSearch();
public:
    int16_t pos;
    byte *prev_key_at;
    byte prefix[60];
    int16_t prefix_len;
    int16_t prev_prefix_len;
    linex_node_handler(byte *m);
    void initBuf();
    void initVars();
    inline void setBuf(byte *m);
    bool isFull(int16_t kv_lens);
    void addData();
    byte *getKey(int16_t pos, int16_t *plen);
    byte *split(byte *first_key, int16_t *first_len_ptr);
    int16_t traverseToLeaf(byte *node_paths[] = null);
    int16_t locate();
    void insertCurrent();
    inline char *getValueAt(int16_t *vlen);
    inline byte *getChildPtr(byte *ptr);
};

class linex : public bplus_tree {
private:
    void recursiveUpdate(linex_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    static long count1, count2;
    linex();
    ~linex();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
};

#endif
