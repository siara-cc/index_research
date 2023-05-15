#ifndef OCTP_H
#define OCTP_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

#define OP_MIDDLE_PREFIX 1

#define OCTP_HDR_SIZE 9

#define OP_BIT_COUNT_CH(x) BIT_COUNT2(x)
#define OP_GET_TRIE_LEN util::get_int(BPT_TRIE_LEN_PTR)
#define OP_SET_TRIE_LEN(x) util::set_int(BPT_TRIE_LEN_PTR, x)
#define OP_GET_CHILD_OFFSET(x) util::get_int(x)
#define OP_SET_CHILD_OFFSET(x, off) util::set_int(x, off)

class octp_iterator_vars {
public:
    octp_iterator_vars(int *tp_ext, uint8_t *t_ext) {
        tp = tp_ext;
        t = t_ext;
        ctr = 9;
        only_leaf = 1;
    }
    int *tp;
    uint8_t *t;
    uint8_t ctr;
    uint8_t tc;
    uint8_t child;
    uint8_t leaf;
    uint8_t only_leaf;
};

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class octp : public bpt_trie_handler<octp> {
public:
    char need_counts[10];
    uint8_t *last_t;
    uint8_t last_child;
    uint8_t last_leaf;
    int pntr_count;
    uint8_t key_char_pos;
    uint8_t bit_map_len;

    octp(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, const uint8_t opts = 0) :
                bpt_trie_handler<octp>(leaf_block_sz, parent_block_sz, cache_sz, fname, opts) {
#if BS_CHILD_PTR_SIZE == 1
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x07\x00\x00\x00", 10);
#else
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x08\x00\x00\x00", 10);
#endif
    }

    octp(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bpt_trie_handler<octp>(block_sz, block, is_leaf) {
        init_stats();
#if BS_CHILD_PTR_SIZE == 1
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x07\x00\x00\x00", 10);
#else
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x08\x00\x00\x00", 10);
#endif
    }

    octp(const char *filename, int blk_size, int page_resv_bytes, const uint8_t opts) :
       bpt_trie_handler<octp>(filename, blk_size, page_resv_bytes, opts) {
        init_stats();
    }

    inline void set_current_block_root() {
        current_block = root_block;
        trie = current_block + OCTP_HDR_SIZE;
    }

    inline void set_current_block(uint8_t *m) {
        current_block = m;
        trie = current_block + OCTP_HDR_SIZE;
    }

    inline uint8_t *get_last_ptr() {
        //key_pos = 0;
        do {
            switch (last_child > last_leaf || (last_leaf ^ last_child) <= last_child ? 2 : 1) {
            case 1:
                return current_block + util::get_int(last_t + (*last_t & x02 ? OP_BIT_COUNT_CH(last_t[1]) + 1 : 0)
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

    inline void set_prefix_last(uint8_t key_char, uint8_t *t, uint8_t pfx_rem_len) {
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

    inline uint8_t peek_trie_char(uint8_t *t) {
        return *t;
    }

    inline uint8_t next_trie_char(uint8_t **pt) {
        return *(*pt)++;
    }

    inline void skip_trie_chars(uint8_t **pt, int count) {
        (*pt) += count;
    }

    int search_current_block(bptree_iter_ctx *ctx = NULL) {
        uint8_t *t = trie;
        uint8_t key_char = *key; //[key_pos++];
        key_pos = 1;
        do {
            orig_pos = t;

            // bft/dft improvement:
            // nnnnnlxx - if xx = 00, n = len, letters followed by ptrs
            //                  if l = 0, then end of len, else msbyte follows
            //                          if ptr < trie_len, then child else leaf
            //                          if n = 63, next byte is the len
            //            if xx = 10 and n = 0, then leaf + child
            //                          else n = next field data type
            //            if xx = y1, n = prefix len, followed by xx = 00 node
            //                  if l = 0, then end of len, else msbyte follows
            //                  y bit is reserved for future use

            // bfos/dfos improvement:
            // kkkkk tx0 - octet with bitmap, t = terminator
            //             if x = 0 and k = 0 then leaf+node
            //             if x = 0 and k > 0 then k+t = next key data type
            // nnnnn ny1 - if y = 0, n = len of prefix
            //             if y = 1, prefix continues in data area

            // kkkkk t00 - octet with bitmap, t = terminator
            // lllll x01 - letter range or letter set, l = length, x = 0 (letter range), x = 1 (letter set)
            // llllx x10 - prefix with xx (00 - leaf and child, 01 - end with key, 10 - end with key & value, 11 - ?)
            // llllx x11 - other with xx (00 - leaf and child, 01 - key change, 10 - pointer set, 11 - ?)

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
                uint8_t trie_char = *t & 0xF8;
                if (key_char < trie_char)
                    return ~INSERT_BEFORE;
                uint8_t bitmap_len;
                uint8_t keychar_pos;
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
                    trie_pos = t;
                    pntr_count = ptr_count;
                    bit_map_len = bitmap_len;
                    key_char_pos = keychar_pos;
                    last_t = (ptr_count ? t + ptr_count : last_t);
                    return ~INSERT_AFTER;
                }
                uint8_t bit_mask = (1 << (key_char & x07)) - 1;
                ptr_count += BIT_COUNT2(*t & bit_mask); // extra
                bitmap_len -= key_char_pos;
                ptr_count += bitmap_len;
                last_t = (ptr_count ? t + ptr_count: last_t);
                if (!(*t & ++bit_mask)) {
                    trie_pos = t;
                    pntr_count = ptr_count;
                    bit_map_len = bitmap_len;
                    key_char_pos = keychar_pos;
                    return ~INSERT_LEAF;
                }
                *t += ptr_count;
                pntr_count = ptr_count;
                bit_map_len = bitmap_len;
                key_char_pos = keychar_pos;
                int idx = util::get_int(t);
                if (idx < get_trie_len()) {
                  *t += idx;
                  if (key_pos == key_len && *t == 0x06) {
                    idx = util::get_int(t+1);
                  } else {
                    key_char = key[key_pos++];
                    continue;
                  }
                }
                int cmp;
                key_at = current_block + idx;
                key_at_len = *key_at++;
                cmp = util::compare(key + key_pos, key_len - key_pos, key_at, key_at_len);
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
                uint8_t pfx_len;
                pfx_len = (*t >> 3);
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
              }
              //case 6: // leaf and child - handled above
              //case 7: // key change
            }
            t++;
        } while (1);
        return -1;
    }

    inline uint8_t *get_child_ptr_pos(int search_result) {
        return key_at == last_t ? last_t - 1 : get_last_ptr();
    }

    inline uint8_t *get_ptr_pos() {
        return NULL;
    }

    void free_blocks() {
    }

    inline int get_header_size() {
        return OCTP_HDR_SIZE;
    }

    void set_ptr_diff(int diff) {
        uint8_t *t = trie;
        uint8_t *t_end = trie + OP_GET_TRIE_LEN;
        while (t < t_end) {
            uint8_t tc = *t++;
            if (tc & x01) {
                t += (tc >> 1);
                continue;
            }
            uint8_t leaves = 0;
            if (tc & x02) {
                leaves = t[1];
                t += OP_BIT_COUNT_CH(*t);
                t += 2;
            } else
                leaves = *t++;
            for (int i = 0; i < BIT_COUNT(leaves); i++) {
                util::set_int(t, util::get_int(t) + diff);
                t += 2;
            }
        }
    }

#if OP_CHILD_PTR_SIZE == 1
    uint8_t copy_kary(uint8_t *t, uint8_t *dest, int lvl, uint8_t *tp,
            uint8_t *brk_key, int brk_key_len, uint8_t which_half) {
#else
    int copy_kary(uint8_t *t, uint8_t *dest, int lvl, int *tp,
                uint8_t *brk_key, int brk_key_len, uint8_t which_half) {
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
#if OP_CHILD_PTR_SIZE == 1
                        *dest++ = t[i];
#else
                        util::set_int(dest, util::get_int(t + i * 2));
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
                        util::set_int(dest, util::get_int(t + i * 2));
                        dest += 2;
#endif
                    }
                    t += OP_BIT_COUNT_CH(orig_children);
                    memcpy(dest, t + BIT_COUNT2(orig_leaves - leaves), BIT_COUNT2(leaves));
                    dest += BIT_COUNT2(leaves);
                }
                t += BIT_COUNT2(orig_leaves);
            } else {
                uint8_t len = (tc & x02) ? 3 + OP_BIT_COUNT_CH(t[1]) + BIT_COUNT2(t[2])
                        : 2 + BIT_COUNT2(t[1]);
                memcpy(dest, t, len);
                t += len;
                dest += len;
            }
        } while (!(tc & x04));
        return dest - orig_dest;
    }

#if OP_CHILD_PTR_SIZE == 1
    uint8_t copy_trie_half(uint8_t *tp, uint8_t *brk_key, int brk_key_len, uint8_t *dest, uint8_t which_half) {
        uint8_t tp_child[BPT_MAX_PFX_LEN];
#else
    int copy_trie_half(int *tp, uint8_t *brk_key, int brk_key_len, uint8_t *dest, uint8_t which_half) {
        int tp_child[BPT_MAX_PFX_LEN];
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
        int last_len = copy_kary(t, dest, lvl, tp, brk_key, brk_key_len, which_half);
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
                    uint8_t *child = trie + OP_GET_CHILD_OFFSET(d);
                    OP_SET_CHILD_OFFSET(d, dest - d);
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
#if OP_CHILD_PTR_SIZE == 1
                    d += (i + 3);
#else
                    d += (i * 2 + 3);
#endif
                    uint8_t *child = trie + OP_GET_CHILD_OFFSET(d);
                    OP_SET_CHILD_OFFSET(d, dest - d);
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
                    d += (tc & x02 ? 3 + OP_BIT_COUNT_CH(d[1]) + BIT_COUNT2(d[2]) :
                            2 + BIT_COUNT2(d[1]));
                    break;
                }
            }
        } while (1);
        return 0;
    }

    void consolidate_initial_prefix(uint8_t *t) {
        t += OCTP_HDR_SIZE;
        uint8_t *t_reader = t;
        uint8_t count = 0;
        if (*t & x01) {
            count = (*t >> 1);
            t_reader += count;
            t_reader++;
        }
        uint8_t *t_writer = t_reader + (*t & x01 ? 0 : 1);
        uint8_t trie_len_diff = 0;
#if OP_CHILD_PTR_SIZE == 1
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
    uint8_t *next_ptr(uint8_t *first_key, octp_iterator_vars& it) {
#else
    uint8_t *next_ptr(uint8_t *first_key, octp_iterator_vars& it) {
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
                    it.t += OP_BIT_COUNT_CH(it.child);
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
                it.t += OP_BIT_COUNT_CH(it.child);
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
                it.t -= OP_BIT_COUNT_CH(it.child & (xFF << it.ctr));
                int child_offset = OP_GET_CHILD_OFFSET(it.t);
                OP_SET_CHILD_OFFSET(it.t, it.t - trie + child_offset);
                it.t += child_offset;
                key_pos++;
                it.ctr = 0x09;
                it.tc = 0;
                continue;
            }
            it.ctr = (it.ctr == x07 ? 8 : FIRST_BIT_OFFSET_FROM_RIGHT((it.child | it.leaf) & (xFE << it.ctr)));
        } while (1); // (s.t - trie) < get_trie_len());
        return 0;
    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int orig_filled_size = filled_size();
        const int OCTP_NODE_SIZE = is_leaf() ? block_size : parent_block_size;
        uint8_t *b = allocate_block(OCTP_NODE_SIZE, is_leaf(), current_block[0] & 0x1F);
        octp new_block(OCTP_NODE_SIZE, b, is_leaf());
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        int kv_last_pos = get_kv_last_pos();
        int half_kv_pos = kv_last_pos + (OCTP_NODE_SIZE - kv_last_pos) / 2;

        int brk_idx;
        int brk_kv_pos;
        brk_idx = brk_kv_pos = 0;
        // (1) move all data to new_block in order
        int idx;
        uint8_t alloc_size = BPT_MAX_PFX_LEN + 1;
        uint8_t curr_key[alloc_size];
#if OP_CHILD_PTR_SIZE == 1
        uint8_t tp[alloc_size];
        uint8_t tp_cpy[alloc_size];
#else
        int tp[alloc_size];
        int tp_cpy[alloc_size];
#endif
        int tp_cpy_len = 0;
        octp_iterator_vars it(tp, new_block.trie);
        //if (!is_leaf())
        //   cout << "Trie len:" << (int) get_trie_len() << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
        new_block.key_pos = 0;
        memcpy(new_block.trie, trie, OP_GET_TRIE_LEN);
#if OP_CHILD_PTR_SIZE == 1
        new_block.get_trie_len() = get_trie_len();
#else
        util::set_int(new_block.BPT_TRIE_LEN_PTR, OP_GET_TRIE_LEN);
#endif
        for (idx = 0; idx < orig_filled_size; idx++) {
            uint8_t *leaf_ptr = new_block.next_ptr(curr_key, it);
            int src_idx = util::get_int(leaf_ptr);
            int kv_len = current_block[src_idx];
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
                memcpy(tp_cpy, tp, tp_cpy_len); // * OP_CHILD_PTR_SIZE);
                //curr_key[new_block.key_pos] = 0;
                //cout << "Middle:" << curr_key << endl;
                new_block.key_pos--;
            }
            kv_last_pos += kv_len;
            if (brk_idx == 0) {
                //brk_key_len = next_key(s);
                //if (tot_len > half_kv_len) {
                if (kv_last_pos > half_kv_pos || idx >= (orig_filled_size * 2 / 3)) {
                    //memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[key_pos+1+current_block[src_idx]] = 0;
                    //cout << first_key << ":";
                    brk_idx = idx + 1;
                    brk_idx = -brk_idx;
                    brk_kv_pos = kv_last_pos;
                    *first_len_ptr = new_block.key_pos + 1;
                    memcpy(first_key, curr_key, *first_len_ptr);
                    OP_SET_TRIE_LEN(new_block.copy_trie_half(tp, first_key, *first_len_ptr, trie, 1));
                }
            }
        }
#if OP_CHILD_PTR_SIZE == 1
        new_block.get_trie_len() = new_block.copy_trie_half(tp_cpy, first_key, tp_cpy_len, trie + get_trie_len(), 2);
        memcpy(new_block.trie, trie + get_trie_len(), new_block.get_trie_len());
#else
        util::set_int(new_block.BPT_TRIE_LEN_PTR, new_block.copy_trie_half(tp_cpy, first_key,
                tp_cpy_len, trie + OP_GET_TRIE_LEN, 2));
        memcpy(new_block.trie, trie + OP_GET_TRIE_LEN, util::get_int(new_block.BPT_TRIE_LEN_PTR));
#endif

        kv_last_pos = get_kv_last_pos() + OCTP_NODE_SIZE - kv_last_pos;
        int diff = (kv_last_pos - get_kv_last_pos());

        {
            memmove(new_block.current_block + brk_kv_pos + diff, new_block.current_block + brk_kv_pos,
                    OCTP_NODE_SIZE - brk_kv_pos - diff);
            brk_kv_pos += diff;
            new_block.set_ptr_diff(diff);
            new_block.set_kv_last_pos(brk_kv_pos);
            new_block.set_filled_size(orig_filled_size - brk_idx);
        }

        {
            int old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + OCTP_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos - diff, old_blk_new_len); // Copy back first half to old block
            diff += (OCTP_NODE_SIZE - brk_kv_pos);
            set_ptr_diff(diff);
            set_kv_last_pos(OCTP_NODE_SIZE - old_blk_new_len);
            set_filled_size(brk_idx);
        }

        if (!is_leaf())
            new_block.set_leaf(false);

#if OP_MIDDLE_PREFIX == 1
        consolidate_initial_prefix(current_block);
        new_block.consolidate_initial_prefix(new_block.current_block);
#endif

        //key_pos = 0;

        return new_block.current_block;

    }

    bool is_full(int search_result) {
        decode_need_count(search_result);
        if (get_kv_last_pos() < (OCTP_HDR_SIZE + OP_GET_TRIE_LEN
                + need_count + key_len - key_pos + value_len + 3))
            return true;
#if OP_CHILD_PTR_SIZE == 1
        if (OP_GET_TRIE_LEN > 254 - need_count)
            return true;
#endif
        return false;
    }

    void add_first_data() {
        add_data(3);
    }

    void add_data(int search_result) {

        insert_state = search_result + 1;

        int ptr = insert_current();

        int key_left = key_len - key_pos;
        int kv_last_pos = get_kv_last_pos() - (key_left + value_len + 2);
        set_kv_last_pos(kv_last_pos);
        util::set_int(trie + ptr, kv_last_pos);
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + key_pos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);
        set_filled_size(filled_size() + 1);

    }

    inline void update_ptrs(uint8_t *upto, int diff) {
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
#if OP_CHILD_PTR_SIZE == 1
                if ((t + *t) >= upto)
                    *t += diff;
                if (insert_state == INSERT_BEFORE) {
                    if (key_pos > 1 && (t + *t) == (orig_pos + 4))
                        *t -= 4;
                }
                t++;
#else
                int child_offset = OP_GET_CHILD_OFFSET(t);
                if ((t + child_offset) >= upto)
                    OP_SET_CHILD_OFFSET(t, child_offset + diff);
                if (insert_state == INSERT_BEFORE) {
                    if (key_pos > 1 && (t + OP_GET_CHILD_OFFSET(t)) == (orig_pos + 4))
                        OP_SET_CHILD_OFFSET(t, OP_GET_CHILD_OFFSET(t) - 4);
                }
                t += 2;
#endif
                // todo: avoid inside loops
            }
            t += BIT_COUNT2(leaves);
        }
    }

    inline void del_at(uint8_t *ptr, int count) {
        int trie_len = OP_GET_TRIE_LEN - count;
        OP_SET_TRIE_LEN(trie_len);
        memmove(ptr, ptr + count, trie + trie_len - ptr);
    }

    inline int ins_at(uint8_t *ptr, uint8_t b) {
        int trie_len = OP_GET_TRIE_LEN;
        memmove(ptr + 1, ptr, trie + trie_len - ptr);
        *ptr = b;
        OP_SET_TRIE_LEN(trie_len + 1);
        return 1;
    }

    inline int ins_at(uint8_t *ptr, uint8_t b1, uint8_t b2) {
        int trie_len = OP_GET_TRIE_LEN;
        memmove(ptr + 2, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr = b2;
        OP_SET_TRIE_LEN(trie_len + 2);
        return 2;
    }

    inline int ins_at(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3) {
        int trie_len = OP_GET_TRIE_LEN;
        memmove(ptr + 3, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        OP_SET_TRIE_LEN(trie_len + 3);
        return 3;
    }

    inline uint8_t ins_at(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
        int trie_len = OP_GET_TRIE_LEN;
        memmove(ptr + 4, ptr, trie + trie_len - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        OP_SET_TRIE_LEN(trie_len + 4);
        return 4;
    }

    void ins_bytes(uint8_t *ptr, int len) {
        int trie_len = OP_GET_TRIE_LEN;
        memmove(ptr + len, ptr, trie + trie_len - ptr);
        OP_SET_TRIE_LEN(trie_len + len);
    }

    int insert_current() {
        uint8_t key_char, mask;
        int diff;
        int ret;

        key_char = key[key_pos - 1];
        mask = x01 << (key_char & x07);
        switch (insert_state) {
        case INSERT_AFTER:
            *orig_pos &= xFB;
            trie_pos = orig_pos + (*orig_pos & x02 ? OP_BIT_COUNT_CH(orig_pos[1])
                    + BIT_COUNT2(orig_pos[2]) + 3 : BIT_COUNT2(orig_pos[1]) + 2);
            update_ptrs(trie_pos, 4);
            ins_at(trie_pos, ((key_char & xF8) | x04), mask, 0, 0);
            ret = trie_pos - trie + 2;
            break;
        case INSERT_BEFORE:
            update_ptrs(orig_pos, 4);
            ins_at(orig_pos, (key_char & xF8), mask, 0, 0);
            ret = orig_pos - trie + 2;
            break;
        case INSERT_LEAF:
            trie_pos = orig_pos + ((*orig_pos & x02) ? 2 : 1);
            *trie_pos |= mask;
            trie_pos += ((*orig_pos & x02 ? OP_BIT_COUNT_CH(orig_pos[1]) : 0)
                    + BIT_COUNT2(*trie_pos & (mask - 1)) + 1);
            update_ptrs(trie_pos, 2);
            ins_at(trie_pos, x00, x00);
            ret = trie_pos - trie;
            break;
#if OP_MIDDLE_PREFIX == 1
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
#if OP_CHILD_PTR_SIZE == 1
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
#if OP_CHILD_PTR_SIZE == 1
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
#if OP_CHILD_PTR_SIZE == 1
                  *trie_pos++ = 1;
#else
                  *trie_pos++ = 0;
                  *trie_pos++ = 2;
#endif
                  break;
              case 2:
                  *trie_pos++ = mask;
#if OP_CHILD_PTR_SIZE == 1
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
              int p, min;
              uint8_t c1, c2;
              uint8_t *child_pos;
              int ptr, pos;
              ret = pos = 0;

              c1 = c2 = key_char;
              p = key_pos;
              min = util::min_b(key_len, key_pos + key_at_len);
              child_pos = orig_pos + 1;
              if (*orig_pos & x02) {
                  *child_pos |= mask;
                  child_pos = child_pos + 2 + OP_BIT_COUNT_CH(*child_pos & (mask - 1));
#if OP_CHILD_PTR_SIZE == 1
                  ins_at(child_pos, (uint8_t) (get_trie_len() - (child_pos - trie) + 1));
                  update_ptrs(child_pos, 1);
#else
                  int offset = (OP_GET_TRIE_LEN + 1 - (child_pos - trie) + 1);
                  ins_at(child_pos, offset >> 8, offset & xFF);
                  update_ptrs(child_pos, 2);
#endif
              } else {
                  *orig_pos |= x02;
#if OP_CHILD_PTR_SIZE == 1
                  ins_at(child_pos, mask, *child_pos);
                  child_pos[2] = (uint8_t) (get_trie_len() - (child_pos + 2 - trie));
                  update_ptrs(child_pos, 2);
#else
                  int offset = OP_GET_TRIE_LEN + 3 - (child_pos + 2 - trie);
                  ins_at(child_pos, mask, *child_pos, (uint8_t) (offset >> 8));
                  child_pos[3] = offset & xFF;
                  update_ptrs(child_pos, 3);
#endif
              }
              trie_pos = orig_pos + OP_BIT_COUNT_CH(orig_pos[1]) + 3
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
              //diff = (p == min ? 4 : (key[diff] ^ key_at[diff - key_pos]) > x07 ? 8
              //        : (key[diff] == key_at[diff - key_pos] ? 9 + OP_CHILD_PTR_SIZE : 6));
              trie_pos = trie + OP_GET_TRIE_LEN;
#if OP_MIDDLE_PREFIX == 1
              //diff += need_count + (need_count ? (need_count / 128) + 1 : 0);
#else
              //diff += need_count * (3 + OP_CHILD_PTR_SIZE);
#endif
              //OP_SET_TRIE_LEN(OP_GET_TRIE_LEN + diff);
#if OP_MIDDLE_PREFIX == 1
              //diff += need_count + (need_count ? (need_count / 128) + 1 : 0);
              if (need_count) {
                  uint8_t copied = 0;
                  while (copied < need_count) {
                      int to_copy = (need_count - copied) > 127 ? 127 : need_count - copied;
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
#if OP_MIDDLE_PREFIX == 1
                  switch ((c1 ^ c2) > x07 ? 0 : (c1 == c2 ? 3 : 1)) {
#else
                  switch ((c1 ^ c2) > x07 ?
                          0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
                  case 2:
                      *trie_pos++ = (c1 & xF8) | x06;
                      *trie_pos++ = x01 << (c1 & x07);
                      *trie_pos++ = 0;
#if OP_CHILD_PTR_SIZE == 1
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
#if OP_CHILD_PTR_SIZE == 1
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
              OP_SET_TRIE_LEN(trie_pos - trie);
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
            OP_SET_TRIE_LEN(4);
            break;
        }

        if (BPT_MAX_PFX_LEN < key_pos)
            BPT_MAX_PFX_LEN = key_pos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

        return ret;
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

    uint8_t *next_rec(bptree_iter_ctx *ctx, uint8_t *val_buf, int *val_buf_len) {
        return NULL;
    }

};

#endif
