#ifndef dfos_H
#define dfos_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define DS_MIDDLE_PREFIX 1

#if BPT_9_BIT_PTR == 1
#define DS_MAX_PTR_BITMAP_BYTES 8
#define DS_MAX_PTRS 63
#else
#define DS_MAX_PTR_BITMAP_BYTES 0
#define DS_MAX_PTRS 255
#endif
#define DFOS_HDR_SIZE 9
//#define MID_KEY_LEN buf[DS_MAX_PTR_BITMAP_BYTES+6]

#if (defined(__AVR_ATmega328P__))
#define DS_SIBLING_PTR_SIZE 1
#else
#define DS_SIBLING_PTR_SIZE 2
#endif
#if DS_SIBLING_PTR_SIZE == 1
#define DS_GET_TRIE_LEN BPT_TRIE_LEN
#define DS_SET_TRIE_LEN(x) BPT_TRIE_LEN = x
#define DS_GET_SIBLING_OFFSET(x) *(x)
#define DS_SET_SIBLING_OFFSET(x, off) *(x) = off
#else
#define DS_GET_TRIE_LEN util::get_int(BPT_TRIE_LEN_PTR)
#define DS_SET_TRIE_LEN(x) util::set_int(BPT_TRIE_LEN_PTR, x)
#define DS_GET_SIBLING_OFFSET(x) (util::get_int(x) & 0x3FFF)
#define DS_SET_SIBLING_OFFSET(x, off) util::set_int(x, off + (((*x) & 0xC0) << 8))
#undef DS_MAX_PTRS
#define DS_MAX_PTRS 1023
#endif

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class dfos : public bpt_trie_handler<dfos> {
public:
#if DS_SIBLING_PTR_SIZE == 1
    uint8_t pos, key_at_pos;
#else
    int pos, key_at_pos;
#endif
    const static uint8_t need_counts[10];

    dfos(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
        bpt_trie_handler<dfos>(leaf_block_sz, parent_block_sz, cache_sz, fname) {
    }

    dfos(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bpt_trie_handler<dfos>(block_sz, block, is_leaf) {
        init_stats();
    }

    inline void set_current_block_root() {
        set_current_block(root_block);
    }

    inline void set_current_block(uint8_t *m) {
        current_block = m;
        trie = current_block + DFOS_HDR_SIZE + DS_MAX_PTR_BITMAP_BYTES;
#if BPT_9_BIT_PTR == 1
#if BPT_INT64MAP == 1
        bitmap = (uint64_t *) (current_block + get_header_size() - 8);
#else
        bitmap1 = (uint32_t *) (current_block + get_header_size() - 8);
        bitmap2 = bitmap1 + 1;
#endif
#endif
    }

    inline uint8_t *skip_children(uint8_t *t, uint8_t count) {
        while (count) {
            uint8_t tc = *t++;
            switch (tc & x03) {
            case 0:
                pos += BIT_COUNT(*t++);
                break;
            case 2:
                pos += *t++;
#if DS_SIBLING_PTR_SIZE == 2
                pos += ((*t & xC0) << 2);
#endif
                t += DS_GET_SIBLING_OFFSET(t);
                break;
            case 1:
            case 3:
                t += (tc >> 1);
                continue;
            }
            if (tc & x04)
                count--;
        }
        return t;
    }

    inline int search_current_block() {
        uint8_t key_char = *key;
        uint8_t *t = trie;
        uint8_t trie_char = *t;
        key_pos = 1;
        orig_pos = t++;
        pos = 0;
        do {
            switch (trie_char & x01 ? 3 : (key_char ^ trie_char) > x07 ? (key_char > trie_char ? 0 : 2) : 1) {
            case 0:
                if (trie_char & x02) {
                    pos += *t++;
#if DS_SIBLING_PTR_SIZE == 2
                    pos += ((*t & xC0) << 2);
#endif
                    t += DS_GET_SIBLING_OFFSET(t);
                } else
                    pos += BIT_COUNT(*t++);
                if (trie_char & x04) {
                    insert_state = INSERT_AFTER;
                    return ~pos;
                }
                break;
            case 1:
                if (trie_char & x02) {
                    t += (1 + DS_SIBLING_PTR_SIZE);
                    uint8_t r_children = *t++;
                    uint8_t r_leaves = *t++;
                    uint8_t r_mask = x01 << (key_char & x07);
                    trie_char = (r_leaves & r_mask ? x02 : x00) |
                            ((r_children & r_mask) && key_pos != key_len ? x01 : x00);
                    //key_char = r_leaves & r_mask ?
                    //        (r_children & r_mask ? (key_pos == key_len ? x01 : x03) : x01) :
                    //        (r_children & r_mask ? (key_pos == key_len ? x00 : x02) : x00);
                    r_mask--;
                    pos += BIT_COUNT(r_leaves & r_mask);
                    t = skip_children(t, BIT_COUNT(r_children & r_mask));
                } else {
                    uint8_t r_leaves = *t++;
                    uint8_t r_mask = x01 << (key_char & x07);
                    trie_char = (r_leaves & r_mask) ? x02 : x00;
                    pos += BIT_COUNT(r_leaves & (r_mask - 1));
                }
                switch (trie_char) {
                case 0:
                    insert_state = INSERT_LEAF;
                    return ~pos;
                case 1:
                    break;
                case 2:
                    int cmp;
                    key_at = get_key(pos, &key_at_len);
                    cmp = util::compare(key + key_pos, key_len - key_pos, key_at, key_at_len);
                    if (cmp == 0)
                        return pos;
                    key_at_pos = pos;
                    if (cmp > 0)
                        pos++;
                    else
                        cmp = -cmp;
                    trie_pos = t;
                    insert_state = INSERT_THREAD;
#if DS_MIDDLE_PREFIX == 1
                    need_count = cmp + (7 + DS_SIBLING_PTR_SIZE);
#else
                    need_count = (cmp * (4 + DS_SIBLING_PTR_SIZE))
                            + 4 + DS_SIBLING_PTR_SIZE;
#endif
                    return ~pos;
                case 3:
                    pos++;
                }
                key_char = key[key_pos++];
                break;
#if DS_MIDDLE_PREFIX == 1
            case 3:
                uint8_t pfx_len;
                pfx_len = (trie_char >> 1);
                while (pfx_len && key_char == *t && key_pos < key_len) {
                    key_char = key[key_pos++];
                    t++;
                    pfx_len--;
                }
                if (!pfx_len)
                    break;
                trie_pos = t;
                if (key_char > *t)
                    key_at = skip_children(t + pfx_len, 1);
                insert_state = INSERT_CONVERT;
                return ~pos;
#endif
            case 2:
                insert_state = INSERT_BEFORE;
                return ~pos;
            }
            trie_char = *t;
            orig_pos = t++;
        } while (1);
        return ~pos;
    }

    inline int get_header_size() {
        return DFOS_HDR_SIZE + DS_MAX_PTR_BITMAP_BYTES;
    }

    inline uint8_t *get_ptr_pos() {
        return trie + DS_GET_TRIE_LEN;
    }

    inline uint8_t *get_child_ptr_pos(int search_result) {
        if (search_result < 0) {
            search_result++;
            search_result = ~search_result;
        }
        return current_block + get_ptr(search_result);
    }

#if DS_SIBLING_PTR_SIZE == 1
    uint8_t *next_key(uint8_t *first_key, uint8_t *tp, uint8_t *t, char& ctr, uint8_t& tc, uint8_t& child, uint8_t& leaf) {
#else
    uint8_t *next_key(uint8_t *first_key, int *tp, uint8_t *t, char& ctr, uint8_t& tc, uint8_t& child, uint8_t& leaf) {
#endif
        do {
            while (ctr > x07) {
                if (tc & x04) {
                    key_pos--;
                    tc = trie[tp[key_pos]];
                    while (tc & x01) {
                        key_pos--;
                        tc = trie[tp[key_pos]];
                    }
                    child = (tc & x02 ? trie[tp[key_pos] + 2 + DS_SIBLING_PTR_SIZE] : 0);
                    leaf = trie[tp[key_pos] + (tc & x02 ? 3 + DS_SIBLING_PTR_SIZE : 1)];
                    ctr = first_key[key_pos] & 0x07;
                    ctr++;
                } else {
                    tp[key_pos] = t - trie;
                    tc = *t++;
                    while (tc & x01) {
                        uint8_t len = tc >> 1;
                        for (int i = 0; i < len; i++)
                            tp[key_pos + i] = t - trie - 1;
                        memcpy(first_key + key_pos, t, len);
                        t += len;
                        key_pos += len;
                        tp[key_pos] = t - trie;
                        tc = *t++;
                    }
                    t += (tc & x02 ? 1 + DS_SIBLING_PTR_SIZE : 0);
                    child = (tc & x02 ? *t++ : 0);
                    leaf = *t++;
                    ctr = 0;
                }
            }
            first_key[key_pos] = (tc & xF8) | ctr;
            uint8_t mask = x01 << ctr;
            if (leaf & mask) {
                leaf &= ~mask;
                if (0 == (child & mask))
                    ctr++;
                return t;
            }
            if (child & mask) {
                key_pos++;
                ctr = x08;
                tc = 0;
            }
            ctr++;
        } while (1); // (t - trie) < DS_GET_TRIE_LEN);
        return t;
    }

#if DS_SIBLING_PTR_SIZE == 1
    void delete_trie_last_half(int brk_key_len, uint8_t *first_key, uint8_t *tp) {
        uint8_t orig_trie_len = DS_GET_TRIE_LEN;
#else
    void delete_trie_last_half(int brk_key_len, uint8_t *first_key, int *tp) {
        int orig_trie_len = DS_GET_TRIE_LEN;
#endif
        for (int idx = brk_key_len; idx >= 0; idx--) {
            uint8_t *t = trie + tp[idx];
            uint8_t tc = *t;
            if (tc & x01)
                continue;
            uint8_t offset = first_key[idx] & x07;
            *t++ = (tc | x04);
            if (tc & x02) {
                uint8_t children = t[1 + DS_SIBLING_PTR_SIZE];
                children &= ~((idx == brk_key_len ? xFF : xFE) << offset);
                                // ryte_mask[offset] : ryte_incl_mask[offset]);
                t[1 + DS_SIBLING_PTR_SIZE] = children;
                uint8_t leaves = t[2 + DS_SIBLING_PTR_SIZE] & ~(xFE << offset);
                t[2 + DS_SIBLING_PTR_SIZE] = leaves; // ryte_incl_mask[offset];
                pos = BIT_COUNT(leaves);
                uint8_t *new_t = skip_children(t + 3 + DS_SIBLING_PTR_SIZE, BIT_COUNT(children));
                *t++ = pos;
#if DS_SIBLING_PTR_SIZE == 2
                *t = (pos & 0x0300) >> 2;
#endif
                DS_SET_SIBLING_OFFSET(t, new_t - t);
                t = new_t;
            } else
                *t++ &= ~(xFE << offset); // ryte_incl_mask[offset];
            if (idx == brk_key_len)
                DS_SET_TRIE_LEN(t - trie);
        }
        move_ptr_list(orig_trie_len);
    }

    int delete_segment(uint8_t *delete_end, uint8_t *delete_start) {
        int count = delete_end - delete_start;
        if (count) {
            DS_SET_TRIE_LEN(DS_GET_TRIE_LEN - count);
            memmove(delete_start, delete_end, trie + DS_GET_TRIE_LEN - delete_start);
        }
        return count;
    }

#if DS_SIBLING_PTR_SIZE == 1
    void delete_trie_first_half(int brk_key_len, uint8_t *first_key, uint8_t *tp) {
        uint8_t orig_trie_len = DS_GET_TRIE_LEN;
#else
    void delete_trie_first_half(int brk_key_len, uint8_t *first_key, int *tp) {
        int orig_trie_len = DS_GET_TRIE_LEN;
#endif
        for (int idx = brk_key_len; idx >= 0; idx--) {
            uint8_t *t = trie + tp[idx];
            uint8_t tc = *t++;
            if (tc & x01) {
                uint8_t len = tc >> 1;
                delete_segment(trie + tp[idx + 1], t + len);
                idx -= len;
                idx++;
                continue;
            }
            uint8_t offset = first_key[idx] & 0x07;
            if (tc & x02) {
                uint8_t children = t[1 + DS_SIBLING_PTR_SIZE];
                int count = BIT_COUNT(children & ~(xFF << offset)); // ryte_mask[offset];
                children &= (xFF << offset); // left_incl_mask[offset];
                t[1 + DS_SIBLING_PTR_SIZE] = children;
                uint8_t leaves = t[2 + DS_SIBLING_PTR_SIZE] & ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
                t[2 + DS_SIBLING_PTR_SIZE] = leaves;
                uint8_t *new_t = skip_children(t + 3 + DS_SIBLING_PTR_SIZE, count);
                delete_segment((idx < brk_key_len) ? (trie + tp[idx + 1]) : new_t, t + 3 + DS_SIBLING_PTR_SIZE);
                pos = BIT_COUNT(leaves);
                new_t = skip_children(t + 3 + DS_SIBLING_PTR_SIZE, BIT_COUNT(children));
                *t++ = pos;
#if DS_SIBLING_PTR_SIZE == 2
                *t = (pos & 0x0300) >> 2;
#endif
                DS_SET_SIBLING_OFFSET(t, new_t - t);
            } else
                *t &= ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
        }
        delete_segment(trie + tp[0], trie);
        move_ptr_list(orig_trie_len);
    }

    void update_skip_lens(uint8_t *loop_upto, uint8_t *covering_upto, int diff) {
        uint8_t *t = trie;
        while (t <= loop_upto) {
            uint8_t tc = *t++;
            if (tc & x01) {
                t += (tc >> 1);
                continue;
            }
            t++;
            if (tc & x02) {
                if ((t + DS_GET_SIBLING_OFFSET(t)) > covering_upto) {
                    // can be improved
                    DS_SET_SIBLING_OFFSET(t, DS_GET_SIBLING_OFFSET(t) + diff);
                    (*(t - 1))++;
#if DS_SIBLING_PTR_SIZE == 2
                    if (*(t - 1) == 0)
                        (*t) += 0x40;
#endif
                    t += (2 + DS_SIBLING_PTR_SIZE);
                } else
                    t += DS_GET_SIBLING_OFFSET(t);
            }
        }
    }

    void add_first_data() {
        add_data(0);
    }

    void add_data(int search_result) {

        insert_current();

        int key_left = key_len - key_pos;
        int kv_last_pos = get_kv_last_pos() - (key_left + value_len + 2);
        set_kv_last_pos(kv_last_pos);
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + key_pos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);

        ins_ptr(search_result, kv_last_pos);

    }

    bool is_full(int search_result) {
        decode_need_count(search_result);
        int ptr_size = filled_size() + 1;
#if BPT_9_BIT_PTR == 0
        ptr_size <<= 1;
#endif
        if (get_kv_last_pos()
                < (DFOS_HDR_SIZE + DS_MAX_PTR_BITMAP_BYTES + DS_GET_TRIE_LEN
                        + need_count + ptr_size + key_len - key_pos + value_len + 3))
            return true;
        if (filled_size() >= DS_MAX_PTRS)
            return true;
#if DS_SIBLING_PTR_SIZE == 1
        if (DS_GET_TRIE_LEN > (254 - need_count))
            return true;
#endif
        return false;
    }

    uint8_t *find_split_source(int search_result) {
        return NULL;
    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int orig_filled_size = filled_size();
        const int DFOS_NODE_SIZE = is_leaf() ? leaf_block_size : parent_block_size;
        uint8_t *b = allocate_block(DFOS_NODE_SIZE, is_leaf(), current_block[0] & 0x1F);
        dfos new_block(DFOS_NODE_SIZE, b, is_leaf());
        memcpy(new_block.trie, trie, DS_GET_TRIE_LEN);
#if DS_SIBLING_PTR_SIZE == 1
        new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
#else
        util::set_int(new_block.BPT_TRIE_LEN_PTR, DS_GET_TRIE_LEN);
#endif
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        int kv_last_pos = get_kv_last_pos();
        int half_kVLen = DFOS_NODE_SIZE - kv_last_pos + 1;
        half_kVLen /= 2;

        int brk_idx = -1;
        int brk_kv_pos;
        int tot_len;
        brk_kv_pos = tot_len = 0;
        char ctr = x08;
#if DS_SIBLING_PTR_SIZE == 1
        uint8_t tp[BPT_MAX_PFX_LEN + 1];
#else
        int tp[BPT_MAX_PFX_LEN + 1];
#endif
        uint8_t last_key[BPT_MAX_PFX_LEN + 1];
        int last_key_len = 0;
        uint8_t *t = trie;
        uint8_t tc, child, leaf;
        tc = child = leaf = 0;
        //if (!is_leaf())
        //   cout << "Trie len:" << (int) DS_GET_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DS_MAX_KEY_LEN << endl;
        key_pos = 0;
        // (1) move all data to new_block in order
        int idx;
        for (idx = 0; idx < orig_filled_size; idx++) {
            int src_idx = get_ptr(idx);
            int kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            tot_len += kv_len;
            memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
            kv_last_pos += kv_len;
            if (brk_idx == -1) {
                t = next_key(first_key, tp, t, ctr, tc, child, leaf);
                //brk_key_len = next_key(s);
                //memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
                //first_key[key_pos+1+current_block[src_idx]] = 0;
                //cout << first_key << endl;
                //if (tot_len > half_kVLen) {
                if (tot_len > half_kVLen || idx == (orig_filled_size / 2)) {
                    //memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[key_pos+1+current_block[src_idx]] = 0;
                    //cout << "Middle: " << first_key << endl;
                    brk_idx = idx + 1;
                    brk_kv_pos = kv_last_pos;
                    last_key_len = key_pos + 1;
                    memcpy(last_key, first_key, last_key_len);
                    delete_trie_last_half(key_pos, first_key, tp);
                    new_block.key_pos = key_pos;
                    t = new_block.trie + (t - trie);
                    t = new_block.next_key(first_key, tp, t, ctr, tc, child, leaf);
                    key_pos = new_block.key_pos;
                    //src_idx = get_ptr(idx + 1);
                    //memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[key_pos+1+current_block[src_idx]] = 0;
                    //cout << first_key << endl;
                }
            }
        }
        kv_last_pos = get_kv_last_pos() + DFOS_NODE_SIZE - kv_last_pos;
        new_block.set_kv_last_pos(kv_last_pos);
        memmove(new_block.current_block + kv_last_pos, new_block.current_block + get_kv_last_pos(), DFOS_NODE_SIZE - kv_last_pos);
        brk_kv_pos += (kv_last_pos - get_kv_last_pos());
        int diff = DFOS_NODE_SIZE - brk_kv_pos;
        for (idx = 0; idx < orig_filled_size; idx++) {
            new_block.ins_ptr(idx, kv_last_pos + (idx < brk_idx ? diff : 0));
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
        }
        kv_last_pos = new_block.get_kv_last_pos();

#if BPT_9_BIT_PTR == 1
        memcpy(current_block + DFOS_HDR_SIZE, new_block.current_block + DFOS_HDR_SIZE, DS_MAX_PTR_BITMAP_BYTES);
        memcpy(trie + DS_GET_TRIE_LEN, new_block.trie + util::get_int(new_block.BPT_TRIE_LEN_PTR), brk_idx);
#else
        memcpy(trie + DS_GET_TRIE_LEN, new_block.trie + util::get_int(new_block.BPT_TRIE_LEN_PTR), (brk_idx << 1));
#endif

        {
            if (is_leaf()) {
                // *first_len_ptr = key_pos + 1;
                *first_len_ptr = util::compare(first_key, key_pos + 1, last_key, last_key_len);
            } else {
                new_block.key_at = new_block.get_key(brk_idx, &new_block.key_at_len);
                key_pos++;
                if (new_block.key_at_len) {
                    memcpy(first_key + key_pos, new_block.key_at,
                            new_block.key_at_len);
                    *first_len_ptr = key_pos + new_block.key_at_len;
                } else
                    *first_len_ptr = key_pos;
                key_pos--;
            }
            new_block.delete_trie_first_half(key_pos, first_key, tp);
        }

        {
            int old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + DFOS_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            set_kv_last_pos(DFOS_NODE_SIZE - old_blk_new_len);
            set_filled_size(brk_idx);
        }

        {
#if BPT_9_BIT_PTR == 1
#if BPT_INT64MAP == 1
            (*new_block.bitmap) <<= brk_idx;
#else
            if (brk_idx & 0xFFE0)
            *new_block.bitmap1 = *new_block.bitmap2 << (brk_idx - 32);
            else {
                *new_block.bitmap1 <<= brk_idx;
                *new_block.bitmap1 |= (*new_block.bitmap2 >> (32 - brk_idx));
            }
#endif
#endif
            int new_size = orig_filled_size - brk_idx;
            uint8_t *block_ptrs = new_block.trie + util::get_int(new_block.BPT_TRIE_LEN_PTR);
#if BPT_9_BIT_PTR == 1
            memmove(block_ptrs, block_ptrs + brk_idx, new_size);
#else
            memmove(block_ptrs, block_ptrs + (brk_idx << 1), new_size << 1);
#endif
            new_block.set_kv_last_pos(brk_kv_pos);
            new_block.set_filled_size(new_size);
        }

#if DS_MIDDLE_PREFIX == 1
        consolidate_initial_prefix(current_block);
        new_block.consolidate_initial_prefix(new_block.current_block);
#endif

        return new_block.current_block;

    }

#if DS_SIBLING_PTR_SIZE == 1
    void move_ptr_list(uint8_t orig_trie_len) {
#else
    void move_ptr_list(int orig_trie_len) {
#endif
#if BPT_9_BIT_PTR == 1
        memmove(trie + DS_GET_TRIE_LEN, trie + orig_trie_len, filled_size());
#else
        memmove(trie + DS_GET_TRIE_LEN, trie + orig_trie_len, filled_size() << 1);
#endif
    }

    void consolidate_initial_prefix(uint8_t *t) {
        t += DFOS_HDR_SIZE;
        uint8_t *t_reader = t;
        uint8_t count = 0;
        if (*t & x01) {
            count = (*t >> 1);
            t_reader += count;
            t_reader++;
        }
        uint8_t *t_writer = t_reader + (*t & x01 ? 0 : 1);
        uint8_t trie_len_diff = 0;
        while ((*t_reader & x01) || ((*t_reader & x02) && (*t_reader & x04)
                && BIT_COUNT(t_reader[2 + DS_SIBLING_PTR_SIZE]) == 1
                && BIT_COUNT(t_reader[3 + DS_SIBLING_PTR_SIZE]) == 0)) {
            if (*t_reader & x01) {
                uint8_t len = *t_reader >> 1;
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
                *t_writer++ = (*t_reader & xF8) + BIT_COUNT(t_reader[2 + DS_SIBLING_PTR_SIZE] - 1);
                t_reader += 4 + DS_SIBLING_PTR_SIZE;
                count++;
                trie_len_diff += 3 + DS_SIBLING_PTR_SIZE;
            }
        }
        if (t_reader > t_writer) {
            memmove(t_writer, t_reader, DS_GET_TRIE_LEN - (t_reader - t) + filled_size() * 2);
            if (!(*t & x01))
                trie_len_diff--;
            *t = (count << 1) + 1;
            DS_SET_TRIE_LEN(DS_GET_TRIE_LEN - trie_len_diff);
            //cout << (int) (*t >> 1) << endl;
        }
    }

    void insert_current() {
        uint8_t key_char, mask;
        int diff;

        key_char = key[key_pos - 1];
        mask = x01 << (key_char & x07);
        switch (insert_state) {
        case INSERT_AFTER:
            *orig_pos &= xFB;
            trie_pos = orig_pos + 2 + ((*orig_pos & x02) ? DS_GET_SIBLING_OFFSET(orig_pos + 2) : 0);
            ins_atWith_ptrs(trie_pos, ((key_char & xF8) | x04), mask);
            if (key_pos > 1)
                update_skip_lens(orig_pos - 1, trie_pos - 1, 2);
            break;
        case INSERT_BEFORE:
            ins_atWith_ptrs(orig_pos, key_char & xF8, mask);
            if (key_pos > 1)
                update_skip_lens(orig_pos, orig_pos + 1, 2);
            break;
        case INSERT_LEAF:
            if (*orig_pos & x02)
                orig_pos[3 + DS_SIBLING_PTR_SIZE] |= mask;
            else
                orig_pos[1] |= mask;
            update_skip_lens(orig_pos, orig_pos + 1, 0);
            break;
#if DS_MIDDLE_PREFIX == 1
        case INSERT_CONVERT:
            uint8_t b, c;
            char cmp_rel;
            diff = trie_pos - orig_pos;
            // 3 possible relationships between key_char and *trie_pos, 4 possible positions of trie_pos
            c = *trie_pos;
            cmp_rel = ((c ^ key_char) > x07 ? (c < key_char ? 0 : 1) : 2);
            if (cmp_rel == 0)
                ins_atWith_ptrs(key_at, (key_char & xF8) | 0x04, 1 << (key_char & x07));
            if (diff == 1)
                trie_pos = orig_pos;
            b = (cmp_rel == 2 ? x04 : x00) | (cmp_rel == 1 ? x00 : x02);
            need_count = (*orig_pos >> 1) - diff;
            diff--;
            *trie_pos++ = ((cmp_rel == 0 ? c : key_char) & xF8) | b;
            b = (cmp_rel == 1 ? (diff ? 6 : 5) : (diff ? 4 : 3));
#if DS_SIBLING_PTR_SIZE == 2
            b++;
#endif
            if (diff)
                *orig_pos = (diff << 1) | x01;
            if (need_count)
                b++;
            ins_bytes_with_ptrs(trie_pos + (diff ? 0 : 1), b);
            update_skip_lens(orig_pos, trie_pos, b + (cmp_rel == 0 ? 2 : 0));
            if (cmp_rel == 1) {
                *trie_pos++ = 1 << (key_char & x07);
                *trie_pos++ = (c & xF8) | x06;
            }
            key_at_pos = pos;
            pos = (cmp_rel == 2 ? 1 : 0);
            orig_pos = skip_children(trie_pos + need_count + (need_count ? 1 : 0)
                    + 3 + DS_SIBLING_PTR_SIZE, 1);
            *trie_pos++ = pos;
#if DS_SIBLING_PTR_SIZE == 2
            *trie_pos = (pos & 0x0300) >> 2;
#endif
            DS_SET_SIBLING_OFFSET(trie_pos, orig_pos - trie_pos);
            trie_pos += DS_SIBLING_PTR_SIZE;
            pos = key_at_pos;
            *trie_pos++ = 1 << (c & x07);
            *trie_pos++ = (cmp_rel == 2) ? 1 << (key_char & x07) : 0;
            if (need_count)
                *trie_pos = (need_count << 1) | x01;
            break;
#endif
        case INSERT_THREAD:
            int p, min;
            uint8_t c1, c2;
            uint8_t *from_pos;
            from_pos = trie_pos;
            if (*orig_pos & x02) {
                orig_pos[2 + DS_SIBLING_PTR_SIZE] |= mask;
                orig_pos[1]++;
            } else {
#if DS_SIBLING_PTR_SIZE == 1
                ins_atWith_ptrs(orig_pos + 1, BIT_COUNT(orig_pos[1]) + 1, x00, mask);
#else
                ins_atWith_ptrs(orig_pos + 1, BIT_COUNT(orig_pos[1]) + 1, x00, x00, mask);
#endif
                trie_pos += (2 + DS_SIBLING_PTR_SIZE);
                *orig_pos |= x02;
            }
            c1 = c2 = key_char;
            p = key_pos;
            min = util::min16(key_len, key_pos + key_at_len);
            if (p < min)
                orig_pos[3 + DS_SIBLING_PTR_SIZE] &= ~mask;
#if DS_MIDDLE_PREFIX == 1
            need_count -= (8 + DS_SIBLING_PTR_SIZE);
            diff = p + need_count;
            if (diff == min && need_count) {
                need_count--;
                diff--;
            }
            diff = need_count + (need_count ? (need_count / 128) + 1 : 0)
                    + (diff == min ? 2 : (key[diff] ^ key_at[diff - key_pos]) > x07 ? 4 :
                            (key[diff] == key_at[diff - key_pos] ? 6 + DS_SIBLING_PTR_SIZE : 2));
            ins_bytes_with_ptrs(trie_pos, diff);
            if (need_count) {
                int copied = 0;
                while (copied < need_count) {
                    int to_copy = (need_count - copied) > 127 ? 127 : need_count - copied;
                    *trie_pos++ = (to_copy << 1) | x01;
                    memcpy(trie_pos, key + key_pos + copied, to_copy);
                    copied += to_copy;
                    trie_pos += to_copy;
                }
                p += need_count;
                //count1 += need_count;
            }
#else
            need_count = (p == min ? 2 : 0);
            while (p < min) {
                c1 = key[p];
                c2 = key_at[p - key_pos];
                need_count += ((c1 ^ c2) > x07 ? 4 : (c1 == c2 ? (p + 1 == min ?
                        6 + DS_SIBLING_PTR_SIZE : 4 + DS_SIBLING_PTR_SIZE) : 2));
                if (c1 != c2)
                    break;
                p++;
            }
            p = key_pos;
            ins_atWith_ptrs(trie_pos, current_block, need_count);
#endif
            while (p < min) {
                c1 = key[p];
                c2 = key_at[p - key_pos];
                if (c1 > c2) {
                    uint8_t swap = c1;
                    c1 = c2;
                    c2 = swap;
                }
#if DS_MIDDLE_PREFIX == 1
                switch ((c1 ^ c2) > x07 ? 0 : (c1 == c2 ? 3 : 1)) {
#else
                switch ((c1 ^ c2) > x07 ?
                        0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
                case 2:
                    need_count -= (4 + DS_SIBLING_PTR_SIZE);
                    *trie_pos++ = (c1 & xF8) | x06;
                    *trie_pos++ = 2;
                    *trie_pos = 0;
                    DS_SET_SIBLING_OFFSET(trie_pos, need_count + 2 + DS_SIBLING_PTR_SIZE);
                    trie_pos += DS_SIBLING_PTR_SIZE;
                    *trie_pos++ = x01 << (c1 & x07);
                    *trie_pos++ = 0;
                    break;
#endif
                case 0:
                    *trie_pos++ = c1 & xF8;
                    *trie_pos++ = x01 << (c1 & x07);
                    *trie_pos++ = (c2 & xF8) | x04;
                    *trie_pos++ = x01 << (c2 & x07);
                    break;
                case 1:
                    *trie_pos++ = (c1 & xF8) | x04;
                    *trie_pos++ = (x01 << (c1 & x07)) | (x01 << (c2 & x07));
                    break;
                case 3:
                    *trie_pos++ = (c1 & xF8) | x06;
                    *trie_pos++ = x02;
                    *trie_pos = 0;
                    DS_SET_SIBLING_OFFSET(trie_pos, 4 + DS_SIBLING_PTR_SIZE);
                    trie_pos += DS_SIBLING_PTR_SIZE;
                    *trie_pos++ = x01 << (c1 & x07);
                    *trie_pos++ = x01 << (c1 & x07);
                    break;
                }
                if (c1 != c2)
                    break;
                p++;
            }
                uint8_t kap;
                kap = pos;
                if (c1 != c2 && key[p] > key_at[p - key_pos])
                    kap--;
            diff = p - key_pos;
            key_pos = p + 1;
            if (c1 == c2) {
                c2 = (p == key_len ? key_at[diff] : key[p]);
                *trie_pos++ = (c2 & xF8) | x04;
                *trie_pos++ = x01 << (c2 & x07);
                if (p == key_len)
                    key_pos--;
                else
                    kap--;
            }
            p = trie_pos - from_pos;
            update_skip_lens(orig_pos - 2, orig_pos, p);
            orig_pos += 2;
            DS_SET_SIBLING_OFFSET(orig_pos, p + DS_GET_SIBLING_OFFSET(orig_pos));
            if (diff < key_at_len)
                diff++;
            if (key_at_len >= diff) {
                key_at_len -= diff;
                p = get_ptr(key_at_pos);
                p += diff;
                current_block[p] = key_at_len;
                set_ptr(key_at_pos, p);
            }
            break;
        case INSERT_EMPTY:
            trie[0] = (key_char & xF8) | x04;
            trie[1] = mask;
            DS_SET_TRIE_LEN(2);
            break;
        }

        if (BPT_MAX_PFX_LEN < key_pos)
            BPT_MAX_PFX_LEN = key_pos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

    }

    void ins_atWith_ptrs(uint8_t *ptr, const uint8_t *s, uint8_t len) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + len, ptr, trie + DS_GET_TRIE_LEN + filled_size() - ptr);
    #else
        memmove(ptr + len, ptr, trie + DS_GET_TRIE_LEN + filled_size() * 2 - ptr);
    #endif
        memcpy(ptr, s, len);
        DS_SET_TRIE_LEN(DS_GET_TRIE_LEN + len);
    }

    uint8_t ins_atWith_ptrs(uint8_t *ptr, uint8_t b1, uint8_t b2) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 2, ptr, trie + DS_GET_TRIE_LEN + filled_size() - ptr);
    #else
        memmove(ptr + 2, ptr, trie + DS_GET_TRIE_LEN + filled_size() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr = b2;
        DS_SET_TRIE_LEN(DS_GET_TRIE_LEN + 2);
        return 2;
    }

    uint8_t ins_atWith_ptrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 3, ptr, trie + DS_GET_TRIE_LEN + filled_size() - ptr);
    #else
        memmove(ptr + 3, ptr, trie + DS_GET_TRIE_LEN + filled_size() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        DS_SET_TRIE_LEN(DS_GET_TRIE_LEN + 3);
        return 3;
    }

    uint8_t ins_atWith_ptrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 4, ptr, trie + DS_GET_TRIE_LEN + filled_size() - ptr);
    #else
        memmove(ptr + 4, ptr, trie + DS_GET_TRIE_LEN + filled_size() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        DS_SET_TRIE_LEN(DS_GET_TRIE_LEN + 4);
        return 4;
    }

    void ins_bytes_with_ptrs(uint8_t *ptr, int len) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + len, ptr, trie + DS_GET_TRIE_LEN + filled_size() - ptr);
    #else
        memmove(ptr + len, ptr, trie + DS_GET_TRIE_LEN + filled_size() * 2 - ptr);
    #endif
        DS_SET_TRIE_LEN(DS_GET_TRIE_LEN + len);
    }

    void decode_need_count(int search_result) {
        if (insert_state != INSERT_THREAD)
            need_count = need_counts[insert_state];
    }

    void init_derived() {
    }

    void cleanup() {
    }

};

#endif
