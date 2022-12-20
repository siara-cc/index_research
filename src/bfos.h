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

#define BFOS_HDR_SIZE 9

#if (defined(__AVR_ATmega328P__))
#define BS_CHILD_PTR_SIZE 1
#else
#define BS_CHILD_PTR_SIZE 2
#endif
#if BS_CHILD_PTR_SIZE == 1
#define BS_BIT_COUNT_CH(x) BIT_COUNT(x)
#define BS_GET_TRIE_LEN BPT_TRIE_LEN
#define BS_SET_TRIE_LEN(x) BPT_TRIE_LEN = x
#define BS_GET_CHILD_OFFSET(x) *(x)
#define BS_SET_CHILD_OFFSET(x, off) *(x) = off
#else
#define BS_BIT_COUNT_CH(x) BIT_COUNT2(x)
#define BS_GET_TRIE_LEN util::getInt(BPT_TRIE_LEN_PTR)
#define BS_SET_TRIE_LEN(x) util::setInt(BPT_TRIE_LEN_PTR, x)
#define BS_GET_CHILD_OFFSET(x) util::getInt(x)
#define BS_SET_CHILD_OFFSET(x, off) util::setInt(x, off)
#endif

class bfos_iterator_vars {
public:
#if BS_CHILD_PTR_SIZE == 1
    bfos_iterator_vars(uint8_t *tp_ext, uint8_t *t_ext) {
#else
    bfos_iterator_vars(uint16_t *tp_ext, uint8_t *t_ext) {
#endif
        tp = tp_ext;
        t = t_ext;
        ctr = 9;
        only_leaf = 1;
    }
#if BS_CHILD_PTR_SIZE == 1
    uint8_t *tp;
#else
    uint16_t *tp;
#endif
    uint8_t *t;
    uint8_t ctr;
    uint8_t tc;
    uint8_t child;
    uint8_t leaf;
    uint8_t only_leaf;
};

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class bfos : public bpt_trie_handler<bfos> {
public:
    const static uint8_t need_counts[10];
    uint8_t *last_t;
    uint8_t last_child;
    uint8_t last_leaf;

    bfos(uint16_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint16_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, uint8_t *block = NULL) :
        bpt_trie_handler<bfos>(leaf_block_sz, parent_block_sz, cache_sz, fname, block) {
    }

    inline void setCurrentBlockRoot() {
        current_block = root_block;
        trie = current_block + BFOS_HDR_SIZE;
    }

    inline void setCurrentBlock(uint8_t *m) {
        current_block = m;
        trie = current_block + BFOS_HDR_SIZE;
    }

    inline uint8_t *getLastPtr() {
        //keyPos = 0;
        do {
            switch (last_child > last_leaf || (last_leaf ^ last_child) <= last_child ? 2 : 1) {
            case 1:
                return current_block + util::getInt(last_t + (*last_t & x02 ? BS_BIT_COUNT_CH(last_t[1]) + 1 : 0)
                        + BIT_COUNT2(last_leaf));
            case 2:
#if BS_CHILD_PTR_SIZE == 1
                last_t += BS_BIT_COUNT_CH(last_child) + 2;
#else
                last_t++;
                last_t += BS_BIT_COUNT_CH(last_child);
#endif
                last_t += BS_GET_CHILD_OFFSET(last_t);
                while (*last_t & x01) {
                    last_t += (*last_t >> 1);
                    last_t++;
                }
                while (!(*last_t & x04)) {
                    last_t += (*last_t & x02 ? BS_BIT_COUNT_CH(last_t[1])
                         + BIT_COUNT2(last_t[2]) + 3 : BIT_COUNT2(last_t[1]) + 2);
                }
                last_child = (*last_t & x02 ? last_t[1] : 0);
                last_leaf = (*last_t & x02 ? last_t[2] : last_t[1]);
            }
        } while (1);
        return 0;
    }

    inline void setPrefixLast(uint8_t key_char, uint8_t *t, uint8_t pfx_rem_len) {
        if (key_char > *t) {
            t += pfx_rem_len;
            while (*t & x01)
                t += (*t >> 1) + 1;
            while (!(*t & x04)) {
                t += (*t & x02 ? BS_BIT_COUNT_CH(t[1])
                     + BIT_COUNT2(t[2]) + 3 : BIT_COUNT2(t[1]) + 2);
            }
            last_t = t++;
            last_child = (*last_t & x02 ? *t++ : 0);
            last_leaf = *t;
        }
    }

    inline int16_t searchCurrentBlock() {
        uint8_t *t = trie;
        uint8_t trie_char = *t;
        origPos = t++;
        //if (keyPos) {
        //    if (trie_char & x01) {
        //        keyPos--;
        //        uint8_t pfx_len = trie_char >> 1;
        //        if (keyPos < pfx_len) {
        //            trie_char = ((pfx_len - keyPos) << 1) + 1;
        //            t += keyPos;
        //        } else {
        //            keyPos = pfx_len;
        //            t += pfx_len;
        //            trie_char = *t;
        //            origPos = t++;
        //        }
        //    } else
        //        keyPos = 0;
        //}
        uint8_t key_char = *key; //[keyPos++];
        keyPos = 1;
        do {
            switch (trie_char & x01 ? 3 : (key_char ^ trie_char) > x07 ? (key_char > trie_char ? 0 : 2) : 1) {
            case 0:
                last_t = origPos;
                last_child = (trie_char & x02 ? *t++ : 0);
                last_leaf = *t++;
                t += BS_BIT_COUNT_CH(last_child) + BIT_COUNT2(last_leaf);
                if (trie_char & x04) {
                    triePos = t;
                    return -INSERT_AFTER;
                }
                break;
            case 1:
                uint8_t r_leaves, r_children;
                r_children = (trie_char & x02 ? *t++ : 0);
                r_leaves = *t++;
                key_char = x01 << (key_char & x07);
                trie_char = (r_leaves & key_char ? x02 : x00) |     // faster
                        ((r_children & key_char) && keyPos != key_len ? x01 : x00);
                //trie_char = r_leaves & r_mask ?                 // slower
                //        (r_children & r_mask ? (keyPos == key_len ? x02 : x03) : x02) :
                //        (r_children & r_mask ? (keyPos == key_len ? x00 : x01) : x00);
                key_char--;
                if (!isLeaf()) {
                    if ((r_children | r_leaves) & key_char) {
                        last_t = origPos;
                        last_child = r_children & key_char;
                        last_leaf = r_leaves & key_char;
                    }
                }
                switch (trie_char) {
                case 0:
                    triePos = t;
                    return -INSERT_LEAF;
                case 1:
                    break;
                case 2:
                    int cmp;
                    triePos = t + BS_BIT_COUNT_CH(r_children) + BIT_COUNT2(r_leaves & key_char);
                    key_at = current_block + util::getInt(triePos);
                    key_at_len = *key_at++;
                    cmp = util::compare(key + keyPos, key_len - keyPos, key_at, key_at_len);
                    if (!cmp) {
                        last_t = key_at;
                        return 0;
                    }
                    if (cmp < 0)
                        cmp = -cmp;
                    else
                        last_t = key_at;
#if BS_MIDDLE_PREFIX == 1
                    need_count = cmp + 8 + BS_CHILD_PTR_SIZE;
#else
                    need_count = (cmp * (3 + BS_CHILD_PTR_SIZE)) + 8;
#endif
                    return -INSERT_THREAD;
                case 3:
                    if (!isLeaf()) {
                        last_t = origPos;
                        last_child = r_children & key_char;
                        last_leaf = (r_leaves & key_char) | (key_char + 1);
                    }
                    break;
                }
                t += BS_BIT_COUNT_CH(r_children & key_char);
                t += BS_GET_CHILD_OFFSET(t);
                key_char = key[keyPos++];
                break;
#if BS_MIDDLE_PREFIX == 1
            case 3:
                uint8_t pfx_len;
                pfx_len = (trie_char >> 1);
                while (pfx_len && key_char == *t && keyPos < key_len) {
                    key_char = key[keyPos++];
                    t++;
                    pfx_len--;
                }
                if (pfx_len) {
                    triePos = t;
                    if (!isLeaf())
                        setPrefixLast(key_char, t, pfx_len);
                    return -INSERT_CONVERT;
                }
                break;
#endif
            case 2:
                return -INSERT_BEFORE;
            }
            trie_char = *t;
            origPos = t++;
        } while (1);
        return -1;
    }

    inline uint8_t *getChildPtrPos(int16_t search_result) {
        return key_at == last_t ? last_t - 1 : getLastPtr();
    }

    inline uint8_t *getPtrPos() {
        return NULL;
    }

    inline int getHeaderSize() {
        return BFOS_HDR_SIZE;
    }

    void setPtrDiff(uint16_t diff) {
        uint8_t *t = trie;
        uint8_t *t_end = trie + BS_GET_TRIE_LEN;
        while (t < t_end) {
            uint8_t tc = *t++;
            if (tc & x01) {
                t += (tc >> 1);
                continue;
            }
            uint8_t leaves = 0;
            if (tc & x02) {
                leaves = t[1];
                t += BS_BIT_COUNT_CH(*t);
                t += 2;
            } else
                leaves = *t++;
            for (int i = 0; i < BIT_COUNT(leaves); i++) {
                util::setInt(t, util::getInt(t) + diff);
                t += 2;
            }
        }
    }

#if BS_CHILD_PTR_SIZE == 1
    uint8_t copyKary(uint8_t *t, uint8_t *dest, int lvl, uint8_t *tp,
            uint8_t *brk_key, int16_t brk_key_len, uint8_t whichHalf) {
#else
    uint16_t copyKary(uint8_t *t, uint8_t *dest, int lvl, uint16_t *tp,
                uint8_t *brk_key, int16_t brk_key_len, uint8_t whichHalf) {
#endif
        uint8_t *orig_dest = dest;
        while (*t & x01) {
            uint8_t len = (*t >> 1) + 1;
            memcpy(dest, t, len);
            dest += len;
            t += len;
        }
        uint8_t *dest_after_prefix = dest;
        uint8_t *limit = trie + (lvl < brk_key_len ? tp[lvl] : 0);
        uint8_t tc;
        uint8_t is_limit = 0;
        do {
            is_limit = (limit == t ? 1 : 0); // && is_limit == 0 ? 1 : 0);
            if (limit == t && whichHalf == 2)
                dest = dest_after_prefix;
            tc = *t;
            if (is_limit) {
                *dest++ = tc;
                uint8_t offset = (lvl < brk_key_len ? brk_key[lvl] & x07 : x07);
                t++;
                uint8_t children = (tc & x02) ? *t++ : 0;
                uint8_t orig_children = children;
                uint8_t leaves = *t++;
                uint8_t orig_leaves = leaves;
                if (whichHalf == 1) {
                    children &= ~(((brk_key_len - lvl) == 1 ? xFF : xFE) << offset);
                    leaves &= ~(xFE << offset);
                    *(dest - 1) |= x04;
                    if (tc & x02)
                        *dest++ = children;
                    *dest++ = leaves;
                    for (int i = 0; i < BIT_COUNT(children); i++) {
#if BS_CHILD_PTR_SIZE == 1
                        *dest++ = t[i];
#else
                        util::setInt(dest, util::getInt(t + i * 2));
                        dest += 2;
#endif
                    }
                    t += BS_BIT_COUNT_CH(orig_children);
                    memcpy(dest, t, BIT_COUNT2(leaves));
                    dest += BIT_COUNT2(leaves);
                    break;
                } else {
                    children &= (xFF << offset);
                    leaves &= (((brk_key_len - lvl) == 1 ? xFF : xFE) << offset);
                    if (tc & x02)
                        *dest++ = children;
                    *dest++ = leaves;
                    for (int i = BIT_COUNT(orig_children - children); i < BIT_COUNT(orig_children); i++) {
#if BS_CHILD_PTR_SIZE == 1
                        *dest++ = t[i];
#else
                        util::setInt(dest, util::getInt(t + i * 2));
                        dest += 2;
#endif
                    }
                    t += BS_BIT_COUNT_CH(orig_children);
                    memcpy(dest, t + BIT_COUNT2(orig_leaves - leaves), BIT_COUNT2(leaves));
                    dest += BIT_COUNT2(leaves);
                }
                t += BIT_COUNT2(orig_leaves);
            } else {
                uint8_t len = (tc & x02) ? 3 + BS_BIT_COUNT_CH(t[1]) + BIT_COUNT2(t[2])
                        : 2 + BIT_COUNT2(t[1]);
                memcpy(dest, t, len);
                t += len;
                dest += len;
            }
        } while (!(tc & x04));
        return dest - orig_dest;
    }

#if BS_CHILD_PTR_SIZE == 1
    uint8_t copyTrieHalf(uint8_t *tp, uint8_t *brk_key, int16_t brk_key_len, uint8_t *dest, uint8_t whichHalf) {
        uint8_t tp_child[BPT_MAX_PFX_LEN];
#else
    uint16_t copyTrieHalf(uint16_t *tp, uint8_t *brk_key, int16_t brk_key_len, uint8_t *dest, uint8_t whichHalf) {
        uint16_t tp_child[BPT_MAX_PFX_LEN];
#endif
        uint8_t child_num[BPT_MAX_PFX_LEN];
        uint8_t *d;
        uint8_t *t = trie;
        uint8_t *new_trie = dest;
        int lvl = 0;
        while (*t & x01) {
            uint8_t len = (*t >> 1);
            for (int i = 0; i < len; i++)
                tp_child[i] = dest - new_trie;
            lvl = len;
            t += len + 1;
        }
        t = trie;
        uint16_t last_len = copyKary(t, dest, lvl, tp, brk_key, brk_key_len, whichHalf);
        d = dest;
        dest += last_len;
        do {
            uint8_t tc = *d++;
            while (tc & x01) {
                d += (tc >> 1);
                tc = *d++;
            }
            if (tc & x02) {
                uint8_t len = BIT_COUNT(*d++);
                if (len) {
                    d++;
                    tp_child[lvl] = d - new_trie - 3;
                    uint8_t *child = trie + BS_GET_CHILD_OFFSET(d);
                    BS_SET_CHILD_OFFSET(d, dest - d);
                    child_num[lvl++] = 0;
                    t = child;
                    while (*t & x01) {
                        uint8_t len = (*t >> 1);
                        for (int i = 0; i < len; i++)
                            tp_child[lvl + i] = dest - new_trie;
                        lvl += len;
                        t += len + 1;
                    }
                    t = child;
                    last_len = copyKary(t, dest, lvl, tp, brk_key, brk_key_len, whichHalf);
                    d = dest;
                    dest += last_len;
                    continue;
                } else
                    d += (BIT_COUNT2(*d) + 1);
            } else
                d += (BIT_COUNT2(*d) + 1);
            while (tc & x04) {
                do {
                    lvl--;
                    if (lvl < 0)
                        return dest - new_trie;
                    d = new_trie + tp_child[lvl];
                    tc = *d;
                } while (tc & x01);
                d = new_trie + tp_child[lvl];
                uint8_t len = BIT_COUNT(d[1]);
                uint8_t i = child_num[lvl];
                i++;
                if (i < len) {
#if BS_CHILD_PTR_SIZE == 1
                    d += (i + 3);
#else
                    d += (i * 2 + 3);
#endif
                    uint8_t *child = trie + BS_GET_CHILD_OFFSET(d);
                    BS_SET_CHILD_OFFSET(d, dest - d);
                    child_num[lvl++] = i;
                    t = child;
                    while (*t & x01) {
                        uint8_t len = (*t >> 1);
                        for (int i = 0; i < len; i++)
                            tp_child[lvl + i] = dest - new_trie;
                        lvl += len;
                        t += len + 1;
                    }
                    t = child;
                    last_len = copyKary(t, dest, lvl, tp, brk_key, brk_key_len, whichHalf);
                    d = dest;
                    dest += last_len;
                    break;
                }
                tc = *d;
                if (!(tc & x04)) {
                    d += (tc & x02 ? 3 + BS_BIT_COUNT_CH(d[1]) + BIT_COUNT2(d[2]) :
                            2 + BIT_COUNT2(d[1]));
                    break;
                }
            }
        } while (1);
        return 0;
    }

    void consolidateInitialPrefix(uint8_t *t) {
        t += BFOS_HDR_SIZE;
        uint8_t *t_reader = t;
        uint8_t count = 0;
        if (*t & x01) {
            count = (*t >> 1);
            t_reader += count;
            t_reader++;
        }
        uint8_t *t_writer = t_reader + (*t & x01 ? 0 : 1);
        uint8_t trie_len_diff = 0;
#if BS_CHILD_PTR_SIZE == 1
        while ((*t_reader & x01) || ((*t_reader & x02) && (*t_reader & x04) && BIT_COUNT(t_reader[1]) == 1
                && BIT_COUNT(t_reader[2]) == 0 && t_reader[3] == 1)) {
#else
        while ((*t_reader & x01) ||
                ((*t_reader & x02) && (*t_reader & x04) && BIT_COUNT(t_reader[1]) == 1
                && BIT_COUNT(t_reader[2]) == 0 && t_reader[3] == 0 && t_reader[4] == 2)) {
#endif
            if (*t_reader & x01) {
                uint8_t len = *t_reader >> 1;
                if (count + len > 127)
                    break;
                memmove(t_writer, ++t_reader, len);
                t_writer += len;
                t_reader += len;
                count += len;
                trie_len_diff++;
            } else {
                if (count > 126)
                    break;
                *t_writer++ = (*t_reader & xF8) + BIT_COUNT(t_reader[1] - 1);
                count++;
#if BS_CHILD_PTR_SIZE == 1
                t_reader += 4;
                trie_len_diff += 3;
#else
                t_reader += 5;
                trie_len_diff += 4;
#endif
            }
        }
        if (t_reader > t_writer) {
            memmove(t_writer, t_reader, BS_GET_TRIE_LEN - (t_reader - t));
            if (!(*t & x01))
                trie_len_diff--;
            *t = (count << 1) + 1;
            BS_SET_TRIE_LEN(BS_GET_TRIE_LEN - trie_len_diff);
            //cout << (int) (*t >> 1) << endl;
        }
    }

#if BS_CHILD_PTR_SIZE == 1
    uint8_t *nextPtr(uint8_t *first_key, bfos_iterator_vars& it) {
#else
    uint8_t *nextPtr(uint8_t *first_key, bfos_iterator_vars& it) {
#endif
        do {
            while (it.ctr == x08) {
                if (it.tc & x04) {
                    keyPos--;
                    it.t = trie + it.tp[keyPos];
                    while (*it.t & x01) {
                        keyPos -= (*it.t >> 1);
                        it.t = trie + it.tp[keyPos];
                    }
                    it.tc = *it.t++;
                    it.child = (it.tc & x02 ? *it.t++ : 0);
                    it.leaf = *it.t++;
                    it.ctr = first_key[keyPos] & x07;
                    it.ctr++;
                    it.t += BS_BIT_COUNT_CH(it.child);
                } else {
                    it.t += BIT_COUNT2(it.leaf);
                    it.ctr = 0x09;
                    break;
                }
            }
            if (it.ctr > x07) {
                it.tp[keyPos] = it.t - trie;
                it.tc = *it.t++;
                while (it.tc & x01) {
                    uint8_t len = it.tc >> 1;
                    for (int i = 0; i < len; i++)
                        it.tp[keyPos + i] = it.t - trie - 1;
                    memcpy(first_key + keyPos, it.t, len);
                    it.t += len;
                    keyPos += len;
                    it.tp[keyPos] = it.t - trie;
                    it.tc = *it.t++;
                }
                it.child = (it.tc & x02 ? *it.t++ : 0);
                it.leaf = *it.t++;
                it.t += BS_BIT_COUNT_CH(it.child);
                it.ctr = FIRST_BIT_OFFSET_FROM_RIGHT(it.child | it.leaf);
            }
            first_key[keyPos] = (it.tc & xF8) | it.ctr;
            uint8_t mask = x01 << it.ctr;
            if ((it.leaf & mask) && it.only_leaf) {
                if (it.child & mask)
                    it.only_leaf = 0;
                else
                    it.ctr++;
                return it.t + BIT_COUNT2(it.leaf & (mask - 1));
            }
            it.only_leaf = 1;
            if (it.child & mask) {
                it.t -= BS_BIT_COUNT_CH(it.child & (xFF << it.ctr));
                uint16_t child_offset = BS_GET_CHILD_OFFSET(it.t);
                BS_SET_CHILD_OFFSET(it.t, it.t - trie + child_offset);
                it.t += child_offset;
                keyPos++;
                it.ctr = 0x09;
                it.tc = 0;
                continue;
            }
            it.ctr = (it.ctr == x07 ? 8 : FIRST_BIT_OFFSET_FROM_RIGHT((it.child | it.leaf) & (xFE << it.ctr)));
        } while (1); // (s.t - trie) < BPT_TRIE_LEN);
        return 0;
    }

    uint8_t *split(uint8_t *first_key, int16_t *first_len_ptr) {
        int16_t orig_filled_size = filledSize();
        uint32_t BFOS_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
        int lvl = current_block[0] & 0x1F;
        uint8_t *b = allocateBlock(BFOS_NODE_SIZE, isLeaf(), lvl);
        bfos new_block(this->leaf_block_size, this->parent_block_size, 0, NULL, b);
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        if (lvl == BPT_PARENT0_LVL && cache_size > 0)
            BFOS_NODE_SIZE -= 8;
        uint16_t kv_last_pos = getKVLastPos();
        uint16_t halfKVPos = kv_last_pos + (BFOS_NODE_SIZE - kv_last_pos) / 2;

        int16_t brk_idx;
        uint16_t brk_kv_pos;
        brk_idx = brk_kv_pos = 0;
        // (1) move all data to new_block in order
        int16_t idx;
        uint8_t alloc_size = BPT_MAX_PFX_LEN + 1;
        uint8_t curr_key[alloc_size];
#if BS_CHILD_PTR_SIZE == 1
        uint8_t tp[alloc_size];
        uint8_t tp_cpy[alloc_size];
#else
        uint16_t tp[alloc_size];
        uint16_t tp_cpy[alloc_size];
#endif
        int16_t tp_cpy_len = 0;
        bfos_iterator_vars it(tp, new_block.trie);
        //if (!isLeaf())
        //   cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
        new_block.keyPos = 0;
        memcpy(new_block.trie, trie, BS_GET_TRIE_LEN);
#if BS_CHILD_PTR_SIZE == 1
        new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
#else
        util::setInt(new_block.BPT_TRIE_LEN_PTR, BS_GET_TRIE_LEN);
#endif
        for (idx = 0; idx < orig_filled_size; idx++) {
            uint8_t *leaf_ptr = new_block.nextPtr(curr_key, it);
            uint16_t src_idx = util::getInt(leaf_ptr);
            uint16_t kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
            util::setInt(leaf_ptr, kv_last_pos);
            //memcpy(curr_key + new_block.keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
            //curr_key[new_block.keyPos+1+current_block[src_idx]] = 0;
            //cout << curr_key << endl;
            if (brk_idx < 0) {
                brk_idx = -brk_idx;
                new_block.keyPos++;
                tp_cpy_len = new_block.keyPos;
                if (isLeaf()) {
                    //*first_len_ptr = s.keyPos;
                    *first_len_ptr = util::compare(curr_key, new_block.keyPos, first_key, *first_len_ptr);
                    memcpy(first_key, curr_key, tp_cpy_len);
                } else {
                    memcpy(first_key, curr_key, new_block.keyPos);
                    memcpy(first_key + new_block.keyPos, current_block + src_idx + 1, current_block[src_idx]);
                    *first_len_ptr = new_block.keyPos + current_block[src_idx];
                }
                memcpy(tp_cpy, tp, tp_cpy_len * BS_CHILD_PTR_SIZE);
                //curr_key[new_block.keyPos] = 0;
                //cout << "Middle:" << curr_key << endl;
                new_block.keyPos--;
            }
            kv_last_pos += kv_len;
            if (brk_idx == 0) {
                //brk_key_len = nextKey(s);
                //if (tot_len > halfKVLen) {
                if (kv_last_pos > halfKVPos || idx >= (orig_filled_size * 2 / 3)) {
                    //memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[keyPos+1+current_block[src_idx]] = 0;
                    //cout << first_key << ":";
                    brk_idx = idx + 1;
                    brk_idx = -brk_idx;
                    brk_kv_pos = kv_last_pos;
                    *first_len_ptr = new_block.keyPos + 1;
                    memcpy(first_key, curr_key, *first_len_ptr);
                    BS_SET_TRIE_LEN(new_block.copyTrieHalf(tp, first_key, *first_len_ptr, trie, 1));
                }
            }
        }
#if BS_CHILD_PTR_SIZE == 1
        new_block.BPT_TRIE_LEN = new_block.copyTrieHalf(tp_cpy, first_key, tp_cpy_len, trie + BPT_TRIE_LEN, 2);
        memcpy(new_block.trie, trie + BPT_TRIE_LEN, new_block.BPT_TRIE_LEN);
#else
        util::setInt(new_block.BPT_TRIE_LEN_PTR, new_block.copyTrieHalf(tp_cpy, first_key,
                tp_cpy_len, trie + BS_GET_TRIE_LEN, 2));
        memcpy(new_block.trie, trie + BS_GET_TRIE_LEN, util::getInt(new_block.BPT_TRIE_LEN_PTR));
#endif

        kv_last_pos = getKVLastPos() + BFOS_NODE_SIZE - kv_last_pos;
        uint16_t diff = (kv_last_pos - getKVLastPos());

        {
            memmove(new_block.current_block + brk_kv_pos + diff, new_block.current_block + brk_kv_pos,
                    BFOS_NODE_SIZE - brk_kv_pos - diff);
            brk_kv_pos += diff;
            new_block.setPtrDiff(diff);
            new_block.setKVLastPos(brk_kv_pos);
            new_block.setFilledSize(orig_filled_size - brk_idx);
        }

        {
            uint16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + BFOS_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos - diff, old_blk_new_len); // Copy back first half to old block
            diff += (BFOS_NODE_SIZE - brk_kv_pos);
            setPtrDiff(diff);
            setKVLastPos(BFOS_NODE_SIZE - old_blk_new_len);
            setFilledSize(brk_idx);
        }

        if (!isLeaf())
            new_block.setLeaf(false);

#if BS_MIDDLE_PREFIX == 1
        consolidateInitialPrefix(current_block);
        new_block.consolidateInitialPrefix(new_block.current_block);
#endif

        //keyPos = 0;

        return new_block.current_block;

    }

    void makeSpace() {
        int block_size = (isLeaf() ? leaf_block_size : parent_block_size);
        int lvl = current_block[0] & 0x1F;
        if (lvl == BPT_PARENT0_LVL && cache_size > 0)
            block_size -= 8;
        const uint16_t data_size = block_size - getKVLastPos();
        uint8_t data_buf[data_size];
        uint16_t new_data_len = 0;
        uint8_t *t = current_block + BFOS_HDR_SIZE;
        uint8_t *upto = t + BS_GET_TRIE_LEN;
        while (t < upto) {
            uint8_t tc = *t++;
            if (tc & 0x01) {
                t += (tc >> 1);
                continue;
            }
            uint8_t child = 0;
            if (tc & 0x02)
                child = *t++;
            uint8_t leaves = *t++;
            t += BS_BIT_COUNT_CH(child);
            int leaf_count = BIT_COUNT(leaves);
            while (leaf_count--) {
                int leaf_pos = util::getInt(t);
                uint8_t *child_ptr_pos = current_block + leaf_pos;
                uint16_t data_len = *child_ptr_pos;
                data_len++;
                data_len += child_ptr_pos[data_len];
                data_len++;
                new_data_len += data_len;
                // if (child_ptr_pos == key_at - 1) {
                //     if (last_t == key_at)
                //       last_t = 0;
                //     key_at = current_block + (block_size - new_data_len) + 1;
                //     if (!last_t)
                //       last_t = key_at;
                // }
                memcpy(data_buf + data_size - new_data_len, child_ptr_pos, data_len);
                util::setInt(t, block_size - new_data_len);
                t += 2;
            }
        }
        uint16_t new_kv_last_pos = block_size - new_data_len;
        memcpy(current_block + new_kv_last_pos, data_buf + data_size - new_data_len, new_data_len);
        //printf("%d, %d\n", data_size, new_data_len);
        setKVLastPos(new_kv_last_pos);
        searchCurrentBlock();
    }

    bool isFull(int16_t search_result) {
        decodeNeedCount(search_result);
        if (getKVLastPos() < (BFOS_HDR_SIZE + BS_GET_TRIE_LEN
                + need_count + key_len - keyPos + value_len + 3)) {
            makeSpace();
            if (getKVLastPos() < (BFOS_HDR_SIZE + BS_GET_TRIE_LEN
                + need_count + key_len - keyPos + value_len + 3))
              return true;
        }
#if BS_CHILD_PTR_SIZE == 1
        if (BS_GET_TRIE_LEN > 254 - need_count)
            return true;
#endif
        return false;
    }

    void addFirstData() {
        addData(3);
    }

    void addData(int16_t search_result) {

        insertState = search_result + 1;

        uint16_t ptr = insertCurrent();

        int16_t key_left = key_len - keyPos;
        uint16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
        setKVLastPos(kv_last_pos);
        util::setInt(trie + ptr, kv_last_pos);
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + keyPos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);
        setFilledSize(filledSize() + 1);

    }

    inline void updatePtrs(uint8_t *upto, int diff) {
        uint8_t *t = trie;
        while (t < upto) {
            uint8_t tc = *t++;
            if (tc & x01) {
                t += (tc >> 1);
                continue;
            }
            if (!(tc & x02)) {
                t += BIT_COUNT2(*t) + 1;
                continue;
            }
            int count = BIT_COUNT(*t++);
            uint8_t leaves = *t++;
            while (count--) {
                if (t >= upto)
                    return;
#if BS_CHILD_PTR_SIZE == 1
                if ((t + *t) >= upto)
                    *t += diff;
                if (insertState == INSERT_BEFORE) {
                    if (keyPos > 1 && (t + *t) == (origPos + 4))
                        *t -= 4;
                }
                t++;
#else
                uint16_t child_offset = BS_GET_CHILD_OFFSET(t);
                if ((t + child_offset) >= upto)
                    BS_SET_CHILD_OFFSET(t, child_offset + diff);
                if (insertState == INSERT_BEFORE) {
                    if (keyPos > 1 && (t + BS_GET_CHILD_OFFSET(t)) == (origPos + 4))
                        BS_SET_CHILD_OFFSET(t, BS_GET_CHILD_OFFSET(t) - 4);
                }
                t += 2;
#endif
                // todo: avoid inside loops
            }
            t += BIT_COUNT2(leaves);
        }
    }

    inline void delAt(uint8_t *ptr, int16_t count) {
        int16_t trie_len = BS_GET_TRIE_LEN - count;
        BS_SET_TRIE_LEN(trie_len);
        memmove(ptr, ptr + count, trie + trie_len - ptr);
    }

    inline int16_t insAt(uint8_t *ptr, uint8_t b) {
        int16_t trie_len = BS_GET_TRIE_LEN;
        memmove(ptr + 1, ptr, trie + trie_len - ptr);
        *ptr = b;
        BS_SET_TRIE_LEN(trie_len + 1);
        return 1;
    }

    inline int16_t insAt(uint8_t *ptr, uint8_t b1, uint8_t b2) {
        int16_t trie_len = BS_GET_TRIE_LEN;
        memmove(ptr + 2, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr = b2;
        BS_SET_TRIE_LEN(trie_len + 2);
        return 2;
    }

    inline int16_t insAt(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3) {
        int16_t trie_len = BS_GET_TRIE_LEN;
        memmove(ptr + 3, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        BS_SET_TRIE_LEN(trie_len + 3);
        return 3;
    }

    inline uint8_t insAt(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
        int16_t trie_len = BS_GET_TRIE_LEN;
        memmove(ptr + 4, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        BS_SET_TRIE_LEN(trie_len + 4);
        return 4;
    }

    void insBytes(uint8_t *ptr, int16_t len) {
        int16_t trie_len = BS_GET_TRIE_LEN;
        memmove(ptr + len, ptr, trie + trie_len - ptr);
        BS_SET_TRIE_LEN(trie_len + len);
    }

    uint16_t insertCurrent() {
        uint8_t key_char, mask;
        uint16_t diff;
        uint16_t ret;

        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        switch (insertState) {
        case INSERT_AFTER:
            *origPos &= xFB;
            triePos = origPos + (*origPos & x02 ? BS_BIT_COUNT_CH(origPos[1])
                    + BIT_COUNT2(origPos[2]) + 3 : BIT_COUNT2(origPos[1]) + 2);
            updatePtrs(triePos, 4);
            insAt(triePos, ((key_char & xF8) | x04), mask, 0, 0);
            ret = triePos - trie + 2;
            break;
        case INSERT_BEFORE:
            updatePtrs(origPos, 4);
            insAt(origPos, (key_char & xF8), mask, 0, 0);
            ret = origPos - trie + 2;
            break;
        case INSERT_LEAF:
            triePos = origPos + ((*origPos & x02) ? 2 : 1);
            *triePos |= mask;
            triePos += ((*origPos & x02 ? BS_BIT_COUNT_CH(origPos[1]) : 0)
                    + BIT_COUNT2(*triePos & (mask - 1)) + 1);
            updatePtrs(triePos, 2);
            insAt(triePos, x00, x00);
            ret = triePos - trie;
            break;
#if BS_MIDDLE_PREFIX == 1
        case INSERT_CONVERT:
              ret = 0;
              uint8_t b, c;
              char cmp_rel;
              diff = triePos - origPos;
              // 3 possible relationships between key_char and *triePos, 4 possible positions of triePos
              c = *triePos;
              cmp_rel = ((c ^ key_char) > x07 ? (c < key_char ? 0 : 1) : 2);
              if (diff == 1)
                  triePos = origPos;
              b = (cmp_rel == 2 ? x04 : x00) | (cmp_rel == 1 ? x00 : x02);
              need_count = (*origPos >> 1) - diff;
              diff--;
              *triePos++ = ((cmp_rel == 0 ? c : key_char) & xF8) | b;
#if BS_CHILD_PTR_SIZE == 1
              b = (cmp_rel == 2 ? 4 : 6);
#else
              b = (cmp_rel == 2 ? 5 : 7);
#endif
              if (diff) {
                  b++;
                  *origPos = (diff << 1) | x01;
              }
              if (need_count)
                  b++;
              insBytes(triePos, b);
              updatePtrs(triePos, b);
              *triePos++ = 1 << ((cmp_rel == 1 ? key_char : c) & x07);
              switch (cmp_rel) {
              case 0:
                  *triePos++ = 0;
#if BS_CHILD_PTR_SIZE == 1
                  *triePos++ = 5;
#else
                  *triePos++ = 0;
                  *triePos++ = 6;
#endif
                  *triePos++ = (key_char & xF8) | 0x04;
                  *triePos++ = mask;
                  ret = triePos - trie;
                  triePos += 2;
                  break;
              case 1:
                  ret = triePos - trie;
                  triePos += 2;
                  *triePos++ = (c & xF8) | x06;
                  *triePos++ = 1 << (c & x07);
                  *triePos++ = 0;
#if BS_CHILD_PTR_SIZE == 1
                  *triePos++ = 1;
#else
                  *triePos++ = 0;
                  *triePos++ = 2;
#endif
                  break;
              case 2:
                  *triePos++ = mask;
#if BS_CHILD_PTR_SIZE == 1
                  *triePos++ = 3;
#else
                  *triePos++ = 0;
                  *triePos++ = 4;
#endif
                  ret = triePos - trie;
                  triePos += 2;
                  break;
              }
              if (need_count)
                  *triePos = (need_count << 1) | x01;
            break;
    #endif
        case INSERT_THREAD:
              uint16_t p, min;
              uint8_t c1, c2;
              uint8_t *childPos;
              uint16_t ptr, pos;
              ret = pos = 0;

              c1 = c2 = key_char;
              p = keyPos;
              min = util::min_b(key_len, keyPos + key_at_len);
              childPos = origPos + 1;
              if (*origPos & x02) {
                  *childPos |= mask;
                  childPos = childPos + 2 + BS_BIT_COUNT_CH(*childPos & (mask - 1));
#if BS_CHILD_PTR_SIZE == 1
                  insAt(childPos, (uint8_t) (BPT_TRIE_LEN - (childPos - trie) + 1));
                  updatePtrs(childPos, 1);
#else
                  int16_t offset = (BS_GET_TRIE_LEN + 1 - (childPos - trie) + 1);
                  insAt(childPos, offset >> 8, offset & xFF);
                  updatePtrs(childPos, 2);
#endif
              } else {
                  *origPos |= x02;
#if BS_CHILD_PTR_SIZE == 1
                  insAt(childPos, mask, *childPos);
                  childPos[2] = (uint8_t) (BPT_TRIE_LEN - (childPos + 2 - trie));
                  updatePtrs(childPos, 2);
#else
                  int16_t offset = BS_GET_TRIE_LEN + 3 - (childPos + 2 - trie);
                  insAt(childPos, mask, *childPos, (uint8_t) (offset >> 8));
                  childPos[3] = offset & xFF;
                  updatePtrs(childPos, 3);
#endif
              }
              triePos = origPos + BS_BIT_COUNT_CH(origPos[1]) + 3
                      + BIT_COUNT2(origPos[2] & (mask - 1));
              ptr = util::getInt(triePos);
              if (p < min) {
                  origPos[2] &= ~mask;
                  delAt(triePos, 2);
                  updatePtrs(triePos, -2);
              } else {
                  pos = triePos - trie;
                  ret = pos;
              }
#if BS_MIDDLE_PREFIX == 1
              need_count -= (9 + BS_CHILD_PTR_SIZE);
#else
              need_count -= 8;
              need_count /= (3 + BS_CHILD_PTR_SIZE);
              need_count--;
#endif
              //diff = p + need_count;
              if (p + need_count == min && need_count) {
                  need_count--;
                  //diff--;
              }
              //diff = (p == min ? 4 : (key[diff] ^ key_at[diff - keyPos]) > x07 ? 8
              //        : (key[diff] == key_at[diff - keyPos] ? 9 + BS_CHILD_PTR_SIZE : 6));
              triePos = trie + BS_GET_TRIE_LEN;
#if BS_MIDDLE_PREFIX == 1
              //diff += need_count + (need_count ? (need_count / 128) + 1 : 0);
#else
              //diff += need_count * (3 + BS_CHILD_PTR_SIZE);
#endif
              //BS_SET_TRIE_LEN(BS_GET_TRIE_LEN + diff);
#if BS_MIDDLE_PREFIX == 1
              //diff += need_count + (need_count ? (need_count / 128) + 1 : 0);
              if (need_count) {
                  uint8_t copied = 0;
                  while (copied < need_count) {
                      int16_t to_copy = (need_count - copied) > 127 ? 127 : need_count - copied;
                      *triePos++ = (to_copy << 1) | x01;
                      memcpy(triePos, key + keyPos + copied, to_copy);
                      triePos += to_copy;
                      copied += to_copy;
                  }
                  p += need_count;
                  //count1 += need_count;
              }
#endif
              while (p < min) {
                  bool isSwapped = false;
                  c1 = key[p];
                  c2 = key_at[p - keyPos];
                  if (c1 > c2) {
                      uint8_t swap = c1;
                      c1 = c2;
                      c2 = swap;
                      isSwapped = true;
                  }
#if BS_MIDDLE_PREFIX == 1
                  switch ((c1 ^ c2) > x07 ? 0 : (c1 == c2 ? 3 : 1)) {
#else
                  switch ((c1 ^ c2) > x07 ?
                          0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
                  case 2:
                      *triePos++ = (c1 & xF8) | x06;
                      *triePos++ = x01 << (c1 & x07);
                      *triePos++ = 0;
#if BS_CHILD_PTR_SIZE == 1
                      *triePos++ = 1;
#else
                      *triePos++ = 0;
                      *triePos++ = 2;
#endif
                      break;
#endif
                  case 0:
                      *triePos++ = c1 & xF8;
                      *triePos++ = x01 << (c1 & x07);
                      ret = isSwapped ? ret : (triePos - trie);
                      pos = isSwapped ? (triePos - trie) : pos;
                      util::setInt(triePos, isSwapped ? ptr : 0);
                      triePos += 2;
                      *triePos++ = (c2 & xF8) | x04;
                      *triePos++ = x01 << (c2 & x07);
                      ret = isSwapped ? (triePos - trie) : ret;
                      pos = isSwapped ? pos : (triePos - trie);
                      util::setInt(triePos, isSwapped ? 0 : ptr);
                      triePos += 2;
                      break;
                  case 1:
                      *triePos++ = (c1 & xF8) | x04;
                      *triePos++ = (x01 << (c1 & x07)) | (x01 << (c2 & x07));
                      ret = isSwapped ? ret : (triePos - trie);
                      pos = isSwapped ? (triePos - trie) : pos;
                      util::setInt(triePos, isSwapped ? ptr : 0);
                      triePos += 2;
                      ret = isSwapped ? (triePos - trie) : ret;
                      pos = isSwapped ? pos : (triePos - trie);
                      util::setInt(triePos, isSwapped ? 0 : ptr);
                      triePos += 2;
                      break;
                  case 3:
                      *triePos++ = (c1 & xF8) | x06;
                      *triePos++ = x01 << (c1 & x07);
                      *triePos++ = x01 << (c1 & x07);
#if BS_CHILD_PTR_SIZE == 1
                      *triePos++ = 3;
#else
                      *triePos++ = 0;
                      *triePos++ = 4;
#endif
                      ret = (p + 1 == key_len) ? (triePos - trie) : ret;
                      pos = (p + 1 == key_len) ? pos : (triePos - trie);
                      util::setInt(triePos, (p + 1 == key_len) ? 0 : ptr);
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
                  *triePos++ = x01 << (c2 & x07);
                  ret = (p == key_len) ? ret : (triePos - trie);
                  pos = (p == key_len) ? (triePos - trie) : pos;
                  util::setInt(triePos, (p == key_len) ? ptr : 0);
                  triePos += 2;
              }
              BS_SET_TRIE_LEN(triePos - trie);
              diff = p - keyPos;
              keyPos = p + (p < key_len ? 1 : 0);
              if (diff < key_at_len)
                  diff++;
              if (diff) {
                  key_at_len -= diff;
                  p = ptr + diff;
                  if (key_at_len >= 0) {
                      current_block[p] = key_at_len;
                      util::setInt(trie + pos, p);
                  }
              }
            break;
        case INSERT_EMPTY:
            trie[0] = (key_char & xF8) | x04;
            trie[1] = mask;
            ret = 2;
            BS_SET_TRIE_LEN(4);
            break;
        }

        if (BPT_MAX_PFX_LEN < keyPos)
            BPT_MAX_PFX_LEN = keyPos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

        return ret;
    }

    void decodeNeedCount(int16_t search_result) {
        insertState = search_result + 1;
        if (insertState != INSERT_THREAD)
            need_count = need_counts[insertState];
    }

};

#endif
/*
uint8_t bfos_node_handler::copyKary(uint8_t *t, uint8_t *dest) {
    uint8_t tc;
    uint8_t tot_len = 0;
    do {
        tc = *t;
        uint8_t len;
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

uint8_t bfos_node_handler::copyTrieFirstHalf(uint8_t *tp, uint8_t *first_key, int16_t first_key_len, uint8_t *dest) {
    uint8_t *t = trie;
    uint8_t trie_len = 0;
    for (int i = 0; i < first_key_len; i++) {
        uint8_t offset = first_key[i] & x07;
        uint8_t *t_end = trie + tp[i];
        memcpy(dest, t, t_end - t);
        uint8_t orig_dest = dest;
        dest += (t_end - t);
        *dest++ = (*t_end | x04);
        if (*t_end & x02) {
            uint8_t children = t_end[1] & ~((i == first_key_len ? xFF : xFE) << offset);
            uint8_t leaves = t_end[2] & ~(xFE << offset);
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
            uint8_t leaves = t_end[1] & ~(xFE << offset);
            *dest++ = leaves;
            memcpy(dest, t_end + 2, BIT_COUNT2(leaves));
            dest += BIT_COUNT2(leaves);
            trie_len += (2 + BIT_COUNT2(leaves));
        }
        while (t <= t_end) {
            uint8_t tc = *t++;
            if (tc & x02) {
                uint8_t children = *t++;
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

uint8_t *bfos_node_handler::copyChildTree(uint8_t *t, uint8_t *orig_dest, uint8_t *dest) {
    uint8_t tp_ch[BX_MAX_KEY_LEN];
    do {
        uint8_t len = copyKary(t + *t, dest);
        orig_dest[t - trie] = dest - (orig_dest + (t - trie));
    } while (1);
}
*/
