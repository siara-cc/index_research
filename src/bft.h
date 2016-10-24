#ifndef bft_H
#define bft_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"
#include "bplus_tree.h"

using namespace std;

#define BFT_9_BIT_PTR 0

#define BFT_NODE_SIZE 512

#define BFT_HDR_SIZE 7
#define BFT_TRIE_LEN buf[5]
#define BFT_PREFIX_LEN buf[6]

#define INSERT_AFTER 1
#define INSERT_BEFORE 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5

#define BFT_MAX_KEY_PREFIX_LEN 60

class bft_iterator_status {
public:
    byte *t;
    int keyPos;
    byte is_next;
    byte is_child_pending;
    byte tp[BFT_MAX_KEY_PREFIX_LEN];
    bft_iterator_status(byte *trie, byte prefix_len) {
        t = trie + 1;
        keyPos = prefix_len;
        is_child_pending = is_next = 0;
    }
};

class bft_node_handler : public bplus_tree_node_handler {
private:
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
    inline void insAt(byte *ptr, byte b);
    inline byte insAt(byte *ptr, byte b1, byte b2);
    inline byte insAt(byte *ptr, byte b1, byte b2, byte b3);
    inline byte insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4);
    inline byte insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4, byte b5);
    inline void setAt(byte pos, byte b);
    inline void append(byte b);
    inline void append(byte b1, byte b2);
    inline void append(byte b1, byte b2, byte b3);
    inline void appendPtr(int16_t p);
    inline byte getAt(byte pos);
    inline void delAt(byte *ptr);
    inline void delAt(byte *ptr, int16_t count);
    int16_t nextPtr(bft_iterator_status& s);
    int16_t getLastPtrOfChild(byte *triePos);
    inline byte *getLastPtr(byte *last_t);
    inline int16_t get9bitPtr(byte *t);
    void set9bitPtr(byte *t, int16_t p);
    int16_t deletePrefix(int16_t prefix_len);
public:
    byte *trie;
    byte *triePos;
    byte *origPos;
    int16_t need_count;
    byte insertState;
    int keyPos;
    int16_t last_child_pos;
    bft_node_handler(byte *m);
    void initBuf();
    inline void initVars();
    void setBuf(byte *m);
    bool isFull(int16_t kv_lens);
    void addData();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    int16_t locate();
    void traverseToLeaf(byte *node_paths[] = null);
    inline char *getValueAt(int16_t *vlen);
    inline byte *getChildPtr(byte *ptr);
    int16_t getFirstPtr();
    int16_t insertCurrent();
    void updatePtrs(byte *upto, int diff);
};

class bft : public bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    static byte split_buf[BFT_NODE_SIZE];
    static int count1, count2;
    bft();
    ~bft();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
};

#endif
