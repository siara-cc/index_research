#ifndef RB_TREE_H
#define RB_TREE_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"
#include "GenTree.h"

using namespace std;

#define RB_TREE_NODE_SIZE 512
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

class rb_tree_node {
private:
    int16_t binarySearchLeaf(const char *key, int16_t key_len);
    int16_t binarySearchNode(const char *key, int16_t key_len);
    inline int16_t getLeft(int16_t n);
    inline int16_t getRight(int16_t n);
    inline int16_t getParent(int16_t n);
    int16_t getSibling(int16_t n);
    inline inline int16_t getUncle(int16_t n);
    inline int16_t getGrandParent(int16_t n);
    inline int16_t getRoot();
    inline int16_t getColor(int16_t n);
    inline void setLeft(int16_t n, int16_t l);
    inline void setRight(int16_t n, int16_t r);
    inline void setParent(int16_t n, int16_t p);
    inline void setRoot(int16_t n);
    inline void setColor(int16_t n, byte c);
    int16_t newNode(byte n_color, int16_t left, int16_t right, int16_t parent);
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
    rb_tree_node(byte *m);
    inline void setBuf(byte *m);
    inline void init();
    inline bool isLeaf();
    inline void setLeaf(char isLeaf);
    inline void setFilledUpto(int16_t filledUpto);
    inline int16_t getDataEndPos();
    inline void setDataEndPos(int16_t pos);
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

class rb_tree {
private:
    long total_size;
    int16_t numLevels;
    int16_t maxKeyCount;
    int16_t blockCount;
    byte *recursiveSearch(const char *key, int16_t key_len, byte *node_data,
            int16_t lastSearchPos[], byte *node_paths[], int16_t *pIdx);
    byte *recursiveSearchForGet(const char *key, int16_t key_len,
            int16_t *pIdx);
    void recursiveUpdate(const char *key, int16_t key_len, byte *foundNode,
            int16_t pos, const char *value, int16_t value_len,
            int16_t lastSearchPos[], byte *node_paths[], int16_t level);
public:
    rb_tree_node *root;
    rb_tree();
    ~rb_tree();
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
