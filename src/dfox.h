#ifndef dfox_H
#define dfox_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

#define DX_MIDDLE_PREFIX 1

#define DFOX_HDR_SIZE 9

#if (defined(__AVR_ATmega328P__))
#define DX_SIBLING_PTR_SIZE 1
#else
#define DX_SIBLING_PTR_SIZE 2
#endif
#if DX_SIBLING_PTR_SIZE == 1
#define DX_GET_TRIE_LEN BPT_TRIE_LEN
#define DX_SET_TRIE_LEN(x) BPT_TRIE_LEN = x
#define DX_GET_SIBLING_OFFSET(x) *(x)
#define DX_SET_SIBLING_OFFSET(x, off) *(x) = off
#else
#define DX_GET_TRIE_LEN util::get_int(BPT_TRIE_LEN_PTR)
#define DX_SET_TRIE_LEN(x) util::set_int(BPT_TRIE_LEN_PTR, x)
#define DX_GET_SIBLING_OFFSET(x) util::get_int(x)
#define DX_SET_SIBLING_OFFSET(x, off) util::set_int(x, off)
#endif

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class dfox : public bpt_trie_handler<dfox> {
public:
    uint8_t need_counts[10];
    uint8_t *last_t;
    dfox(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, const uint8_t opts = 0) :
                bpt_trie_handler<dfox>(leaf_block_sz, parent_block_sz, cache_sz, fname, opts) {
#if DX_SIBLING_PTR_SIZE == 1
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x07\x00\x00\x00", 10);
#else
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x08\x00\x00\x00", 10);
#endif
    }

    dfox(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bpt_trie_handler<dfox>(block_sz, block, is_leaf) {
        init_stats();
#if DX_SIBLING_PTR_SIZE == 1
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x07\x00\x00\x00", 10);
#else
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x08\x00\x00\x00", 10);
#endif
    }

    dfox(const char *filename, int blk_size, int page_resv_bytes, const uint8_t opts) :
       bpt_trie_handler<dfox>(filename, blk_size, page_resv_bytes, opts) {
        init_stats();
    }

    inline void set_current_block_root() {
        current_block = root_block;
        trie = current_block + DFOX_HDR_SIZE;
    }

    inline void set_current_block(uint8_t *m) {
        current_block = m;
        trie = current_block + DFOX_HDR_SIZE;
    }

    inline uint8_t *skip_children(uint8_t *t, uint8_t count) {
        while (count) {
            uint8_t tc = *t++;
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

    int search_current_block(bptree_iter_ctx *ctx = NULL) {
        uint8_t key_char = *key;
        uint8_t *t = trie;
        uint8_t trie_char = *t;
        key_pos = 1;
        orig_pos = t++;
        do {
            switch (trie_char & x01 ? 3 : (key_char ^ trie_char) > x07 ? (key_char > trie_char ? 0 : 2) : 1) {
            case 0:
                if (trie_char & x02)
                    t += DX_GET_SIBLING_OFFSET(t);
                else
                    t += BIT_COUNT2(*t) + 1;
                last_t = t - 2;
                if (trie_char & x04)
                    return -INSERT_AFTER;
                break;
            case 1:
                if (trie_char & x02) {
                    t += DX_SIBLING_PTR_SIZE;
                    uint8_t r_children = *t++;
                    uint8_t r_leaves = *t++;
                    uint8_t r_mask = x01 << (key_char & x07);
                    trie_char = (r_leaves & r_mask ? x02 : x00) |
                            ((r_children & r_mask) && key_pos != key_len ? x01 : x00);
                    //key_char = (r_leaves & r_mask ? x01 : x00)
                    //        | (r_children & r_mask ? x02 : x00)
                    //        | (key_pos == key_len ? x04 : x00);
                    r_mask--;
                    t = skip_children(t, BIT_COUNT(r_children & r_mask) + BIT_COUNT(r_leaves & r_mask));
                } else {
                    uint8_t r_leaves = *t++;
                    uint8_t r_mask = x01 << (key_char & x07);
                    trie_char = (r_leaves & r_mask ? x02 : x00);
                    t = skip_children(t, BIT_COUNT(r_leaves & (r_mask - 1)));
                }
                switch (trie_char) {
                case 0:
                    trie_pos = t;
                    return -INSERT_LEAF;
                case 1:
                    break;
                case 2:
                    int cmp;
                    key_at = get_key(t, &key_at_len);
                    cmp = util::compare(key + key_pos, key_len - key_pos, key_at, key_at_len);
                    if (cmp == 0) {
                        last_t = t;
                        return 0;
                    }
                    if (cmp > 0)
                        last_t = t;
                    else
                        cmp = -cmp;
                    trie_pos = t;
    #if DX_MIDDLE_PREFIX == 1
                    need_count = cmp + 7 + DX_SIBLING_PTR_SIZE;
    #else
                    need_count = (cmp * 5) + 5 + DX_SIBLING_PTR_SIZE;
    #endif
                    return -INSERT_THREAD;
                case 3:
                    last_t = t;
                    t += 2;
                    break;
                }
                key_char = key[key_pos++];
                break;
#if DX_MIDDLE_PREFIX == 1
            case 3:
                uint8_t pfx_len;
                pfx_len = (trie_char >> 2);
                while (pfx_len && key_char == *t && key_pos < key_len) {
                    key_char = key[key_pos++];
                    t++;
                    pfx_len--;
                }
                if (pfx_len) {
                    trie_pos = t;
                    if (key_char > *t)
                        key_at = skip_children(t + pfx_len, 1);
                    return -INSERT_CONVERT;
                }
                break;
#endif
            case 2:
                return -INSERT_BEFORE;
            }
            trie_char = *t;
            orig_pos = t++;
        } while (1);
        return -1;
    }

    inline int get_ptr(uint8_t *t) {
        return ((*t >> 2) << 8) + t[1];
    }

    inline uint8_t *get_key(uint8_t *t, int *plen) {
        uint8_t *kv_idx = current_block + get_ptr(t);
        *plen = kv_idx[0];
        kv_idx++;
        return kv_idx;
    }

    void free_blocks() {
    }

    inline int get_header_size() {
        return DFOX_HDR_SIZE;
    }

    inline uint8_t *get_ptr_pos() {
        return NULL;
    }

    inline uint8_t *get_child_ptr_pos(int search_result) {
        return current_block + get_ptr(last_t);
    }

#if DX_SIBLING_PTR_SIZE == 1
    uint8_t *next_key(uint8_t *first_key, uint8_t *tp, uint8_t *t, char& ctr, uint8_t& tc, uint8_t& child, uint8_t& leaf) {
#else
    uint8_t *next_key(uint8_t *first_key, int *tp, uint8_t *t, char& ctr, uint8_t& tc, uint8_t& child, uint8_t& leaf) {
#endif
        do {
            while (ctr > x07) {
                if (tc & x04) {
                    key_pos--;
                    tc = trie[tp[key_pos]];
                    while (x01 == (tc & x03)) {
                        key_pos--;
                        tc = trie[tp[key_pos]];
                    }
                    child = (tc & x02 ? trie[tp[key_pos] + 1 + DX_SIBLING_PTR_SIZE] : 0);
                    leaf = trie[tp[key_pos] + (tc & x02 ? 2 + DX_SIBLING_PTR_SIZE : 1)];
                    ctr = first_key[key_pos] & 0x07;
                    ctr++;
                } else {
                    tp[key_pos] = t - trie;
                    tc = *t++;
                    while (x01 == (tc & x03)) {
                        uint8_t len = tc >> 2;
                        for (int i = 0; i < len; i++)
                            tp[key_pos + i] = t - trie - 1;
                        memcpy(first_key + key_pos, t, len);
                        t += len;
                        key_pos += len;
                        tp[key_pos] = t - trie;
                        tc = *t++;
                    }
                    t += (tc & x02 ? DX_SIBLING_PTR_SIZE : 0);
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
                child = leaf = 0;
                key_pos++;
                ctr = x08;
                tc = 0;
            }
            ctr++;
        } while (1); // (t - trie) < DX_GET_TRIE_LEN);
        return t;
    }

    uint8_t *next_ptr(uint8_t *t) {
        uint8_t tc = *t & x03;
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

    void update_prefix() {

    }

#if DX_SIBLING_PTR_SIZE == 1
    void delete_trie_last_half(int brk_key_len, uint8_t *first_key, uint8_t *tp) {
#else
    void delete_trie_last_half(int brk_key_len, uint8_t *first_key, int *tp) {
#endif
        for (int idx = brk_key_len; idx >= 0; idx--) {
            uint8_t *t = trie + tp[idx];
            uint8_t tc = *t;
            if (x01 == (tc & x03))
                continue;
            uint8_t offset = first_key[idx] & x07;
            *t++ = (tc | x04);
            if (tc & x02) {
                uint8_t children = t[DX_SIBLING_PTR_SIZE];
                children &= ~((idx == brk_key_len ? xFF : xFE) << offset);
                                // ryte_mask[offset] : ryte_incl_mask[offset]);
                t[DX_SIBLING_PTR_SIZE] = children;
                uint8_t leaves = t[1 + DX_SIBLING_PTR_SIZE] & ~(xFE << offset);
                t[DX_SIBLING_PTR_SIZE + 1] = leaves; // ryte_incl_mask[offset];
                uint8_t *new_t = skip_children(t + 2 + DX_SIBLING_PTR_SIZE,
                        BIT_COUNT(children) + BIT_COUNT(leaves));
                DX_SET_SIBLING_OFFSET(t, new_t - t);
                t = new_t;
            } else {
                uint8_t leaves = *t;
                leaves &= ~(xFE << offset); // ryte_incl_mask[offset];
                *t++ = leaves;
                t += BIT_COUNT2(leaves);
            }
            if (idx == brk_key_len)
                DX_SET_TRIE_LEN(t - trie);
        }
    }

    int delete_segment(uint8_t *delete_end, uint8_t *delete_start) {
        int count = delete_end - delete_start;
        if (count) {
            DX_SET_TRIE_LEN(DX_GET_TRIE_LEN - count);
            memmove(delete_start, delete_end, trie + DX_GET_TRIE_LEN - delete_start);
        }
        return count;
    }

#if DX_SIBLING_PTR_SIZE == 1
    void delete_trie_first_half(int brk_key_len, uint8_t *first_key, uint8_t *tp) {
#else
    void delete_trie_first_half(int brk_key_len, uint8_t *first_key, int *tp) {
#endif
        for (int idx = brk_key_len; idx >= 0; idx--) {
            uint8_t *t = trie + tp[idx];
            uint8_t tc = *t++;
            if (x01 == (tc & x03)) {
                uint8_t len = (tc >> 2);
                delete_segment(trie + tp[idx + 1], t + len);
                idx -= len;
                idx++;
                continue;
            }
            uint8_t offset = first_key[idx] & 0x07;
            if (tc & x02) {
                uint8_t children = t[DX_SIBLING_PTR_SIZE];
                uint8_t leaves = t[1 + DX_SIBLING_PTR_SIZE];
                uint8_t mask = (xFF << offset);
                children &= mask; // left_incl_mask[offset];
                leaves &= ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
                uint8_t count = BIT_COUNT(t[DX_SIBLING_PTR_SIZE]) - BIT_COUNT(children)
                        + BIT_COUNT(t[1 + DX_SIBLING_PTR_SIZE]) - BIT_COUNT(leaves); // ryte_mask[offset];
                t[DX_SIBLING_PTR_SIZE] = children;
                t[1 + DX_SIBLING_PTR_SIZE] = leaves;
                uint8_t *child_ptr = t + 2 + DX_SIBLING_PTR_SIZE;
                uint8_t *new_t = skip_children(child_ptr, count);
                delete_segment((idx < brk_key_len) ? (trie + tp[idx + 1]) : new_t, child_ptr);
                new_t = skip_children(child_ptr, BIT_COUNT(children) + BIT_COUNT(leaves));
                DX_SET_SIBLING_OFFSET(t, new_t - t);
            } else {
                uint8_t leaves = *t;
                leaves &= ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
                delete_segment(t + 1 + BIT_COUNT2(*t) - BIT_COUNT2(leaves), t + 1);
                *t = leaves;
            }
        }
        delete_segment(trie + tp[0], trie);
    }

    void update_skip_lens(uint8_t *loop_upto, int8_t diff) {
        uint8_t *t = trie;
        while (t < loop_upto) {
            uint8_t tc = *t++;
            switch (tc & x03) {
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
                break;
            case x01:
                t += (tc >> 2);
                break;
            }
        }
    }

    void add_first_data() {
        add_data(3);
    }

    void add_data(int search_result) {

        insert_state = search_result + 1;

        uint8_t *ptr = insert_current();

        int key_left = key_len - key_pos;
        int kv_last_pos = get_kv_last_pos() - (key_left + value_len + 2);
        set_kv_last_pos(kv_last_pos);
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + key_pos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);

        set_ptr(ptr, kv_last_pos);
        set_filled_size(filled_size() + 1);

    }

    bool is_full(int search_result) {
        decode_need_count(search_result);
        if (get_kv_last_pos() < (DFOX_HDR_SIZE + DX_GET_TRIE_LEN +
                need_count + key_len - key_pos + value_len + 3))
            return true;
#if DX_SIBLING_PTR_SIZE == 1
        if (DX_GET_TRIE_LEN > (254 - need_count))
            return true;
#endif
        return false;
    }

    void set_ptr(uint8_t *t, int ptr) {
        *t++ = ((ptr >> 8) << 2) | x03;
        *t = ptr & xFF;
    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int orig_filled_size = filled_size();
        const int DFOX_NODE_SIZE = is_leaf() ? block_size : parent_block_size;
        uint8_t *b = allocate_block(DFOX_NODE_SIZE, is_leaf(), current_block[0] & 0x1F);
        dfox new_block(DFOX_NODE_SIZE, b, is_leaf());
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        int kv_last_pos = get_kv_last_pos();
        int half_kVLen = DFOX_NODE_SIZE - kv_last_pos + 1;
        half_kVLen /= 2;

        int brk_idx = -1;
        int brk_kv_pos;
        uint8_t *brk_trie_pos = trie;
        int tot_len;
        brk_kv_pos = tot_len = 0;
        char ctr = x08;
#if DX_SIBLING_PTR_SIZE == 1
        uint8_t tp[BPT_MAX_PFX_LEN + 1];
#else
        int tp[BPT_MAX_PFX_LEN + 1];
#endif
        uint8_t *t = trie;
        uint8_t tc, child, leaf;
        tc = child = leaf = 0;
        //if (!is_leaf())
        //   cout << "Trie len:" << (int) DX_GET_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
        key_pos = 0;
        // (1) move all data to new_block in order
        int idx;
        for (idx = 0; idx < orig_filled_size; idx++) {
            if (brk_idx == -1)
                t = next_key(first_key, tp, t, ctr, tc, child, leaf);
            else
                t = next_ptr(t);
            int src_idx = get_ptr(t);
            //if (brk_idx == -1) {
            //    memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
            //   first_key[key_pos+1+current_block[src_idx]] = 0;
            //   cout << first_key << endl;
            //}
            t += 2;
            int kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            tot_len += kv_len;
            memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
            kv_last_pos += kv_len;
            if (brk_idx == -1) {
                //brk_key_len = next_key(s);
                //if (tot_len > half_kVLen) {
                if (tot_len > half_kVLen || idx == (orig_filled_size / 2)) {
                    //memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[key_pos+1+current_block[src_idx]] = 0;
                    //cout << "Middle:" << first_key << endl;
                    brk_idx = idx + 1;
                    brk_kv_pos = kv_last_pos;
                    brk_trie_pos = t;
                }
            }
        }
        kv_last_pos = get_kv_last_pos() + DFOX_NODE_SIZE - kv_last_pos;
        new_block.set_kv_last_pos(kv_last_pos);
        memmove(new_block.current_block + kv_last_pos, new_block.current_block + get_kv_last_pos(), DFOX_NODE_SIZE - kv_last_pos);
        brk_kv_pos += (kv_last_pos - get_kv_last_pos());
        int diff = DFOX_NODE_SIZE - brk_kv_pos;
        t = trie;
        for (idx = 0; idx < orig_filled_size; idx++) {
            t = next_ptr(t);
            set_ptr(t, kv_last_pos + (idx < brk_idx ? diff : 0));
            t += 2;
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
        }
        kv_last_pos = new_block.get_kv_last_pos();
        memcpy(new_block.trie, trie, DX_GET_TRIE_LEN);
#if DX_SIBLING_PTR_SIZE == 1
        new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
#else
        util::set_int(new_block.BPT_TRIE_LEN_PTR, DX_GET_TRIE_LEN);
#endif
        {
            int last_key_len = key_pos + 1;
            uint8_t last_key[last_key_len + 1];
            memcpy(last_key, first_key, last_key_len);
            delete_trie_last_half(key_pos, first_key, tp);
            new_block.key_pos = key_pos;
            t = new_block.trie + (brk_trie_pos - trie);
            t = new_block.next_key(first_key, tp, t, ctr, tc, child, leaf);
            key_pos = new_block.key_pos;
            //src_idx = get_ptr(idx + 1);
            //memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
            //first_key[key_pos+1+current_block[src_idx]] = 0;
            //cout << first_key << endl;
            if (is_leaf()) {
                // *first_len_ptr = key_pos + 1;
                *first_len_ptr = util::compare(first_key, key_pos + 1, last_key, last_key_len);
            } else {
                new_block.key_at = new_block.get_key(t, &new_block.key_at_len);
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
            memcpy(current_block + DFOX_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            set_kv_last_pos(DFOX_NODE_SIZE - old_blk_new_len);
            set_filled_size(brk_idx);
        }

        {
            int new_size = orig_filled_size - brk_idx;
            new_block.set_kv_last_pos(brk_kv_pos);
            new_block.set_filled_size(new_size);
        }

        consolidate_initial_prefix(current_block);
        new_block.consolidate_initial_prefix(new_block.current_block);

        return new_block.current_block;

    }

    void consolidate_initial_prefix(uint8_t *t) {
        t += DFOX_HDR_SIZE;
        uint8_t *t_reader = t;
        uint8_t count = 0;
        if ((*t & x03) == 1) {
            count = (*t >> 2);
            t_reader += count;
            t_reader++;
        }
        uint8_t *t_writer = t_reader + (((*t & x03) == 1) ? 0 : 1);
        uint8_t trie_len_diff = 0;
        while (((*t_reader & x03) == 1) || ((*t_reader & x02) && (*t_reader & x04)
                && BIT_COUNT(t_reader[2 + DX_SIBLING_PTR_SIZE]) == 1
                && BIT_COUNT(t_reader[3 + DX_SIBLING_PTR_SIZE]) == 0)) {
            if ((*t_reader & x03) == 1) {
                uint8_t len = *t_reader >> 2;
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
            memmove(t_writer, t_reader, DX_GET_TRIE_LEN - (t_reader - t) + filled_size() * 2);
            if ((*t & x03) != 1)
                trie_len_diff--;
            *t = (count << 2) + 1;
            DX_SET_TRIE_LEN(DX_GET_TRIE_LEN - trie_len_diff);
            //cout << (int) (*t >> 1) << endl;
        }
    }

    uint8_t *insert_current() {
        uint8_t key_char, mask;
        int diff;
        uint8_t *ret;

        key_char = key[key_pos - 1];
        mask = x01 << (key_char & x07);
        switch (insert_state) {
        case INSERT_AFTER:
            *orig_pos &= xFB;
            trie_pos = orig_pos + ((*orig_pos & x02) ? DX_GET_SIBLING_OFFSET(orig_pos + 1) + 1 : BIT_COUNT2(orig_pos[1]) + 2);
            ins_at(trie_pos, ((key_char & xF8) | x04), mask, x00, x00);
            ret = trie_pos + 2;
            if (key_pos > 1)
                update_skip_lens(orig_pos, 4);
            break;
        case INSERT_BEFORE:
            ins_at(orig_pos, key_char & xF8, mask, x00, x00);
            ret = orig_pos + 2;
            if (key_pos > 1)
                update_skip_lens(orig_pos + 1, 4);
            break;
        case INSERT_LEAF:
            if (*orig_pos & x02)
                orig_pos[2 + DX_SIBLING_PTR_SIZE] |= mask;
            else
                orig_pos[1] |= mask;
            ins_at(trie_pos, x00, x00);
            ret = trie_pos;
            update_skip_lens(orig_pos + 1, 2);
            break;
    #if DX_MIDDLE_PREFIX == 1
        case INSERT_CONVERT:
            uint8_t b, c;
            char cmp_rel;
            diff = trie_pos - orig_pos;
            // 3 possible relationships between key_char and *trie_pos, 4 possible positions of trie_pos
            c = *trie_pos;
            cmp_rel = ((c ^ key_char) > x07 ? (c < key_char ? 0 : 1) : 2);
            if (cmp_rel == 0) {
                ins_at(key_at, (key_char & xF8) | 0x04, 1 << (key_char & x07), x00, x00);
                ret = key_at + 2;
            }
            if (diff == 1)
                trie_pos = orig_pos;
            b = (cmp_rel == 2 ? x04 : x00) | (cmp_rel == 1 ? x00 : x02);
            need_count = (*orig_pos >> 2) - diff;
            diff--;
            *trie_pos++ = ((cmp_rel == 0 ? c : key_char) & xF8) | b;
            b = (cmp_rel == 1 ? 6 : (cmp_rel == 2 ? (c < key_char ? 2 : 4) : 2));
#if DX_SIBLING_PTR_SIZE == 2
            b++;
#endif
            if (diff) {
                *orig_pos = (diff << 2) | x01;
                b++;
            }
            if (need_count)
                b++;
            ins_bytes(trie_pos + (diff ? 0 : 1), b);
            update_skip_lens(orig_pos + 1, b + (cmp_rel == 0 ? 4
                    : (cmp_rel == 2 && c < key_char ? 2 : 0)));
            if (cmp_rel == 1) {
                *trie_pos++ = 1 << (key_char & x07);
                ret = trie_pos;
                trie_pos += 2;
                *trie_pos++ = (c & xF8) | x06;
            }
            key_at = skip_children(trie_pos + DX_SIBLING_PTR_SIZE - 1
                    + (cmp_rel == 2 ? (c < key_char ? 3 : 5) : 3)
                    + need_count + (need_count ? 1 : 0), 1);
            DX_SET_SIBLING_OFFSET(trie_pos, key_at - trie_pos + (cmp_rel == 2 && c < key_char ? 2 : 0));
            trie_pos += DX_SIBLING_PTR_SIZE;
            *trie_pos++ = 1 << (c & x07);
            if (cmp_rel == 2) {
                *trie_pos++ = 1 << (key_char & x07);
                if (c >= key_char) {
                    ret = trie_pos;
                    trie_pos += 2;
                }
            } else
                *trie_pos++ = 0;
            if (need_count)
                *trie_pos = (need_count << 2) | x01;
            if (cmp_rel == 0)
                ret += b;
            if (cmp_rel == 2 && c < key_char) {
                ins_at(key_at, x00, x00);
                ret = key_at;
            }
            break;
    #endif
        case INSERT_THREAD:
            int p, min, ptr;
            uint8_t c1, c2;
            uint8_t *ptr_pos;
            ret = ptr_pos = 0;
            if (*orig_pos & x02) {
                orig_pos[1 + DX_SIBLING_PTR_SIZE] |= mask;
            } else {
#if DX_SIBLING_PTR_SIZE == 1
                ins_at(orig_pos + 1, BIT_COUNT2(orig_pos[1]) + 3, mask);
                trie_pos += 2;
#else
                int offset = BIT_COUNT2(orig_pos[1]) + 4;
                ins_at(orig_pos + 1, offset >> 8, offset & xFF, mask);
                trie_pos += 3;
#endif
            }
            c1 = c2 = key_char;
            p = key_pos;
            min = util::min16(key_len, key_pos + key_at_len);
            ptr = get_ptr(trie_pos);
            if (p < min)
                orig_pos[2 + DX_SIBLING_PTR_SIZE] &= ~mask;
            else {
                if (p == key_len)
                    ret = trie_pos;
                else
                    ptr_pos = trie_pos;
                trie_pos += 2;
            }
#if DX_MIDDLE_PREFIX == 1
            need_count -= (8 + DX_SIBLING_PTR_SIZE);
            diff = p + need_count;
            if (diff == min && need_count) {
                need_count--;
                diff--;
            }
            diff = need_count + (need_count ? (need_count / 64) + 1 : 0)
                    + (diff == min ? 4 : (key[diff] ^ key_at[diff - key_pos]) > x07 ? 6 :
                            (key[diff] == key_at[diff - key_pos] ? 7 + DX_SIBLING_PTR_SIZE : 4));
            ins_bytes(trie_pos, diff);
#else
            need_count = (p == min ? 4 : 0);
            while (p < min) {
                c1 = key[p];
                c2 = key_at[p - key_pos];
                need_count += ((c1 ^ c2) > x07 ? 6 : (c1 == c2 ? (p + 1 == min ?
                        7 + DX_SIBLING_PTR_SIZE : 3 + DX_SIBLING_PTR_SIZE) : 4));
                if (c1 != c2)
                    break;
                p++;
            }
            p = key_pos;
            ins_bytes(trie_pos, need_count);
            diff = need_count;
            need_count += (p == min ? 0 : 2);
#endif
            if (!ret)
                ret = trie_pos + diff - (p < min ? 0 : 2);
            if (!ptr_pos)
                ptr_pos = trie_pos + diff - (p < min ? 0 : 2);
#if DX_MIDDLE_PREFIX == 1
            if (need_count) {
                int copied = 0;
                while (copied < need_count) {
                    int to_copy = (need_count - copied) > 63 ? 63 : need_count - copied;
                    *trie_pos++ = (to_copy << 2) | x01;
                    memcpy(trie_pos, key + key_pos + copied, to_copy);
                    copied += to_copy;
                    trie_pos += to_copy;
                }
                p += need_count;
                //count1 += need_count;
            }
#endif
            while (p < min) {
                c1 = key[p];
                c2 = key_at[p - key_pos];
                if (c1 > c2) {
                    uint8_t swap = c1;
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
                    *trie_pos++ = (c1 & xF8) | x06;
#if DX_SIBLING_PTR_SIZE == 1
                    *trie_pos++ = need_count + 3;
#else
                    DX_SET_SIBLING_OFFSET(trie_pos, need_count + 4);
                    trie_pos += 2;
#endif
                    *trie_pos++ = x01 << (c1 & x07);
                    *trie_pos++ = 0;
                    break;
#endif
                case 0:
                    *trie_pos++ = c1 & xF8;
                    *trie_pos++ = x01 << (c1 & x07);
                    if (c1 == key[p])
                        ret = trie_pos;
                    else
                        ptr_pos = trie_pos;
                    trie_pos += 2;
                    *trie_pos++ = (c2 & xF8) | x04;
                    *trie_pos = x01 << (c2 & x07);
                    break;
                case 1:
                    *trie_pos++ = (c1 & xF8) | x04;
                    *trie_pos++ = (x01 << (c1 & x07)) | (x01 << (c2 & x07));
                    if (c1 == key[p])
                        ret = trie_pos;
                    else
                        ptr_pos = trie_pos;
                    break;
                case 3:
                    *trie_pos++ = (c1 & xF8) | x06;
#if DX_SIBLING_PTR_SIZE == 1
                    *trie_pos++ = 9;
#else
                    DX_SET_SIBLING_OFFSET(trie_pos, 10);
                    trie_pos += 2;
#endif
                    *trie_pos++ = x01 << (c1 & x07);
                    *trie_pos++ = x01 << (c1 & x07);
                    if (p + 1 == key_len)
                        ret = trie_pos;
                    else
                        ptr_pos = trie_pos;
                    trie_pos += 2;
                    break;
                }
                if (c1 != c2)
                    break;
                p++;
            }
            if (c1 == c2) {
                c2 = (p == key_len ? key_at[p - key_pos] : key[p]);
                *trie_pos++ = (c2 & xF8) | x04;
                *trie_pos = x01 << (c2 & x07);
            }
            update_skip_lens(orig_pos, diff + (*orig_pos & x02 ? 0 : 1 + DX_SIBLING_PTR_SIZE));
#if DX_SIBLING_PTR_SIZE == 1
            orig_pos[1] += diff;
#else
            DX_SET_SIBLING_OFFSET(orig_pos + 1, DX_GET_SIBLING_OFFSET(orig_pos + 1) + diff);
#endif
            *orig_pos |= x02;
            diff = p - key_pos;
            key_pos = p + (p < key_len ? 1 : 0);
            if (diff < key_at_len)
                diff++;
            if (diff) {
                key_at_len -= diff;
                ptr += diff;
                if (key_at_len >= 0)
                    current_block[ptr] = key_at_len;
            }
            if (ptr_pos)
                set_ptr(ptr_pos, ptr);
            break;
        case INSERT_EMPTY:
            *trie = (key_char & xF8) | x04;
            trie[1] = mask;
            DX_SET_TRIE_LEN(4);
            ret = trie + 2;
            break;
        }

        if (BPT_MAX_PFX_LEN < key_pos)
            BPT_MAX_PFX_LEN = key_pos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

        return ret;

    }

    inline int ins_at(uint8_t *ptr, uint8_t b1, uint8_t b2) {
        int trie_len = DX_GET_TRIE_LEN;
        memmove(ptr + 2, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr = b2;
        DX_SET_TRIE_LEN(trie_len + 2);
        return 2;
    }

    inline int ins_at(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3) {
        int trie_len = DX_GET_TRIE_LEN;
        memmove(ptr + 3, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        DX_SET_TRIE_LEN(trie_len + 3);
        return 3;
    }

    inline uint8_t ins_at(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
        int trie_len = DX_GET_TRIE_LEN;
        memmove(ptr + 4, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        DX_SET_TRIE_LEN(trie_len + 4);
        return 4;
    }

    void ins_bytes(uint8_t *ptr, int len) {
        int trie_len = DX_GET_TRIE_LEN;
        memmove(ptr + len, ptr, trie + trie_len - ptr);
        DX_SET_TRIE_LEN(trie_len + len);
    }

    void decode_need_count(int search_result) {
        insert_state = search_result + 1;
        if (insert_state != INSERT_THREAD)
            need_count = need_counts[insert_state];
    }

    void init_derived() {
    }

    void cleanup() {
    }

    uint8_t *find_split_source(int search_result) {
        return NULL;
    }

    uint8_t *next_rec(bptree_iter_ctx *ctx, uint8_t *val_buf, int *val_buf_len) {
        return NULL;
    }

};

#endif
