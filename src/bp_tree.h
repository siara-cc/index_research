#ifndef BP_TREE_H
#define BP_TREE_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"
#include "GenTree.h"

using namespace std;

#define MAX_DATA_LEN 127

class bplus_tree_var {
public:
    const char *key;
    int key_len;
    int lastSearchPos;
    bplus_tree_var() {
        init();
    }
    virtual ~bplus_tree_var() {
    }
    virtual void init() {
        lastSearchPos = -1;
    }
};

class bplus_tree_node {
public:
    byte *buf;
    bool to_locate_again_after_split;
    bplus_tree_node() {
        to_locate_again_after_split = 0;
    }
    virtual ~bplus_tree_node() {
    }
    virtual bool isLeaf() = 0;
    virtual void setLeaf(char isLeaf) = 0;
    virtual bool isFull(int kv_len, bplus_tree_var *v) = 0;
    virtual int filledSize() = 0;
    virtual void setFilledSize(int filledSize) = 0;
    virtual void addData(int idx, const char *value, int value_len,
            bplus_tree_var *v) = 0;
    virtual bplus_tree_node *getChild(int pos) = 0;
    virtual byte *getKey(int pos, int *plen) = 0;
    virtual byte *getData(int pos, int *plen) = 0;
    virtual bplus_tree_node *split(int *pbrk_idx) = 0;
    virtual int locate(bplus_tree_var *v) = 0;
    void set_locate_option(bool option) {
        to_locate_again_after_split = option;
    }
};

class bplus_tree {
private:
    virtual bplus_tree_node *newNode() = 0;
    virtual bplus_tree_var *newVar() = 0;
    bplus_tree_node *recursiveSearch(bplus_tree_node *node, int lastSearchPos[],
            bplus_tree_node *node_paths[], int *pIdx, bplus_tree_var *v) {
        int level = 0;
        while (!node->isLeaf()) {
            lastSearchPos[level] = node->locate(v);
            node_paths[level] = node;
            if (lastSearchPos[level] < 0) {
                lastSearchPos[level] = ~lastSearchPos[level];
                lastSearchPos[level]--;
                if (lastSearchPos[level] >= node->filledSize()) {
                    lastSearchPos[level] -= node->filledSize();
                    do {
                        node = node->getChild(lastSearchPos[level]);
                        level++;
                        node_paths[level] = node;
                        lastSearchPos[level] = 0;
                    } while (!node->isLeaf());
                    *pIdx = lastSearchPos[level];
                    return node;
                }
            }
            node = node->getChild(lastSearchPos[level]);
            v->init();
            level++;
        }
        node_paths[level] = node;
        *pIdx = node->locate(v);
        lastSearchPos[level] = *pIdx; // required?
        return node;
    }

    void recursiveUpdate(bplus_tree_node *foundNode, int pos, const char *value,
            int value_len, int lastSearchPos[], bplus_tree_node *node_paths[],
            int level, bplus_tree_var *v) {
        int idx = pos; // lastSearchPos[level];
        if (idx < 0) {
            idx = ~idx;
            if (foundNode->isFull(v->key_len + value_len, v)) {
                //std::cout << "Full\n" << std::endl;
                //if (maxKeyCount < block->filledSize())
                //    maxKeyCount = block->filledSize();
                //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
                maxKeyCount += foundNode->filledSize();
                int brk_idx;
                bplus_tree_node *new_block = foundNode->split(&brk_idx);
                blockCount++;
                bplus_tree_var *rv = newVar();
                if (root == foundNode) {
                    root = newNode();
                    blockCount++;
                    int first_len;
                    char *first_key;
                    char addr[5];
                    util::ptrToFourBytes((unsigned long) foundNode, addr);
                    rv->key = "";
                    rv->key_len = 1;
                    root->addData(0, addr, sizeof(char *), rv);
                    first_key = (char *) new_block->getKey(0, &first_len);
                    rv->key = first_key;
                    rv->key_len = first_len;
                    util::ptrToFourBytes((unsigned long) new_block, addr);
                    rv->init();
                    if (root->to_locate_again_after_split)
                        root->locate(rv);
                    root->addData(1, addr, sizeof(char *), rv);
                    root->setLeaf(false);
                    numLevels++;
                } else {
                    int prev_level = level - 1;
                    bplus_tree_node *parent = node_paths[prev_level];
                    rv->key = (char *) new_block->getKey(0, &rv->key_len);
                    char addr[5];
                    util::ptrToFourBytes((unsigned long) new_block, addr);
                    if (root->to_locate_again_after_split)
                        parent->locate(rv);
                    recursiveUpdate(parent, ~(lastSearchPos[prev_level] + 1),
                            addr, sizeof(char *), lastSearchPos, node_paths,
                            prev_level, rv);
                }
                brk_idx++;
                if (idx > brk_idx) {
                    foundNode = new_block;
                    idx -= brk_idx;
                }
                if (root->to_locate_again_after_split) {
                    v->init();
                    foundNode->locate(v);
                    idx = ~v->lastSearchPos;
                }
            }
            foundNode->addData(idx, value, value_len, v);
        } else {
            //if (node->isLeaf) {
            //    int vIdx = idx + mSizeBy2;
            //    returnValue = (V) arr[vIdx];
            //    arr[vIdx] = value;
            //}
        }
    }
protected:
    long total_size;
    int numLevels;
    int maxKeyCount;
    int blockCount;
public:
    bplus_tree_node *root;
    virtual ~bplus_tree() {
    }
    virtual char *get(const char *key, int key_len, int *pValueLen) {
        int lastSearchPos[numLevels];
        bplus_tree_node *block_paths[numLevels];
        int pos = -1;
        bplus_tree_var *v = newVar();
        v->key = key;
        v->key_len = key_len;
        bplus_tree_node *foundNode = recursiveSearch(root, lastSearchPos,
                block_paths, &pos, v);
        if (pos < 0)
            return null;
        return (char *) foundNode->getData(pos, pValueLen);
    }
    virtual void put(const char *key, int key_len, const char *value,
            int value_len) {
        // isPut = true;
        int lastSearchPos[numLevels];
        bplus_tree_node *block_paths[numLevels];
        bplus_tree_var *v = newVar();
        v->key = key;
        v->key_len = key_len;
        if (root->filledSize() == 0) {
            root->addData(0, value, value_len, v);
            total_size++;
        } else {
            int pos = 0;
            bplus_tree_node *foundNode = recursiveSearch(root, lastSearchPos,
                    block_paths, &pos, v);
            recursiveUpdate(foundNode, pos, value, value_len, lastSearchPos,
                    block_paths, numLevels - 1, v);
        }
        // isPut = false;
    }

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
