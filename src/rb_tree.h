#ifndef RB_TREE_H
#define RB_TREE_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "GenTree.h"
#include "bplus_tree.h"

using namespace std;

#define RB_TREE_NODE_SIZE 512

#if RB_TREE_NODE_SIZE == 512
#define RB_TREE_HDR_SIZE 6
#define DATA_END_POS 2
#define ROOT_NODE_POS 4
#else
#define RB_TREE_HDR_SIZE 7
#define DATA_END_POS 3
#define ROOT_NODE_POS 5
#endif

#define RB_RED 0
#define RB_BLACK 1

#if RB_TREE_NODE_SIZE == 512
#define RBT_BITMAP_POS 0
#define LEFT_PTR_POS 1
#define RYTE_PTR_POS 2
#define PARENT_PTR_POS 3
#define KEY_LEN_POS 4
#else
#define COLOR_POS 0
#define LEFT_PTR_POS 1
#define RYTE_PTR_POS 3
#define PARENT_PTR_POS 5
#define KEY_LEN_POS 7
#endif

class rb_tree_node_handler : public bplus_tree_node_handler {
private:
    int16_t binarySearch(const char *key, int16_t key_len);
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
    int16_t pos;
    byte last_direction;
    rb_tree_node_handler(byte *m);
    inline void setBuf(byte *m);
    inline void initBuf();
    inline int16_t getDataEndPos();
    inline void setDataEndPos(int16_t pos);
    int16_t filledUpto();
    void setFilledUpto(int16_t filledUpto);
    inline int16_t getPtr(int16_t pos);
    inline void setPtr(int16_t pos, int16_t ptr);
    bool isFull(int16_t kv_len);
    int16_t traverseToLeaf(byte *node_paths[] = null);
    int16_t locate();
    void addData();
    byte *getKey(int16_t pos, int16_t *plen);
    byte *getFirstKey(int16_t *plen);
    int16_t getFirst();
    int16_t getNext(int16_t n);
    int16_t getPrevious(int16_t n);
    byte *split(byte *first_key, int16_t *first_len_ptr);
    void initVars();
    inline char *getValueAt(int16_t *vlen);
    inline byte *getChildPtr(int16_t pos);
};

class rb_tree : public bplus_tree {
private:
    void recursiveUpdate(rb_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    rb_tree_node_handler *root;
    static int count1, count2;
    rb_tree();
    ~rb_tree();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
};


#endif
