#ifndef dfox_H
#define dfox_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define DX_MIDDLE_PREFIX 1

#define DFOX_HDR_SIZE 9

#define DX_SIBLING_PTR_SIZE 2
#if DX_SIBLING_PTR_SIZE == 1
#define DX_BIT_COUNT_SB(x) BIT_COUNT(x)
#define DX_GET_TRIE_LEN BPT_TRIE_LEN
#define DX_SET_TRIE_LEN(x) BPT_TRIE_LEN = x
#define DX_GET_SIBLING_OFFSET(x) *x
#define DX_SET_SIBLING_OFFSET(x, off) *x = off
#else
#define DX_BIT_COUNT_SB(x) BIT_COUNT2(x)
#define DX_GET_TRIE_LEN util::getInt(BPT_TRIE_LEN_PTR)
#define DX_SET_TRIE_LEN(x) util::setInt(BPT_TRIE_LEN_PTR, x)
#define DX_GET_SIBLING_OFFSET(x) util::getInt(x)
#define DX_SET_SIBLING_OFFSET(x, off) util::setInt(x, off)
#endif

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class dfox : public bpt_trie_handler<dfox> {
public:
    const static byte need_counts[10];
    byte *last_t;
    dfox(uint16_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint16_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE) :
        bpt_trie_handler<dfox>(leaf_block_sz, parent_block_sz) {
    }

#if DX_SIBLING_PTR_SIZE == 1
    inline byte *skipChildren(byte *t, byte count) {
#else
    inline byte *skipChildren(byte *t, int16_t count) {
#endif
        while (count) {
            byte tc = *t++;
            switch (tc & x03) {
            case x03:
                count--;
                t++;
                last_t = t - 2;
                continue;
            case x02:
                t += DX_GET_SIBLING_OFFSET(t);
                break;
            case x00:
                t += BIT_COUNT2(*t);
                t++;
                break;
            case x01:
                t += (tc >> 2);
                continue;
            }
            last_t = t - 2;
            if (tc & x04)
                count--;
        }
        return t;
    }

    inline int16_t searchCurrentBlock() {
        byte key_char = *key;
        byte *t = trie;
        byte trie_char = *t;
        keyPos = 1;
        origPos = t++;
        do {
#if DX_MIDDLE_PREFIX == 1
            while ((trie_char & x03) == x01) {
                byte pfx_len;
                pfx_len = (trie_char >> 2);
                while (pfx_len && key_char == *t && keyPos < key_len) {
                    key_char = key[keyPos++];
                    t++;
                    pfx_len--;
                }
                if (pfx_len) {
                    triePos = t;
                    if (key_char > *t) {
                        t = skipChildren(t + pfx_len, 1);
                        key_at = t;
                    }
                    insertState = INSERT_CONVERT;
                    return -1;
                }
                trie_char = *t;
                origPos = t++;
            }
#endif
            while ((key_char & xF8) > trie_char) {
                if (trie_char & x02)
                    t += DX_GET_SIBLING_OFFSET(t);
                else
                    t += BIT_COUNT2(*t) + 1;
                last_t = t - 2;
                if (trie_char & x04) {
                    insertState = INSERT_AFTER;
                    return -1;
                }
                trie_char = *t;
                origPos = t++;
            }
            if ((key_char ^ trie_char) & xF8) {
                insertState = INSERT_BEFORE;
                return -1;
            }
            if (trie_char & x02) {
                t += DX_SIBLING_PTR_SIZE;
                byte r_children = *t++;
                byte r_leaves = *t++;
                byte r_mask = x01 << (key_char & x07);
                trie_char = (r_leaves & r_mask ? x02 : x00) |
                        ((r_children & r_mask) && keyPos != key_len ? x01 : x00);
                //key_char = (r_leaves & r_mask ? x01 : x00)
                //        | (r_children & r_mask ? x02 : x00)
                //        | (keyPos == key_len ? x04 : x00);
                r_mask--;
                t = skipChildren(t, BIT_COUNT(r_children & r_mask) + BIT_COUNT(r_leaves & r_mask));
            } else {
                byte r_leaves = *t++;
                byte r_mask = x01 << (key_char & x07);
                trie_char = (r_leaves & r_mask ? x02 : x00);
                t = skipChildren(t, BIT_COUNT(r_leaves & (r_mask - 1)));
            }
            switch (trie_char) {
            case 0:
                triePos = t;
                insertState = INSERT_LEAF;
                return -1;
            case 1:
                break;
            case 2:
                int16_t cmp;
                key_at = getKey(t, &key_at_len);
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at, key_at_len);
                if (cmp == 0) {
                    last_t = t;
                    return 1;
                }
                if (cmp > 0)
                    last_t = t;
                else
                    cmp = -cmp;
                triePos = t;
                insertState = INSERT_THREAD;
#if DX_MIDDLE_PREFIX == 1
                need_count = cmp + 7 + DX_SIBLING_PTR_SIZE;
#else
                need_count = (cmp * 5) + 5 + DX_SIBLING_PTR_SIZE;
#endif
                return -1;
            case 3:
                last_t = t;
                t += 2;
                break;
            }
            key_char = key[keyPos++];
            trie_char = *t;
            origPos = t++;
        } while (1);
        return -1;
    }

    inline uint16_t getPtr(byte *t) {
        return ((*t >> 2) << 8) + t[1];
    }

    inline byte *getKey(byte *t, byte *plen) {
        byte *kvIdx = current_block + getPtr(t);
        *plen = kvIdx[0];
        kvIdx++;
        return kvIdx;
    }

    inline int getHeaderSize() {
        return DFOX_HDR_SIZE;
    }

    inline byte *getPtrPos() {
        return NULL;
    }

    inline byte *getChildPtrPos(int16_t idx) {
        return current_block + getPtr(last_t);
    }

#if DX_SIBLING_PTR_SIZE == 1
    byte *nextKey(byte *first_key, byte *tp, byte *t, char& ctr, byte& tc, byte& child, byte& leaf) {
#else
    byte *nextKey(byte *first_key, int16_t *tp, byte *t, char& ctr, byte& tc, byte& child, byte& leaf) {
#endif
        do {
            while (ctr > x07) {
                if (tc & x04) {
                    keyPos--;
                    tc = trie[tp[keyPos]];
                    while (x01 == (tc & x03)) {
                        keyPos--;
                        tc = trie[tp[keyPos]];
                    }
                    child = (tc & x02 ? trie[tp[keyPos] + 1 + DX_SIBLING_PTR_SIZE] : 0);
                    leaf = trie[tp[keyPos] + (tc & x02 ? 2 + DX_SIBLING_PTR_SIZE : 1)];
                    ctr = first_key[keyPos] & 0x07;
                    ctr++;
                } else {
                    tp[keyPos] = t - trie;
                    tc = *t++;
                    while (x01 == (tc & x03)) {
                        byte len = tc >> 2;
                        for (int i = 0; i < len; i++)
                            tp[keyPos + i] = t - trie - 1;
                        memcpy(first_key + keyPos, t, len);
                        t += len;
                        keyPos += len;
                        tp[keyPos] = t - trie;
                        tc = *t++;
                    }
                    t += (tc & x02 ? DX_SIBLING_PTR_SIZE : 0);
                    child = (tc & x02 ? *t++ : 0);
                    leaf = *t++;
                    ctr = 0;
                }
            }
            first_key[keyPos] = (tc & xF8) | ctr;
            byte mask = x01 << ctr;
            if (leaf & mask) {
                leaf &= ~mask;
                if (0 == (child & mask))
                    ctr++;
                return t;
            }
            if (child & mask) {
                child = leaf = 0;
                keyPos++;
                ctr = x08;
                tc = 0;
            }
            ctr++;
        } while (1); // (t - trie) < DX_GET_TRIE_LEN);
        return t;
    }

    byte *nextPtr(byte *t) {
        byte tc = *t & x03;
        while (x03 != tc) {
            while (x01 == tc) {
                t += (*t >> 2);
                t++;
                tc = *t & x03;
            }
            t += (tc & x02 ? 3 + DX_SIBLING_PTR_SIZE : 2);
            tc = *t & x03;
        }
        return t;
    }

    void updatePrefix() {

    }

#if DX_SIBLING_PTR_SIZE == 1
    void deleteTrieLastHalf(int16_t brk_key_len, byte *first_key, byte *tp) {
#else
    void deleteTrieLastHalf(int16_t brk_key_len, byte *first_key, int16_t *tp) {
#endif
        for (int idx = brk_key_len; idx >= 0; idx--) {
            byte *t = trie + tp[idx];
            byte tc = *t;
            if (x01 == (tc & x03))
                continue;
            byte offset = first_key[idx] & x07;
            *t++ = (tc | x04);
            if (tc & x02) {
                byte children = t[DX_SIBLING_PTR_SIZE];
                children &= ~((idx == brk_key_len ? xFF : xFE) << offset);
                                // ryte_mask[offset] : ryte_incl_mask[offset]);
                t[DX_SIBLING_PTR_SIZE] = children;
                byte leaves = t[1 + DX_SIBLING_PTR_SIZE] & ~(xFE << offset);
                t[DX_SIBLING_PTR_SIZE + 1] = leaves; // ryte_incl_mask[offset];
                byte *new_t = skipChildren(t + 2 + DX_SIBLING_PTR_SIZE,
                        BIT_COUNT(children) + BIT_COUNT(leaves));
                DX_SET_SIBLING_OFFSET(t, new_t - t);
                t = new_t;
            } else {
                byte leaves = *t;
                leaves &= ~(xFE << offset); // ryte_incl_mask[offset];
                *t++ = leaves;
                t += BIT_COUNT2(leaves);
            }
            if (idx == brk_key_len)
                DX_SET_TRIE_LEN(t - trie);
        }
    }

    int deleteSegment(byte *delete_end, byte *delete_start) {
        int count = delete_end - delete_start;
        if (count) {
            DX_SET_TRIE_LEN(DX_GET_TRIE_LEN - count);
            memmove(delete_start, delete_end, trie + DX_GET_TRIE_LEN - delete_start);
        }
        return count;
    }

#if DX_SIBLING_PTR_SIZE == 1
    void deleteTrieFirstHalf(int16_t brk_key_len, byte *first_key, byte *tp) {
#else
    void deleteTrieFirstHalf(int16_t brk_key_len, byte *first_key, int16_t *tp) {
#endif
        for (int idx = brk_key_len; idx >= 0; idx--) {
            byte *t = trie + tp[idx];
            byte tc = *t++;
            if (x01 == (tc & x03)) {
                byte len = (tc >> 2);
                deleteSegment(trie + tp[idx + 1], t + len);
                idx -= len;
                idx++;
                continue;
            }
            byte offset = first_key[idx] & 0x07;
            if (tc & x02) {
                byte children = t[DX_SIBLING_PTR_SIZE];
                byte leaves = t[1 + DX_SIBLING_PTR_SIZE];
                byte mask = (xFF << offset);
                children &= mask; // left_incl_mask[offset];
                leaves &= ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
                byte count = BIT_COUNT(t[DX_SIBLING_PTR_SIZE]) - BIT_COUNT(children)
                        + BIT_COUNT(t[1 + DX_SIBLING_PTR_SIZE]) - BIT_COUNT(leaves); // ryte_mask[offset];
                t[DX_SIBLING_PTR_SIZE] = children;
                t[1 + DX_SIBLING_PTR_SIZE] = leaves;
                byte *child_ptr = t + 2 + DX_SIBLING_PTR_SIZE;
                byte *new_t = skipChildren(child_ptr, count);
                deleteSegment((idx < brk_key_len) ? (trie + tp[idx + 1]) : new_t, child_ptr);
                new_t = skipChildren(child_ptr, BIT_COUNT(children) + BIT_COUNT(leaves));
                DX_SET_SIBLING_OFFSET(t, new_t - t);
            } else {
                byte leaves = *t;
                leaves &= ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
                deleteSegment(t + 1 + BIT_COUNT2(*t) - BIT_COUNT2(leaves), t + 1);
                *t = leaves;
            }
        }
        deleteSegment(trie + tp[0], trie);
    }

    void updateSkipLens(byte *loop_upto, int8_t diff) {
        byte *t = trie;
        byte tc = *t++;
        while (t <= loop_upto) {
            switch (tc & x03) {
            case x01:
                t += (tc >> 2);
                break;
            case x02:
                unsigned int s;
                s = DX_GET_SIBLING_OFFSET(t);
                if ((t + s) > loop_upto) {
                    DX_SET_SIBLING_OFFSET(t, s + diff);
                    t += (2 + DX_SIBLING_PTR_SIZE);
                } else
                    t += s;
                break;
            case x00:
                t += BIT_COUNT2(*t);
                t++;
                break;
            case x03:
                t++;
            }
            tc = *t++;
        }
    }

    void addFirstData() {
        addData(0);
    }

    void addData(int16_t idx) {

        byte *ptr = insertCurrent();

        uint16_t key_left = key_len - keyPos;
        uint16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
        setKVLastPos(kv_last_pos);
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + keyPos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);

        setPtr(ptr, kv_last_pos);
        setFilledSize(filledSize() + 1);

    }

    bool isFull() {
        decodeNeedCount();
        if (getKVLastPos() < (DFOX_HDR_SIZE + DX_GET_TRIE_LEN +
                need_count + key_len - keyPos + value_len + 3))
            return true;
#if DX_SIBLING_PTR_SIZE == 1
        if (DX_GET_TRIE_LEN > (254 - need_count))
            return true;
#endif
        return false;
    }

    void setPtr(byte *t, uint16_t ptr) {
        *t++ = ((ptr >> 8) << 2) | x03;
        *t = ptr & xFF;
    }

    byte *split(byte *first_key, int16_t *first_len_ptr) {
        int16_t orig_filled_size = filledSize();
        const uint16_t DFOX_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
        byte *b = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
        dfox new_block;
        new_block.setCurrentBlock(b);
        new_block.initCurrentBlock();
        new_block.setKVLastPos(DFOX_NODE_SIZE);
        if (!isLeaf())
            new_block.setLeaf(false);
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        uint16_t kv_last_pos = getKVLastPos();
        uint16_t halfKVLen = DFOX_NODE_SIZE - kv_last_pos + 1;
        halfKVLen /= 2;

        int16_t brk_idx = -1;
        uint16_t brk_kv_pos;
        byte *brk_trie_pos = trie;
        uint16_t tot_len;
        brk_kv_pos = tot_len = 0;
        char ctr = x08;
#if DX_SIBLING_PTR_SIZE == 1
        byte tp[BPT_MAX_PFX_LEN + 1];
#else
        int16_t tp[BPT_MAX_PFX_LEN + 1];
#endif
        byte *t = trie;
        byte tc, child, leaf;
        tc = child = leaf = 0;
        //if (!isLeaf())
        //   cout << "Trie len:" << (int) DX_GET_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
        keyPos = 0;
        // (1) move all data to new_block in order
        int16_t idx;
        for (idx = 0; idx < orig_filled_size; idx++) {
            if (brk_idx == -1)
                t = nextKey(first_key, tp, t, ctr, tc, child, leaf);
            else
                t = nextPtr(t);
            uint16_t src_idx = getPtr(t);
            //if (brk_idx == -1) {
            //    memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
            //   first_key[keyPos+1+current_block[src_idx]] = 0;
            //   cout << first_key << endl;
            //}
            t += 2;
            uint16_t kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            tot_len += kv_len;
            memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
            kv_last_pos += kv_len;
            if (brk_idx == -1) {
                //brk_key_len = nextKey(s);
                //if (tot_len > halfKVLen) {
                if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                    //memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[keyPos+1+current_block[src_idx]] = 0;
                    //cout << "Middle:" << first_key << endl;
                    brk_idx = idx + 1;
                    brk_kv_pos = kv_last_pos;
                    brk_trie_pos = t;
                }
            }
        }
        kv_last_pos = getKVLastPos() + DFOX_NODE_SIZE - kv_last_pos;
        new_block.setKVLastPos(kv_last_pos);
        memmove(new_block.current_block + kv_last_pos, new_block.current_block + getKVLastPos(), DFOX_NODE_SIZE - kv_last_pos);
        brk_kv_pos += (kv_last_pos - getKVLastPos());
        uint16_t diff = DFOX_NODE_SIZE - brk_kv_pos;
        t = trie;
        for (idx = 0; idx < orig_filled_size; idx++) {
            t = nextPtr(t);
            setPtr(t, kv_last_pos + (idx < brk_idx ? diff : 0));
            t += 2;
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
        }
        kv_last_pos = new_block.getKVLastPos();
        memcpy(new_block.trie, trie, DX_GET_TRIE_LEN);
#if DX_SIBLING_PTR_SIZE == 1
        new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
#else
        util::setInt(new_block.BPT_TRIE_LEN_PTR, DX_GET_TRIE_LEN);
#endif
        {
            int16_t last_key_len = keyPos + 1;
            byte last_key[last_key_len + 1];
            memcpy(last_key, first_key, last_key_len);
            deleteTrieLastHalf(keyPos, first_key, tp);
            new_block.keyPos = keyPos;
            t = new_block.trie + (brk_trie_pos - trie);
            t = new_block.nextKey(first_key, tp, t, ctr, tc, child, leaf);
            keyPos = new_block.keyPos;
            //src_idx = getPtr(idx + 1);
            //memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
            //first_key[keyPos+1+current_block[src_idx]] = 0;
            //cout << first_key << endl;
            if (isLeaf()) {
                // *first_len_ptr = keyPos + 1;
                *first_len_ptr = util::compare((const char *) first_key, keyPos + 1, (const char *) last_key, last_key_len);
            } else {
                new_block.key_at = new_block.getKey(t, &new_block.key_at_len);
                keyPos++;
                if (new_block.key_at_len) {
                    memcpy(first_key + keyPos, new_block.key_at,
                            new_block.key_at_len);
                    *first_len_ptr = keyPos + new_block.key_at_len;
                } else
                    *first_len_ptr = keyPos;
                keyPos--;
            }
            new_block.deleteTrieFirstHalf(keyPos, first_key, tp);
        }

        {
            uint16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + DFOX_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            setKVLastPos(DFOX_NODE_SIZE - old_blk_new_len);
            setFilledSize(brk_idx);
        }

        {
            uint16_t new_size = orig_filled_size - brk_idx;
            new_block.setKVLastPos(brk_kv_pos);
            new_block.setFilledSize(new_size);
        }

        consolidateInitialPrefix(current_block);
        new_block.consolidateInitialPrefix(new_block.current_block);

        return new_block.current_block;

    }

    void consolidateInitialPrefix(byte *t) {
        t += DFOX_HDR_SIZE;
        byte *t_reader = t;
        byte count = 0;
        if ((*t & x03) == 1) {
            count = (*t >> 2);
            t_reader += count;
            t_reader++;
        }
        byte *t_writer = t_reader + (((*t & x03) == 1) ? 0 : 1);
        byte trie_len_diff = 0;
        while (((*t_reader & x03) == 1) || ((*t_reader & x02) && (*t_reader & x04)
                && BIT_COUNT(t_reader[2 + DX_SIBLING_PTR_SIZE]) == 1
                && BIT_COUNT(t_reader[3 + DX_SIBLING_PTR_SIZE]) == 0)) {
            if ((*t_reader & x03) == 1) {
                byte len = *t_reader >> 2;
                if (count + len > 63)
                    break;
                memcpy(t_writer, ++t_reader, len);
                t_writer += len;
                t_reader += len;
                count += len;
                trie_len_diff++;
            } else {
                if (count > 62)
                    break;
                *t_writer++ = (*t_reader & xF8) + BIT_COUNT(t_reader[2 + DX_SIBLING_PTR_SIZE] - 1);
                t_reader += (4 + DX_SIBLING_PTR_SIZE);
                count++;
                trie_len_diff += (3 + DX_SIBLING_PTR_SIZE);
            }
        }
        if (t_reader > t_writer) {
            memmove(t_writer, t_reader, DX_GET_TRIE_LEN - (t_reader - t) + filledSize() * 2);
            if ((*t & x03) != 1)
                trie_len_diff--;
            *t = (count << 2) + 1;
            DX_SET_TRIE_LEN(DX_GET_TRIE_LEN - trie_len_diff);
            //cout << (int) (*t >> 1) << endl;
        }
    }

    byte *insertCurrent() {
        byte key_char, mask;
        uint16_t diff;
        byte *ret;

        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        switch (insertState) {
        case INSERT_AFTER:
            *origPos &= xFB;
            triePos = origPos + ((*origPos & x02) ? DX_GET_SIBLING_OFFSET(origPos + 1) + 1 : BIT_COUNT2(origPos[1]) + 2);
            insAt(triePos, ((key_char & xF8) | x04), mask, x00, x00);
            ret = triePos + 2;
            if (keyPos > 1)
                updateSkipLens(origPos, 4);
            break;
        case INSERT_BEFORE:
            insAt(origPos, key_char & xF8, mask, x00, x00);
            ret = origPos + 2;
            if (keyPos > 1)
                updateSkipLens(origPos + 1, 4);
            break;
        case INSERT_LEAF:
            if (*origPos & x02)
                origPos[2 + DX_SIBLING_PTR_SIZE] |= mask;
            else
                origPos[1] |= mask;
            insAt(triePos, x00, x00);
            ret = triePos;
            updateSkipLens(origPos + 1, 2);
            break;
    #if DX_MIDDLE_PREFIX == 1
        case INSERT_CONVERT:
            byte b, c;
            char cmp_rel;
            diff = triePos - origPos;
            // 3 possible relationships between key_char and *triePos, 4 possible positions of triePos
            c = *triePos;
            cmp_rel = ((c ^ key_char) > x07 ? (c < key_char ? 0 : 1) : 2);
            if (cmp_rel == 0) {
                insAt(key_at, (key_char & xF8) | 0x04, 1 << (key_char & x07), x00, x00);
                ret = key_at + 2;
            }
            if (diff == 1)
                triePos = origPos;
            b = (cmp_rel == 2 ? x04 : x00) | (cmp_rel == 1 ? x00 : x02);
            need_count = (*origPos >> 2) - diff;
            diff--;
            *triePos++ = ((cmp_rel == 0 ? c : key_char) & xF8) | b;
            b = (cmp_rel == 1 ? 6 : (cmp_rel == 2 ? (c < key_char ? 2 : 4) : 2));
#if DX_SIBLING_PTR_SIZE == 2
            b++;
#endif
            if (diff) {
                *origPos = (diff << 2) | x01;
                b++;
            }
            if (need_count)
                b++;
            insBytes(triePos + (diff ? 0 : 1), b);
            updateSkipLens(origPos + 1, b + (cmp_rel == 0 ? 4
                    : (cmp_rel == 2 && c < key_char ? 2 : 0)));
            if (cmp_rel == 1) {
                *triePos++ = 1 << (key_char & x07);
                ret = triePos;
                triePos += 2;
                *triePos++ = (c & xF8) | x06;
            }
            key_at = skipChildren(triePos + DX_SIBLING_PTR_SIZE - 1
                    + (cmp_rel == 2 ? (c < key_char ? 3 : 5) : 3)
                    + need_count + (need_count ? 1 : 0), 1);
            DX_SET_SIBLING_OFFSET(triePos, key_at - triePos + (cmp_rel == 2 && c < key_char ? 2 : 0));
            triePos += DX_SIBLING_PTR_SIZE;
            *triePos++ = 1 << (c & x07);
            if (cmp_rel == 2) {
                *triePos++ = 1 << (key_char & x07);
                if (c >= key_char) {
                    ret = triePos;
                    triePos += 2;
                }
            } else
                *triePos++ = 0;
            if (need_count)
                *triePos = (need_count << 2) | x01;
            if (cmp_rel == 0)
                ret += b;
            if (cmp_rel == 2 && c < key_char) {
                insAt(key_at, x00, x00);
                ret = key_at;
            }
            break;
    #endif
        case INSERT_THREAD:
            uint16_t p, min, ptr;
            byte c1, c2;
            byte *ptrPos;
            ret = ptrPos = 0;
            if (*origPos & x02) {
                origPos[1 + DX_SIBLING_PTR_SIZE] |= mask;
            } else {
#if DX_SIBLING_PTR_SIZE == 1
                insAt(origPos + 1, BIT_COUNT2(origPos[1]) + 3, mask);
                triePos += 2;
#else
                uint16_t offset = BIT_COUNT2(origPos[1]) + 4;
                insAt(origPos + 1, offset >> 8, offset & xFF, mask);
                triePos += 3;
#endif
            }
            c1 = c2 = key_char;
            p = keyPos;
            min = util::min16(key_len, keyPos + key_at_len);
            ptr = getPtr(triePos);
            if (p < min)
                origPos[2 + DX_SIBLING_PTR_SIZE] &= ~mask;
            else {
                if (p == key_len)
                    ret = triePos;
                else
                    ptrPos = triePos;
                triePos += 2;
            }
#if DX_MIDDLE_PREFIX == 1
            need_count -= (8 + DX_SIBLING_PTR_SIZE);
            diff = p + need_count;
            if (diff == min) {
                if (need_count) {
                    need_count--;
                    diff--;
                }
            }
            diff = need_count + (need_count ? (need_count / 64) + 1 : 0)
                    + (diff == min ? 4 : (key[diff] ^ key_at[diff - keyPos]) > x07 ? 6 :
                            (key[diff] == key_at[diff - keyPos] ? 7 + DX_SIBLING_PTR_SIZE : 4));
            insBytes(triePos, diff);
#else
            need_count = (p == min ? 4 : 0);
            while (p < min) {
                c1 = key[p];
                c2 = key_at[p - keyPos];
                need_count += ((c1 ^ c2) > x07 ? 6 : (c1 == c2 ? (p + 1 == min ?
                        7 + DX_SIBLING_PTR_SIZE : 3 + DX_SIBLING_PTR_SIZE) : 4));
                if (c1 != c2)
                    break;
                p++;
            }
            p = keyPos;
            insBytes(triePos, need_count);
            diff = need_count;
            need_count += (p == min ? 0 : 2);
#endif
            if (!ret)
                ret = triePos + diff - (p < min ? 0 : 2);
            if (!ptrPos)
                ptrPos = triePos + diff - (p < min ? 0 : 2);
#if DX_MIDDLE_PREFIX == 1
            if (need_count) {
                int16_t copied = 0;
                while (copied < need_count) {
                    int16_t to_copy = (need_count - copied) > 63 ? 63 : need_count - copied;
                    *triePos++ = (to_copy << 2) | x01;
                    memcpy(triePos, key + keyPos + copied, to_copy);
                    copied += to_copy;
                    triePos += to_copy;
                }
                p += need_count;
                //count1 += need_count;
            }
#endif
            while (p < min) {
                c1 = key[p];
                c2 = key_at[p - keyPos];
                if (c1 > c2) {
                    byte swap = c1;
                    c1 = c2;
                    c2 = swap;
                }
#if DX_MIDDLE_PREFIX == 1
                switch ((c1 ^ c2) > x07 ? 0 : (c1 == c2 ? 3 : 1)) {
#else
                switch ((c1 ^ c2) > x07 ?
                        0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
                case 2:
                    need_count -= (3 + DX_SIBLING_PTR_SIZE);
                    *triePos++ = (c1 & xF8) | x06;
#if DX_SIBLING_PTR_SIZE == 1
                    *triePos++ = need_count + 3;
#else
                    DX_SET_SIBLING_OFFSET(triePos, need_count + 4);
                    triePos += 2;
#endif
                    *triePos++ = x01 << (c1 & x07);
                    *triePos++ = 0;
                    break;
#endif
                case 0:
                    *triePos++ = c1 & xF8;
                    *triePos++ = x01 << (c1 & x07);
                    if (c1 == key[p])
                        ret = triePos;
                    else
                        ptrPos = triePos;
                    triePos += 2;
                    *triePos++ = (c2 & xF8) | x04;
                    *triePos = x01 << (c2 & x07);
                    break;
                case 1:
                    *triePos++ = (c1 & xF8) | x04;
                    *triePos++ = (x01 << (c1 & x07)) | (x01 << (c2 & x07));
                    if (c1 == key[p])
                        ret = triePos;
                    else
                        ptrPos = triePos;
                    break;
                case 3:
                    *triePos++ = (c1 & xF8) | x06;
#if DX_SIBLING_PTR_SIZE == 1
                    *triePos++ = 9;
#else
                    DX_SET_SIBLING_OFFSET(triePos, 10);
                    triePos += 2;
#endif
                    *triePos++ = x01 << (c1 & x07);
                    *triePos++ = x01 << (c1 & x07);
                    if (p + 1 == key_len)
                        ret = triePos;
                    else
                        ptrPos = triePos;
                    triePos += 2;
                    break;
                }
                if (c1 != c2)
                    break;
                p++;
            }
            if (c1 == c2) {
                c2 = (p == key_len ? key_at[p - keyPos] : key[p]);
                *triePos++ = (c2 & xF8) | x04;
                *triePos = x01 << (c2 & x07);
            }
            updateSkipLens(origPos, diff + (*origPos & x02 ? 0 : 1 + DX_SIBLING_PTR_SIZE));
#if DX_SIBLING_PTR_SIZE == 1
            origPos[1] += diff;
#else
            DX_SET_SIBLING_OFFSET(origPos + 1, DX_GET_SIBLING_OFFSET(origPos + 1) + diff);
#endif
            *origPos |= x02;
            diff = p - keyPos;
            keyPos = p + (p < key_len ? 1 : 0);
            if (diff < key_at_len)
                diff++;
            if (diff) {
                key_at_len -= diff;
                ptr += diff;
                if (key_at_len >= 0)
                    current_block[ptr] = key_at_len;
            }
            if (ptrPos)
                setPtr(ptrPos, ptr);
            break;
        case INSERT_EMPTY:
            *trie = (key_char & xF8) | x04;
            trie[1] = mask;
            DX_SET_TRIE_LEN(4);
            ret = trie + 2;
            break;
        }

        if (BPT_MAX_PFX_LEN < keyPos)
            BPT_MAX_PFX_LEN = keyPos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

        return ret;

    }

    inline int16_t insAt(byte *ptr, byte b1, byte b2) {
        int16_t trie_len = DX_GET_TRIE_LEN;
        memmove(ptr + 2, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr = b2;
        DX_SET_TRIE_LEN(trie_len + 2);
        return 2;
    }

    inline int16_t insAt(byte *ptr, byte b1, byte b2, byte b3) {
        int16_t trie_len = DX_GET_TRIE_LEN;
        memmove(ptr + 3, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        DX_SET_TRIE_LEN(trie_len + 3);
        return 3;
    }

    inline byte insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4) {
        int16_t trie_len = DX_GET_TRIE_LEN;
        memmove(ptr + 4, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        DX_SET_TRIE_LEN(trie_len + 4);
        return 4;
    }

    void insBytes(byte *ptr, uint16_t len) {
        uint16_t trie_len = DX_GET_TRIE_LEN;
        memmove(ptr + len, ptr, trie + trie_len - ptr);
        DX_SET_TRIE_LEN(trie_len + len);
    }

    void decodeNeedCount() {
        if (insertState != INSERT_THREAD)
            need_count = need_counts[insertState];
    }

};

#endif
