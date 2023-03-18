#ifndef bft_H
#define bft_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define BFT_UNIT_SIZE 4

#define BFT_HDR_SIZE 9

#define BFT_MAX_KEY_PREFIX_LEN 60

class bft_iterator_status {
public:
    uint8_t *t;
    int key_pos;
    uint8_t is_next;
    uint8_t is_child_pending;
    uint8_t tp[BFT_MAX_KEY_PREFIX_LEN];
    bft_iterator_status(uint8_t *trie, uint8_t prefix_len) {
        t = trie + 1;
        key_pos = prefix_len;
        is_child_pending = is_next = 0;
    }
};

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class bft : public bpt_trie_handler<bft> {
public:
    uint8_t *last_t;
    uint8_t *split_buf;
    char need_counts[10];
    uint8_t last_child_pos;
    uint8_t to_pick_leaf;
    bft(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
        bpt_trie_handler(leaf_block_sz, parent_block_sz, cache_sz, fname) {
        split_buf = (uint8_t *) util::aligned_alloc(leaf_block_size > parent_block_size ?
                leaf_block_size : parent_block_size);
        memcpy(need_counts, "\x00\x03\x03\x00\x03\x00\x00\x00\x00\x00", 10);
    }

    bft(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bpt_trie_handler<bft>(block_sz, block, is_leaf) {
        init_stats();
        memcpy(need_counts, "\x00\x03\x03\x00\x03\x00\x00\x00\x00\x00", 10);
   }

    ~bft() {
        if (!is_block_given)
            delete split_buf;
    }

    inline void set_current_block_root() {
        current_block = root_block;
        trie = current_block + BFT_HDR_SIZE;
    }

    inline void set_current_block(uint8_t *m) {
        current_block = m;
        trie = current_block + BFT_HDR_SIZE;
    }

    int search_current_block(bptree_iter_ctx *ctx = NULL) {
        uint8_t *t;
        t = trie;
        last_t = trie + 1;
        key_pos = 0;
        uint8_t key_char = key[key_pos++];
        last_child_pos = 0;
        do {
            orig_pos = t;
            switch (*t & 0x03) {
                case 0x00: {
                    int sib_len = *t++ >> 2;
                    if (sib_len == 0x3F)
                        sib_len += *t++;
                    while (sib_len && key_char > *t) {
                        last_t = t++;
                        sib_len--;
                    }
                    last_t = trie_pos = t;
                    if (!sib_len)
                        return ~INSERT_AFTER;
                    if (key_char < *t)
                        return ~INSERT_BEFORE;
                    while (sib_len--)
                        t++;
                    int ptr_pos = util::get_int(t + ((trie_pos - orig_pos) << 1));
                    int sw = (key_pos == key_len ? 0x01 : 0x00) |
                        (ptr_pos < BPT_TRIE_LEN ? (t[ptr_pos] == 0x02 ? 0x02 : 0x04) : 0x06);
                    switch (sw) {
                    case 0:
                        return ~INSERT_LEAF;
                    case 4:
                        break;
                    case 2:
                        int cmp;
                        key_at = current_block + ptr;
                        key_at_len = *key_at++;
                        cmp = util::compare(key + key_pos, key_len - key_pos, key_at, key_at_len);
                        if (cmp == 0) {
                            last_t = key_at - 1;
                            return ptr;
                        }
                            insert_state = INSERT_THREAD;
                            if (cmp < 0) {
                                if (!is_leaf())
                                    last_t = get_last_ptr(last_t);
                                cmp = -cmp;
                            } else
                                last_t = key_at - 1;
                            need_count = (cmp * BFT_UNIT_SIZE) + BFT_UNIT_SIZE * 2;
                        return -1;
                    case 3:
                        last_t = t;
                        to_pick_leaf = 1;
                        break;
                    }
                    t += (r_children * BFT_UNIT_SIZE);
                    t--;
                    key_char = key[key_pos++];
                    break; }
                case 0x01:
                case 0x03: {
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
                    break; }
                case 0x02:
            }
        } while (1);
        return -1;
    }

    inline uint8_t *get_ptr_pos() {
        return NULL;
    }

    inline uint8_t *get_child_ptr_pos(int search_result) {
        return last_t;
    }

    inline int get_header_size() {
        return BFT_HDR_SIZE;
    }

    int next_ptr(bft_iterator_status& s) {
        if (s.is_child_pending) {
            s.key_pos++;
            s.t += (s.is_child_pending * BFT_UNIT_SIZE);
        } else if (s.is_next) {
            while (*s.t & (BFT_UNIT_SIZE == 3 ? x40 : x80)) {
                s.key_pos--;
                s.t = trie + s.tp[s.key_pos];
            }
            s.t += BFT_UNIT_SIZE;
        } else
            s.is_next = 1;
        do {
            s.tp[s.key_pos] = s.t - trie;
            s.is_child_pending = *s.t & x7F;
            int leaf_ptr = util::get_int(s.t + 1);
            if (leaf_ptr)
                return leaf_ptr;
            else {
                s.key_pos++;
                s.t += (s.is_child_pending * BFT_UNIT_SIZE);
            }
        } while (1); // (s.t - trie) < BPT_TRIE_LEN);
        return 0;
    }

    int get_last_ptr_ofChild(uint8_t *t) {
        do {
            if (*t & x80) {
                uint8_t children = (*t & x7F);
                if (children)
                    t += (children * 4);
                else
                    return util::get_int(t + 1);
            } else
                t += 4;
        } while (1);
        return -1;
    }

    uint8_t *get_last_ptr(uint8_t *last_t) {
        uint8_t last_child = (*last_t & x7F);
        if (!last_child || to_pick_leaf)
            return current_block + util::get_int(last_t + 1);
        return current_block + get_last_ptr_ofChild(last_t + (last_child * 4));
    }

    void append_ptr(int p) {
        util::set_int(trie + BPT_TRIE_LEN, p);
        BPT_TRIE_LEN += 2;
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

    int delete_prefix(int prefix_len) {
        uint8_t *t = trie;
        while (prefix_len--) {
            uint8_t *delete_start = t++;
            if (util::get_int(t)) {
                prefix_len++;
                return prefix_len;
            }
            t += (*t & x7F);
            BPT_TRIE_LEN -= 4;
            memmove(delete_start, delete_start + 4, BPT_TRIE_LEN);
            t -= 4;
        }
        return prefix_len + 1;
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
        if (get_kv_last_pos() < (BFT_HDR_SIZE + BPT_TRIE_LEN
                + need_count + key_len - key_pos + value_len + 3)) {
            return true;
        }
        if (BPT_TRIE_LEN > 254 - need_count)
            return true;
        return false;
    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int orig_filled_size = filled_size();
        const int BFT_NODE_SIZE = is_leaf() ? leaf_block_size : parent_block_size;
        uint8_t *b = allocate_block(BFT_NODE_SIZE, is_leaf(), current_block[0] & 0x1F);
        bft new_block(BFT_NODE_SIZE, b, is_leaf());
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        bft old_block(BFT_NODE_SIZE, split_buf, is_leaf());
        old_block.set_leaf(is_leaf());
        old_block.set_filled_size(0);
        old_block.set_kv_last_pos(new_block.get_kv_last_pos());
        old_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        old_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        bft *ins_block = &old_block;
        int kv_last_pos = get_kv_last_pos();
        int half_kVLen = BFT_NODE_SIZE - kv_last_pos + 1;
        half_kVLen /= 2;

        int brk_idx = 0;
        int tot_len = 0;
        // (1) move all data to new_block in order
        int idx;
        uint8_t ins_key[BPT_MAX_PFX_LEN], old_first_key[BPT_MAX_PFX_LEN];
        int ins_key_len, old_first_len;
        bft_iterator_status s(trie, 0); //BPT_MAX_PFX_LEN);
        for (idx = 0; idx < orig_filled_size; idx++) {
            int src_idx = next_ptr(s);
            int kv_len = current_block[src_idx];
            ins_key_len = kv_len;
            memcpy(ins_key + s.key_pos + 1, current_block + src_idx + 1, kv_len);
            kv_len++;
            ins_block->value_len = current_block[src_idx + kv_len];
            kv_len++;
            ins_block->value = current_block + src_idx + kv_len;
            kv_len += ins_block->value_len;
            tot_len += kv_len;
            for (int i = 0; i <= s.key_pos; i++)
                ins_key[i] = trie[s.tp[i] - 1];
            //for (int i = 0; i < BPT_MAX_PFX_LEN; i++)
            //    ins_key[i] = key[i];
            ins_key_len += s.key_pos;
            ins_key_len++;
            if (idx == 0) {
                memcpy(old_first_key, ins_key, ins_key_len);
                old_first_len = ins_key_len;
            }
            //ins_key[ins_key_len] = 0;
            //cout << ins_key << endl;
            ins_block->key = ins_key;
            ins_block->key_len = ins_key_len;
            if (idx && brk_idx >= 0)
                ins_block->search_current_block();
            ins_block->add_data(0);
            if (brk_idx < 0) {
                brk_idx = -brk_idx;
                s.key_pos++;
                if (is_leaf()) {
                    *first_len_ptr = s.key_pos;
                    memcpy(first_key, ins_key, s.key_pos);
                } else {
                    *first_len_ptr = ins_key_len;
                    memcpy(first_key, ins_key, ins_key_len);
                }
                //first_key[*first_len_ptr] = 0;
                //cout << (int) is_leaf() << "First key:" << first_key << endl;
                s.key_pos--;
            }
            kv_last_pos += kv_len;
            if (brk_idx == 0) {
                //if (tot_len > half_kVLen) {
                if (tot_len > half_kVLen || idx == (orig_filled_size / 2)) {
                    brk_idx = idx + 1;
                    brk_idx = -brk_idx;
                    ins_block = &new_block;
                }
            }
        }
        memcpy(current_block, old_block.current_block, BFT_NODE_SIZE);

        /*
        if (first_key[0] - old_first_key[0] < 2) {
            int len_to_chk = util::min(old_first_len, *first_len_ptr);
            int prefix = 0;
            while (prefix < len_to_chk) {
                if (old_first_key[prefix] != first_key[prefix]) {
                    if (first_key[prefix] - old_first_key[prefix] > 1)
                        prefix--;
                    break;
                }
                prefix++;
            }
            if (BPT_MAX_PFX_LEN) {
                new_block.delete_prefix(BPT_MAX_PFX_LEN);
                new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
            }
            prefix -= delete_prefix(prefix);
            BPT_MAX_PFX_LEN = prefix;
        }*/

        return new_block.current_block;
    }

    int get_first_ptr() {
        bft_iterator_status s(trie, 0);
        return next_ptr(s);
    }

    int insert_current() {
        uint8_t key_char;
        int ret, ptr, pos;
        ret = pos = 0;

        switch (insert_state) {
        case INSERT_AFTER:
            key_char = key[key_pos - 1];
            orig_pos++;
            *orig_pos &= (BFT_UNIT_SIZE == 3 ? xBF : x7F);
            update_ptrs(trie_pos, 1);
            ins_at(trie_pos, key_char, x80, 0, 0);
            ret = trie_pos - trie + 2;
            break;
        case INSERT_BEFORE:
            key_char = key[key_pos - 1];
            update_ptrs(orig_pos, 1);
            if (key_pos > 1 && last_child_pos)
                trie[last_child_pos]--;
            ins_at(orig_pos, key_char, x00, 0, 0);
            ret = orig_pos - trie + 2;
            break;
        case INSERT_LEAF:
            key_char = key[key_pos - 1];
            ret = orig_pos - trie + 2;
            break;
        case INSERT_THREAD:
            int p, min;
            uint8_t c1, c2;
            key_char = key[key_pos - 1];
            c1 = c2 = key_char;
            p = key_pos;
            min = util::min16(key_len, key_pos + key_at_len);
            orig_pos++;
            *orig_pos |= ((BPT_TRIE_LEN - (orig_pos - trie - 1)) / 4);
            ptr = util::get_int(orig_pos + 1);
            if (p < min) {
                util::set_int(orig_pos + 1, 0);
            } else {
                orig_pos++;
                pos = orig_pos - trie;
                ret = pos;
            }
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
                    append(c1);
                    append(x00);
                    if (swap) {
                        pos = BPT_TRIE_LEN;
                        append_ptr(ptr);
                    } else {
                        ret = BPT_TRIE_LEN;
                        append_ptr(0);
                    }
                    append(c2);
                    append(BFT_UNIT_SIZE == 3 ? x40 : x80);
                    if (swap) {
                        ret = BPT_TRIE_LEN;
                        append_ptr(0);
                    } else {
                        pos = BPT_TRIE_LEN;
                        append_ptr(ptr);
                    }
                    break;
                case 1:
                    append(c1);
                    append(BFT_UNIT_SIZE == 3 ? x41 : x81);
                    append_ptr(0);
                    break;
                case 2:
                    append(c1);
                    append(BFT_UNIT_SIZE == 3 ? x41 : x81);
                    if (p + 1 == key_len) {
                        ret = BPT_TRIE_LEN;
                        append_ptr(0);
                    } else {
                        pos = BPT_TRIE_LEN;
                        append_ptr(ptr);
                    }
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
                append(c2);
                append(BFT_UNIT_SIZE == 3 ? x40 : x80);
                if (p == key_len) {
                    pos = BPT_TRIE_LEN;
                    append_ptr(ptr);
                    key_pos--;
                } else {
                    ret = BPT_TRIE_LEN;
                    append_ptr(0);
                }
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
            break;
        case INSERT_EMPTY:
            key_char = *key;
            append(key_char);
            append(BFT_UNIT_SIZE == 3 ? x40 : x80);
            ret = BPT_TRIE_LEN;
            append_ptr(0);
            key_pos = 1;
            break;
        }

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

        if (BPT_MAX_PFX_LEN <= key_pos)
            BPT_MAX_PFX_LEN = key_pos;

        return ret;
    }

    void update_ptrs(uint8_t *upto, int diff) {
        uint8_t *t = trie + 1;
        while (t <= upto) {
            uint8_t child = (*t & x7F);
            if (child && (t + child * 4) >= upto)
                *t += diff;
            t += 4;
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

};

#endif
