#ifndef BP_TREE_H
#define BP_TREE_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"
#include "GenTree.h"

using namespace std;

#define BPT_IS_LEAF_BYTE buf[0]
#define BPT_FILLED_SIZE buf + 1
#define BPT_LAST_DATA_PTR buf + 3

class bplus_tree_node_handler {
public:
    byte *buf;
    const char *key;
    int16_t key_len;
    byte *key_at;
    int16_t key_at_len;
    const char *value;
    int16_t value_len;
    bool isPut;
    virtual ~bplus_tree_node_handler() {
    }
    virtual void setBuf(byte *m) = 0;
    virtual void initBuf() = 0;
    virtual void initVars() = 0;
    inline bool isLeaf() {
        return BPT_IS_LEAF_BYTE;
    }

    inline void setLeaf(char isLeaf) {
        BPT_IS_LEAF_BYTE = isLeaf;
    }

    inline void setFilledSize(int16_t filledSize) {
        util::setInt(BPT_FILLED_SIZE, filledSize);
    }

    inline int16_t filledSize() {
        return util::getInt(BPT_FILLED_SIZE);
    }

    inline void setKVLastPos(int16_t val) {
        util::setInt(BPT_LAST_DATA_PTR, val);
    }

    inline int16_t getKVLastPos() {
        return util::getInt(BPT_LAST_DATA_PTR);
    }

    virtual bool isFull(int16_t kv_len) = 0;
    virtual void addData() = 0;
    virtual char *getValueAt(int16_t *vlen) = 0;
    virtual void traverseToLeaf(byte *node_paths[] = null) = 0;
    virtual int16_t locate() = 0;
    virtual byte *split(byte *first_key, int16_t *first_len_ptr) = 0;
};

class bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
protected:
    long total_size;
    int numLevels;
    int maxKeyCount;
    int blockCount;
    int node_size;

public:
    byte *root_data;
    int maxThread;
    bplus_tree() {
        node_size = 512;
    }
    virtual ~bplus_tree() {
    }
    virtual void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len) = 0;
    virtual char *get(const char *key, int16_t key_len, int16_t *pValueLen) = 0;
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
