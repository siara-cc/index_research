#ifndef dfqx_H
#define dfqx_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

#if BPT_9_BIT_PTR == 1
#define DQ_MAX_PTR_BITMAP_BYTES 8
#define DQ_MAX_PTRS 63
#else
#define DQ_MAX_PTR_BITMAP_BYTES 0
#define DQ_MAX_PTRS 240
#endif
#define DFQX_HDR_SIZE 9

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class dfqx: public bpt_trie_handler<dfqx> {
private:
    void init_consts() {

        memcpy(need_counts, (const uint8_t[]) {0, 2, 2, 2, 2, 0, 0, 0, 0, 0}, 10);
        memcpy(left_mask, (const uint8_t[]) { 0x07, 0x03, 0x01, 0x00 }, 4);
        memcpy(left_incl_mask, (const uint8_t[]) { 0x0F, 0x07, 0x03, 0x01 }, 4);
        memcpy(ryte_mask, (const uint8_t[]) { 0x00, 0x08, 0x0C, 0x0E }, 4);
        memcpy(dbl_ryte_mask, (const uint8_t[]) { 0x00, 0x88, 0xCC, 0xEE }, 4);
        memcpy(ryte_incl_mask, (const uint8_t[]) { 0x08, 0x0C, 0x0E, 0x0F }, 4);
        memcpy(first_bit_offset, (const uint8_t[]) { 0x04, 0x03, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 16);
        memcpy(bit_count, (const uint8_t[]) { 0x00, 0x01, 0x01, 0x02, 0x01, 0x02, 0x02, 0x03, 0x01, 0x02, 0x02, 0x03, 0x02, 0x03, 0x03, 0x04 }, 16);

#if (defined(__AVR__))
        memcpy_s(dbl_bit_count,
#else
        memcpy(dbl_bit_count,
#endif
            (const int[256]) {
                //0000   0001   0010   0011   0100   0101   0110   0111   1000   1001   1010   1011   1100   1101   1110   1111
                0x000, 0x001, 0x001, 0x002, 0x001, 0x002, 0x002, 0x003, 0x001, 0x002, 0x002, 0x003, 0x002, 0x003, 0x003, 0x004, //0000
                0x100, 0x101, 0x101, 0x102, 0x101, 0x102, 0x102, 0x103, 0x101, 0x102, 0x102, 0x103, 0x102, 0x103, 0x103, 0x104, //0001
                0x100, 0x101, 0x101, 0x102, 0x101, 0x102, 0x102, 0x103, 0x101, 0x102, 0x102, 0x103, 0x102, 0x103, 0x103, 0x104, //0010
                0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //0011
                0x100, 0x101, 0x101, 0x102, 0x101, 0x102, 0x102, 0x103, 0x101, 0x102, 0x102, 0x103, 0x102, 0x103, 0x103, 0x104, //0100
                0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //0101
                0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //0110
                0x300, 0x301, 0x301, 0x302, 0x301, 0x302, 0x302, 0x303, 0x301, 0x302, 0x302, 0x303, 0x302, 0x303, 0x303, 0x304, //0111
                0x100, 0x101, 0x101, 0x102, 0x101, 0x102, 0x102, 0x103, 0x101, 0x102, 0x102, 0x103, 0x102, 0x103, 0x103, 0x104, //1000
                0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //1001
                0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //1010
                0x300, 0x301, 0x301, 0x302, 0x301, 0x302, 0x302, 0x303, 0x301, 0x302, 0x302, 0x303, 0x302, 0x303, 0x303, 0x304, //1011
                0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //1100
                0x300, 0x301, 0x301, 0x302, 0x301, 0x302, 0x302, 0x303, 0x301, 0x302, 0x302, 0x303, 0x302, 0x303, 0x303, 0x304, //1101
                0x300, 0x301, 0x301, 0x302, 0x301, 0x302, 0x302, 0x303, 0x301, 0x302, 0x302, 0x303, 0x302, 0x303, 0x303, 0x304, //1110
                0x400, 0x401, 0x401, 0x402, 0x401, 0x402, 0x402, 0x403, 0x401, 0x402, 0x402, 0x403, 0x402, 0x403, 0x403, 0x404  //1111
            }, 256 * sizeof(int));

    }

public:
    uint8_t need_counts[10];
    uint8_t left_mask[4];
    uint8_t left_incl_mask[4];
    uint8_t ryte_mask[4];
    uint8_t dbl_ryte_mask[4];
    uint8_t ryte_incl_mask[4];
    uint8_t first_bit_offset[16];
    uint8_t bit_count[16];
#if (defined(__AVR__))
    const static PROGMEM int dbl_bit_count[256];
#else
    int dbl_bit_count[256];
#endif
public:
    uint8_t pos, key_at_pos;

    dfqx(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
                bpt_trie_handler<dfqx>(leaf_block_sz, parent_block_sz, cache_sz, fname) {
        init_consts();
    }

    dfqx(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bpt_trie_handler<dfqx>(block_sz, block, is_leaf) {
        init_stats();
        init_consts();
    }

    inline void set_current_block_root() {
        set_current_block(root_block);
    }

    inline void set_current_block(uint8_t *m) {
        current_block = m;
        trie = current_block + DFQX_HDR_SIZE;
#if BPT_9_BIT_PTR == 1
#if BPT_INT64MAP == 1
        bitmap = (uint64_t *) (current_block + get_header_size() - 8);
#else
        bitmap1 = (uint32_t *) (current_block + get_header_size() - 8);
        bitmap2 = bitmap1 + 1;
#endif
#endif
    }

    //static uint8_t first_bit_offset4[16];
    //static uint8_t bit_count_16[16];
    inline uint8_t *skip_children(uint8_t *t, int& count) {
        while (count & xFF) {
            uint8_t tc = *t++;
            count -= tc & x01;
            count += (tc & x02 ? *t++ << 8 : dbl_bit_count[*t++]);
            t += (tc & x02 ? *t : 0);
        }
        return t;
    }

    int search_current_block(bptree_iter_ctx *ctx = NULL) {
        uint8_t *t = trie;
        int to_skip = 0;
        uint8_t key_char = *key;
        key_pos = 1;
        do {
            uint8_t trie_char = *t;
            switch ((key_char ^ trie_char) > x03 ?
                    (key_char > trie_char ? 0 : 2) : 1) {
            case 0:
                orig_pos = t++;
                to_skip += (trie_char & x02 ? *t++ << 8 : dbl_bit_count[*t++]);
                t = (trie_char & x02 ? t + *t : skip_children(t, to_skip));
                if (trie_char & x01) {
                        trie_pos = t;
                        insert_state = INSERT_AFTER;
                    pos = to_skip >> 8;
                    return ~pos;
                }
                break;
            case 1:
                uint8_t r_leaves_children;
                orig_pos = t;
                t += (trie_char & x02 ? 3 : 1);
                r_leaves_children = *t++;
                key_char &= x03;
                //to_skip += dbl_bit_count[r_leaves_children & dbl_ryte_mask[key_char]];
                to_skip += dbl_bit_count[r_leaves_children & ((0xEECC8800 >> (key_char << 3)) & xFF)];
                t = skip_children(t, to_skip);
                switch (((r_leaves_children << key_char) & 0x88) | (key_pos == key_len ? x01 : x00)) {
                case 0x80: // 10000000
                case 0x81: // 10000001
                    int cmp;
                    pos = to_skip >> 8;
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
                        need_count = (cmp * 2) + 4;
                    return ~pos;
                case 0x08: // 00001000
                    break;
                case 0x89: // 10001001
                    pos = to_skip >> 8;
                    key_at = get_key(pos, &key_at_len);
                    return pos;
                case 0x88: // 10001000
                    to_skip += 0x100;
                    break;
                case 0x00: // 00000000
                case 0x01: // 00000001
                case 0x09: // 00001001
                        trie_pos = t;
                        insert_state = INSERT_LEAF;
                    pos = to_skip >> 8;
                    return ~pos;
                }
                key_char = key[key_pos++];
                break;
            case 2:
                    trie_pos = t;
                    insert_state = INSERT_BEFORE;
                pos = to_skip >> 8;
                return ~pos;
            }
        } while (1);
        return -1; // dummy - will never reach here
    }

    inline uint8_t *get_child_ptr_pos(int search_result) {
        if (search_result < 0) {
            search_result++;
            search_result = ~search_result;
        }
        return current_block + get_ptr(search_result);
    }

    inline uint8_t *get_ptr_pos() {
        return trie + get_trie_len();
    }

    inline int get_header_size() {
        return DFQX_HDR_SIZE + DQ_MAX_PTR_BITMAP_BYTES;
    }

    uint8_t *next_key(uint8_t *first_key, uint8_t *tp, uint8_t *t, char& ctr, uint8_t& tc, uint8_t& child_leaf) {
        do {
            while (ctr > x03) {
                if (tc & x01) {
                    key_pos--;
                    tc = trie[tp[key_pos]];
                    child_leaf = trie[tp[key_pos] + (tc & x02 ? 3 : 1)];
                    ctr = first_key[key_pos] & 0x03;
                    ctr++;
                } else {
                    tp[key_pos] = t - trie;
                    tc = *t;
                    t += (tc & x02 ? 3 : 1);
                    child_leaf = *t++;
                    ctr = 0;
                }
            }
            first_key[key_pos] = (tc & xFC) | ctr;
            uint8_t mask = x80 >> ctr;
            if (child_leaf & mask) {
                child_leaf &= ~mask;
                if (0 == (child_leaf & (x08 >> ctr)))
                    ctr++;
                return t;
            }
            if (child_leaf & (x08 >> ctr)) {
                key_pos++;
                ctr = x04;
                tc = 0;
            }
            ctr++;
        } while (1); // (t - trie) < get_trie_len());
        return t;
    }

    void move_ptr_list(uint8_t orig_trie_len) {
    #if BPT_9_BIT_PTR == 1
        memmove(trie + get_trie_len(), trie + orig_trie_len, filled_size());
    #else
        memmove(trie + get_trie_len(), trie + orig_trie_len, filled_size() << 1);
    #endif
    }

    void delete_trie_last_half(int brk_key_len, uint8_t *first_key, uint8_t *tp) {
        uint8_t orig_trie_len = get_trie_len();
        for (int idx = brk_key_len; idx >= 0; idx--) {
            uint8_t *t = trie + tp[idx];
            uint8_t tc = *t;
            uint8_t offset = first_key[idx] & x03;
            *t = (tc | x01);
            t += (tc & x02 ? 3 : 1);
            uint8_t children = *t & x0F;
            children &= (idx == brk_key_len ? ryte_mask[offset] : ryte_incl_mask[offset]);
            uint8_t child_leaf = (*t & (ryte_incl_mask[offset] << 4)) + children;
            *t++ = child_leaf;
            if (tc & x02 || idx == brk_key_len) {
                int count = bit_count[children] + (bit_count[child_leaf >> 4] << 8);
                uint8_t *new_t = skip_children(t, count);
                if (tc & x02) {
                    *(t - 3) = count >> 8;
                    *(t - 2) = new_t - t + 2;
                }
                t = new_t;
                if (idx == brk_key_len)
                    set_trie_len(t - trie);
            }
        }
        move_ptr_list(orig_trie_len);
    }

    int delete_segment(uint8_t *delete_end, uint8_t *delete_start) {
        int count = delete_end - delete_start;
        if (count) {
            change_trie_len(-count);
            memmove(delete_start, delete_end, trie + get_trie_len() - delete_start);
        }
        return count;
    }

    void delete_trie_first_half(int brk_key_len, uint8_t *first_key, uint8_t *tp) {
        uint8_t orig_trie_len = get_trie_len();
        for (int idx = brk_key_len; idx >= 0; idx--) {
            uint8_t *t = trie + tp[idx];
            uint8_t tc = *t;
            t += (tc & x02 ? 3 : 1);
            uint8_t offset = first_key[idx] & 0x03;
            uint8_t children = *t & x0F;
            int count = bit_count[children & ryte_mask[offset]];
            children &= left_incl_mask[offset];
            uint8_t leaves = (*t & ((idx == brk_key_len ? left_incl_mask[offset] : left_mask[offset]) << 4));
            *t++ = (children + leaves);
            uint8_t *new_t = skip_children(t, count);
            delete_segment(idx < brk_key_len ? trie + tp[idx + 1] : new_t, t);
            if (tc & x02) {
                count = bit_count[children] + (bit_count[leaves >> 4] << 8);
                new_t = skip_children(t, count);
                *(t - 3) = count >> 8;
                *(t - 2) = new_t - t + 2;
            }
        }
        delete_segment(trie + tp[0], trie);
        move_ptr_list(orig_trie_len);
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
        decode_need_count();
        int ptr_size = filled_size() + 1;
    #if BPT_9_BIT_PTR == 0
        ptr_size <<= 1;
    #endif
        if (get_kv_last_pos()
                < (DFQX_HDR_SIZE + DQ_MAX_PTR_BITMAP_BYTES + get_trie_len()
                        + need_count + ptr_size + key_len - key_pos + value_len + 3))
            return true;
        if (filled_size() > DQ_MAX_PTRS)
            return true;
        if (get_trie_len() > 252 - need_count)
            return true;
        return false;
    }

    void ins_bytes_with_ptrs(uint8_t *ptr, int len) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + len, ptr, trie + get_trie_len() + filled_size() - ptr);
    #else
        memmove(ptr + len, ptr, trie + get_trie_len() + filled_size() * 2 - ptr);
    #endif
        change_trie_len(len);
    }

    void ins_atWith_ptrs(uint8_t *ptr, const uint8_t *s, uint8_t len) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + len, ptr, trie + get_trie_len() + filled_size() - ptr);
    #else
        memmove(ptr + len, ptr, trie + get_trie_len() + filled_size() * 2 - ptr);
    #endif
        memcpy(ptr, s, len);
        change_trie_len(len);
    }

    void ins_atWith_ptrs(uint8_t *ptr, uint8_t b, const uint8_t *s, uint8_t len) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 1 + len, ptr, trie + get_trie_len() + filled_size() - ptr);
    #else
        memmove(ptr + 1 + len, ptr, trie + get_trie_len() + filled_size() * 2 - ptr);
    #endif
        *ptr++ = b;
        memcpy(ptr, s, len);
        change_trie_len(len + 1);
    }

    uint8_t ins_atWith_ptrs(uint8_t *ptr, uint8_t b1, uint8_t b2) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 2, ptr, trie + get_trie_len() + filled_size() - ptr);
    #else
        memmove(ptr + 2, ptr, trie + get_trie_len() + filled_size() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr = b2;
        change_trie_len(2);
        return 2;
    }

    uint8_t ins_atWith_ptrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 3, ptr, trie + get_trie_len() + filled_size() - ptr);
    #else
        memmove(ptr + 3, ptr, trie + get_trie_len() + filled_size() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        change_trie_len(3);
        return 3;
    }

    uint8_t ins_atWith_ptrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 4, ptr, trie + get_trie_len() + filled_size() - ptr);
    #else
        memmove(ptr + 4, ptr, trie + get_trie_len() + filled_size() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        change_trie_len(4);
        return 4;
    }

    uint8_t ins_atWith_ptrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
            uint8_t b5) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 5, ptr, trie + get_trie_len() + filled_size() - ptr);
    #else
        memmove(ptr + 5, ptr, trie + get_trie_len() + filled_size() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr++ = b4;
        *ptr = b5;
        change_trie_len(5);
        return 5;
    }

    uint8_t ins_atWith_ptrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
            uint8_t b5, uint8_t b6) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 6, ptr, trie + get_trie_len() + filled_size() - ptr);
    #else
        memmove(ptr + 6, ptr, trie + get_trie_len() + filled_size() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr++ = b4;
        *ptr++ = b5;
        *ptr = b6;
        change_trie_len(6);
        return 6;
    }

    void update_skip_lens(uint8_t *loop_upto, uint8_t *covering_upto, int diff) {
        uint8_t *t = trie;
        uint8_t tc = *t++;
        loop_upto++;
        while (t <= loop_upto) {
            if (tc & x02) {
                t++;
                if ((t + *t) > covering_upto) {
                    *t = *t + diff;
                    (*(t-1))++;
                    t += 2;
                } else
                    t += *t;
            } else
                t++;
            tc = *t++;
        }
    }

    void insert_current() {
        uint8_t key_char;
        uint8_t mask;

        key_char = key[key_pos - 1];
        mask = x80 >> (key_char & x03);
        switch (insert_state) {
        case INSERT_AFTER:
            *orig_pos &= xFE;
            ins_atWith_ptrs(trie_pos, ((key_char & xFC) | x01), mask);
            if (key_pos > 1)
                update_skip_lens(orig_pos - 1, trie_pos - 1, 2);
            break;
        case INSERT_BEFORE:
            ins_atWith_ptrs(trie_pos, (key_char & xFC), mask);
            if (key_pos > 1)
                update_skip_lens(orig_pos, trie_pos + 1, 2);
            break;
        case INSERT_LEAF:
            if (*orig_pos & x02)
                orig_pos[3] |= mask;
            else
                orig_pos[1] |= mask;
            update_skip_lens(orig_pos, orig_pos + 1, 0);
            break;
        case INSERT_THREAD:
            int p, min;
            uint8_t c1, c2;
            uint8_t *from_pos;
            from_pos = trie_pos;
            if (*orig_pos & x02) {
                orig_pos[1]++;
            } else {
                if (orig_pos[1] & x0F || need_count > 4) {
                    p = dbl_bit_count[orig_pos[1]];
                    uint8_t *new_t = skip_children(orig_pos + 2, p);
                    ins_atWith_ptrs(orig_pos + 1, (p >> 8) + 1, new_t - orig_pos - 2);
                    trie_pos += 2;
                    *orig_pos |= x02;
                }
            }
            orig_pos[(*orig_pos & x02) ? 3 : 1] |= (x08 >> (key_char & x03));
            c1 = c2 = key_char;
            p = key_pos;
            min = util::min16(key_len, key_pos + key_at_len);
            if (p < min)
                orig_pos[(*orig_pos & x02) ? 3 : 1] &= ~mask;
            need_count -= 6;
            need_count /= 2;
            int diff;
            diff = p + need_count;
            if (diff == min && need_count) {
                need_count--;
                diff--;
            }
            ins_bytes_with_ptrs(trie_pos, (diff == min ? 2 :
                    (need_count * 2) + ((key[diff] ^ key_at[diff - key_pos]) > x03 ? 4 :
                            (key[diff] == key_at[diff - key_pos] ? 6 : 2))));
            while (p < min) {
                c1 = key[p];
                c2 = key_at[p - key_pos];
                if (c1 > c2) {
                    uint8_t swap = c1;
                    c1 = c2;
                    c2 = swap;
                }
                switch ((c1 ^ c2) > x03 ?
                        0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
                case 0:
                    *trie_pos++ = c1 & xFC;
                    *trie_pos++ = x80 >> (c1 & x03);
                    *trie_pos++ = (c2 & xFC) | x01;
                    *trie_pos++ = x80 >> (c2 & x03);
                    break;
                case 1:
                    *trie_pos++ = (c1 & xFC) | x01;
                    *trie_pos++ = (x80 >> (c1 & x03)) | (x80 >> (c2 & x03));
                    break;
                case 2:
                    *trie_pos++ = (c1 & xFC) | x01;
                    *trie_pos++ = x08 >> (c1 & x03);
                    break;
                case 3:
                    *trie_pos++ = (c1 & xFC) | x03;
                    *trie_pos++ = 2;
                    *trie_pos++ = 4;
                    *trie_pos++ = 0x88 >> (c1 & x03);
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
                *trie_pos++ = (c2 & xFC) | x01;
                *trie_pos++ = x80 >> (c2 & x03);
                if (p == key_len)
                    key_pos--;
            }
            p = trie_pos - from_pos;
            update_skip_lens(orig_pos - 1, orig_pos, p);
            if (*orig_pos & x02)
                orig_pos[2] += p;
            if (diff < key_at_len)
                diff++;
            if (diff) {
                p = get_ptr(key_at_pos);
                key_at_len -= diff;
                p += diff;
                if (key_at_len >= 0) {
                    current_block[p] = key_at_len;
                    set_ptr(key_at_pos, p);
                }
            }
            break;
        case INSERT_EMPTY:
            append((key_char & xFC) | x01);
            append(mask);
            break;
        }

        if (BPT_MAX_PFX_LEN < key_pos)
            BPT_MAX_PFX_LEN = key_pos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int orig_filled_size = filled_size();
        const int DFQX_NODE_SIZE = is_leaf() ? leaf_block_size : parent_block_size;
        uint8_t *b = allocate_block(DFQX_NODE_SIZE, is_leaf(), current_block[0] & 0x1F);
        dfqx new_block(DFQX_NODE_SIZE, b, is_leaf());
        memcpy(new_block.trie, trie, get_trie_len());
        new_block.set_trie_len(get_trie_len());
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        int kv_last_pos = get_kv_last_pos();
        int half_kVLen = DFQX_NODE_SIZE - kv_last_pos + 1;
        half_kVLen /= 2;

        int brk_idx = -1;
        int brk_kv_pos;
        int tot_len;
        brk_kv_pos = tot_len = 0;
        char ctr = 4;
        uint8_t tp[BPT_MAX_PFX_LEN + 1];
        uint8_t *t = trie;
        uint8_t tc, child_leaf;
        tc = child_leaf = 0;
        //if (!is_leaf())
        //   cout << "Trie len:" << (int) get_trie_len() << ", filled size:" << orig_filled_size << endl;
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
                t = next_key(first_key, tp, t, ctr, tc, child_leaf);
                //if (tot_len > half_kVLen) {
                //memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
                //first_key[key_pos+1+current_block[src_idx]] = 0;
                //cout << first_key << endl;
                if (tot_len > half_kVLen || idx == (orig_filled_size / 2)) {
                    //memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[key_pos+1+current_block[src_idx]] = 0;
                    //cout << first_key << ":";
                    brk_idx = idx + 1;
                    brk_kv_pos = kv_last_pos;
                    delete_trie_last_half(key_pos, first_key, tp);
                    new_block.key_pos = key_pos;
                    t = new_block.trie + (t - trie);
                    t = new_block.next_key(first_key, tp, t, ctr, tc, child_leaf);
                    key_pos = new_block.key_pos;
                    //src_idx = get_ptr(idx + 1);
                    //memcpy(first_key + key_pos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[key_pos+1+current_block[src_idx]] = 0;
                    //cout << first_key << endl;
                }
            }
        }
        kv_last_pos = get_kv_last_pos() + DFQX_NODE_SIZE - kv_last_pos;
        new_block.set_kv_last_pos(kv_last_pos);
        memmove(new_block.current_block + kv_last_pos, new_block.current_block + get_kv_last_pos(), DFQX_NODE_SIZE - kv_last_pos);
        brk_kv_pos += (kv_last_pos - get_kv_last_pos());
        int diff = DFQX_NODE_SIZE - brk_kv_pos;
        for (idx = 0; idx < orig_filled_size; idx++) {
            new_block.ins_ptr(idx, kv_last_pos + (idx < brk_idx ? diff : 0));
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
        }
        kv_last_pos = new_block.get_kv_last_pos();
    #if BPT_9_BIT_PTR == 1
        memcpy(current_block + DFQX_HDR_SIZE, new_block.current_block + DFQX_HDR_SIZE, DQ_MAX_PTR_BITMAP_BYTES);
        memcpy(trie + get_trie_len(), new_block.trie + new_block.get_trie_len(), brk_idx);
    #else
        memcpy(trie + get_trie_len(), new_block.trie + new_block.get_trie_len(), (brk_idx << 1));
    #endif

        {
            new_block.key_at = new_block.get_key(brk_idx, &new_block.key_at_len);
            key_pos++;
            if (is_leaf())
                *first_len_ptr = key_pos;
            else {
                if (new_block.key_at_len) {
                    memcpy(first_key + key_pos, new_block.key_at,
                            new_block.key_at_len);
                    *first_len_ptr = key_pos + new_block.key_at_len;
                } else
                    *first_len_ptr = key_pos;
            }
            key_pos--;
            new_block.delete_trie_first_half(key_pos, first_key, tp);
        }

        {
            int old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + DFQX_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            set_kv_last_pos(DFQX_NODE_SIZE - old_blk_new_len);
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
            uint8_t *block_ptrs = new_block.trie + new_block.get_trie_len();
    #if BPT_9_BIT_PTR == 1
            memmove(block_ptrs, block_ptrs + brk_idx, new_size);
    #else
            memmove(block_ptrs, block_ptrs + (brk_idx << 1), new_size << 1);
    #endif
            new_block.set_kv_last_pos(brk_kv_pos);
            new_block.set_filled_size(new_size);
        }

        return new_block.current_block;
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

};

#endif
