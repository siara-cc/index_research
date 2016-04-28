#ifndef RB_TREE_H
#define RB_TREE_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"
#include "GenTree.h"
#include "bp_tree.h"

using namespace std;

#define RB_TREE_NODE_SIZE 512
#define RB_TREE_HDR_SIZE 7
#define IS_LEAF_POS 0
#define FILLED_SIZE_POS 1
#define DATA_END_POS 3
#define ROOT_NODE_POS 5

#define RB_RED 0
#define RB_BLACK 1

#define COLOR_POS 0
#define LEFT_PTR_POS 1
#define RYTE_PTR_POS 3
#define PARENT_PTR_POS 5
#define KEY_LEN_POS 7

class rb_tree_node: public bplus_tree_node {
private:
    int binarySearchLeaf(const char *key, int key_len);
    int binarySearchNode(const char *key, int key_len);
    inline int getLeft(int n);
    inline int getRight(int n);
    inline int getParent(int n);
    int getSibling(int n);
    inline inline int getUncle(int n);
    inline int getGrandParent(int n);
    inline int getRoot();
    inline int getColor(int n);
    inline void setLeft(int n, int l);
    inline void setRight(int n, int r);
    inline void setParent(int n, int p);
    inline void setRoot(int n);
    inline void setColor(int n, byte c);
    int newNode(byte n_color, int left, int right, int parent);
    void rotateLeft(int n);
    void rotateRight(int n);
    void replaceNode(int oldn, int newn);
    void insertCase1(int n);
    void insertCase2(int n);
    void insertCase3(int n);
    void insertCase4(int n);
    void insertCase5(int n);
public:
    rb_tree_node();
    bool isLeaf();
    void setLeaf(char isLeaf);
    bool isFull(int kv_len, bplus_tree_var *v);
    bool isFull(int kv_len, rb_tree_var *v);
    int filledSize();
    void setFilledSize(int filledSize);
    int getDataEndPos();
    void setDataEndPos(int pos);
    int locate(rb_tree_var *v, int level);
    int locate(bplus_tree_var *v, int level);
    void addData(int idx, const char *value, int value_len, rb_tree_var *v);
    void addData(int idx, const char *value, int value_len, bplus_tree_var *v);
    rb_tree_node *getChild(int pos);
    byte *getKey(int pos, int *plen);
    byte *getData(int pos, int *plen);
    rb_tree_node *split(int *pbrk_idx);

};

class rb_tree : public bplus_tree {
private:
    inline rb_tree_node *newNode();
    inline rb_tree_var *newVar();
public:
    rb_tree();
};

#endif
