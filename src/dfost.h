#ifndef dfos_H
#define dfos_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

#define DS_SIBLING_PTR_SIZE 2
#define DFOS_HDR_SIZE 9

#define DS_GET_TRIE_LEN util::get_int(BPT_TRIE_LEN_PTR)
#define DS_SET_TRIE_LEN(x) util::set_int(BPT_TRIE_LEN_PTR, x)
#define DS_GET_SIBLING_OFFSET(x) (util::get_int(x) & 0x3FFF)
#define DS_SET_SIBLING_OFFSET(x, off) util::set_int(x, off + (((*x) & 0xC0) << 8))

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class dfos : public bpt_trie_handler<dfos> {
public:
    int pos, key_at_pos;
    uint8_t need_counts[10];

    dfos(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
                bpt_trie_handler<dfos>(leaf_block_sz, parent_block_sz, cache_sz, fname) {
        memcpy(need_counts, "\x00\x02\x02\x02\x02\x00\x07\x00\x00\x00", 10);
    }

    dfos(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bpt_trie_handler<dfos>(block_sz, block, is_leaf) {
        init_stats();
        memcpy(need_counts, "\x00\x02\x02\x02\x02\x00\x07\x00\x00\x00", 10);
    }

    inline void set_current_block_root() {
        set_current_block(root_block);
    }

    inline void set_current_block(uint8_t *m) {
        current_block = m;
        trie = current_block + DFOS_HDR_SIZE;
    }

    inline uint8_t *skip_children(uint8_t *t, int count) {
        while (count) {
            uint8_t tc = *t++;
            if (tc & x01) {
                t += (tc >> 3);
                continue;
            }
            if (tc & x02)
                count += BIT_COUNT(*t++);
            pos += BIT_COUNT(*t++);
            if (tc & x04)
                count--;
        }
        return t;
    }

    int search_current_block(bptree_iter_ctx *ctx = NULL) {
        uint8_t key_char;
        uint8_t *t = trie;
        pos = 0;
        key_pos = 1;
        key_char = *key;
        do {
            uint8_t trie_char = *t;
            int to_skip;
            // nnnnn tc0 - octets, t=terminator, c=child
            // nnnnn l01 - prefix, l=0 then prefix len=n, else see next byte
            // nnnnn 011 - n+next=sibling count, n+2+3=sibling size
            // nnnn0 111 - n = leaf len
            // nnnn1 111 - n = data type
            switch ((trie_char & x01) == x00 ? 0 : ((trie_char & x03) == x01 ? 1 :
                   ((trie_char & x07) == x03 ? 2 : ((trie_char & x0F) == x07 ? 3 :
                   ((trie_char & x0F) == x0F ? 4 : 5))))) {
            case 0:
                switch ((key_char ^ trie_char) > x07 ? (key_char > trie_char ? 0 : 2) : 1) {
                case 0:
                    orig_pos = t++;
                    to_skip = (trie_char & x02 ? BIT_COUNT(*t++) : x00);
                    pos += BIT_COUNT(*t++);
                    t = skip_children(t, to_skip);
                    if (trie_char & x04) {
                        trie_pos = t;
                        insert_state = INSERT_AFTER;
                        need_count = 3;
                        return ~pos;
                    }
                    break;
                case 1:
                    uint8_t r_leaves, r_children, r_mask;
                    uint16_t to_skip;
                    orig_pos = t++;
                    r_children = (trie_char & x02 ? *t++ : x00);
                    r_leaves = *t++;
                    key_char &= x07;
                    r_mask = ~(xFF << key_char);
                    pos += BIT_COUNT(r_leaves & r_mask);
                    to_skip = BIT_COUNT(r_children & r_mask);
                    t = skip_children(t, to_skip);
                    r_mask = (x01 << key_char);
                    switch (r_leaves & r_mask ?
                            (r_children & r_mask ? (key_pos == key_len ? 3 : 4) : 2) :
                            (r_children & r_mask ? (key_pos == key_len ? 0 : 1) : 0)) {
                    case 0:
                        trie_pos = t;
                        insert_state = INSERT_LEAF;
                        need_count = 3;
                        return ~pos;
                    case 1:
                        break;
                    case 2:
                        int16_t cmp;
                        key_at = get_key(pos, &key_at_len);
                        cmp = util::compare(key + key_pos, key_len - key_pos,
                                key_at, key_at_len);
                        if (cmp == 0)
                            return pos;
                        key_at_pos = pos;
                        if (cmp > 0)
                            pos++;
                        else
                            cmp = -cmp;
                        trie_pos = t;
                        insert_state = INSERT_THREAD;
                        need_count = cmp + 6;
                        return ~pos;
                    case 3:
                        key_at = get_key(pos, &key_at_len);
                        return pos;
                    case 4:
                        pos++;
                        break;
                    }
                    key_char = key[key_pos++];
                    break;
                case 2:
                    trie_pos = t;
                    insert_state = INSERT_BEFORE;
                    need_count = 3;
                    return ~pos;
                }
                break;
            case 1:
                uint8_t pfx_len;
                orig_pos = t++;
                pfx_len = (trie_char >> 3);
                do {
                    if (key_char != *t || key_pos == key_len) {
                        trie_pos = t;
                        if (key_char > *t) {
                            t = skip_children(t + pfx_len, 1);
                            key_at_pos = t - trie_pos;
                        }
                        insert_state = INSERT_CONVERT;
                        need_count = 6;
                        return ~pos;
                    }
                    t++;
                    key_char = key[key_pos++];
                } while (--pfx_len);
                break;
            }
        } while (1);
        return ~pos;
    }

    inline int get_header_size() {
        return DFOS_HDR_SIZE;
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

    uint8_t *next_key(uint8_t *first_key, int *tp, uint8_t *t, char& ctr, uint8_t& tc, uint8_t& child, uint8_t& leaf) {
        do {
            while (ctr > x07) {
                if (tc & x04) {
                    key_pos--;
                    tc = trie[tp[key_pos]];
                    while (tc & x01) {
                        key_pos--;
                        tc = trie[tp[key_pos]];
                    }
                    child = (tc & x02 ? trie[tp[key_pos] + 1] : 0);
                    leaf = trie[tp[key_pos] + (tc & x02 ? 2 : 1)];
                    ctr = first_key[key_pos] & 0x07;
                    ctr++;
                } else {
                    tp[key_pos] = t - trie;
                    tc = *t++;
                    if (tc & x01) {
                        uint8_t len = tc >> 3;
                        for (int i = 0; i < len; i++)
                            tp[key_pos + i] = t - trie - 1;
                        memcpy(first_key + key_pos, t, len);
                        t += len;
                        key_pos += len;
                        tp[key_pos] = t - trie;
                        tc = *t++;
                    }
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
        } while (1); // (t - trie) < BPT_TRIE_LEN);
        return t;
    }

    void delete_trie_last_half(int brk_key_len, uint8_t *first_key, int *tp) {
        uint16_t orig_trie_len = get_trie_len();
        uint8_t *t = 0;
        uint8_t children = 0;
        for (int idx = 0; idx <= brk_key_len; idx++) {
            if (trie[tp[idx]] & x01)
                continue;
            uint8_t offset = first_key[idx] & 0x07;
            t = trie + tp[idx];
            uint8_t tc = *t;
            *t++ = (tc | x04);
            children = 0;
            if (tc & x02) {
                children = *t;
                children &= ~((idx == brk_key_len ? xFF : xFE) << offset);
                                // ryte_mask[offset] : ryte_incl_mask[offset]);
                *t++ = children;
            }
            *t++ &= ~(xFE << offset); // ryte_incl_mask[offset];
        }
        uint8_t to_skip = BIT_COUNT(children);
        t = skip_children(t, to_skip);
        set_trie_len(t - trie);
        memmove(t, trie + orig_trie_len, filled_size() << 1);
    }

    int delete_segment(uint8_t *delete_end, uint8_t *delete_start) {
        int count = delete_end - delete_start;
        if (count) {
            DS_SET_TRIE_LEN(DS_GET_TRIE_LEN - count);
            memmove(delete_start, delete_end, trie + DS_GET_TRIE_LEN - delete_start);
        }
        return count;
    }

    void delete_trie_first_half(int brk_key_len, uint8_t *first_key, int *tp) {
        int tot_del = 0;
        uint16_t orig_trie_len = get_trie_len();
        uint8_t *delete_start = trie;
        for (int idx = 0; idx <= brk_key_len; idx++) {
            int count;
            uint8_t *t = trie + tp[idx] - tot_del;
            if (delete_start != t) {
                count = delete_segment(t, delete_start);
                t -= count;
                tot_del += count;
            }
            uint8_t tc = *t;
            if (tc & x01) {
                uint8_t len = tc >> 3;
                idx += len;
                delete_start = t + 1 + len;
                t = trie + tp[idx] - tot_del;
                if (delete_start != t) {
                    count = delete_segment(t, delete_start);
                    tot_del += count;
                    t -= count;
                }
            }
            count = 0;
            uint8_t offset = first_key[idx] & 0x07;
            tc = *t++;
            if (tc & x02) {
                uint8_t children = *t;
                count = BIT_COUNT(children & ~(xFF << offset)); // ryte_mask[offset];
                children &= (xFF << offset); // left_incl_mask[offset];
                *t++ = children;
            }
            *t++ &= (idx == brk_key_len ? xFF : xFE) << offset;
                                // left_incl_mask[offset] : left_mask[offset]);
            delete_start = t;
            if (idx == brk_key_len && count) {
                t = skip_children(t, count);
                delete_segment(t, delete_start);
            }
        }
        set_trie_len(orig_trie_len - tot_del);
        memmove(trie + get_trie_len(), trie + orig_trie_len, filled_size() << 1);
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

    void make_space() {
        int block_sz = (is_leaf() ? block_size : parent_block_size);
        int lvl = current_block[0] & 0x1F;
        const uint16_t data_size = block_sz - get_kv_last_pos();
        uint8_t data_buf[data_size];
        uint16_t new_data_len = 0;
        int idx = 0;
        int filled_sz = filled_size();
        while (idx < filled_sz) {
            uint16_t src_idx = get_ptr(idx);
            uint8_t *ptr_pos = current_block + src_idx;
            uint16_t data_len = *ptr_pos;
            data_len++;
            data_len += ptr_pos[data_len];
            data_len++;
            new_data_len += data_len;
            memcpy(data_buf + data_size - new_data_len, ptr_pos, data_len);
            set_ptr(idx, block_sz - new_data_len);
            idx++;
        }
        uint16_t new_kv_last_pos = block_sz - new_data_len;
        memcpy(current_block + new_kv_last_pos, data_buf + data_size - new_data_len, new_data_len);
        //printf("%d, %d\n", data_size, new_data_len);
        set_kv_last_pos(new_kv_last_pos);
        search_current_block();
    }

    bool is_full(int search_result) {
        decode_need_count(search_result);
        int ptr_size = filled_size() + 1;
        ptr_size <<= 1;
        if (get_kv_last_pos() < (DFOS_HDR_SIZE + DS_GET_TRIE_LEN
                        + need_count + ptr_size + key_len - key_pos + value_len + 2)) {
            make_space();
            if (get_kv_last_pos() < (DFOS_HDR_SIZE + DS_GET_TRIE_LEN
                            + need_count + ptr_size + key_len - key_pos + value_len + 2))
                return true;
        }
        return false;
    }

    uint8_t *find_split_source(int search_result) {
        return NULL;
    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int orig_filled_size = filled_size();
        const int DFOS_NODE_SIZE = is_leaf() ? block_size : parent_block_size;
        uint8_t *b = allocate_block(DFOS_NODE_SIZE, is_leaf(), current_block[0] & 0x1F);
        dfos new_block(DFOS_NODE_SIZE, b, is_leaf());
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        int kv_last_pos = get_kv_last_pos();
        int half_kVLen = DFOS_NODE_SIZE - kv_last_pos + 1;
        half_kVLen /= 2;

        memcpy(new_block.trie, trie, get_trie_len());
        new_block.set_trie_len(get_trie_len());

        int16_t brk_idx = -1;
        int16_t brk_kv_pos;
        int16_t data_len;
        brk_kv_pos = data_len = 0;
        char ctr = x08;
        int tp[BPT_MAX_PFX_LEN + 1];
        uint8_t *t = trie;
        uint8_t tc, child, leaf;
        tc = child = leaf = 0;
        key_pos = 0;
        // (1) move all data to new_block in order
        int16_t idx;
        for (idx = 0; idx < orig_filled_size; idx++) {
            int16_t src_idx = get_ptr(idx);
            int16_t kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            data_len += kv_len;
            memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
            if (brk_idx == -1)
                set_ptr(idx, kv_last_pos);
            else
                new_block.set_ptr(idx, kv_last_pos);
            kv_last_pos += kv_len;
            if (brk_idx == -1) {
                t = next_key(first_key, tp, t, ctr, tc, child, leaf);
                if (data_len > half_kVLen || idx == (orig_filled_size / 2)) {
                    data_len = 0;
                    brk_idx = idx + 1;
                    brk_kv_pos = kv_last_pos;
                    delete_trie_last_half(key_pos, first_key, tp);
                    new_block.key_pos = key_pos;
                    t = new_block.trie + (t - trie);
                    t = new_block.next_key(first_key, tp, t, ctr, tc, child, leaf);
                    key_pos = new_block.key_pos;
                    src_idx = get_ptr(idx + 1);
                    key_pos++;
                    if (is_leaf())
                        *first_len_ptr = key_pos;
                    else {
                        memcpy(first_key + key_pos, current_block + src_idx + 1, current_block[src_idx]);
                        *first_len_ptr = key_pos + current_block[src_idx];
                    }
                    key_pos--;
                    new_block.delete_trie_first_half(key_pos, first_key, tp);
                }
            }
        }

        kv_last_pos = get_kv_last_pos();

        {
            // Copy back first half to old block
            int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + DFOS_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len);
            int16_t diff1 = DFOS_NODE_SIZE - brk_kv_pos;
            idx = brk_idx;
            while (idx--)
                set_ptr(idx, get_ptr(idx) + diff1);
            set_kv_last_pos(DFOS_NODE_SIZE - old_blk_new_len);
            set_filled_size(brk_idx);
        }

        {
            int16_t new_size = orig_filled_size - brk_idx;
            memmove(new_block.get_ptr_pos(), new_block.get_ptr_pos() + (brk_idx << 1),
                        new_size << 1);
            uint16_t diff2 = DFOS_NODE_SIZE - brk_kv_pos - data_len;
            if (diff2 > 0) {
                memmove(new_block.current_block + brk_kv_pos + diff2,
                        new_block.current_block + brk_kv_pos, data_len);
                brk_kv_pos += diff2;
                idx = new_size;
                while (idx--)
                    new_block.set_ptr(idx, new_block.get_ptr(idx) + diff2);
            }
            new_block.set_kv_last_pos(brk_kv_pos);
            new_block.set_filled_size(new_size);
        }

        return new_block.current_block;
    }

    void insert_current() {
        uint8_t key_char, mask;
        int diff;

        key_char = key[key_pos - 1];
        mask = x01 << (key_char & x07);
        switch (insert_state) {
        case INSERT_AFTER:
            *orig_pos &= xFB;
            ins_b2_with_ptrs(trie_pos, ((key_char & xF8) | x04), mask);
            break;
        case INSERT_BEFORE:
            ins_b2_with_ptrs(trie_pos, key_char & xF8, mask);
            break;
        case INSERT_LEAF:
            trie_pos = orig_pos + (*orig_pos & x02 ? 2 : 1);
            *trie_pos |= mask;
            break;
        case INSERT_CONVERT:
            uint8_t b, c;
            char cmp_rel;
            diff = trie_pos - orig_pos;
            // 3 possible relationships between key_char and *trie_pos, 4 possible positions of trie_pos
            cmp_rel = ((*trie_pos ^ key_char) > x07 ? (*trie_pos < key_char ? 0 : 1) : 2);
            if (cmp_rel == 0)
                ins_b2_with_ptrs(trie_pos + key_at_pos, (key_char & xF8) | 0x04, 1 << (key_char & x07));
            diff--;
            c = *trie_pos;
            if (diff == 0)
                trie_pos = orig_pos;
            b = (cmp_rel == 2 ? x04 : x00) | (cmp_rel == 1 ? x00 : x02);
            need_count = (*orig_pos >> 3) - 1 - diff; // save original count
            *trie_pos++ = ((cmp_rel == 0 ? c : key_char) & xF8) | b;
            b = (cmp_rel == 1 ? (diff ? 4 : 3) : (diff ? 2 : 1));
            if (diff) {
                trie_pos = orig_pos + diff + 2;
                *orig_pos = (diff << 3) | x01;
            }
            if (need_count)
                b++;
            // this just inserts b number of bytes - current_block is dummy
            ins_bytes_with_ptrs(trie_pos + (diff ? 0 : 1), current_block, b);
            *trie_pos++ = 1 << ((cmp_rel == 1 ? key_char : c) & x07);
            if (diff && cmp_rel != 1)
                *trie_pos++ = (cmp_rel == 0 ? 0 : (1 << (key_char & x07)));
            if (cmp_rel == 1) {
                *trie_pos++ = (c & xF8) | x06;
                *trie_pos++ = 1 << (c & x07);
                *trie_pos++ = 0;
            } else if (diff == 0)
                *trie_pos++ = ((cmp_rel == 0) ? 0 : (1 << ((cmp_rel == 2 ? key_char : c) & x07)));
            if (need_count)
                *trie_pos = (need_count << 3) | x01;
            break;
        case INSERT_THREAD:
            int16_t p, min;
            uint8_t c1, c2;
            uint8_t *child_pos;
            child_pos = orig_pos + 1;
            if (*orig_pos & x02) {
                *child_pos |= mask;
            } else {
                ins_b1_with_ptrs(child_pos, mask);
                trie_pos++;
                *orig_pos |= x02;
            }
            c1 = c2 = key_char;
            p = key_pos;
            min = util::min16(key_len, key_pos + key_at_len);
            if (p < min) {
                child_pos[1] &= ~mask; // leaf_pos
            }
            need_count -= 7;
            if (p + need_count == min && need_count)
                need_count--;
            if (need_count) {
                ins_pfx_with_ptrs(trie_pos, (need_count << 3) | x01, key + key_pos, need_count);
                trie_pos += need_count;
                trie_pos++;
                p += need_count;
                count1 += need_count;
            }
            while (p < min) {
                c1 = key[p];
                c2 = key_at[p - key_pos];
                if (c1 > c2) {
                    uint8_t swap = c1;
                    c1 = c2;
                    c2 = swap;
                }
                switch ((c1 ^ c2) > x07 ?
                        0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
                case 0:
                    trie_pos += ins_b4_with_ptrs(trie_pos, c1 & xF8, x01 << (c1 & x07),
                            (c2 & xF8) | x04, x01 << (c2 & x07));
                    break;
                case 1:
                    trie_pos += ins_b2_with_ptrs(trie_pos, (c1 & xF8) | x04,
                            (x01 << (c1 & x07)) | (x01 << (c2 & x07)));
                    break;
                case 2:
                    trie_pos += ins_b3_with_ptrs(trie_pos,
                                    (c1 & xF8) | x06, x01 << (c1 & x07), 0);
                    break;
                case 3:
                    trie_pos += ins_b3_with_ptrs(trie_pos, (c1 & xF8) | x06,
                            x01 << (c1 & x07), x01 << (c1 & x07));
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
                ins_b2_with_ptrs(trie_pos, (c2 & xF8) | x04, (x01 << (c2 & x07)));
                if (p == key_len)
                    key_pos--;
            }
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

    void ins_pfx_with_ptrs(uint8_t *ptr, uint8_t b1, const uint8_t *s, uint8_t len) {
        memmove(ptr + len + 1, ptr, trie + DS_GET_TRIE_LEN + filled_size() * 2 - ptr);
        *ptr++ = b1;
        memcpy(ptr, s, len);
        DS_SET_TRIE_LEN(DS_GET_TRIE_LEN + len + 1);
    }

    void ins_bytes_with_ptrs(uint8_t *ptr, const uint8_t *s, uint8_t len) {
        memmove(ptr + len, ptr, trie + DS_GET_TRIE_LEN + filled_size() * 2 - ptr);
        memcpy(ptr, s, len);
        DS_SET_TRIE_LEN(DS_GET_TRIE_LEN + len);
    }

    uint8_t ins_b1_with_ptrs(uint8_t *ptr, uint8_t b1) {
        memmove(ptr + 1, ptr, trie + DS_GET_TRIE_LEN + filled_size() * 2 - ptr);
        *ptr = b1;
        DS_SET_TRIE_LEN(DS_GET_TRIE_LEN + 1);
        return 1;
    }

    uint8_t ins_b2_with_ptrs(uint8_t *ptr, uint8_t b1, uint8_t b2) {
        memmove(ptr + 2, ptr, trie + DS_GET_TRIE_LEN + filled_size() * 2 - ptr);
        *ptr++ = b1;
        *ptr = b2;
        DS_SET_TRIE_LEN(DS_GET_TRIE_LEN + 2);
        return 2;
    }

    uint8_t ins_b3_with_ptrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3) {
        memmove(ptr + 3, ptr, trie + DS_GET_TRIE_LEN + filled_size() * 2 - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        DS_SET_TRIE_LEN(DS_GET_TRIE_LEN + 3);
        return 3;
    }

    uint8_t ins_b4_with_ptrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
        memmove(ptr + 4, ptr, trie + DS_GET_TRIE_LEN + filled_size() * 2 - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        DS_SET_TRIE_LEN(DS_GET_TRIE_LEN + 4);
        return 4;
    }

    void decode_need_count(int search_result) {
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
