#ifndef BP_TREE_H
#define BP_TREE_H
#ifdef ARDUINO
#include <HardwareSerial.h>
#include <string.h>
#else
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include <stdint.h>
#include "univix_util.h"

using namespace std;

#define USE_POP_CNT 0

#if USE_POP_CNT == 1
//#define BIT_COUNT(x) __builtin_popcount(x)
#define BIT_COUNT(x) util::bitcount(x)
#define BIT_COUNT2(x) util::popcnt2(x)
#else
#define BIT_COUNT(x) util::bit_count[x]
#define BIT_COUNT2(x) util::bit_count2x[x]
#endif

#define FIRST_BIT_OFFSET_FROM_RIGHT(x) BIT_COUNT(254 & ((x) ^ ((x) - 1)))

#define BPT_IS_LEAF_BYTE buf[0]
#define BPT_FILLED_SIZE buf + 1
#define BPT_LAST_DATA_PTR buf + 3
#define BPT_MAX_KEY_LEN buf[5]
#define BPT_TRIE_LEN buf[6]

class bplus_tree_node_handler {
public:
    byte *buf;
    const char *key;
    int16_t key_len;
    byte *key_at;
    int16_t key_at_len;
    const char *value;
    int16_t value_len;
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

    inline void setKVLastPos(uint16_t val) {
        util::setInt(BPT_LAST_DATA_PTR, val);
    }

    inline uint16_t getKVLastPos() {
        return util::getInt(BPT_LAST_DATA_PTR);
    }

    virtual bool isFull(int16_t kv_len) = 0;
    virtual void addData() = 0;
    virtual char *getValueAt(int16_t *vlen) = 0;
    virtual int16_t traverseToLeaf(byte *node_paths[] = null) = 0;
    virtual int16_t locate() = 0;
    virtual byte *split(byte *first_key, int16_t *first_len_ptr) = 0;
};

class trie_node_handler: public bplus_tree_node_handler {
protected:
    static byte need_counts[10];
    virtual void decodeNeedCount() = 0;
    inline void delAt(byte *ptr) {
        BPT_TRIE_LEN--;
        memmove(ptr, ptr + 1, trie + BPT_TRIE_LEN - ptr);
    }

    inline void delAt(byte *ptr, int16_t count) {
        BPT_TRIE_LEN -= count;
        memmove(ptr, ptr + count, trie + BPT_TRIE_LEN - ptr);
    }

    inline byte insAt(byte *ptr, byte b) {
        memmove(ptr + 1, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr = b;
        BPT_TRIE_LEN++;
        return 1;
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

    inline void insAt(byte *ptr, byte b, const char *s, byte len) {
        memmove(ptr + 1 + len, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr++ = b;
        memcpy(ptr, s, len);
        BPT_TRIE_LEN += len;
        BPT_TRIE_LEN++;
    }

    inline void insAt(byte *ptr, const char *s, byte len) {
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN - ptr);
        memcpy(ptr, s, len);
        BPT_TRIE_LEN += len;
    }

    void insBytes(byte *ptr, int16_t len) {
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN - ptr);
        BPT_TRIE_LEN += len;
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

    void append(const char *s, int16_t need_count) {
        memcpy(trie + BPT_TRIE_LEN, s, need_count);
        BPT_TRIE_LEN += need_count;
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
    static const byte x0F = 0x0F;
    static const byte x10 = 0x10;
    static const byte x11 = 0x11;
    static const byte x3F = 0x3F;
    static const byte x40 = 0x40;
    static const byte x41 = 0x41;
    static const byte x55 = 0x55;
    static const byte x7F = 0x7F;
    static const byte x80 = 0x80;
    static const byte x81 = 0x81;
    static const byte xAA = 0xAA;
    static const byte xBF = 0xBF;
    static const byte xC0 = 0xC0;
    static const byte xF8 = 0xF8;
    static const byte xFB = 0xFB;
    static const byte xFC = 0xFC;
    static const byte xFD = 0xFD;
    static const byte xFE = 0xFE;
    static const byte xFF = 0xFF;
    static const int16_t x100 = 0x100;
    byte *trie;
    byte *triePos;
    byte *origPos;
    byte need_count;
    byte insertState;
    int16_t keyPos;
    virtual ~trie_node_handler() {}
};

class bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
protected:
    long total_size;
    int numLevels;
    int maxKeyCountNode;
    int maxKeyCountLeaf;
    int maxTrieLenNode;
    int maxTrieLenLeaf;
    int blockCountNode;
    int blockCountLeaf;

public:
    byte *root_data;
    int maxThread;
    bplus_tree() {
        numLevels = blockCountNode = blockCountLeaf = maxKeyCountNode =
                maxKeyCountLeaf = total_size = maxThread = 0;
        root_data = NULL;
    }
    virtual ~bplus_tree() {
    }
    virtual void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len) = 0;
    virtual char *get(const char *key, int16_t key_len, int16_t *pValueLen) = 0;
    void printMaxKeyCount(long num_entries) {
        util::print("Block Count:");
        util::print((long) blockCountNode);
        util::print(", ");
        util::print((long) blockCountLeaf);
        util::endl();
        util::print("Avg Block Count:");
        util::print((long) (num_entries / blockCountLeaf));
        util::endl();
        util::print("Avg Max Count:");
        util::print((long) (maxKeyCountNode / (blockCountNode ? blockCountNode : 1)));
        util::print(", ");
        util::print((long) (maxKeyCountLeaf / blockCountLeaf));
        util::endl();
        util::print("Avg Trie Len:");
        util::print((long) (maxTrieLenNode / (blockCountNode ? blockCountNode : 1)));
        util::print(", ");
        util::print((long) (maxTrieLenLeaf / blockCountLeaf));
        util::endl();
    }
    void printNumLevels() {
        util::print("Level Count:");
        util::print((long) numLevels);
    }
    long size() {
        return total_size;
    }

};

#endif
