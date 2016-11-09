#ifndef BP_TREE_H
#define BP_TREE_H
#ifdef ARDUINO
#include <HardwareSerial.h>
#include <string.h>
#else
#include <cstdio>
#include <cstring>
#include <iostream>
#include <malloc.h>
#endif
#include <stdint.h>
#include "util.h"
#include "GenTree.h"

using namespace std;

#define BPT_IS_LEAF_BYTE buf[0]
#define BPT_FILLED_SIZE buf + 1
#define BPT_LAST_DATA_PTR buf + 3
#define BPT_TRIE_LEN buf[5]

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

class trie_node_handler : public bplus_tree_node_handler {
protected:
    inline void delAt(byte *ptr) {
        BPT_TRIE_LEN--;
        memmove(ptr, ptr + 1, trie + BPT_TRIE_LEN - ptr);
    }

    inline void delAt(byte *ptr, int16_t count) {
        BPT_TRIE_LEN -= count;
        memmove(ptr, ptr + count, trie + BPT_TRIE_LEN - ptr);
    }

    inline void insAt(byte *ptr, byte b) {
        memmove(ptr + 1, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr = b;
        BPT_TRIE_LEN++;
    }

    inline byte insAt(byte *ptr, byte b1, byte b2) {
        memmove(ptr + 2, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr = b2;
        BPT_TRIE_LEN += 2;
        return 2;
    }

    inline byte insAt(byte *ptr, byte b1, byte b2, byte b3) {
        memmove(ptr + 3, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        BPT_TRIE_LEN += 3;
        return 3;
    }

    inline byte insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4) {
        memmove(ptr + 4, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        BPT_TRIE_LEN += 4;
        return 4;
    }

    inline byte insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4,
            byte b5) {
        memmove(ptr + 5, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr++ = b4;
        *ptr = b5;
        BPT_TRIE_LEN += 5;
        return 5;
    }

    inline byte insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4,
            byte b5, byte b6) {
        memmove(ptr + 6, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr++ = b4;
        *ptr++ = b5;
        *ptr = b6;
        BPT_TRIE_LEN += 6;
        return 6;
    }

    inline void setAt(byte pos, byte b) {
        trie[pos] = b;
    }

    inline void append(byte b) {
        trie[BPT_TRIE_LEN++] = b;
    }

    inline void append(byte b1, byte b2) {
        trie[BPT_TRIE_LEN++] = b1;
        trie[BPT_TRIE_LEN++] = b2;
    }

    inline void append(byte b1, byte b2, byte b3) {
        trie[BPT_TRIE_LEN++] = b1;
        trie[BPT_TRIE_LEN++] = b2;
        trie[BPT_TRIE_LEN++] = b3;
    }

    inline void appendPtr(int16_t p) {
        util::setInt(trie + BPT_TRIE_LEN, p);
        BPT_TRIE_LEN += 2;
    }

    inline byte getAt(byte pos) {
        return trie[pos];
    }

public:
    static const byte x00 = 0;
    static const byte x01 = 1;
    static const byte x02 = 2;
    static const byte x03 = 3;
    static const byte x04 = 4;
    static const byte x05 = 5;
    static const byte x06 = 6;
    static const byte x07 = 7;
    static const byte x08 = 8;
    static const byte x3F = 0x3F;
    static const byte x40 = 0x40;
    static const byte x41 = 0x41;
    static const byte x7F = 0x7F;
    static const byte x80 = 0x80;
    static const byte xBF = 0xBF;
    static const byte xF8 = 0xF8;
    static const byte xFB = 0xFB;
    static const byte xFE = 0xFE;
    static const byte xFF = 0xFF;
    static const int16_t x100 = 0x100;
    byte *trie;
    byte *triePos;
    byte *origPos;
    int16_t need_count;
    byte insertState;
    int keyPos;
    virtual ~trie_node_handler() {}
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
        util::print("Block Count:");
        util::print((long)blockCount);
        util::endl();
        util::print("Avg Block Count:");
        util::print((long)(num_entries / blockCount));
        util::endl();
        util::print("Avg Max Count:");
        util::print((long)(maxKeyCount / blockCount));
        util::endl();
    }
    void printNumLevels() {
        util::print("Level Count:");
        util::print((long)numLevels);
        util::endl();
    }
    long size() {
        return total_size;
    }

};

#endif
