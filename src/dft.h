#ifndef dft_H
#define dft_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

#define DFT_UNIT_SIZE 4

#define DFT_HDR_SIZE 9

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class dft : public bpt_trie_handler<dft> {
public:
    char need_counts[10];
    int last_sibling_pos;
    int pos, key_at_pos;

    dft(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, const uint8_t opts = 0) :
                bpt_trie_handler<dft>(leaf_block_sz, parent_block_sz, cache_sz, fname, opts) {
#if DFT_UNIT_SIZE == 4
        memcpy(need_counts, "\x00\x04\x04\x00\x04\x00\x00\x00\x00\x00", 10);
#else
        memcpy(need_counts, "\x00\x03\x03\x00\x03\x00\x00\x00\x00\x00", 10);
#endif
    }

    dft(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bpt_trie_handler<dft>(block_sz, block, is_leaf) {
        init_stats();
#if DFT_UNIT_SIZE == 4
        memcpy(need_counts, "\x00\x04\x04\x00\x04\x00\x00\x00\x00\x00", 10);
#else
        memcpy(need_counts, "\x00\x03\x03\x00\x03\x00\x00\x00\x00\x00", 10);
#endif
    }

    dft(const char *filename, int blk_size, int page_resv_bytes, const uint8_t opts) :
       bpt_trie_handler<dft>(filename, blk_size, page_resv_bytes, opts) {
        init_stats();
    }

    inline void set_current_block_root() {
        current_block = root_block;
        trie = current_block + DFT_HDR_SIZE;
    }

    inline void set_current_block(uint8_t *m) {
        current_block = m;
        trie = current_block + DFT_HDR_SIZE;
    }

    int search_current_block(bptree_iter_ctx *ctx = NULL) {
        uint8_t *t = trie;
        key_pos = 0;
        uint8_t key_char = key[key_pos++];
        trie_pos = 0;
        do {
            orig_pos = t;
            last_sibling_pos = 0;
            while (key_char > *t) {
                t++;
                last_sibling_pos = t - trie;
    #if DFT_UNIT_SIZE == 3
                int s = *t & x3F;
    #else
                int s = *t & x7F;
    #endif
                if (s) {
                    t += (s * DFT_UNIT_SIZE);
                    t--;
                    orig_pos = t;
                    continue;
                } else {
    #if DFT_UNIT_SIZE == 3
                        int child = *t & x40;
    #else
                        int child = *t & x80;
    #endif
                        t += DFT_UNIT_SIZE;
                        while (child) {
    #if DFT_UNIT_SIZE == 3
                            s = *t & x3F;
                            child = *t & x40;
    #else
                            s = *t & x7F;
                            child = *t & x80;
    #endif
                            if (s) {
                                t += s * DFT_UNIT_SIZE;
                                child = 1;
                            } else
                                t += DFT_UNIT_SIZE;
                        }
                        insert_state = INSERT_AFTER;
                        trie_pos = t - 1;
                    return -1;
                }
            }
            if (key_char < *t++) {
                trie_pos = t - 1;
                    insert_state = INSERT_BEFORE;
                return -1;
            }
#if DFT_UNIT_SIZE == 3
            uint8_t r_children = *t & x40;
            int ptr = get9bit_ptr(t);
#else
            uint8_t r_children = *t & x80;
            int ptr = util::get_int(t + 1);
#endif
            switch ((ptr ? 2 : 0) | ((r_children && key_pos != key_len) ? 1 : 0)) {
            //switch (ptr ?
            //        (r_children ? (key_pos == key_len ? 2 : 1) : 2) :
            //        (r_children ? (key_pos == key_len ? 0 : 1) : 0)) {
            case 0:
                trie_pos = t - 1;
                    insert_state = INSERT_LEAF;
                return -1;
            case 1:
                break;
            case 2:
                int cmp;
                key_at = current_block + ptr;
                key_at_len = *key_at++;
                cmp = util::compare(key + key_pos, key_len - key_pos, key_at, key_at_len);
                if (cmp == 0)
                    return ptr;
                if (cmp < 0) {
                    cmp = -cmp;
                    trie_pos = t - 1;
                }
                insert_state = INSERT_THREAD;
                need_count = (cmp * DFT_UNIT_SIZE) + DFT_UNIT_SIZE * 2;
                return -1;
            }
            t += DFT_UNIT_SIZE;
            t--;
            key_char = key[key_pos++];
        } while (1);
        return -1;
    }

    inline int get_header_size() {
        return DFT_HDR_SIZE;
    }

    inline uint8_t *get_ptr_pos() {
        return NULL;
    }

    inline uint8_t *get_child_ptr_pos(int search_result) {
        if (trie_pos++) {
            while (trie_pos > trie) {
                trie_pos -= DFT_UNIT_SIZE;
                int p = (DFT_UNIT_SIZE == 3 ? get9bit_ptr(trie_pos) : util::get_int(trie_pos + 1));
                if (p) {
                    key_at = current_block + p;
                    break;
                }
            }
        } else
            key_at--;
        return key_at;
    }

    void update_ptrs(uint8_t *upto, int diff) {
        uint8_t *t = trie + 1;
        while (t <= upto) {
    #if DFT_UNIT_SIZE == 3
            uint8_t sibling = (*t & x3F);
    #else
            uint8_t sibling = (*t & x7F);
    #endif
            if (sibling && (t + sibling * DFT_UNIT_SIZE) >= upto)
                *t += diff;
            t += DFT_UNIT_SIZE;
        }
    }

    uint8_t insert_unit(uint8_t *t, uint8_t c1, uint8_t s1, int ptr1) {
        memmove(t + DFT_UNIT_SIZE, t, trie + get_trie_len()- t);
        t[0] = c1;
        t[1] = s1;
    #if DFT_UNIT_SIZE == 3
                if (ptr1 & x100)
                t[1] |= x80;
                t[2] = ptr1;
    #else
                util::set_int(t + 2, ptr1);
    #endif
                change_trie_len(DFT_UNIT_SIZE);
                return DFT_UNIT_SIZE;
            }

    uint8_t insert2Units(uint8_t *t, uint8_t c1, uint8_t s1, int ptr1,
            uint8_t c2, uint8_t s2, int ptr2) {
        uint8_t size = DFT_UNIT_SIZE * 2;
        memmove(t + size, t, trie + get_trie_len()- t);
        t[0] = c1;
        t[1] = s1;
    #if DFT_UNIT_SIZE == 3
        if (ptr1 & x100)
            t[1] |= x80;
        t[2] = ptr1;
        t[3] = c2;
        t[4] = s2;
        if (ptr2 & x100)
            t[4] |= x80;
        t[5] = ptr2;
    #else
        util::set_int(t + 2, ptr1);
        t[4] = c2;
        t[5] = s2;
        util::set_int(t + 6, ptr2);
    #endif
        change_trie_len(size);
        return size;
    }

    void append_ptr(int p) {
    #if DFT_UNIT_SIZE == 3
        trie[get_trie_len()] = p;
        if (p & x100)
        trie[get_trie_len() - 1] |= x80;
        else
        trie[get_trie_len() - 1] &= x7F;
        change_trie_len(1);
    #else
        util::set_int(trie + get_trie_len(), p);
        change_trie_len(2);
    #endif
    }

    int get9bit_ptr(uint8_t *t) {
        int ptr = (*t++ & x80 ? 256 : 0);
        ptr |= *t;
        return ptr;
    }

    void set9bit_ptr(uint8_t *t, int p) {
        *t-- = p;
        if (p & x100)
            *t |= x80;
        else
            *t &= x7F;
    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int orig_filled_size = filled_size();
        const int DFT_NODE_SIZE = is_leaf() ? block_size : parent_block_size;
        uint8_t *b = allocate_block(DFT_NODE_SIZE, is_leaf(), current_block[0] & 0x1F);
        dft new_block(DFT_NODE_SIZE, b, is_leaf());
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        int kv_last_pos = get_kv_last_pos();
        int half_kVLen = DFT_NODE_SIZE - kv_last_pos + 1;
        half_kVLen /= 2;

        int brk_idx = 0;
        int brk_kv_pos;
        int brk_trie_pos;
        int brk_old_trie_pos;
        int brk_tp_old_pos;
        int tot_len;
        brk_kv_pos = tot_len = 0;
        // (1) move all data to new_block in order
        int idx = 0;
        key_pos = 0;
        uint8_t tp[BPT_MAX_PFX_LEN + 1];
        uint8_t brk_tp_old[BPT_MAX_PFX_LEN + 1];
        uint8_t brk_tp_new[BPT_MAX_PFX_LEN + 1];
        uint8_t *t_end = trie + get_trie_len();
        for (uint8_t *t = trie + 1; t < t_end; t += DFT_UNIT_SIZE) {
    #if DFT_UNIT_SIZE == 3
            int src_idx = get9bit_ptr(t);
            uint8_t child = *t & x40;
    #else
            int src_idx = util::get_int(t + 1);
            uint8_t child = *t & x80;
    #endif
            tp[key_pos] = t - trie;
            if (child) {
                key_pos++;
                tp[key_pos] = t - trie;
            }
            if (src_idx) {
                int kv_len = current_block[src_idx];
                kv_len++;
                kv_len += current_block[src_idx + kv_len];
                kv_len++;
                tot_len += kv_len;
                memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
    #if DFT_UNIT_SIZE == 3
                set9bit_ptr(t + 1, kv_last_pos);
    #else
                util::set_int(t + 1, kv_last_pos);
    #endif
                kv_last_pos += kv_len;
                if (brk_idx == 0) {
                    //if (tot_len > half_kVLen) {
                    //if (tot_len > half_kVLen || ((t-trie) > (get_trie_len() * 2 / 3)) ) {
                    if (tot_len > half_kVLen || idx == (orig_filled_size / 2)
                                || ((t - trie) > (get_trie_len()* 2 / 3)) ) {
                        brk_idx = -1;
                        brk_kv_pos = kv_last_pos;
                        brk_tp_old_pos = (child ? key_pos : key_pos + 1);
                        brk_old_trie_pos = t - trie + DFT_UNIT_SIZE - 1;
                        memcpy(brk_tp_old, tp, brk_tp_old_pos);
                    }
                } else if (brk_idx == -1) {
                    brk_idx = idx;
                    brk_trie_pos = t - trie - 1;
                    *first_len_ptr = (child ? key_pos : key_pos + 1);
                    memcpy(brk_tp_new, tp, *first_len_ptr);
                }
                idx++;
            }
            if (!child) {
    #if DFT_UNIT_SIZE == 3
                uint8_t p = *t & x3F;
    #else
                uint8_t p = *t & x7F;
    #endif
                while (p == 0) {
                    key_pos--;
    #if DFT_UNIT_SIZE == 3
                    p = trie[tp[key_pos]] & x3F;
    #else
                    p = trie[tp[key_pos]] & x7F;
    #endif
                }
            }
        }
        memcpy(new_block.trie, trie, get_trie_len());
        new_block.set_trie_len(get_trie_len());
        idx = *first_len_ptr;
        first_key[--idx] = trie[brk_trie_pos];
        int k = brk_trie_pos;
        while (idx--) {
            int j = brk_tp_new[idx] - 1;
            first_key[idx] = trie[j];
            k -= DFT_UNIT_SIZE;
            new_block.trie[k++] = trie[j++];
            new_block.trie[k] = trie[j];
            if (new_block.trie[k] & (DFT_UNIT_SIZE == 3 ? x3F : x7F))
                new_block.trie[k] -= ((k - j) / DFT_UNIT_SIZE);
            k++;
            j++;
            new_block.trie[k] = trie[j];
    #if DFT_UNIT_SIZE == 3
            set9bit_ptr(new_block.trie + k, 0);
            k++;
    #else
            new_block.trie[k++] = 0;
            new_block.trie[k++] = 0;
    #endif
            k -= DFT_UNIT_SIZE;
        }
        new_block.change_trie_len(-k);
        memmove(new_block.trie, new_block.trie + k, new_block.get_trie_len());
        set_trie_len(brk_old_trie_pos);
        idx = brk_tp_old_pos;
        while (idx--) {
    #if DFT_UNIT_SIZE == 3
            trie[brk_tp_old[idx]] &= xC0;
    #else
            trie[brk_tp_old[idx]] &= x80;
    #endif
        }
    #if DFT_UNIT_SIZE == 3
        trie[brk_old_trie_pos - DFT_UNIT_SIZE + 1] &= x80;
    #else
        trie[brk_old_trie_pos - DFT_UNIT_SIZE + 1] = 0;
    #endif
        if (!is_leaf()) {
            if (new_block.current_block[brk_kv_pos]) {
                memcpy(first_key + *first_len_ptr, new_block.current_block + brk_kv_pos + 1,
                        new_block.current_block[brk_kv_pos]);
                *first_len_ptr += new_block.current_block[brk_kv_pos];
            }
        }

        {
            kv_last_pos = get_kv_last_pos();
            int old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + DFT_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            set_kv_last_pos(DFT_NODE_SIZE - old_blk_new_len);
            set_filled_size(brk_idx);
            int diff = DFT_NODE_SIZE - brk_kv_pos;
            t_end = trie + get_trie_len();
            for (uint8_t *t = trie + 1; t < t_end; t += DFT_UNIT_SIZE) {
    #if DFT_UNIT_SIZE == 3
                int ptr = get9bit_ptr(t);
    #else
                int ptr = util::get_int(t + 1);
    #endif
                if (ptr) {
                    ptr += diff;
    #if DFT_UNIT_SIZE == 3
                    set9bit_ptr(t + 1, ptr);
    #else
                    util::set_int(t + 1, ptr);
    #endif
                }
            }
        }

        int new_size = orig_filled_size - brk_idx;
        new_block.set_kv_last_pos(brk_kv_pos);
        new_block.set_filled_size(new_size);
        return new_block.current_block;
    }

    void add_first_data() {
        add_data(0);
    }

    void add_data(int search_result) {

        int ptr = insert_current();

        int key_left = key_len - key_pos;
        int kv_last_pos = get_kv_last_pos() - (key_left + value_len + 2);
        set_kv_last_pos(kv_last_pos);
    #if DFT_UNIT_SIZE == 3
        trie[ptr--] = kv_last_pos;
        if (kv_last_pos & x100)
            trie[ptr] |= x80;
        else
            trie[ptr] &= x7F;
    #else
        util::set_int(trie + ptr, kv_last_pos);
    #endif
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + key_pos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);
        set_filled_size(filled_size() + 1);

    }

    bool is_full(int search_result) {
        decode_need_count();
    #if DFT_UNIT_SIZE == 3
        if (get_trie_len() > 189 - need_count) {
            //if ((orig_pos - trie) <= (72 + need_count)) {
            return true;
            //}
        }
    #endif
        if (get_kv_last_pos() < (DFT_HDR_SIZE + get_trie_len()
                + need_count + key_len - key_pos + value_len + 3)) {
            return true;
        }
        if (get_trie_len() > 254 - need_count) {
            return true;
        }
        return false;
    }

    int insert_current() {
        uint8_t key_char;
        int ret, ptr, pos;
        ret = pos = 0;

        switch (insert_state) {
        case INSERT_AFTER:
            key_char = key[key_pos - 1];
            update_ptrs(trie_pos, 1);
            orig_pos++;
            *orig_pos += ((trie_pos - orig_pos + 1) / DFT_UNIT_SIZE);
    #if DFT_UNIT_SIZE == 3
            ins_b3(trie_pos, key_char, x00, 0);
    #else
            ins_b4(trie_pos, key_char, x00, 0, 0);
    #endif
            ret = trie_pos - trie + 2;
            break;
        case INSERT_BEFORE:
            key_char = key[key_pos - 1];
            update_ptrs(orig_pos, 1);
            if (last_sibling_pos)
                trie[last_sibling_pos]--;
    #if DFT_UNIT_SIZE == 3
            ins_at(orig_pos, key_char, 1, 0);
    #else
            ins_b4(orig_pos, key_char, 1, 0, 0);
    #endif
            ret = orig_pos - trie + 2;
            break;
        case INSERT_LEAF:
            ret = orig_pos - trie + 2;
            break;
        case INSERT_THREAD:
            int p, min, unit_ctr;
            uint8_t c1, c2;
            unit_ctr = 0;
            key_char = key[key_pos - 1];
            c1 = c2 = key_char;
            p = key_pos;
            min = util::min16(key_len, key_pos + key_at_len);
            orig_pos++;
    #if DFT_UNIT_SIZE == 3
            *orig_pos |= x40;
            ptr = *(orig_pos + 1);
            if (*orig_pos & x80)
                ptr |= x100;
    #else
            *orig_pos |= x80;
            ptr = util::get_int(orig_pos + 1);
    #endif
            if (p < min) {
    #if DFT_UNIT_SIZE == 3
                *orig_pos &= x7F;
                orig_pos++;
                *orig_pos = 0;
    #else
                orig_pos++;
                util::set_int(orig_pos, 0);
                orig_pos++;
    #endif
            } else {
                orig_pos++;
                pos = orig_pos - trie;
                ret = pos;
    #if DFT_UNIT_SIZE == 4
                orig_pos++;
    #endif
            }
            orig_pos++;
            trie_pos = orig_pos;
            while (p < min) {
                uint8_t swap = 0;
                c1 = key[p];
                c2 = key_at[p - key_pos];
                if (c1 > c2) {
                    swap = c1;
                    c1 = c2;
                    c2 = swap;
                }
                switch (c1 == c2 ? (p + 1 == min ? 2 : 1) : 0) {
                case 0:
                    if (swap) {
                        pos = trie_pos - trie + 2;
                        ret = pos + DFT_UNIT_SIZE;
                    } else {
                        ret = trie_pos - trie + 2;
                        pos = ret + DFT_UNIT_SIZE;
                    }
                    trie_pos += insert2Units(trie_pos, c1, x01, (swap ? ptr : 0), c2,
                            0, (swap ? 0 : ptr));
                    unit_ctr += 2;
                    break;
                case 1:
    #if DFT_UNIT_SIZE == 3
                    trie_pos += insert_unit(trie_pos, c1, x40, 0);
    #else
                    trie_pos += insert_unit(trie_pos, c1, x80, 0);
    #endif
                    unit_ctr++;
                    break;
                case 2:
                    if (p + 1 == key_len)
                        ret = trie_pos - trie + 2;
                    else
                        pos = trie_pos - trie + 2;
    #if DFT_UNIT_SIZE == 3
                    trie_pos += insert_unit(trie_pos, c1, x40, 0);
    #else
                    trie_pos += insert_unit(trie_pos, c1, x80, 0);
    #endif
                    unit_ctr++;
                    break;
                }
                if (c1 != c2)
                    break;
                p++;
            }
            int diff;
            diff = p - key_pos;
            key_pos = p + 1;
            if (c1 == c2) {
                c2 = (p == key_len ? key_at[diff] : key[p]);
                if (p == key_len) {
                    pos = trie_pos - trie + 2;
                    key_pos--;
                } else
                    ret = trie_pos - trie + 2;
                trie_pos += insert_unit(trie_pos, c2, x00, 0);
                unit_ctr++;
            }
            update_ptrs(orig_pos, unit_ctr);
            if (diff < key_at_len)
                diff++;
            if (diff) {
                p = ptr;
                key_at_len -= diff;
                p += diff;
                if (key_at_len >= 0) {
                    current_block[p] = key_at_len;
    #if DFT_UNIT_SIZE == 3
                    set9bit_ptr(trie + pos, p);
    #else
                    util::set_int(trie + pos, p);
    #endif
                }
            }
            break;
        case INSERT_EMPTY:
            key_char = *key;
            append(key_char);
            append(x00);
            ret = get_trie_len();
            append_ptr(0);
            key_pos = 1;
            break;
        }

        if (BPT_MAX_PFX_LEN < key_pos)
            BPT_MAX_PFX_LEN = key_pos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

        return ret;
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
