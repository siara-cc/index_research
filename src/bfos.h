#ifndef bfos_H
#define bfos_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree.h"

using namespace std;

#define BS_MIDDLE_PREFIX 1
#define MAX_PTR_BITMAP_BYTES 0

#define BFOS_NODE_SIZE 512

#define BFOS_HDR_SIZE 7
#define BX_MAX_KEY_LEN buf[6]

#define INSERT_AFTER 1
#define INSERT_BEFORE 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5
#define INSERT_CONVERT 6
#define INSERT_THREAD1 7

class bfos_node_handler: public trie_node_handler {
private:
    inline byte *getLastPtr();
    inline void setPrefixLast(byte key_char, byte *t, byte pfx_rem_len);
    void setPtrDiff(int16_t diff);
    byte copyKary(byte *t, byte *dest, byte *copied_len, int lvl, byte *tp, byte *key, int16_t key_len, byte whichHalf);
    byte copyTrieHalf(byte *tp, byte *key, int16_t key_len, byte *dest, byte whichHalf);
    byte *nextPtr(byte *first_key, byte *tp, byte **t_ptr, byte& ctr, byte& tc, byte& child, byte& leaf);
    void consolidateInitialPrefix(byte *t);
public:
    byte *last_t;
    byte last_child;
    byte last_leaf;
    bfos_node_handler(byte *m);
    inline int16_t locate();
    int16_t traverseToLeaf(byte *node_paths[] = null);
    inline char *getValueAt(int16_t *vlen);
    inline byte *getChildPtr(byte *ptr);
    void setBuf(byte *m);
    void initBuf();
    inline void initVars();
    bool isFull(int16_t kv_lens);
    void addData();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    int16_t insertCurrent();
    void updatePtrs(byte *upto, int diff);
};

class bfos : public bplus_tree {
private:
    void recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
            byte *node_paths[], int16_t level);
public:
    static byte split_buf[BFOS_NODE_SIZE];
    static int count1, count2;
    bfos();
    ~bfos();
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
};

#endif
/*
byte bfos_node_handler::copyKary(byte *t, byte *dest) {
    byte tc;
    byte tot_len = 0;
    do {
        tc = *t;
        byte len;
        if (tc & x01) {
            len = (tc >> 1) + 1;
            memcpy(dest, t, len);
            dest += len;
            t += len;
            tc = *t;
        }
        len = (tc & x02 ? 3 + BIT_COUNT(t[1]) + BIT_COUNT2(t[2]) :
                2 + BIT_COUNT2(t[1]));
        memcpy(dest, t, len);
        tot_len += len;
        t += len;
    } while (!(tc & x04));
    return tot_len;
}

byte bfos_node_handler::copyTrieFirstHalf(byte *tp, byte *first_key, int16_t first_key_len, byte *dest) {
    byte *t = trie;
    byte trie_len = 0;
    for (int i = 0; i < first_key_len; i++) {
        byte offset = first_key[i] & x07;
        byte *t_end = trie + tp[i];
        memcpy(dest, t, t_end - t);
        byte orig_dest = dest;
        dest += (t_end - t);
        *dest++ = (*t_end | x04);
        if (*t_end & x02) {
            byte children = t_end[1] & ~((i == first_key_len ? xFF : xFE) << offset);
            byte leaves = t_end[2] & ~(xFE << offset);
            *dest++ = children;
            *dest++ = leaves;
            memcpy(dest, t_end + 3, BIT_COUNT(children));
            dest += BIT_COUNT(children);
            if (children)
                t_end = (dest - 1);
            memcpy(dest, t_end + 3 + BIT_COUNT(t_end[1]), BIT_COUNT2(leaves));
            dest += BIT_COUNT2(leaves);
            trie_len += (3 + BIT_COUNT(children) + BIT_COUNT2(leaves));
        } else {
            byte leaves = t_end[1] & ~(xFE << offset);
            *dest++ = leaves;
            memcpy(dest, t_end + 2, BIT_COUNT2(leaves));
            dest += BIT_COUNT2(leaves);
            trie_len += (2 + BIT_COUNT2(leaves));
        }
        while (t <= t_end) {
            byte tc = *t++;
            if (tc & x02) {
                byte children = *t++;
                t++;
                for (int j = 0; j < BIT_COUNT(children) && t <= t_end; j++) {
                    dest = copyChildTree(t + *t, orig_dest, dest);
                }
            }
            t += BIT_COUNT2(*t);
        }
        t = t_end; // child of i
    }
    return trie_len;
}

byte *bfos_node_handler::copyChildTree(byte *t, byte *orig_dest, byte *dest) {
    byte tp_ch[BX_MAX_KEY_LEN];
    do {
        byte len = copyKary(t + *t, dest);
        orig_dest[t - trie] = dest - (orig_dest + (t - trie));
    } while (1);
}

 */
