#ifndef LINEX_H
#define LINEX_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"
#include "GenTree.h"
#include "bp_tree.h"

using namespace std;

#define LINEX_NODE_SIZE 512
#define BLK_HDR_SIZE 5

class linex_var: public bplus_tree_var {
};

class linex_node: public bplus_tree_node {
private:
    int binarySearchLeaf(const char *key, int key_len);
    int binarySearchNode(const char *key, int key_len);
public:
    linex_node();
    bool isLeaf();
    void setLeaf(char isLeaf);
    bool isFull(int kv_len, bplus_tree_var *v);
    bool isFull(int kv_len, linex_var *v);
    int filledSize();
    void setFilledSize(int filledSize);
    int locate(linex_var *v);
    int locate(bplus_tree_var *v);
    void addData(int idx, const char *value, int value_len, linex_var *v);
    void addData(int idx, const char *value, int value_len, bplus_tree_var *v);
    linex_node *getChild(int pos);
    byte *getKey(int pos, int *plen);
    byte *getData(int pos, int *plen);
    void setKVLastPos(int val);
    int getKVLastPos();
    linex_node *split(int *pbrk_idx);
};

class linex : public bplus_tree {
private:
    linex_node *newNode();
    linex_var *newVar();
public:
    linex();
};

#endif
