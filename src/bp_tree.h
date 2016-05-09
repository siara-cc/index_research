#ifndef BP_TREE_H
#define BP_TREE_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"
#include "GenTree.h"

using namespace std;

class bplus_tree_node_handler {
public:
    byte *buf;
    bool to_locate_again_after_split;
    virtual bplus_tree_node_handler(byte *m) {
        buf = m;
        to_locate_again_after_split = 0;
    }
    virtual ~bplus_tree_node_handler() {
    }
    byte *buf;
    char *key;
    char *key_len;
    char *key_at;
    char *key_at_len;
    int lastSearchPos;
    int level;
    int16_t nodePos[10];
    byte *node_paths[10];
    void setBuf(byte *m);
    virtual void init() = 0;
    virtual bool isLeaf() = 0;
    virtual void setLeaf(char isLeaf) = 0;
    virtual void setFilledUpto(int16_t filledUpto) = 0;
    virtual void setKVLastPos(int16_t val) = 0;
    virtual int16_t getKVLastPos() = 0;
    virtual int16_t filledUpto() = 0;
    virtual bool isFull(int16_t kv_len) = 0;
    virtual int16_t locate() = 0;
    virtual void addData() = 0;
    virtual byte *getChild(int16_t pos) = 0;
    virtual byte *getKey(int16_t pos, int16_t *plen) = 0;
    virtual byte *getData(int16_t pos, int16_t *plen) = 0;
    virtual byte *split(int16_t *pbrk_idx) = 0;
    virtual bplus_tree_node_handler *newNode() = 0;
    void set_locate_option(bool option) {
        to_locate_again_after_split = option;
    }
};

class bplus_tree {
private:
    byte *recursiveSearchForGet(const char *key, int16_t key_len,
            int16_t *pIdx) {
        int16_t level = 0;
        int pos = -1;
        while (!node.isLeaf()) {
            pos = node.locate(key, key_len, level);
            if (pos < 0) {
                pos = ~pos;
            } else {
                do {
                    node_data = node.getChild(pos);
                    node.setBuf(node_data);
                    level++;
                    pos = 0;
                } while (!node.isLeaf());
                *pIdx = pos;
                return node_data;
            }
            node_data = node.getChild(pos);
            node.setBuf(node_data);
            level++;
        }
        pos = node.locate(key, key_len, level);
        *pIdx = pos;
        return node_data;
    }
    byte *recursiveSearch(bp_tree_node_handler *node, int16_t *pIdx) {
        int16_t level = 0;
        while (!node.isLeaf()) {
            lastSearchPos[level] = node.locate(key, key_len, level);
            node_paths[level] = node_data;
            if (lastSearchPos[level] < 0) {
                lastSearchPos[level] = ~lastSearchPos[level];
            } else {
                do {
                    node_data = node.getChild(lastSearchPos[level]);
                    node.setBuf(node_data);
                    level++;
                    node_paths[level] = node.buf;
                    lastSearchPos[level] = 0;
                } while (!node.isLeaf());
                *pIdx = lastSearchPos[level];
                return node_data;
            }
            node_data = node.getChild(lastSearchPos[level]);
            node.setBuf(node_data);
            level++;
        }
        node_paths[level] = node_data;
        lastSearchPos[level] = node.locate(key, key_len, level);
        *pIdx = lastSearchPos[level];
        return node_data;
    }
    void recursiveUpdate() {
        linex_node_handler node(foundNode);
        int16_t idx = pos; // lastSearchPos[level];
        if (idx < 0) {
            idx = ~idx;
            if (node.isFull(key_len + value_len)) {
                //std::cout << "Full\n" << std::endl;
                //if (maxKeyCount < block->filledSize())
                //    maxKeyCount = block->filledSize();
                //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
                maxKeyCount += node.filledUpto();
                int16_t brk_idx;
                byte *b = node.split(&brk_idx);
                linex_node_handler new_block(b);
                blockCount++;
                bool isRoot = false;
                if (root->buf == node.buf)
                    isRoot = true;
                int16_t first_key_len = key_len;
                const char *new_block_first_key = key;
                if (idx > brk_idx) {
                    node.setBuf(new_block.buf);
                    idx -= brk_idx;
                    idx--;
                    if (idx > 0) {
                        new_block_first_key = (char *) new_block.getKey(0,
                                &first_key_len);
                    }
                } else {
                    new_block_first_key = (char *) new_block.getKey(0,
                            &first_key_len);
                }
                if (isRoot) {
                    byte *buf = util::alignedAlloc(LINEX_NODE_SIZE);
                    root->setBuf(buf);
                    root->init();
                    blockCount++;
                    char addr[5];
                    util::ptrToFourBytes((unsigned long) foundNode, addr);
                    root->addData(0, "", 1, addr, sizeof(char *));
                    util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                    root->addData(1, new_block_first_key, first_key_len, addr,
                            sizeof(char *));
                    root->setLeaf(false);
                    numLevels++;
                } else {
                    int16_t prev_level = level - 1;
                    byte *parent = node_paths[prev_level];
                    char addr[5];
                    util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                    recursiveUpdate(new_block_first_key, first_key_len, parent,
                            ~(lastSearchPos[prev_level] + 1), addr, sizeof(char *),
                            lastSearchPos, node_paths, prev_level);
                }
            }
            node.addData(idx, key, key_len, value, value_len);
        } else {
            //if (node->isLeaf) {
            //    int16_t vIdx = idx + mSizeBy2;
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
    bplus_tree_node_handler *root;
    virtual ~bplus_tree() {
    }
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len) {
        int16_t lastSearchPos[numLevels];
        byte *block_paths[numLevels];
        if (root->filledUpto() == -1) {
            root->addData(0, key, key_len, value, value_len);
            total_size++;
        } else {
            int16_t pos = -1;
            byte *foundNode = recursiveSearch(key, key_len, root->buf,
                    lastSearchPos, block_paths, &pos);
            recursiveUpdate(key, key_len, foundNode, pos, value, value_len,
                    lastSearchPos, block_paths, numLevels - 1);
        }
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
