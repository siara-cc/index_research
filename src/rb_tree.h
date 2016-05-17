#ifndef RB_TREE_H
#define RB_TREE_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"
#include "GenTree.h"

using namespace std;

#define RB_TREE_NODE_SIZE 32767
#define RB_TREE_HDR_SIZE 7
#define IS_LEAF_POS 0
#define FILLED_UPTO_POS 1
#define DATA_END_POS 3
#define ROOT_NODE_POS 5

#define RB_RED 0
#define RB_BLACK 1

#define COLOR_POS 0
#define LEFT_PTR_POS 1
#define RYTE_PTR_POS 3
#define PARENT_PTR_POS 5
#define KEY_LEN_POS 7

class rb_tree_node_handler {
private:
    int16_t binarySearchLeaf(const char *key, int16_t key_len);
    int16_t binarySearchNode(const char *key, int16_t key_len);
    inline int16_t getLeft(int16_t n);
    inline int16_t getRight(int16_t n);
    inline int16_t getParent(int16_t n);
    int16_t getSibling(int16_t n);
    inline int16_t getUncle(int16_t n);
    inline int16_t getGrandParent(int16_t n);
    inline int16_t getRoot();
    inline int16_t getColor(int16_t n);
    inline void setLeft(int16_t n, int16_t l);
    inline void setRight(int16_t n, int16_t r);
    inline void setParent(int16_t n, int16_t p);
    inline void setRoot(int16_t n);
    inline void setColor(int16_t n, byte c);
    void rotateLeft(int16_t n);
    void rotateRight(int16_t n);
    void replaceNode(int16_t oldn, int16_t newn);
    void insertCase1(int16_t n);
    void insertCase2(int16_t n);
    void insertCase3(int16_t n);
    void insertCase4(int16_t n);
    void insertCase5(int16_t n);
public:
    byte *buf;
    byte isPut;
    const char *key;
    int16_t key_len;
    const char *key_at;
    int16_t key_at_len;
    const char *value;
    int16_t value_len;
    byte last_direction;
    int16_t depth;
    rb_tree_node_handler(byte *m);
    inline void setBuf(byte *m);
    inline void initBuf();
    inline bool isLeaf();
    inline void setLeaf(char isLeaf);
    inline void setFilledUpto(int16_t filledUpto);
    inline int16_t getDataEndPos();
    inline void setDataEndPos(int16_t pos);
    int16_t filledUpto();
    bool isFull(int16_t kv_len);
    int16_t locate(int16_t level);
    void addData(int16_t idx);
    byte *getChild(int16_t pos);
    byte *getKey(int16_t pos, int16_t *plen);
    byte *getFirstKey(int16_t *plen);
    int16_t getFirst();
    int16_t getNext(int16_t n);
    byte *getData(int16_t pos, int16_t *plen);
    byte *split(int16_t *pbrk_idx);
};

class rb_tree {
private:
    long total_size;
    int numLevels;
    int maxKeyCount;
    int blockCount;
    int maxDepth;
    void recursiveSearch(rb_tree_node_handler *node, int16_t lastSearchPos[],
            byte lastSearchDir[], byte *node_paths[], int16_t *pIdx);
    void recursiveSearchForGet(rb_tree_node_handler *node, int16_t *pIdx);
    void recursiveUpdate(rb_tree_node_handler *node, int16_t pos, int16_t lastSearchPos[],
            byte lastSearchDir[], byte *node_paths[], int16_t level);
public:
    byte *root_data;
    rb_tree();
    ~rb_tree();
    rb_tree_node_handler *root;
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
