#ifndef bfqs_H
#define bfqs_H
#ifdef ARDUINO
#if (defined(__AVR__))
#include <avr/pgmspace.h>
#else
#include <pgmspace.h>
#endif
#else
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

#if defined(ARDUINO)
#define BIT_COUNT_LF_CH(x) pgm_read_byte_near(util::bit_count_lf_ch + (x))
#else
#define BIT_COUNT_LF_CH(x) util::bit_count_lf_ch[x]
//#define BIT_COUNT_LF_CH(x) (BIT_COUNT(x & xAA) + BIT_COUNT2(x & x55))
#endif

#define BQ_MIDDLE_PREFIX 1

#define BFQS_HDR_SIZE 9

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class bfqs: public bpt_trie_handler<bfqs> {
public:
    uint8_t need_counts[10];
    uint8_t switch_map[8];
    uint8_t shift_mask[8];
    uint8_t *last_t;
    uint8_t last_leaf_child;

    bfqs(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, const uint8_t opts = 0) :
                bpt_trie_handler<bfqs>(leaf_block_sz, parent_block_sz, cache_sz, fname, opts) {
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x06\x00\x00\x00", 10);
        memcpy(switch_map, "\x00\x01\x02\x03\x00\x01\x00\x01", 8);
        memcpy(shift_mask, "\x00\x00\x03\x03\x0F\x0F\x3F\x3F", 8);
    }

    bfqs(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bpt_trie_handler<bfqs>(block_sz, block, is_leaf) {
        init_stats();
        memcpy(need_counts, "\x00\x04\x04\x02\x04\x00\x06\x00\x00\x00", 10);
        memcpy(switch_map, "\x00\x01\x02\x03\x00\x01\x00\x01", 8);
        memcpy(shift_mask, "\x00\x00\x03\x03\x0F\x0F\x3F\x3F", 8);
    }

    bfqs(const char *filename, int blk_size, int page_resv_bytes, const uint8_t opts) :
       bpt_trie_handler<bfqs>(filename, blk_size, page_resv_bytes, opts) {
        init_stats();
    }

    inline void set_current_block_root() {
        current_block = root_block;
        trie = current_block + BFQS_HDR_SIZE;
    }

    inline void set_current_block(uint8_t *m) {
        current_block = m;
        trie = current_block + BFQS_HDR_SIZE;
    }

    inline uint8_t *get_last_ptr() {
        //key_pos = 0;
        while ((last_leaf_child & xAA) > (last_leaf_child & x55)) {
                last_t += BIT_COUNT_LF_CH(last_leaf_child & xAA) + 1;
                last_t += *last_t;
                while (*last_t & x01) {
                    last_t += (*last_t >> 1);
                    last_t++;
                }
                while (!(*last_t & x02))
                    last_t += BIT_COUNT_LF_CH(last_t[1]) + 2;
                last_leaf_child = last_t[1];
        }
        return current_block + util::get_int(last_t +
                BIT_COUNT_LF_CH((last_t[1] & xAA) | (last_leaf_child & x55)));
    //    do {
    //        int rslt = ((last_leaf_child & xAA) > (last_leaf_child & x55) ? 2 : 1);
    //        switch (rslt) {
    //        case 1:
    //            return current_block + util::get_int(last_t +
    //                    BIT_COUNT_LF_CH((last_t[1] & xAA) | (last_leaf_child & x55)));
    //        case 2:
    //            last_t += BIT_COUNT_LF_CH(last_leaf_child & xAA) + 1;
    //            last_t += *last_t;
    //            while (*last_t & x01) {
    //                last_t += (*last_t >> 1);
    //                last_t++;
    //            }
    //            while (!(*last_t & x02))
    //                last_t += BIT_COUNT_LF_CH(last_t[1]) + 2;
    //            last_leaf_child = last_t[1];
    //        }
    //    } while (1);
        return 0;
    }

    inline void set_prefix_last(uint8_t key_char, uint8_t *t, uint8_t pfx_rem_len) {
        if (key_char > *t) {
            t += pfx_rem_len;
            while (!(*t & x02))
                t += BIT_COUNT_LF_CH(t[1]) + 2;
            last_t = t;
            last_leaf_child = t[1];
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
    #if BQ_MIDDLE_PREFIX == 1
            switch (trie_char & x01 ? 3 : (key_char ^ trie_char) > x03
                    ? (key_char > trie_char ? 0 : 2) : 1) {
    #else
            switch ((key_char ^ trie_char) > x03 ?
                    (key_char > trie_char ? 0 : 2) : 1) {
    #endif
            case 0:
                last_t = orig_pos;
                last_leaf_child = *t++;
                t += BIT_COUNT_LF_CH(last_leaf_child);
                if (trie_char & x02) {
                    //if (!is_leaf())
                    //    last_t = get_last_ptr();
                        insert_state = INSERT_AFTER;
                    return -1;
                }
                break;
            case 1:
                uint8_t r_shft, r_leaves_children;
                r_leaves_children = *t++;
                key_char = (key_char & x03) << 1;
                r_shft = ~(xFF << key_char); //shift_mask[key_char];
                if (!is_leaf() && (r_leaves_children & r_shft)) {
                    last_t = orig_pos;
                    last_leaf_child = r_leaves_children & r_shft;
                }
                trie_char = switch_map[((r_leaves_children >> key_char) & x03) | (key_pos == key_len ? 4 : 0)];
                switch (trie_char) {
                case 0:
                //case 4:
                //case 6:
                       insert_state = INSERT_LEAF;
                    //if (!is_leaf())
                    //    last_t = get_last_ptr();
                    return -1;
                case 2:
                    break;
                case 1:
                //case 5:
                //case 7:
                    int cmp;
                    t += BIT_COUNT_LF_CH(r_leaves_children & (r_shft | xAA));
                    key_at = current_block + util::get_int(t);
                    key_at_len = *key_at++;
                    cmp = util::compare(key + key_pos, key_len - key_pos, key_at, key_at_len);
                    if (cmp == 0) {
                        last_t = key_at - 1;
                        return 1;
                    }
                    if (cmp < 0) {
                        cmp = -cmp;
                        //if (!is_leaf())
                        //    last_t = get_last_ptr();
                    } else
                        last_t = key_at - 1;
                        insert_state = INSERT_THREAD;
    #if BQ_MIDDLE_PREFIX == 1
                        need_count = cmp + 6;
    #else
                        need_count = (cmp * 4) + 10;
    #endif
                    return -1;
                case 3:
                    if (!is_leaf()) {
                        last_t = orig_pos;
                        last_leaf_child = (r_leaves_children & r_shft) | (x01 << key_char);
                    }
                    break;
                }
                t += BIT_COUNT_LF_CH(r_leaves_children & r_shft & xAA);
                t += *t;
                key_char = key[key_pos++];
                break;
            case 2:
                //if (!is_leaf())
                //    last_t = get_last_ptr();
                    insert_state = INSERT_BEFORE;
                return -1;
    #if BQ_MIDDLE_PREFIX == 1
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
                if (!is_leaf()) {
                    set_prefix_last(key_char, t, pfx_len);
                    //last_t = get_last_ptr();
                }
                    insert_state = INSERT_CONVERT;
                return -1;
    #endif
            }
            trie_char = *t;
            orig_pos = t++;
        } while (1);
        return -1;
    }

    inline uint8_t *get_child_ptr_pos(int search_result) {
        return last_t - current_block < get_kv_last_pos() ? get_last_ptr() : last_t;
    }

    inline uint8_t *get_ptr_pos() {
        return trie + get_trie_len();
    }

    inline int get_header_size() {
        return BFQS_HDR_SIZE;
    }

    uint8_t copy_kary(uint8_t *t, uint8_t *dest, int lvl, uint8_t *tp,
            uint8_t *brk_key, int brk_key_len, uint8_t which_half) {
        uint8_t *orig_dest = dest;
        if (*t & x01) {
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
                *dest = tc;
                uint8_t offset = (lvl < brk_key_len ? (brk_key[lvl] & x03) << 1 : x03);
                t++;
                uint8_t leaves_children = *t++;
                uint8_t orig_leaves_children = leaves_children;
                if (which_half == 1) {
                    leaves_children &= ~(((brk_key_len - lvl) == 1 ? xFE : xFC) << offset);
                    *dest++ |= x02;
                    *dest++ = leaves_children;
                    for (int i = 0; i < BIT_COUNT_LF_CH(leaves_children & xAA); i++)
                        *dest++ = t[i];
                    t += BIT_COUNT_LF_CH(orig_leaves_children & xAA);
                    memcpy(dest, t, BIT_COUNT_LF_CH(leaves_children & x55));
                    dest += BIT_COUNT_LF_CH(leaves_children & x55);
                    break;
                } else {
                    leaves_children &= (((brk_key_len - lvl) == 1 ? xFF : xFE) << offset);
                    dest++;
                    *dest++ = leaves_children;
                    for (int i = BIT_COUNT_LF_CH(xAA & (orig_leaves_children - leaves_children));
                            i < BIT_COUNT_LF_CH(xAA & orig_leaves_children); i++)
                        *dest++ = t[i];
                    t += BIT_COUNT_LF_CH(orig_leaves_children & xAA);
                    memcpy(dest, t + BIT_COUNT_LF_CH(x55 & (orig_leaves_children - leaves_children)),
                            BIT_COUNT_LF_CH(x55 & leaves_children));
                    dest += BIT_COUNT_LF_CH(x55 & leaves_children);
                }
                t += BIT_COUNT_LF_CH(orig_leaves_children & x55);
            } else {
                uint8_t len = BIT_COUNT_LF_CH(t[1]) + 2;
                memcpy(dest, t, len);
                t += len;
                dest += len;
            }
        } while (!(tc & x02));
        return dest - orig_dest;
    }

    uint8_t copy_trie_half(uint8_t *tp, uint8_t *brk_key, int brk_key_len, uint8_t *dest, uint8_t which_half) {
        uint8_t *d;
        uint8_t *t = trie;
        uint8_t *new_trie = dest;
        uint8_t tp_child[BPT_MAX_PFX_LEN];
        uint8_t child_num[BPT_MAX_PFX_LEN];
        int lvl = 0;
        if (*t & x01) {
            uint8_t len = (*t >> 1);
            memset(tp_child, dest - new_trie, len);
            lvl = len;
        }
        uint8_t last_len = copy_kary(t, dest, lvl, tp, brk_key, brk_key_len, which_half);
        d = dest;
        dest += last_len;
        do {
            uint8_t tc = *d++;
            if (tc & x01) {
                d += (tc >> 1);
                tc = *d++;
            }
            uint8_t len = BIT_COUNT_LF_CH(xAA & *d);
            if (len) {
                d++;
                tp_child[lvl] = d - new_trie - 2;
                uint8_t *child = trie + *d;
                *d = dest - d;
                child_num[lvl++] = 0;
                t = child;
                if (*t & x01) {
                    uint8_t len = (*t >> 1);
                    memset(tp_child + lvl, dest - new_trie, len);
                    lvl += len;
                }
                last_len = copy_kary(t, dest, lvl, tp, brk_key, brk_key_len, which_half);
                d = dest;
                dest += last_len;
                continue;
            } else
                d += (BIT_COUNT_LF_CH(*d) + 1);
            while (tc & x02) {
                do {
                    lvl--;
                    d = new_trie + tp_child[lvl];
                    tc = *d;
                } while (tc & x01);
                if (lvl < 0)
                    return dest - new_trie;
                d = new_trie + tp_child[lvl];
                uint8_t len = BIT_COUNT_LF_CH(xAA & d[1]);
                uint8_t i = child_num[lvl];
                i++;
                if (i < len) {
                    d += (i + 2);
                    uint8_t *child = trie + *d;
                    *d = dest - d;
                    child_num[lvl++] = i;
                    t = child;
                    if (*t & x01) {
                        uint8_t len = (*t >> 1);
                        memset(tp_child + lvl, dest - new_trie, len);
                        lvl += len;
                    }
                    last_len = copy_kary(t, dest, lvl, tp, brk_key, brk_key_len, which_half);
                    d = dest;
                    dest += last_len;
                    break;
                }
                tc = *d;
                if (!(tc & x02)) {
                    d += BIT_COUNT_LF_CH(d[1]) + 2;
                    break;
                }
            }
        } while (1);
        return 0;
    }

    void set_ptr_diff(int diff) {
        uint8_t *t = trie;
        uint8_t *t_end = trie + get_trie_len();
        while (t < t_end) {
            uint8_t tc = *t++;
            if (tc & x01) {
                t += (tc >> 1);
                continue;
            }
            uint8_t leaves_count = BIT_COUNT_LF_CH(*t & x55);
            t += BIT_COUNT_LF_CH(*t & xAA);
            t++;
            for (int i = 0; i < leaves_count; i += 2) {
                util::set_int(t, util::get_int(t) + diff);
                t += 2;
            }
        }
    }

    void consolidate_initial_prefix(uint8_t *t) {
        t += BFQS_HDR_SIZE;
        uint8_t *t1 = t;
        if (*t & x01) {
            t1 += (*t >> 1);
            t1++;
        }
        uint8_t *t2 = t1 + (*t & x01 ? 0 : 1);
        uint8_t count = 0;
        uint8_t trie_len_diff = 0;
        while ((*t1 & x01) || ((*t1 & x02) && BIT_COUNT_LF_CH(t1[1]) == 1 && t1[2] == 1)) {
            if (*t1 & x01) {
                uint8_t len = *t1++ >> 1;
                memcpy(t2, t1, len);
                t2 += len;
                t1 += len;
                count += len;
                trie_len_diff++;
            } else {
                *t2++ = (*t1 & xFC) + (BIT_COUNT(t1[1] - 1) >> 1);
                t1 += 3;
                count++;
                trie_len_diff += 2;
            }
        }
        if (t1 > t2) {
            memmove(t2, t1, get_trie_len() - (t1 - t));
            if (*t & x01) {
                *t = (((*t >> 1) + count) << 1) + 1;
            } else {
                *t = (count << 1) + 1;
                trie_len_diff--;
            }
            change_trie_len(-trie_len_diff);
            //cout << (int) (*t >> 1) << endl;
        }
    }

    int insert_after() {
        uint8_t key_char;
        uint8_t mask;
        key_char = key[key_pos - 1];
        mask = x01 << ((key_char & x03) << 1);
        *orig_pos &= xFC;
        trie_pos = orig_pos + 2 + BIT_COUNT_LF_CH(orig_pos[1]);
        update_ptrs(trie_pos, 4);
        ins_b4(trie_pos, ((key_char & xFC) | x02), mask, 0, 0);
        return trie_pos - trie + 2;
    }

    int insert_before() {
        uint8_t key_char;
        uint8_t mask;
        key_char = key[key_pos - 1];
        mask = x01 << ((key_char & x03) << 1);
        update_ptrs(orig_pos, 4);
        ins_b4(orig_pos, (key_char & xFC), mask, 0, 0);
        return orig_pos - trie + 2;
    }

    int insert_leaf() {
        uint8_t key_char;
        uint8_t mask;
        key_char = key[key_pos - 1];
        mask = x01 << ((key_char & x03) << 1);
        trie_pos = orig_pos + 1;
        *trie_pos++ |= mask;
        trie_pos += BIT_COUNT_LF_CH(orig_pos[1])
                - BIT_COUNT_LF_CH(orig_pos[1] & (0x55 << ((key_char & x03) << 1)));
        update_ptrs(trie_pos, 2);
        ins_b2(trie_pos, x00, x00);
        return trie_pos - trie;
    }

    #if BQ_MIDDLE_PREFIX == 1
    int insert_convert() {
        uint8_t key_char;
        uint8_t mask;
        int ret = 0;
        key_char = key[key_pos - 1];
        mask = x01 << ((key_char & x03) << 1);
        uint8_t b, c;
        char cmp_rel;
        int diff;
        diff = trie_pos - orig_pos;
        // 3 possible relationships between key_char and *trie_pos, 4 possible positions of trie_pos
        c = *trie_pos;
        cmp_rel = ((c ^ key_char) > x03 ? (c < key_char ? 0 : 1) : 2);
        if (diff == 1)
            trie_pos = orig_pos;
        b = (cmp_rel == 2 ? x02 : x00); // | (cmp_rel == 1 ? x00 : x02);
        need_count = (*orig_pos >> 1) - diff;
        diff--;
        *trie_pos++ = ((cmp_rel == 0 ? c : key_char) & xFC) | b;
        b = (cmp_rel == 2 ? 3 : 5);
        if (diff) {
            b++;
            *orig_pos = (diff << 1) | x01;
        }
        if (need_count)
            b++;
        ins_bytes(trie_pos, b);
        update_ptrs(trie_pos, b);
        switch (cmp_rel) {
        case 0:
            *trie_pos++ = x02 << ((c & x03) << 1);
            *trie_pos++ = 5;
            *trie_pos++ = (key_char & xFC) | 0x02;
            *trie_pos++ = mask;
            ret = trie_pos - trie;
            trie_pos += 2;
            break;
        case 1:
            *trie_pos++ = mask;
            ret = trie_pos - trie;
            trie_pos += 2;
            *trie_pos++ = (c & xFC) | x02;
            *trie_pos++ = x02 << ((c & x03) << 1);
            *trie_pos++ = 1;
            break;
        case 2:
            *trie_pos++ = (x02 << ((c & x03) << 1)) | mask;
            *trie_pos++ = 3;
            ret = trie_pos - trie;
            trie_pos += 2;
            break;
        }
        if (need_count)
            *trie_pos = (need_count << 1) | x01;
        return ret;
    }
    #endif

    int insert_thread() {
        uint8_t key_char;
        uint8_t mask;
        int p, min;
        uint8_t c1, c2;
        int diff;
        int ret, ptr, pos;
        ret = pos = 0;

        key_char = key[key_pos - 1];
        mask = x01 << ((key_char & x03) << 1);
        c1 = c2 = key_char;
        {
            trie_pos = orig_pos + 2 + BIT_COUNT_LF_CH(orig_pos[1] & xAA & (mask - 1));
            ins_b(trie_pos, (uint8_t) (get_trie_len() - (trie_pos - trie) + 1));
            update_ptrs(trie_pos, 1);
            orig_pos[1] |= (mask << 1);
        }
        p = key_pos;
        min = util::min16(key_len, key_pos + key_at_len);
        trie_pos = orig_pos + 2 + (BIT_COUNT_LF_CH(orig_pos[1])
                - BIT_COUNT_LF_CH(orig_pos[1] & (0x55 << ((key_char & x03) << 1))));
        ptr = util::get_int(trie_pos);
        if (p < min) {
            orig_pos[1] &= ~mask;
            del_at(trie_pos, 2);
            update_ptrs(trie_pos, -2);
        } else {
            pos = trie_pos - trie;
            ret = pos;
        }
    #if BQ_MIDDLE_PREFIX == 1
        need_count -= 7;
        if (p + need_count == min) {
            if (need_count) {
                need_count--;
            }
        }
        if (need_count) {
            append((need_count << 1) | x01);
            append(key + key_pos, need_count);
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
    #if BQ_MIDDLE_PREFIX == 1
            switch ((c1 ^ c2) > x03 ? 0 : (c1 == c2 ? 3 : 1)) {
    #else
            switch ((c1 ^ c2) > x03 ?
                    0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
            case 2:
                append((c1 & xFC) | x02);
                append(x02 << ((c1 & x03) << 1));
                append(1);
                break;
    #endif
            case 0:
                append(c1 & xFC);
                append(x01 << ((c1 & x03) << 1));
                ret = is_swapped ? ret : get_trie_len();
                pos = is_swapped ? get_trie_len() : pos;
                append_ptr(is_swapped ? ptr : 0);
                append((c2 & xFC) | x02);
                append(x01 << ((c2 & x03) << 1));
                ret = is_swapped ? get_trie_len() : ret;
                pos = is_swapped ? pos : get_trie_len();
                append_ptr(is_swapped ? 0 : ptr);
                break;
            case 1:
                append((c1 & xFC) | x02);
                append((x01 << ((c1 & x03) << 1) | (x01 << ((c2 & x03) << 1))));
                ret = is_swapped ? ret : get_trie_len();
                pos = is_swapped ? get_trie_len() : pos;
                append_ptr(is_swapped ? ptr : 0);
                ret = is_swapped ? get_trie_len() : ret;
                pos = is_swapped ? pos : get_trie_len();
                append_ptr(is_swapped ? 0 : ptr);
                break;
            case 3:
                append((c1 & xFC) | x02);
                append(x03 << ((c1 & x03) << 1));
                append(3);
                ret = (p + 1 == key_len) ? get_trie_len() : ret;
                pos = (p + 1 == key_len) ? pos : get_trie_len();
                append_ptr((p + 1 == key_len) ? 0 : ptr);
                break;
            }
            if (c1 != c2)
                break;
            p++;
        }
        diff = p - key_pos;
        key_pos = p + 1;
        if (c1 == c2) {
            c2 = (p == key_len ? key_at[diff] : key[p]);
            append((c2 & xFC) | x02);
            append(x01 << ((c2 & x03) << 1));
            ret = (p == key_len) ? ret : get_trie_len();
            pos = (p == key_len) ? get_trie_len() : pos;
            append_ptr((p == key_len) ? ptr : 0);
            if (p == key_len)
                key_pos--;
        }
        if (diff < key_at_len)
            diff++;
        if (diff) {
            p = ptr;
            key_at_len -= diff;
            p += diff;
            if (key_at_len >= 0) {
                current_block[p] = key_at_len;
                util::set_int(trie + pos, p);
            }
        }
        return ret;
    }

    int insert_empty() {
        append((*key & xFC) | x02);
        append(x01 << ((*key & x03) << 1));
        int ret = get_trie_len();
        key_pos = 1;
        append(0);
        append(0);
        return ret;
    }

    void add_first_data() {
        add_data(0);
    }

    void add_data(int search_result) {

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

    bool is_full(int search_result) {
        decode_need_count();
        if (get_kv_last_pos() < (BFQS_HDR_SIZE + get_trie_len() +
                need_count + key_len - key_pos + value_len + 3))
            return true;
        if (get_trie_len() > 254 - need_count)
            return true;
        return false;
    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int orig_filled_size = filled_size();
        const int BFQS_NODE_SIZE = is_leaf() ? block_size : parent_block_size;
        uint8_t *b = allocate_block(BFQS_NODE_SIZE, is_leaf(), current_block[0] & 0x1F);
        bfqs new_block(BFQS_NODE_SIZE, b, is_leaf());
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        int kv_last_pos = get_kv_last_pos();
        int half_kVPos = kv_last_pos + (BFQS_NODE_SIZE - kv_last_pos) / 2;

        int brk_idx, brk_kv_pos;
        brk_idx = brk_kv_pos = 0;
        // (1) move all data to new_block in order
        int idx = 0;
        uint8_t alloc_size = BPT_MAX_PFX_LEN + 1;
        uint8_t curr_key[alloc_size];
        uint8_t tp[alloc_size];
        uint8_t tp_cpy[alloc_size];
        int tp_cpy_len = 0;
        uint8_t *t = new_block.trie;
        //if (!is_leaf())
        //   cout << "Trie len:" << (int) get_trie_len() << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
        new_block.key_pos = 0;
        memcpy(new_block.trie, trie, get_trie_len());
        new_block.set_trie_len(get_trie_len());
        uint8_t tc, leaf_child;
        tc = leaf_child = 0;
        uint8_t ctr = 5;
        do {
            while (ctr == x04) {
                if (tc & x02) {
                    new_block.key_pos--;
                    t = new_block.trie + tp[new_block.key_pos];
                    if (*t & x01) {
                        new_block.key_pos -= (*t >> 1);
                        t = new_block.trie + tp[new_block.key_pos];
                    }
                    tc = *t++;
                    leaf_child = *t++;
                    ctr = curr_key[new_block.key_pos] & x03;
                    leaf_child &= (xFC << (ctr * 2));
                    ctr = leaf_child ? FIRST_BIT_OFFSET_FROM_RIGHT(leaf_child) >> 1 : 4;
                } else {
                    t += BIT_COUNT_LF_CH(*(t - 1));
                    ctr = 0x05;
                    break;
                }
            }
            if (ctr > x03) {
                tp[new_block.key_pos] = t - new_block.trie;
                tc = *t++;
                if (tc & x01) {
                    uint8_t len = tc >> 1;
                    memset(tp + new_block.key_pos, t - new_block.trie - 1, len);
                    memcpy(curr_key + new_block.key_pos, t, len);
                    t += len;
                    new_block.key_pos += len;
                    tp[new_block.key_pos] = t - new_block.trie;
                    tc = *t++;
                }
                leaf_child = *t++;
                ctr = FIRST_BIT_OFFSET_FROM_RIGHT(leaf_child) >> 1;
            }
            curr_key[new_block.key_pos] = (tc & xFC) | ctr;
            uint8_t mask = x01 << (ctr * 2);
            if (leaf_child & mask) {
                uint8_t *leaf_ptr = t + BIT_COUNT_LF_CH(*(t - 1) & ((mask - 1) | xAA));
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
                    memcpy(tp_cpy, tp, tp_cpy_len);
                    //curr_key[new_block.key_pos] = 0;
                    //cout << "Middle:" << curr_key << endl;
                    new_block.key_pos--;
                }
                kv_last_pos += kv_len;
                if (brk_idx == 0) {
                    //brk_key_len = next_key(s);
                    //if (kv_last_pos > half_kVLen) {
                    if (kv_last_pos > half_kVPos || idx == (orig_filled_size / 2)) {
                        //memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
                        //first_key[key_pos+1+current_block[src_idx]] = 0;
                        //cout << first_key << ":";
                        brk_idx = idx + 1;
                        brk_idx = -brk_idx;
                        brk_kv_pos = kv_last_pos;
                        *first_len_ptr = new_block.key_pos + 1;
                        memcpy(first_key, curr_key, *first_len_ptr);
                        set_trie_len(new_block.copy_trie_half(tp, first_key, *first_len_ptr, trie, 1));
                    }
                }
                idx++;
                if (idx == orig_filled_size)
                    break;
            }
            if (leaf_child & (x02 << (ctr * 2))) {
                t += BIT_COUNT_LF_CH(*(t - 1) & xAA & (mask - 1));
                uint8_t child_offset = *t;
                *t = t - new_block.trie + child_offset;
                t += child_offset;
                new_block.key_pos++;
                ctr = 5;
                continue;
            }
            leaf_child &= ~(x03 << (ctr * 2));
            ctr = leaf_child ? FIRST_BIT_OFFSET_FROM_RIGHT(leaf_child) >> 1 : 4;
        } while (1);
        new_block.set_trie_len(new_block.copy_trie_half(tp_cpy, first_key, tp_cpy_len, trie + get_trie_len(), 2));
        memcpy(new_block.trie, trie + get_trie_len(), new_block.get_trie_len());

        kv_last_pos = get_kv_last_pos() + BFQS_NODE_SIZE - kv_last_pos;
        int diff = (kv_last_pos - get_kv_last_pos());

        {
            memmove(new_block.current_block + brk_kv_pos + diff, new_block.current_block + brk_kv_pos,
                    BFQS_NODE_SIZE - brk_kv_pos - diff);
            brk_kv_pos += diff;
            new_block.set_ptr_diff(diff);
            new_block.set_kv_last_pos(brk_kv_pos);
            new_block.set_filled_size(orig_filled_size - brk_idx);
        }

        {
            int old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + BFQS_NODE_SIZE - old_blk_new_len, // Copy back first half to old block
                    new_block.current_block + kv_last_pos - diff, old_blk_new_len);
            diff += (BFQS_NODE_SIZE - brk_kv_pos);
            set_ptr_diff(diff);
            set_kv_last_pos(BFQS_NODE_SIZE - old_blk_new_len);
            set_filled_size(brk_idx);
        }

        if (!is_leaf())
            new_block.set_leaf(false);

        consolidate_initial_prefix(current_block);
        new_block.consolidate_initial_prefix(new_block.current_block);

        //key_pos = 0;

        return new_block.current_block;

    }

    int insert_current() {
        int ret;

        switch (insert_state) {
        case INSERT_AFTER:
            ret = insert_after();
            break;
        case INSERT_BEFORE:
            ret = insert_before();
            break;
        case INSERT_THREAD:
            ret = insert_thread();
            break;
        case INSERT_LEAF:
            ret = insert_leaf();
            break;
    #if BQ_MIDDLE_PREFIX == 1
        case INSERT_CONVERT:
            ret = insert_convert();
            break;
    #endif
        case INSERT_EMPTY:
            ret = insert_empty();
            break;
        }

        if (BPT_MAX_PFX_LEN < key_pos)
            BPT_MAX_PFX_LEN = key_pos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

        return ret;
    }
    void update_ptrs(uint8_t *upto, int diff) {
        uint8_t *t = trie;
        uint8_t tc = *t++;
        while (t <= upto) {
            if (tc & x01) {
                t += (tc >> 1);
                tc = *t++;
                continue;
            }
            int leaf_count = BIT_COUNT_LF_CH(*t & x55);
            int child_count = BIT_COUNT_LF_CH(*t & xAA);
            t++;
            while (child_count--) {
                if (t < upto && (t + *t) >= upto)
                    *t += diff;
                // todo: avoid inside loops
                if (insert_state == INSERT_BEFORE && key_pos > 1 && (t + *t) == (orig_pos + 4))
                    *t -= 4;
                t++;
            }
            t += leaf_count;
            tc = *t++;
        }
    }

    void decode_need_count() {
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
