#ifndef OCTP_H
#define OCTP_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define OP_MIDDLE_PREFIX 1

#define OCTP_HDR_SIZE 9

#define OP_BIT_COUNT_CH(x) BIT_COUNT2(x)
#define OP_GET_TRIE_LEN util::getInt(BPT_TRIE_LEN_PTR)
#define OP_SET_TRIE_LEN(x) util::setInt(BPT_TRIE_LEN_PTR, x)
#define OP_GET_CHILD_OFFSET(x) util::getInt(x)
#define OP_SET_CHILD_OFFSET(x, off) util::setInt(x, off)

class octp_iterator_vars {
public:
    octp_iterator_vars(uint16_t *tp_ext, byte *t_ext) {
        tp = tp_ext;
        t = t_ext;
        ctr = 9;
        only_leaf = 1;
    }
    uint16_t *tp;
    byte *t;
    byte ctr;
    byte tc;
    byte child;
    byte leaf;
    byte only_leaf;
};

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class octp : public bpt_trie_handler<octp> {
public:
    const static byte need_counts[10];
    byte *last_t;
    byte last_child;
    byte last_leaf;
    int pntr_count;
    byte key_char_pos;
    byte bit_map_len;

    octp(uint16_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint16_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, byte *block = NULL) :
        bpt_trie_handler<octp>(leaf_block_sz, parent_block_sz, cache_sz, fname, block) {
    }

    inline void setCurrentBlockRoot() {
        current_block = root_block;
        trie = current_block + OCTP_HDR_SIZE;
    }

    inline void setCurrentBlock(byte *m) {
        current_block = m;
        trie = current_block + OCTP_HDR_SIZE;
    }

    inline byte *getLastPtr() {
        //keyPos = 0;
        do {
            switch (last_child > last_leaf || (last_leaf ^ last_child) <= last_child ? 2 : 1) {
            case 1:
                return current_block + util::getInt(last_t + (*last_t & x02 ? OP_BIT_COUNT_CH(last_t[1]) + 1 : 0)
                        + BIT_COUNT2(last_leaf));
            case 2:
#if OP_CHILD_PTR_SIZE == 1
                last_t += OP_BIT_COUNT_CH(last_child) + 2;
#else
                last_t++;
                last_t += OP_BIT_COUNT_CH(last_child);
#endif
                last_t += OP_GET_CHILD_OFFSET(last_t);
                while (*last_t & x01) {
                    last_t += (*last_t >> 1);
                    last_t++;
                }
                while (!(*last_t & x04)) {
                    last_t += (*last_t & x02 ? OP_BIT_COUNT_CH(last_t[1])
                         + BIT_COUNT2(last_t[2]) + 3 : BIT_COUNT2(last_t[1]) + 2);
                }
                last_child = (*last_t & x02 ? last_t[1] : 0);
                last_leaf = (*last_t & x02 ? last_t[2] : last_t[1]);
            }
        } while (1);
        return 0;
    }

    inline void setPrefixLast(byte key_char, byte *t, byte pfx_rem_len) {
        if (key_char > *t) {
            t += pfx_rem_len;
            while (*t & x01)
                t += (*t >> 1) + 1;
            while (!(*t & x04)) {
                t += (*t & x02 ? OP_BIT_COUNT_CH(t[1])
                     + BIT_COUNT2(t[2]) + 3 : BIT_COUNT2(t[1]) + 2);
            }
            last_t = t++;
            last_child = (*last_t & x02 ? *t++ : 0);
            last_leaf = *t;
        }
    }

    inline byte peekTrieChar(byte *t) {
        return *t;
    }

    inline byte nextTrieChar(byte **pt) {
        return *(*pt)++;
    }

    inline void skipTrieChars(byte **pt, int count) {
        (*pt) += count;
    }

    // 
    inline int16_t searchCurrentBlock() {
        byte *t = trie;
        byte key_char = *key; //[keyPos++];
        keyPos = 1;
        do {
            origPos = t;

            // kkkkk t00 - octet with bitmap, t = terminator
            // lllll x01 - letter range or letter set, l = length, x = 0 (letter range), x = 1 (letter set)
            // llllx y10 - prefix - x = 

            // octet with bitmap kkkkk 00x, x=terminator
            // only bitmap kkkkk 010
            // letter range kkkkk 011
            // prefix: lllly 10x, x = terminator, y = end with key
            // leaf & child: lllll 110
            // key change: lllll 111
            switch (*t & 0x07) {
              case 0: // octets wiith bitmap
              case 1: // octets wiith bitmap
              case 2: { // only bitmap
                byte trie_char = *t & 0xF8;
                if (key_char < trie_char)
                    return ~INSERT_BEFORE;
                byte bitmap_len;
                byte keychar_pos;
                if (*t & 0x02) {
                    bitmap_len = *(++t);
                    keychar_pos = (key_char - trie_char) >> 3;
                    t++;
                } else {
                    bitmap_len = 0;
                    keychar_pos = 33;
                    do {
                        keychar_pos = ((key_char ^ *t) > 0x07 ? bitmap_len : keychar_pos);
                        bitmap_len++;
                    } while (*t++ & 0x01);
                }
                int ptr_count = 0;
                for (int i = (keychar_pos > bitmap_len ? bitmap_len : keychar_pos); i--;)
                    ptr_count += BIT_COUNT2(*t++);
                if (keychar_pos > bitmap_len) {
                    triePos = t;
                    pntr_count = ptr_count;
                    bit_map_len = bitmap_len;
                    key_char_pos = keychar_pos;
                    last_t = (ptr_count ? t + ptr_count : last_t);
                    return ~INSERT_AFTER;
                }
                byte bit_mask = (1 << (key_char & x07)) - 1;
                ptr_count += BIT_COUNT2(*t & bit_mask); // extra
                bitmap_len -= key_char_pos;
                ptr_count += bitmap_len;
                last_t = (ptr_count ? t + ptr_count: last_t);
                if (!(*t & ++bit_mask)) {
                    triePos = t;
                    pntr_count = ptr_count;
                    bit_map_len = bitmap_len;
                    key_char_pos = keychar_pos;
                    return ~INSERT_LEAF;
                }
                *t += ptr_count;
                pntr_count = ptr_count;
                bit_map_len = bitmap_len;
                key_char_pos = keychar_pos;
                int idx = util::getInt(t);
                if (idx < BPT_TRIE_LEN) {
                  *t += idx;
                  if (keyPos == key_len && *t == 0x06) {
                    idx = util::getInt(t+1);
                  } else {
                    key_char = key[keyPos++];
                    continue;
                  }
                }
                int cmp;
                key_at = current_block + idx;
                key_at_len = *key_at++;
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at, key_at_len);
                if (!cmp)
                    return 0;
                if (cmp < 0)
                    cmp = -cmp;
                else
                    last_t = key_at;
                need_count = cmp + 8 + 2;
                return -INSERT_THREAD;
              }
              case 4:
              case 5: {
                byte pfx_len;
                pfx_len = (*t >> 3);
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
              }
              //case 6: // leaf and child - handled above
              //case 7: // key change
            }
            t++;
        } while (1);
        return -1;
    }

    inline byte *getChildPtrPos(int16_t search_result) {
        return key_at == last_t ? last_t - 1 : getLastPtr();
    }

    inline byte *getPtrPos() {
        return NULL;
    }

    inline int getHeaderSize() {
        return OCTP_HDR_SIZE;
    }

    void setPtrDiff(uint16_t diff) {
        byte *t = trie;
        byte *t_end = trie + OP_GET_TRIE_LEN;
        while (t < t_end) {
            byte tc = *t++;
            if (tc & x01) {
                t += (tc >> 1);
                continue;
            }
            byte leaves = 0;
            if (tc & x02) {
                leaves = t[1];
                t += OP_BIT_COUNT_CH(*t);
                t += 2;
            } else
                leaves = *t++;
            for (int i = 0; i < BIT_COUNT(leaves); i++) {
                util::setInt(t, util::getInt(t) + diff);
                t += 2;
            }
        }
    }

#if OP_CHILD_PTR_SIZE == 1
    byte copyKary(byte *t, byte *dest, int lvl, byte *tp,
            byte *brk_key, int16_t brk_key_len, byte whichHalf) {
#else
    uint16_t copyKary(byte *t, byte *dest, int lvl, uint16_t *tp,
                byte *brk_key, int16_t brk_key_len, byte whichHalf) {
#endif
        byte *orig_dest = dest;
        while (*t & x01) {
            byte len = (*t >> 1) + 1;
            memcpy(dest, t, len);
            dest += len;
            t += len;
        }
        byte *dest_after_prefix = dest;
        byte *limit = trie + (lvl < brk_key_len ? tp[lvl] : 0);
        byte tc;
        byte is_limit = 0;
        do {
            is_limit = (limit == t ? 1 : 0); // && is_limit == 0 ? 1 : 0);
            if (limit == t && whichHalf == 2)
                dest = dest_after_prefix;
            tc = *t;
            if (is_limit) {
                *dest++ = tc;
                byte offset = (lvl < brk_key_len ? brk_key[lvl] & x07 : x07);
                t++;
                byte children = (tc & x02) ? *t++ : 0;
                byte orig_children = children;
                byte leaves = *t++;
                byte orig_leaves = leaves;
                if (whichHalf == 1) {
                    children &= ~(((brk_key_len - lvl) == 1 ? xFF : xFE) << offset);
                    leaves &= ~(xFE << offset);
                    *(dest - 1) |= x04;
                    if (tc & x02)
                        *dest++ = children;
                    *dest++ = leaves;
                    for (int i = 0; i < BIT_COUNT(children); i++) {
#if OP_CHILD_PTR_SIZE == 1
                        *dest++ = t[i];
#else
                        util::setInt(dest, util::getInt(t + i * 2));
                        dest += 2;
#endif
                    }
                    t += OP_BIT_COUNT_CH(orig_children);
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
#if OP_CHILD_PTR_SIZE == 1
                        *dest++ = t[i];
#else
                        util::setInt(dest, util::getInt(t + i * 2));
                        dest += 2;
#endif
                    }
                    t += OP_BIT_COUNT_CH(orig_children);
                    memcpy(dest, t + BIT_COUNT2(orig_leaves - leaves), BIT_COUNT2(leaves));
                    dest += BIT_COUNT2(leaves);
                }
                t += BIT_COUNT2(orig_leaves);
            } else {
                byte len = (tc & x02) ? 3 + OP_BIT_COUNT_CH(t[1]) + BIT_COUNT2(t[2])
                        : 2 + BIT_COUNT2(t[1]);
                memcpy(dest, t, len);
                t += len;
                dest += len;
            }
        } while (!(tc & x04));
        return dest - orig_dest;
    }

#if OP_CHILD_PTR_SIZE == 1
    byte copyTrieHalf(byte *tp, byte *brk_key, int16_t brk_key_len, byte *dest, byte whichHalf) {
        byte tp_child[BPT_MAX_PFX_LEN];
#else
    uint16_t copyTrieHalf(uint16_t *tp, byte *brk_key, int16_t brk_key_len, byte *dest, byte whichHalf) {
        uint16_t tp_child[BPT_MAX_PFX_LEN];
#endif
        byte child_num[BPT_MAX_PFX_LEN];
        byte *d;
        byte *t = trie;
        byte *new_trie = dest;
        int lvl = 0;
        while (*t & x01) {
            byte len = (*t >> 1);
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
            byte tc = *d++;
            while (tc & x01) {
                d += (tc >> 1);
                tc = *d++;
            }
            if (tc & x02) {
                byte len = BIT_COUNT(*d++);
                if (len) {
                    d++;
                    tp_child[lvl] = d - new_trie - 3;
                    byte *child = trie + OP_GET_CHILD_OFFSET(d);
                    OP_SET_CHILD_OFFSET(d, dest - d);
                    child_num[lvl++] = 0;
                    t = child;
                    while (*t & x01) {
                        byte len = (*t >> 1);
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
                byte len = BIT_COUNT(d[1]);
                byte i = child_num[lvl];
                i++;
                if (i < len) {
#if OP_CHILD_PTR_SIZE == 1
                    d += (i + 3);
#else
                    d += (i * 2 + 3);
#endif
                    byte *child = trie + OP_GET_CHILD_OFFSET(d);
                    OP_SET_CHILD_OFFSET(d, dest - d);
                    child_num[lvl++] = i;
                    t = child;
                    while (*t & x01) {
                        byte len = (*t >> 1);
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
                    d += (tc & x02 ? 3 + OP_BIT_COUNT_CH(d[1]) + BIT_COUNT2(d[2]) :
                            2 + BIT_COUNT2(d[1]));
                    break;
                }
            }
        } while (1);
        return 0;
    }

    void consolidateInitialPrefix(byte *t) {
        t += OCTP_HDR_SIZE;
        byte *t_reader = t;
        byte count = 0;
        if (*t & x01) {
            count = (*t >> 1);
            t_reader += count;
            t_reader++;
        }
        byte *t_writer = t_reader + (*t & x01 ? 0 : 1);
        byte trie_len_diff = 0;
#if OP_CHILD_PTR_SIZE == 1
        while ((*t_reader & x01) || ((*t_reader & x02) && (*t_reader & x04) && BIT_COUNT(t_reader[1]) == 1
                && BIT_COUNT(t_reader[2]) == 0 && t_reader[3] == 1)) {
#else
        while ((*t_reader & x01) ||
                ((*t_reader & x02) && (*t_reader & x04) && BIT_COUNT(t_reader[1]) == 1
                && BIT_COUNT(t_reader[2]) == 0 && t_reader[3] == 0 && t_reader[4] == 2)) {
#endif
            if (*t_reader & x01) {
                byte len = *t_reader >> 1;
                if (count + len > 127)
                    break;
                memcpy(t_writer, ++t_reader, len);
                t_writer += len;
                t_reader += len;
                count += len;
                trie_len_diff++;
            } else {
                if (count > 126)
                    break;
                *t_writer++ = (*t_reader & xF8) + BIT_COUNT(t_reader[1] - 1);
                count++;
#if OP_CHILD_PTR_SIZE == 1
                t_reader += 4;
                trie_len_diff += 3;
#else
                t_reader += 5;
                trie_len_diff += 4;
#endif
            }
        }
        if (t_reader > t_writer) {
            memmove(t_writer, t_reader, OP_GET_TRIE_LEN - (t_reader - t));
            if (!(*t & x01))
                trie_len_diff--;
            *t = (count << 1) + 1;
            OP_SET_TRIE_LEN(OP_GET_TRIE_LEN - trie_len_diff);
            //cout << (int) (*t >> 1) << endl;
        }
    }

#if OP_CHILD_PTR_SIZE == 1
    byte *nextPtr(byte *first_key, octp_iterator_vars& it) {
#else
    byte *nextPtr(byte *first_key, octp_iterator_vars& it) {
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
                    it.t += OP_BIT_COUNT_CH(it.child);
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
                    byte len = it.tc >> 1;
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
                it.t += OP_BIT_COUNT_CH(it.child);
                it.ctr = FIRST_BIT_OFFSET_FROM_RIGHT(it.child | it.leaf);
            }
            first_key[keyPos] = (it.tc & xF8) | it.ctr;
            byte mask = x01 << it.ctr;
            if ((it.leaf & mask) && it.only_leaf) {
                if (it.child & mask)
                    it.only_leaf = 0;
                else
                    it.ctr++;
                return it.t + BIT_COUNT2(it.leaf & (mask - 1));
            }
            it.only_leaf = 1;
            if (it.child & mask) {
                it.t -= OP_BIT_COUNT_CH(it.child & (xFF << it.ctr));
                uint16_t child_offset = OP_GET_CHILD_OFFSET(it.t);
                OP_SET_CHILD_OFFSET(it.t, it.t - trie + child_offset);
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

    byte *split(byte *first_key, int16_t *first_len_ptr) {
        int16_t orig_filled_size = filledSize();
        const uint16_t OCTP_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
        byte *b = allocateBlock(OCTP_NODE_SIZE);
        octp new_block(this->leaf_block_size, this->parent_block_size, 0, NULL, b);
        new_block.setKVLastPos(OCTP_NODE_SIZE);
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        uint16_t kv_last_pos = getKVLastPos();
        uint16_t halfKVPos = kv_last_pos + (OCTP_NODE_SIZE - kv_last_pos) / 2;

        int16_t brk_idx;
        uint16_t brk_kv_pos;
        brk_idx = brk_kv_pos = 0;
        // (1) move all data to new_block in order
        int16_t idx;
        byte alloc_size = BPT_MAX_PFX_LEN + 1;
        byte curr_key[alloc_size];
#if OP_CHILD_PTR_SIZE == 1
        byte tp[alloc_size];
        byte tp_cpy[alloc_size];
#else
        uint16_t tp[alloc_size];
        uint16_t tp_cpy[alloc_size];
#endif
        int16_t tp_cpy_len = 0;
        octp_iterator_vars it(tp, new_block.trie);
        //if (!isLeaf())
        //   cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
        new_block.keyPos = 0;
        memcpy(new_block.trie, trie, OP_GET_TRIE_LEN);
#if OP_CHILD_PTR_SIZE == 1
        new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
#else
        util::setInt(new_block.BPT_TRIE_LEN_PTR, OP_GET_TRIE_LEN);
#endif
        for (idx = 0; idx < orig_filled_size; idx++) {
            byte *leaf_ptr = new_block.nextPtr(curr_key, it);
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
                    *first_len_ptr = util::compare((const char *) curr_key, new_block.keyPos,
                            (const char *) first_key, *first_len_ptr);
                    memcpy(first_key, curr_key, tp_cpy_len);
                } else {
                    memcpy(first_key, curr_key, new_block.keyPos);
                    memcpy(first_key + new_block.keyPos, current_block + src_idx + 1, current_block[src_idx]);
                    *first_len_ptr = new_block.keyPos + current_block[src_idx];
                }
                memcpy(tp_cpy, tp, tp_cpy_len); // * OP_CHILD_PTR_SIZE);
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
                    OP_SET_TRIE_LEN(new_block.copyTrieHalf(tp, first_key, *first_len_ptr, trie, 1));
                }
            }
        }
#if OP_CHILD_PTR_SIZE == 1
        new_block.BPT_TRIE_LEN = new_block.copyTrieHalf(tp_cpy, first_key, tp_cpy_len, trie + BPT_TRIE_LEN, 2);
        memcpy(new_block.trie, trie + BPT_TRIE_LEN, new_block.BPT_TRIE_LEN);
#else
        util::setInt(new_block.BPT_TRIE_LEN_PTR, new_block.copyTrieHalf(tp_cpy, first_key,
                tp_cpy_len, trie + OP_GET_TRIE_LEN, 2));
        memcpy(new_block.trie, trie + OP_GET_TRIE_LEN, util::getInt(new_block.BPT_TRIE_LEN_PTR));
#endif

        kv_last_pos = getKVLastPos() + OCTP_NODE_SIZE - kv_last_pos;
        uint16_t diff = (kv_last_pos - getKVLastPos());

        {
            memmove(new_block.current_block + brk_kv_pos + diff, new_block.current_block + brk_kv_pos,
                    OCTP_NODE_SIZE - brk_kv_pos - diff);
            brk_kv_pos += diff;
            new_block.setPtrDiff(diff);
            new_block.setKVLastPos(brk_kv_pos);
            new_block.setFilledSize(orig_filled_size - brk_idx);
        }

        {
            uint16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + OCTP_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos - diff, old_blk_new_len); // Copy back first half to old block
            diff += (OCTP_NODE_SIZE - brk_kv_pos);
            setPtrDiff(diff);
            setKVLastPos(OCTP_NODE_SIZE - old_blk_new_len);
            setFilledSize(brk_idx);
        }

        if (!isLeaf())
            new_block.setLeaf(false);

#if OP_MIDDLE_PREFIX == 1
        consolidateInitialPrefix(current_block);
        new_block.consolidateInitialPrefix(new_block.current_block);
#endif

        //keyPos = 0;

        return new_block.current_block;

    }

    bool isFull(int16_t search_result) {
        decodeNeedCount(search_result);
        if (getKVLastPos() < (OCTP_HDR_SIZE + OP_GET_TRIE_LEN
                + need_count + key_len - keyPos + value_len + 3))
            return true;
#if OP_CHILD_PTR_SIZE == 1
        if (OP_GET_TRIE_LEN > 254 - need_count)
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

    inline void updatePtrs(byte *upto, int diff) {
        byte *t = trie;
        while (t < upto) {
            byte tc = *t++;
            if (tc & x01) {
                t += (tc >> 1);
                continue;
            }
            if (!(tc & x02)) {
                t += BIT_COUNT2(*t) + 1;
                continue;
            }
            int count = BIT_COUNT(*t++);
            byte leaves = *t++;
            while (count--) {
                if (t >= upto)
                    return;
#if OP_CHILD_PTR_SIZE == 1
                if ((t + *t) >= upto)
                    *t += diff;
                if (insertState == INSERT_BEFORE) {
                    if (keyPos > 1 && (t + *t) == (origPos + 4))
                        *t -= 4;
                }
                t++;
#else
                uint16_t child_offset = OP_GET_CHILD_OFFSET(t);
                if ((t + child_offset) >= upto)
                    OP_SET_CHILD_OFFSET(t, child_offset + diff);
                if (insertState == INSERT_BEFORE) {
                    if (keyPos > 1 && (t + OP_GET_CHILD_OFFSET(t)) == (origPos + 4))
                        OP_SET_CHILD_OFFSET(t, OP_GET_CHILD_OFFSET(t) - 4);
                }
                t += 2;
#endif
                // todo: avoid inside loops
            }
            t += BIT_COUNT2(leaves);
        }
    }

    inline void delAt(byte *ptr, int16_t count) {
        int16_t trie_len = OP_GET_TRIE_LEN - count;
        OP_SET_TRIE_LEN(trie_len);
        memmove(ptr, ptr + count, trie + trie_len - ptr);
    }

    inline int16_t insAt(byte *ptr, byte b) {
        int16_t trie_len = OP_GET_TRIE_LEN;
        memmove(ptr + 1, ptr, trie + trie_len - ptr);
        *ptr = b;
        OP_SET_TRIE_LEN(trie_len + 1);
        return 1;
    }

    inline int16_t insAt(byte *ptr, byte b1, byte b2) {
        int16_t trie_len = OP_GET_TRIE_LEN;
        memmove(ptr + 2, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr = b2;
        OP_SET_TRIE_LEN(trie_len + 2);
        return 2;
    }

    inline int16_t insAt(byte *ptr, byte b1, byte b2, byte b3) {
        int16_t trie_len = OP_GET_TRIE_LEN;
        memmove(ptr + 3, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        OP_SET_TRIE_LEN(trie_len + 3);
        return 3;
    }

    inline byte insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4) {
        int16_t trie_len = OP_GET_TRIE_LEN;
        memmove(ptr + 4, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        OP_SET_TRIE_LEN(trie_len + 4);
        return 4;
    }

    void insBytes(byte *ptr, int16_t len) {
        int16_t trie_len = OP_GET_TRIE_LEN;
        memmove(ptr + len, ptr, trie + trie_len - ptr);
        OP_SET_TRIE_LEN(trie_len + len);
    }

    uint16_t insertCurrent() {
        byte key_char, mask;
        uint16_t diff;
        uint16_t ret;

        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        switch (insertState) {
        case INSERT_AFTER:
            *origPos &= xFB;
            triePos = origPos + (*origPos & x02 ? OP_BIT_COUNT_CH(origPos[1])
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
            triePos += ((*origPos & x02 ? OP_BIT_COUNT_CH(origPos[1]) : 0)
                    + BIT_COUNT2(*triePos & (mask - 1)) + 1);
            updatePtrs(triePos, 2);
            insAt(triePos, x00, x00);
            ret = triePos - trie;
            break;
#if OP_MIDDLE_PREFIX == 1
        case INSERT_CONVERT:
              ret = 0;
              byte b, c;
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
#if OP_CHILD_PTR_SIZE == 1
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
#if OP_CHILD_PTR_SIZE == 1
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
#if OP_CHILD_PTR_SIZE == 1
                  *triePos++ = 1;
#else
                  *triePos++ = 0;
                  *triePos++ = 2;
#endif
                  break;
              case 2:
                  *triePos++ = mask;
#if OP_CHILD_PTR_SIZE == 1
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
              byte c1, c2;
              byte *childPos;
              uint16_t ptr, pos;
              ret = pos = 0;

              c1 = c2 = key_char;
              p = keyPos;
              min = util::min_b(key_len, keyPos + key_at_len);
              childPos = origPos + 1;
              if (*origPos & x02) {
                  *childPos |= mask;
                  childPos = childPos + 2 + OP_BIT_COUNT_CH(*childPos & (mask - 1));
#if OP_CHILD_PTR_SIZE == 1
                  insAt(childPos, (byte) (BPT_TRIE_LEN - (childPos - trie) + 1));
                  updatePtrs(childPos, 1);
#else
                  int16_t offset = (OP_GET_TRIE_LEN + 1 - (childPos - trie) + 1);
                  insAt(childPos, offset >> 8, offset & xFF);
                  updatePtrs(childPos, 2);
#endif
              } else {
                  *origPos |= x02;
#if OP_CHILD_PTR_SIZE == 1
                  insAt(childPos, mask, *childPos);
                  childPos[2] = (byte) (BPT_TRIE_LEN - (childPos + 2 - trie));
                  updatePtrs(childPos, 2);
#else
                  int16_t offset = OP_GET_TRIE_LEN + 3 - (childPos + 2 - trie);
                  insAt(childPos, mask, *childPos, (byte) (offset >> 8));
                  childPos[3] = offset & xFF;
                  updatePtrs(childPos, 3);
#endif
              }
              triePos = origPos + OP_BIT_COUNT_CH(origPos[1]) + 3
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
#if OP_MIDDLE_PREFIX == 1
              need_count -= (9 + 0); //OP_CHILD_PTR_SIZE);
#else
              need_count -= 8;
              need_count /= (3 + OP_CHILD_PTR_SIZE);
              need_count--;
#endif
              //diff = p + need_count;
              if (p + need_count == min && need_count) {
                  need_count--;
                  //diff--;
              }
              //diff = (p == min ? 4 : (key[diff] ^ key_at[diff - keyPos]) > x07 ? 8
              //        : (key[diff] == key_at[diff - keyPos] ? 9 + OP_CHILD_PTR_SIZE : 6));
              triePos = trie + OP_GET_TRIE_LEN;
#if OP_MIDDLE_PREFIX == 1
              //diff += need_count + (need_count ? (need_count / 128) + 1 : 0);
#else
              //diff += need_count * (3 + OP_CHILD_PTR_SIZE);
#endif
              //OP_SET_TRIE_LEN(OP_GET_TRIE_LEN + diff);
#if OP_MIDDLE_PREFIX == 1
              //diff += need_count + (need_count ? (need_count / 128) + 1 : 0);
              if (need_count) {
                  byte copied = 0;
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
                      byte swap = c1;
                      c1 = c2;
                      c2 = swap;
                      isSwapped = true;
                  }
#if OP_MIDDLE_PREFIX == 1
                  switch ((c1 ^ c2) > x07 ? 0 : (c1 == c2 ? 3 : 1)) {
#else
                  switch ((c1 ^ c2) > x07 ?
                          0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
                  case 2:
                      *triePos++ = (c1 & xF8) | x06;
                      *triePos++ = x01 << (c1 & x07);
                      *triePos++ = 0;
#if OP_CHILD_PTR_SIZE == 1
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
#if OP_CHILD_PTR_SIZE == 1
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
              OP_SET_TRIE_LEN(triePos - trie);
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
            OP_SET_TRIE_LEN(4);
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