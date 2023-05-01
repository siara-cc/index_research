#ifndef bfos_H
#define bfos_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

#define BS_MIDDLE_PREFIX 1

#define BFOS_HDR_SIZE 9

#if (defined(__AVR_ATmega328P__))
#define BS_CHILD_PTR_SIZE 1
#else
#define BS_CHILD_PTR_SIZE 2
#endif
#if BS_CHILD_PTR_SIZE == 1
#define BS_BIT_COUNT_CH(x) BIT_COUNT(x)
#define BS_SET_TRIE_LEN(x) BPT_TRIE_LEN = x
#define BS_GET_CHILD_OFFSET(x) *(x)
#define BS_SET_CHILD_OFFSET(x, off) *(x) = off
#else
#define BS_BIT_COUNT_CH(x) BIT_COUNT2(x)
#define BS_SET_TRIE_LEN(x) util::set_int(BPT_TRIE_LEN_PTR, x)
#define BS_GET_CHILD_OFFSET(x) util::get_int(x)
#define BS_SET_CHILD_OFFSET(x, off) util::set_int(x, off)
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
    uint8_t need_counts[10];
    uint8_t *last_t;
    uint8_t last_child;
    uint8_t last_leaf;

    bfos(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
                bpt_trie_handler<bfos>(leaf_block_sz, parent_block_sz, cache_sz, fname) {
#if BS_CHILD_PTR_SIZE == 1
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x07\x00\x00\x00", 10);
#else
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x08\x00\x00\x00", 10);
#endif
    }

    bfos(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bpt_trie_handler<bfos>(block_sz, block, is_leaf) {
        init_stats();
#if BS_CHILD_PTR_SIZE == 1
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x07\x00\x00\x00", 10);
#else
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x08\x00\x00\x00", 10);
#endif
    }

    bfos(const char *filename, int blk_size, int page_resv_bytes) :
       bpt_trie_handler<bfos>(filename, blk_size, page_resv_bytes) {
        init_stats();
    }

    void set_current_block_root() {
        current_block = root_block;
        trie = current_block + BFOS_HDR_SIZE;
    }

    void set_current_block(uint8_t *m) {
        current_block = m;
        trie = current_block + BFOS_HDR_SIZE;
    }

    uint8_t *get_last_ptr() {
        //key_pos = 0;
        do {
            switch (last_child > last_leaf || (last_leaf ^ last_child) <= last_child ? 2 : 1) {
            case 1:
                return current_block + util::get_int(last_t + (*last_t & x02 ? BS_BIT_COUNT_CH(last_t[1]) + 1 : 0)
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

    void set_prefix_last(uint8_t key_char, uint8_t *t, uint8_t pfx_rem_len) {
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

    int search_current_block(bptree_iter_ctx *ctx = NULL) {
        uint8_t *t = trie;
        uint8_t trie_char = *t;
        orig_pos = t++;
        //if (key_pos) {
        //    if (trie_char & x01) {
        //        key_pos--;
        //        uint8_t pfx_len = trie_char >> 1;
        //        if (key_pos < pfx_len) {
        //            trie_char = ((pfx_len - key_pos) << 1) + 1;
        //            t += key_pos;
        //        } else {
        //            key_pos = pfx_len;
        //            t += pfx_len;
        //            trie_char = *t;
        //            orig_pos = t++;
        //        }
        //    } else
        //        key_pos = 0;
        //}
        uint8_t key_char = *key; //[key_pos++];
        key_pos = 1;
        do {
            switch (trie_char & x01 ? 3 : (key_char ^ trie_char) > x07 ? (key_char > trie_char ? 0 : 2) : 1) {
            case 0:
                last_t = orig_pos;
                last_child = (trie_char & x02 ? *t++ : 0);
                last_leaf = *t++;
                t += BS_BIT_COUNT_CH(last_child) + BIT_COUNT2(last_leaf);
                if (trie_char & x04) {
                    trie_pos = t;
                    return -INSERT_AFTER;
                }
                break;
            case 1:
                uint8_t r_leaves, r_children;
                r_children = (trie_char & x02 ? *t++ : 0);
                r_leaves = *t++;
                key_char = x01 << (key_char & x07);
                trie_char = (r_leaves & key_char ? x02 : x00) |     // faster
                        ((r_children & key_char) && key_pos != key_len ? x01 : x00);
                //trie_char = r_leaves & r_mask ?                 // slower
                //        (r_children & r_mask ? (key_pos == key_len ? x02 : x03) : x02) :
                //        (r_children & r_mask ? (key_pos == key_len ? x00 : x01) : x00);
                key_char--;
                if (!is_leaf()) {
                    if ((r_children | r_leaves) & key_char) {
                        last_t = orig_pos;
                        last_child = r_children & key_char;
                        last_leaf = r_leaves & key_char;
                    }
                }
                switch (trie_char) {
                case 0:
                    trie_pos = t;
                    return -INSERT_LEAF;
                case 1:
                    break;
                case 2:
                    int cmp;
                    trie_pos = t + BS_BIT_COUNT_CH(r_children) + BIT_COUNT2(r_leaves & key_char);
                    key_at = current_block + util::get_int(trie_pos);
                    key_at_len = *key_at++;
                    cmp = util::compare(key + key_pos, key_len - key_pos, key_at, key_at_len);
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
                    if (!is_leaf()) {
                        last_t = orig_pos;
                        last_child = r_children & key_char;
                        last_leaf = (r_leaves & key_char) | (key_char + 1);
                    }
                    break;
                }
                t += BS_BIT_COUNT_CH(r_children & key_char);
                t += BS_GET_CHILD_OFFSET(t);
                key_char = key[key_pos++];
                break;
#if BS_MIDDLE_PREFIX == 1
            case 3:
                uint8_t pfx_len;
                pfx_len = (trie_char >> 1);
                while (pfx_len && key_char == *t && key_pos < key_len) {
                    key_char = key[key_pos++];
                    t++;
                    pfx_len--;
                }
                if (pfx_len) {
                    trie_pos = t;
                    if (!is_leaf())
                        set_prefix_last(key_char, t, pfx_len);
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

    uint8_t *get_child_ptr_pos(int search_result) {
        return key_at == last_t ? last_t - 1 : get_last_ptr();
    }

    uint8_t *get_ptr_pos() {
        return NULL;
    }

    int get_header_size() {
        return BFOS_HDR_SIZE;
    }

    void set_ptr_diff(uint16_t diff) {
        uint8_t *t = trie;
        uint8_t *t_end = trie + get_trie_len();
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
                util::set_int(t, util::get_int(t) + diff);
                t += 2;
            }
        }
    }

#if BS_CHILD_PTR_SIZE == 1
    uint8_t copy_kary(uint8_t *t, uint8_t *dest, int lvl, uint8_t *tp,
            uint8_t *brk_key, int16_t brk_key_len, uint8_t which_half) {
#else
    uint16_t copy_kary(uint8_t *t, uint8_t *dest, int lvl, uint16_t *tp,
                uint8_t *brk_key, int16_t brk_key_len, uint8_t which_half) {
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
            if (limit == t && which_half == 2)
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
                if (which_half == 1) {
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
                        util::set_int(dest, util::get_int(t + i * 2));
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
                        util::set_int(dest, util::get_int(t + i * 2));
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
    uint8_t copy_trie_half(uint8_t *tp, uint8_t *brk_key, int16_t brk_key_len, uint8_t *dest, uint8_t which_half) {
        uint8_t tp_child[BPT_MAX_PFX_LEN];
#else
    uint16_t copy_trie_half(uint16_t *tp, uint8_t *brk_key, int16_t brk_key_len, uint8_t *dest, uint8_t which_half) {
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
        uint16_t last_len = copy_kary(t, dest, lvl, tp, brk_key, brk_key_len, which_half);
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
                    last_len = copy_kary(t, dest, lvl, tp, brk_key, brk_key_len, which_half);
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
                    last_len = copy_kary(t, dest, lvl, tp, brk_key, brk_key_len, which_half);
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

    void consolidate_initial_prefix(uint8_t *t) {
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
            memmove(t_writer, t_reader, get_trie_len() - (t_reader - t));
            if (!(*t & x01))
                trie_len_diff--;
            *t = (count << 1) + 1;
            BS_SET_TRIE_LEN(get_trie_len() - trie_len_diff);
            //cout << (int) (*t >> 1) << endl;
        }
    }

#if BS_CHILD_PTR_SIZE == 1
    uint8_t *next_ptr(uint8_t *first_key, bfos_iterator_vars& it) {
#else
    uint8_t *next_ptr(uint8_t *first_key, bfos_iterator_vars& it) {
#endif
        do {
            while (it.ctr == x08) {
                if (it.tc & x04) {
                    key_pos--;
                    it.t = trie + it.tp[key_pos];
                    while (*it.t & x01) {
                        key_pos -= (*it.t >> 1);
                        it.t = trie + it.tp[key_pos];
                    }
                    it.tc = *it.t++;
                    it.child = (it.tc & x02 ? *it.t++ : 0);
                    it.leaf = *it.t++;
                    it.ctr = first_key[key_pos] & x07;
                    it.ctr++;
                    it.t += BS_BIT_COUNT_CH(it.child);
                } else {
                    it.t += BIT_COUNT2(it.leaf);
                    it.ctr = 0x09;
                    break;
                }
            }
            if (it.ctr > x07) {
                it.tp[key_pos] = it.t - trie;
                it.tc = *it.t++;
                while (it.tc & x01) {
                    uint8_t len = it.tc >> 1;
                    for (int i = 0; i < len; i++)
                        it.tp[key_pos + i] = it.t - trie - 1;
                    memcpy(first_key + key_pos, it.t, len);
                    it.t += len;
                    key_pos += len;
                    it.tp[key_pos] = it.t - trie;
                    it.tc = *it.t++;
                }
                it.child = (it.tc & x02 ? *it.t++ : 0);
                it.leaf = *it.t++;
                it.t += BS_BIT_COUNT_CH(it.child);
                it.ctr = FIRST_BIT_OFFSET_FROM_RIGHT(it.child | it.leaf);
            }
            first_key[key_pos] = (it.tc & xF8) | it.ctr;
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
                key_pos++;
                it.ctr = 0x09;
                it.tc = 0;
                continue;
            }
            it.ctr = (it.ctr == x07 ? 8 : FIRST_BIT_OFFSET_FROM_RIGHT((it.child | it.leaf) & (xFE << it.ctr)));
        } while (1); // (s.t - trie) < BPT_TRIE_LEN);
        return 0;
    }

    uint8_t *find_split_source(int16_t search_result) {
        return NULL;
    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int16_t orig_filled_size = filled_size();
        uint32_t BFOS_NODE_SIZE = is_leaf() ? block_size : parent_block_size;
        int lvl = current_block[0] & 0x1F;
        uint8_t *b = allocate_block(BFOS_NODE_SIZE, is_leaf(), lvl);
        bfos new_block(BFOS_NODE_SIZE, b, is_leaf());
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        uint16_t kv_last_pos = get_kv_last_pos();
        uint16_t half_kVPos = kv_last_pos + (BFOS_NODE_SIZE - kv_last_pos) / 2;

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
        //if (!is_leaf())
        //   cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
        new_block.key_pos = 0;
        memcpy(new_block.trie, trie, get_trie_len());
#if BS_CHILD_PTR_SIZE == 1
        new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
#else
        util::set_int(new_block.BPT_TRIE_LEN_PTR, get_trie_len());
#endif
        for (idx = 0; idx < orig_filled_size; idx++) {
            uint8_t *leaf_ptr = new_block.next_ptr(curr_key, it);
            uint16_t src_idx = util::get_int(leaf_ptr);
            uint16_t kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
            util::set_int(leaf_ptr, kv_last_pos);
            //memcpy(curr_key + new_block.key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
            //curr_key[new_block.key_pos+1+current_block[src_idx]] = 0;
            //cout << curr_key << endl;
            if (brk_idx < 0) {
                brk_idx = -brk_idx;
                new_block.key_pos++;
                tp_cpy_len = new_block.key_pos;
                if (is_leaf()) {
                    //*first_len_ptr = s.key_pos;
                    *first_len_ptr = util::compare(curr_key, new_block.key_pos, first_key, *first_len_ptr);
                    memcpy(first_key, curr_key, tp_cpy_len);
                } else {
                    memcpy(first_key, curr_key, new_block.key_pos);
                    memcpy(first_key + new_block.key_pos, current_block + src_idx + 1, current_block[src_idx]);
                    *first_len_ptr = new_block.key_pos + current_block[src_idx];
                }
                memcpy(tp_cpy, tp, tp_cpy_len * BS_CHILD_PTR_SIZE);
                //curr_key[new_block.key_pos] = 0;
                //cout << "Middle:" << curr_key << endl;
                new_block.key_pos--;
            }
            kv_last_pos += kv_len;
            if (brk_idx == 0) {
                //brk_key_len = next_key(s);
                //if (tot_len > half_kVLen) {
                if (kv_last_pos > half_kVPos || idx >= (orig_filled_size * 2 / 3)) {
                    //memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[key_pos+1+current_block[src_idx]] = 0;
                    //cout << first_key << ":";
                    brk_idx = idx + 1;
                    brk_idx = -brk_idx;
                    brk_kv_pos = kv_last_pos;
                    *first_len_ptr = new_block.key_pos + 1;
                    memcpy(first_key, curr_key, *first_len_ptr);
                    BS_SET_TRIE_LEN(new_block.copy_trie_half(tp, first_key, *first_len_ptr, trie, 1));
                }
            }
        }
#if BS_CHILD_PTR_SIZE == 1
        new_block.BPT_TRIE_LEN = new_block.copy_trie_half(tp_cpy, first_key, tp_cpy_len, trie + BPT_TRIE_LEN, 2);
        memcpy(new_block.trie, trie + BPT_TRIE_LEN, new_block.BPT_TRIE_LEN);
#else
        util::set_int(new_block.BPT_TRIE_LEN_PTR, new_block.copy_trie_half(tp_cpy, first_key,
                tp_cpy_len, trie + get_trie_len(), 2));
        memcpy(new_block.trie, trie + get_trie_len(), util::get_int(new_block.BPT_TRIE_LEN_PTR));
#endif

        kv_last_pos = get_kv_last_pos() + BFOS_NODE_SIZE - kv_last_pos;
        uint16_t diff = (kv_last_pos - get_kv_last_pos());

        {
            memmove(new_block.current_block + brk_kv_pos + diff, new_block.current_block + brk_kv_pos,
                    BFOS_NODE_SIZE - brk_kv_pos - diff);
            brk_kv_pos += diff;
            new_block.set_ptr_diff(diff);
            new_block.set_kv_last_pos(brk_kv_pos);
            new_block.set_filled_size(orig_filled_size - brk_idx);
        }

        {
            uint16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + BFOS_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos - diff, old_blk_new_len); // Copy back first half to old block
            diff += (BFOS_NODE_SIZE - brk_kv_pos);
            set_ptr_diff(diff);
            set_kv_last_pos(BFOS_NODE_SIZE - old_blk_new_len);
            set_filled_size(brk_idx);
        }

        if (!is_leaf())
            new_block.set_leaf(false);

#if BS_MIDDLE_PREFIX == 1
        consolidate_initial_prefix(current_block);
        new_block.consolidate_initial_prefix(new_block.current_block);
#endif

        //key_pos = 0;

        return new_block.current_block;

    }

    void make_space() {
        int block_sz = (is_leaf() ? block_size : parent_block_size);
        int lvl = current_block[0] & 0x1F;
        const uint16_t data_size = block_sz - get_kv_last_pos();
        uint8_t data_buf[data_size];
        uint16_t new_data_len = 0;
        uint8_t *t = current_block + BFOS_HDR_SIZE;
        uint8_t *upto = t + get_trie_len();
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
                int leaf_pos = util::get_int(t);
                uint8_t *child_ptr_pos = current_block + leaf_pos;
                uint16_t data_len = *child_ptr_pos;
                data_len++;
                data_len += child_ptr_pos[data_len];
                data_len++;
                new_data_len += data_len;
                // if (child_ptr_pos == key_at - 1) {
                //     if (last_t == key_at)
                //       last_t = 0;
                //     key_at = current_block + (block_sz - new_data_len) + 1;
                //     if (!last_t)
                //       last_t = key_at;
                // }
                memcpy(data_buf + data_size - new_data_len, child_ptr_pos, data_len);
                util::set_int(t, block_sz - new_data_len);
                t += 2;
            }
        }
        uint16_t new_kv_last_pos = block_sz - new_data_len;
        memcpy(current_block + new_kv_last_pos, data_buf + data_size - new_data_len, new_data_len);
        //printf("%d, %d\n", data_size, new_data_len);
        set_kv_last_pos(new_kv_last_pos);
        search_current_block();
    }

    bool is_full(int search_result) {
        decode_need_count(search_result);
        if (get_kv_last_pos() < (BFOS_HDR_SIZE + get_trie_len()
                + need_count + key_len - key_pos + value_len + 3)) {
            make_space();
            if (get_kv_last_pos() < (BFOS_HDR_SIZE + get_trie_len()
                + need_count + key_len - key_pos + value_len + 3))
              return true;
        }
#if BS_CHILD_PTR_SIZE == 1
        if (get_trie_len() > 254 - need_count)
            return true;
#endif
        return false;
    }

    void add_first_data() {
        add_data(3);
    }

    uint8_t *add_data(int search_result) {

        if (search_result < 0)
            search_result = ~search_result;

        insert_state = search_result + 1;

        uint16_t ptr = insert_current();

        int16_t key_left = key_len - key_pos;
        uint16_t kv_last_pos = get_kv_last_pos() - (key_left + value_len + 2);
        set_kv_last_pos(kv_last_pos);
        util::set_int(trie + ptr, kv_last_pos);
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + key_pos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);
        set_filled_size(filled_size() + 1);

        return current_block + ptr;

    }

    void update_ptrs(uint8_t *upto, int diff) {
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
                if (insert_state == INSERT_BEFORE) {
                    if (key_pos > 1 && (t + *t) == (orig_pos + 4))
                        *t -= 4;
                }
                t++;
#else
                uint16_t child_offset = BS_GET_CHILD_OFFSET(t);
                if ((t + child_offset) >= upto)
                    BS_SET_CHILD_OFFSET(t, child_offset + diff);
                if (insert_state == INSERT_BEFORE) {
                    if (key_pos > 1 && (t + BS_GET_CHILD_OFFSET(t)) == (orig_pos + 4))
                        BS_SET_CHILD_OFFSET(t, BS_GET_CHILD_OFFSET(t) - 4);
                }
                t += 2;
#endif
                // todo: avoid inside loops
            }
            t += BIT_COUNT2(leaves);
        }
    }

    uint16_t insert_current() {
        uint8_t key_char, mask;
        uint16_t diff;
        uint16_t ret;

        key_char = key[key_pos - 1];
        mask = x01 << (key_char & x07);
        switch (insert_state) {
        case INSERT_AFTER:
            *orig_pos &= xFB;
            trie_pos = orig_pos + (*orig_pos & x02 ? BS_BIT_COUNT_CH(orig_pos[1])
                    + BIT_COUNT2(orig_pos[2]) + 3 : BIT_COUNT2(orig_pos[1]) + 2);
            update_ptrs(trie_pos, 4);
            ins_b4(trie_pos, ((key_char & xF8) | x04), mask, 0, 0);
            ret = trie_pos - trie + 2;
            break;
        case INSERT_BEFORE:
            update_ptrs(orig_pos, 4);
            ins_b4(orig_pos, (key_char & xF8), mask, 0, 0);
            ret = orig_pos - trie + 2;
            break;
        case INSERT_LEAF:
            trie_pos = orig_pos + ((*orig_pos & x02) ? 2 : 1);
            *trie_pos |= mask;
            trie_pos += ((*orig_pos & x02 ? BS_BIT_COUNT_CH(orig_pos[1]) : 0)
                    + BIT_COUNT2(*trie_pos & (mask - 1)) + 1);
            update_ptrs(trie_pos, 2);
            ins_b2(trie_pos, x00, x00);
            ret = trie_pos - trie;
            break;
#if BS_MIDDLE_PREFIX == 1
        case INSERT_CONVERT:
              ret = 0;
              uint8_t b, c;
              char cmp_rel;
              diff = trie_pos - orig_pos;
              // 3 possible relationships between key_char and *trie_pos, 4 possible positions of trie_pos
              c = *trie_pos;
              cmp_rel = ((c ^ key_char) > x07 ? (c < key_char ? 0 : 1) : 2);
              if (diff == 1)
                  trie_pos = orig_pos;
              b = (cmp_rel == 2 ? x04 : x00) | (cmp_rel == 1 ? x00 : x02);
              need_count = (*orig_pos >> 1) - diff;
              diff--;
              *trie_pos++ = ((cmp_rel == 0 ? c : key_char) & xF8) | b;
#if BS_CHILD_PTR_SIZE == 1
              b = (cmp_rel == 2 ? 4 : 6);
#else
              b = (cmp_rel == 2 ? 5 : 7);
#endif
              if (diff) {
                  b++;
                  *orig_pos = (diff << 1) | x01;
              }
              if (need_count)
                  b++;
              ins_bytes(trie_pos, b);
              update_ptrs(trie_pos, b);
              *trie_pos++ = 1 << ((cmp_rel == 1 ? key_char : c) & x07);
              switch (cmp_rel) {
              case 0:
                  *trie_pos++ = 0;
#if BS_CHILD_PTR_SIZE == 1
                  *trie_pos++ = 5;
#else
                  *trie_pos++ = 0;
                  *trie_pos++ = 6;
#endif
                  *trie_pos++ = (key_char & xF8) | 0x04;
                  *trie_pos++ = mask;
                  ret = trie_pos - trie;
                  trie_pos += 2;
                  break;
              case 1:
                  ret = trie_pos - trie;
                  trie_pos += 2;
                  *trie_pos++ = (c & xF8) | x06;
                  *trie_pos++ = 1 << (c & x07);
                  *trie_pos++ = 0;
#if BS_CHILD_PTR_SIZE == 1
                  *trie_pos++ = 1;
#else
                  *trie_pos++ = 0;
                  *trie_pos++ = 2;
#endif
                  break;
              case 2:
                  *trie_pos++ = mask;
#if BS_CHILD_PTR_SIZE == 1
                  *trie_pos++ = 3;
#else
                  *trie_pos++ = 0;
                  *trie_pos++ = 4;
#endif
                  ret = trie_pos - trie;
                  trie_pos += 2;
                  break;
              }
              if (need_count)
                  *trie_pos = (need_count << 1) | x01;
            break;
    #endif
        case INSERT_THREAD:
              uint16_t p, min;
              uint8_t c1, c2;
              uint8_t *child_pos;
              uint16_t ptr, pos;
              ret = pos = 0;

              c1 = c2 = key_char;
              p = key_pos;
              min = util::min_b(key_len, key_pos + key_at_len);
              child_pos = orig_pos + 1;
              if (*orig_pos & x02) {
                  *child_pos |= mask;
                  child_pos = child_pos + 2 + BS_BIT_COUNT_CH(*child_pos & (mask - 1));
#if BS_CHILD_PTR_SIZE == 1
                  ins_b(child_pos, (uint8_t) (BPT_TRIE_LEN - (child_pos - trie) + 1));
                  update_ptrs(child_pos, 1);
#else
                  int16_t offset = (get_trie_len() + 1 - (child_pos - trie) + 1);
                  ins_b2(child_pos, offset >> 8, offset & xFF);
                  update_ptrs(child_pos, 2);
#endif
              } else {
                  *orig_pos |= x02;
#if BS_CHILD_PTR_SIZE == 1
                  ins_b2(child_pos, mask, *child_pos);
                  child_pos[2] = (uint8_t) (BPT_TRIE_LEN - (child_pos + 2 - trie));
                  update_ptrs(child_pos, 2);
#else
                  int16_t offset = get_trie_len() + 3 - (child_pos + 2 - trie);
                  ins_b3(child_pos, mask, *child_pos, offset >> 8);
                  child_pos[3] = offset & xFF;
                  update_ptrs(child_pos, 3);
#endif
              }
              trie_pos = orig_pos + BS_BIT_COUNT_CH(orig_pos[1]) + 3
                      + BIT_COUNT2(orig_pos[2] & (mask - 1));
              ptr = util::get_int(trie_pos);
              if (p < min) {
                  orig_pos[2] &= ~mask;
                  del_at(trie_pos, 2);
                  update_ptrs(trie_pos, -2);
              } else {
                  pos = trie_pos - trie;
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
              //diff = (p == min ? 4 : (key[diff] ^ key_at[diff - key_pos]) > x07 ? 8
              //        : (key[diff] == key_at[diff - key_pos] ? 9 + BS_CHILD_PTR_SIZE : 6));
              trie_pos = trie + get_trie_len();
#if BS_MIDDLE_PREFIX == 1
              //diff += need_count + (need_count ? (need_count / 128) + 1 : 0);
#else
              //diff += need_count * (3 + BS_CHILD_PTR_SIZE);
#endif
              //BS_SET_TRIE_LEN(get_trie_len() + diff);
#if BS_MIDDLE_PREFIX == 1
              //diff += need_count + (need_count ? (need_count / 128) + 1 : 0);
              if (need_count) {
                  uint8_t copied = 0;
                  while (copied < need_count) {
                      int16_t to_copy = (need_count - copied) > 127 ? 127 : need_count - copied;
                      *trie_pos++ = (to_copy << 1) | x01;
                      memcpy(trie_pos, key + key_pos + copied, to_copy);
                      trie_pos += to_copy;
                      copied += to_copy;
                  }
                  p += need_count;
                  //count1 += need_count;
              }
#endif
              while (p < min) {
                  bool is_swapped = false;
                  c1 = key[p];
                  c2 = key_at[p - key_pos];
                  if (c1 > c2) {
                      uint8_t swap = c1;
                      c1 = c2;
                      c2 = swap;
                      is_swapped = true;
                  }
#if BS_MIDDLE_PREFIX == 1
                  switch ((c1 ^ c2) > x07 ? 0 : (c1 == c2 ? 3 : 1)) {
#else
                  switch ((c1 ^ c2) > x07 ?
                          0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
                  case 2:
                      *trie_pos++ = (c1 & xF8) | x06;
                      *trie_pos++ = x01 << (c1 & x07);
                      *trie_pos++ = 0;
#if BS_CHILD_PTR_SIZE == 1
                      *trie_pos++ = 1;
#else
                      *trie_pos++ = 0;
                      *trie_pos++ = 2;
#endif
                      break;
#endif
                  case 0:
                      *trie_pos++ = c1 & xF8;
                      *trie_pos++ = x01 << (c1 & x07);
                      ret = is_swapped ? ret : (trie_pos - trie);
                      pos = is_swapped ? (trie_pos - trie) : pos;
                      util::set_int(trie_pos, is_swapped ? ptr : 0);
                      trie_pos += 2;
                      *trie_pos++ = (c2 & xF8) | x04;
                      *trie_pos++ = x01 << (c2 & x07);
                      ret = is_swapped ? (trie_pos - trie) : ret;
                      pos = is_swapped ? pos : (trie_pos - trie);
                      util::set_int(trie_pos, is_swapped ? 0 : ptr);
                      trie_pos += 2;
                      break;
                  case 1:
                      *trie_pos++ = (c1 & xF8) | x04;
                      *trie_pos++ = (x01 << (c1 & x07)) | (x01 << (c2 & x07));
                      ret = is_swapped ? ret : (trie_pos - trie);
                      pos = is_swapped ? (trie_pos - trie) : pos;
                      util::set_int(trie_pos, is_swapped ? ptr : 0);
                      trie_pos += 2;
                      ret = is_swapped ? (trie_pos - trie) : ret;
                      pos = is_swapped ? pos : (trie_pos - trie);
                      util::set_int(trie_pos, is_swapped ? 0 : ptr);
                      trie_pos += 2;
                      break;
                  case 3:
                      *trie_pos++ = (c1 & xF8) | x06;
                      *trie_pos++ = x01 << (c1 & x07);
                      *trie_pos++ = x01 << (c1 & x07);
#if BS_CHILD_PTR_SIZE == 1
                      *trie_pos++ = 3;
#else
                      *trie_pos++ = 0;
                      *trie_pos++ = 4;
#endif
                      ret = (p + 1 == key_len) ? (trie_pos - trie) : ret;
                      pos = (p + 1 == key_len) ? pos : (trie_pos - trie);
                      util::set_int(trie_pos, (p + 1 == key_len) ? 0 : ptr);
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
                  *trie_pos++ = x01 << (c2 & x07);
                  ret = (p == key_len) ? ret : (trie_pos - trie);
                  pos = (p == key_len) ? (trie_pos - trie) : pos;
                  util::set_int(trie_pos, (p == key_len) ? ptr : 0);
                  trie_pos += 2;
              }
              BS_SET_TRIE_LEN(trie_pos - trie);
              diff = p - key_pos;
              key_pos = p + (p < key_len ? 1 : 0);
              if (diff < key_at_len)
                  diff++;
              if (diff) {
                  key_at_len -= diff;
                  p = ptr + diff;
                  if (key_at_len >= 0) {
                      current_block[p] = key_at_len;
                      util::set_int(trie + pos, p);
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

        if (BPT_MAX_PFX_LEN < key_pos)
            BPT_MAX_PFX_LEN = key_pos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

        return ret;
    }

    void decode_need_count(int16_t search_result) {
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
/*
uint8_t bfos_node_handler::copy_kary(uint8_t *t, uint8_t *dest) {
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

uint8_t bfos_node_handler::copy_trie_first_half(uint8_t *tp, uint8_t *first_key, int16_t first_key_len, uint8_t *dest) {
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
                    dest = copy_child_tree(t + *t, orig_dest, dest);
                }
            }
            t += BIT_COUNT2(*t);
        }
        t = t_end; // child of i
    }
    return trie_len;
}

uint8_t *bfos_node_handler::copy_child_tree(uint8_t *t, uint8_t *orig_dest, uint8_t *dest) {
    uint8_t tp_ch[BX_MAX_KEY_LEN];
    do {
        uint8_t len = copy_kary(t + *t, dest);
        orig_dest[t - trie] = dest - (orig_dest + (t - trie));
    } while (1);
}
*/
