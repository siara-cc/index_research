#ifndef bfos_H
#define bfos_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define BS_MIDDLE_PREFIX 1

#define BFOS_HDR_SIZE 8
#define BX_MAX_PFX_LEN buf[7]

class bfos : public bpt_trie_handler {
private:
    const static byte need_counts[10];
    void decodeNeedCount();
    inline byte *getLastPtr();
    inline void setPrefixLast(byte key_char, byte *t, byte pfx_rem_len);
    void setPtrDiff(int16_t diff);
    byte copyKary(byte *t, byte *dest, int lvl, byte *tp, byte *brk_key, int16_t brk_key_len, byte whichHalf);
    byte copyTrieHalf(byte *tp, byte *brk_key, int16_t brk_key_len, byte *dest, byte whichHalf);
    void consolidateInitialPrefix(byte *t);
    int16_t insertAfter();
    int16_t insertBefore();
    int16_t insertLeaf();
    int16_t insertConvert();
    int16_t insertThread();
    int16_t insertEmpty();
public:
    byte *last_t;
    byte last_child;
    byte last_leaf;
    bool isFull();
    inline int16_t searchCurrentBlock();
    void addData(int16_t idx);
    void addFirstData();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    int16_t insertCurrent();
    void updatePtrs(byte *upto, int diff);
    inline byte *getChildPtrPos(int16_t idx);
    inline byte *getPtrPos();
    inline int getHeaderSize();
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
