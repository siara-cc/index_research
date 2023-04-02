#ifndef bft_H
#define bft_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

#define BFT_UNIT_SIZE 4

#define BFT_HDR_SIZE 9

#define BFT_MIDDLE_PREFIX 1

#define BFT_MAX_KEY_PREFIX_LEN 255

class bft_iterator_status {
public:
    int v_pos, trie_len, last_len;
    uint8_t *node_start[BFT_MAX_KEY_PREFIX_LEN];
    int16_t cur_pos[BFT_MAX_KEY_PREFIX_LEN];
    int16_t len[BFT_MAX_KEY_PREFIX_LEN];
    int8_t len_len[BFT_MAX_KEY_PREFIX_LEN];
    void set_node(uint8_t *n) {
        node_start[v_pos] = n;
            uint8_t *t = n;
            cur_pos[v_pos] = 0;
            len[v_pos] = (*t >> 3);
            len_len[v_pos] = 1;
            if (*t++ & 0x04) {
                len[v_pos] += (((int)*t++) << 5);
                len_len[v_pos] = 2;
            }
    }
    bft_iterator_status(uint8_t *trie, uint8_t prefix_len, int _trie_len) {
        memset(this, '\0', sizeof(bft_iterator_status));
        trie_len = _trie_len;
        set_node(trie);
    }
};

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class bft : public bpt_trie_handler<bft> {
public:
    uint8_t *last_t;
    int last_t_pos;
    uint8_t *split_buf;
    char need_counts[10];
    uint8_t to_pick_leaf;
    bft(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
        bpt_trie_handler(leaf_block_sz, parent_block_sz, cache_sz, fname) {
        split_buf = (uint8_t *) util::aligned_alloc(leaf_block_size > parent_block_size ?
                leaf_block_size : parent_block_size);
        memcpy(need_counts, "\x00\x04\x04\x00\x04\x00\x08\x03\x00\x00", 10);
    }

    bft(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bpt_trie_handler<bft>(block_sz, block, is_leaf) {
        init_stats();
        memcpy(need_counts, "\x00\x04\x04\x00\x04\x00\x08\x03\x00\x00", 10);
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
        key_pos = 0;
        uint8_t key_char = key[key_pos++];
        do {
            orig_pos = t;
            switch (*t & 0x03) {
                case 0x00: {
                    int len_of_len;
                    int sib_len = get_pfx_len(t, &len_of_len);
                    t += len_of_len;
                    while (sib_len && key_char > *t) {
                        last_t = orig_pos;
                        last_t_pos = sib_len;
                        sib_len--;
                        t++;
                    }
                    trie_pos = t;
                    if (!sib_len)
                        return ~INSERT_AFTER;
                    if (key_char < *t)
                        return ~INSERT_BEFORE;
                    t += sib_len;
                    t += ((trie_pos - orig_pos - len_of_len) << 1);
                    int ptr_pos = util::get_int(t);
                    int sw = (key_pos == key_len ? 0x01 : 0x00) |
                        (ptr_pos < get_trie_len() ? 0x02 : 0x00);
                    if (sw < 2) {
                        int cmp;
                        key_at = current_block + ptr_pos;
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
#if BFT_MIDDLE_PREFIX == 1
                        // cmp = pfx_len, 2=len_of_len, 7 = end node, 3 - child_leaf
                        need_count = cmp + 2 + 7 + 3;
#else
                        // cmp * 4 for pfx, 7 = end node
                        need_count = (cmp * 4) + 3;
#endif
                        return ~INSERT_THREAD;
                    }
                    t += ptr_pos;
                    if (*t != 0x02) {
                        if (sw == 0x03)
                            return ~INSERT_CHILD_LEAF;
                        key_char = key[key_pos++];
                    }
                    break; }
                case 0x01:
                case 0x03: {
                    int pfx_len;
                    pfx_len = (*t >> 3);
                    if (*t++ & 0x04)
                        pfx_len += (((int) *t++) << 5);
                    while (pfx_len && key_char == *t && key_pos < key_len) {
                        t++;
                        pfx_len--;
                        key_char = key[key_pos++];
                    }
                    if (pfx_len) {
                        trie_pos = t;
                        return ~INSERT_CONVERT;
                    }
                    break; }
                case 0x02: // next data type
                    last_t = key_at;
                    if (key_pos == key_len) {
                        key_at = current_block + util::get_int(t + 1);
                        key_at_len = *key_at++;
                        return 0;
                    }
                    t += 3;
                    key_char = key[key_pos++];
                    break;
            }
        } while (1);
        return -1;
    }

    inline uint8_t *get_ptr_pos() {
        return NULL;
    }

    uint8_t *get_last_ptr_of_child(uint8_t *t) {
        int len, len_len;
        switch (*t & 0x03) {
            case 0x00: {
                len = get_pfx_len(t, &len_len);
                t += (len_len + len * 3);
                t -= 2;
                int ptr = util::get_int(t);
                if (ptr < get_trie_len())
                    return get_last_ptr_of_child(t + ptr);
                else
                    return current_block + ptr;
                break; }
            case 0x02:
                return get_last_ptr_of_child(t + 3);
            case 0x01:
            case 0x03:
                len = get_pfx_len(t, &len_len);
                return get_last_ptr_of_child(t + len_len + len);
                break;
        }
        return 0;
    }

    uint8_t *get_last_ptr(uint8_t *t) {
        int len, len_len;
        if (*orig_pos & 0x01) {
            if (key[key_pos - 1] > *trie_pos)
                return get_last_ptr_of_child(orig_pos);
            else
                t = last_t;
        }
        if (*t == 0x02)
            return current_block + util::get_int(t + 1);
        len = get_pfx_len(t, &len_len);
        t += (len_len + len + ((len - last_t_pos) << 1));
        int ptr = util::get_int(t);
        if (ptr < get_trie_len())
            return get_last_ptr_of_child(t + ptr);
        return current_block + ptr;
    }

    inline uint8_t *get_child_ptr_pos(int search_result) {
        return last_t == key_at ? last_t - 1 : get_last_ptr(last_t);
    }

    inline int get_header_size() {
        return BFT_HDR_SIZE;
    }

    int next_ptr(bft_iterator_status& s, uint8_t *out_str, int& out_len) {
        out_len = s.last_len;
        uint8_t *cur_loc = s.node_start[s.v_pos];
        if (s.v_pos < 0)
            return 0;
        do {
            switch (*cur_loc & 0x03) {
                case 0x00: {
                    if (s.cur_pos[s.v_pos] && out_len)
                        out_len--;
                    out_str[out_len++] = cur_loc[s.len_len[s.v_pos] + s.cur_pos[s.v_pos]];
                    cur_loc = cur_loc + s.len_len[s.v_pos]
                                + s.len[s.v_pos] + (s.cur_pos[s.v_pos] << 1);
                    int ptr = util::get_int(cur_loc);
                    if (ptr < s.trie_len) {
                        cur_loc += ptr;
                        if (*cur_loc != 0x02) {
                            s.v_pos++;
                            s.set_node(cur_loc);
                        }
                    } else {
                        s.cur_pos[s.v_pos]++;
                        s.last_len = out_len;
                        while (s.v_pos > -1 && s.cur_pos[s.v_pos] == s.len[s.v_pos]) {
                            s.v_pos--;
                            while (s.v_pos > -1 && (s.node_start[s.v_pos][0] & 0x01) == 0x01) {
                                s.last_len -= s.len[s.v_pos];
                                s.v_pos--;
                            }
                            if (s.v_pos < 0)
                                break;
                            s.last_len--;
                            s.cur_pos[s.v_pos]++;
                        }
                        if (s.v_pos == 0 && s.cur_pos[s.v_pos] >= s.len[s.v_pos])
                            s.v_pos = -1;
                        return ptr;
                    }
                    break; }
                case 0x02:
                    s.v_pos++;
                    s.set_node(cur_loc + 3);
                    s.last_len = out_len;
                    return util::get_int(cur_loc + 1);
                case 0x01:
                case 0x03:
                    cur_loc += s.len_len[s.v_pos];
                    memcpy(out_str + out_len, cur_loc, s.len[s.v_pos]);
                    out_len += s.len[s.v_pos];
                    cur_loc += s.len[s.v_pos];
                    if (*cur_loc != 0x02) {
                        s.v_pos++;
                        s.set_node(cur_loc);
                    }
                    break;
            }
        } while (1); // (s.t - trie) < get_trie_len());
        return 0;
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
            change_trie_len(-4);
            memmove(delete_start, delete_start + 4, get_trie_len());
            t -= 4;
        }
        return prefix_len + 1;
    }

    void add_first_data() {
        add_data(4);
    }

    void add_data(int search_result) {

        if (search_result < 0)
            search_result = ~search_result;
        insert_state = search_result;
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
        if (search_result < 0)
            search_result = ~search_result;
        if (search_result != INSERT_THREAD)
            need_count = need_counts[search_result];
        if (get_kv_last_pos() < (BFT_HDR_SIZE + get_trie_len()
                + need_count + key_len - key_pos + value_len + 3)) {
            return true;
        }
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
        uint8_t ins_key[256];
        //uint8_t old_first_key[BPT_MAX_PFX_LEN];
        int ins_key_len, old_first_len;
        bft_iterator_status *s = new bft_iterator_status(trie, 0, get_trie_len()); //BPT_MAX_PFX_LEN);
        for (idx = 0; idx < orig_filled_size; idx++) {
            int src_idx = next_ptr(*s, ins_key, ins_key_len);
            int kv_len = current_block[src_idx];
            memcpy(ins_key + ins_key_len, current_block + src_idx + 1, kv_len);
            ins_key_len += kv_len;
            kv_len++;
            ins_block->value_len = current_block[src_idx + kv_len];
            kv_len++;
            ins_block->value = current_block + src_idx + kv_len;
            kv_len += ins_block->value_len;
            tot_len += kv_len;
            // if (idx == 0) {
            //     memcpy(old_first_key, ins_key, ins_key_len);
            //     old_first_len = ins_key_len;
            // }
            ins_block->key = ins_key;
            ins_block->key_len = ins_key_len;
            // printf("Key: %d,%.*s, Value: %.*s\n", current_block[src_idx], ins_block->key_len, ins_block->key, ins_block->value_len, ins_block->value);
            int search_result = 4;
            if (ins_block->filled_size() > 0)
                search_result = ins_block->search_current_block();
            ins_block->add_data(search_result);
            if (brk_idx < 0) {
                brk_idx = -brk_idx;
                if (is_leaf()) {
                    // *first_len_ptr = ins_key_len;
                    // memcpy(first_key, ins_key, ins_key_len);
                    int cmp = util::compare(ins_key, ins_key_len, first_key, *first_len_ptr);
                    if (cmp < ins_key_len)
                        cmp++;
                    if (cmp < ins_key_len)
                        cmp++;
                    *first_len_ptr = cmp;
                    memcpy(first_key, ins_key, *first_len_ptr);
                } else {
                    *first_len_ptr = ins_key_len;
                    memcpy(first_key, ins_key, ins_key_len);
                }
                //first_key[*first_len_ptr] = 0;
                //cout << (int) is_leaf() << "First key:" << first_key << endl;
            }
            kv_last_pos += kv_len;
            if (brk_idx == 0) {
                //if (tot_len > half_kVLen) {
                if (tot_len > half_kVLen || idx == (orig_filled_size / 2)) {
                    brk_idx = idx + 1;
                    brk_idx = -brk_idx;
                    memcpy(first_key, ins_key, ins_key_len);
                    *first_len_ptr = ins_key_len;
                    ins_block = &new_block;
                }
            }
        }
        delete s;
        memcpy(current_block, old_block.current_block, BFT_NODE_SIZE);

        return new_block.current_block;
    }

    int get_first_ptr() {
        bft_iterator_status s(trie, 0, get_trie_len());
        uint8_t key_str[BPT_MAX_KEY_LEN];
        int key_str_len;
        return next_ptr(s, key_str, key_str_len);
    }

    void update_ptrs(uint8_t *upto, int diff) {
        uint8_t *t = trie;
        int len_of_len, len;
        while (t <= upto) {
            switch (*t & 0x03) {
                case 0: {
                    len = get_pfx_len(t, &len_of_len);
                    t += len_of_len;
                    t += len;
                    int ptr_idx = 0;
                    while (t < upto && ptr_idx++ < len) {
                        int ptr_pos = util::get_int(t);
                        if (ptr_pos < get_trie_len() && (t + ptr_pos) >= upto)
                            util::set_int(t, ptr_pos + diff);
                        t += 2;
                    }
                    break; }
                case 1:
                case 3:
                    t += get_pfx_len(t, &len_of_len);
                    t += len_of_len;
                    break;
                case 2:
                    t += 3;
            }
       }
    }

    void update_sibling_ptrs(int ret, int count) {
        uint8_t *t = trie + ret;
        while (count) {
            int ptr_pos = util::get_int(t - (count << 1));
            if (ptr_pos < get_trie_len())
                util::set_int(t - (count << 1), ptr_pos + 2);
            count--;
        }
    }

    int set_pfx_len(uint8_t *ptr, int len) {
        *ptr = (*ptr & 0x07) | ((len & 0x1F) << 3);
        if (len > 0x1F) {
            *ptr++ |= 0x04;
            *ptr = len >> 5;
            return 2;
        }
        return 1;
    }

    int get_pfx_len(uint8_t *t, int *len_of_len) {
        int len = *t >> 3;
        *len_of_len = 1;
        if (*t++ & 0x04) {
            len += (((int)*t) << 5);
            *len_of_len = 2;
        }
        return len;
    }

    bool change_len(uint8_t *ptr, int& ret_len, int delta = 1) {
        ret_len = *ptr >> 3;
        if (*ptr & 0x04)
            ret_len += (((int)ptr[1]) << 5);
        ret_len += delta;
        *ptr = (*ptr & 0x07) | ((ret_len & 0x1F) << 3);
        if (ret_len > 0x1F) {
            if ((*ptr & 0x04) == 0) {
                *ptr++ |= 0x04;
                ins_b(ptr, ret_len >> 5);
                update_ptrs(ptr, 1);
                return true;
            }
            ptr++;
            *ptr = ret_len >> 5;
        }
        return false;
    }

    uint8_t *find_last_child_rec(uint8_t *last_node, int sib_len, int len_of_sib_len, int count) {
        while (count--) {
            uint8_t *ptr_loc = last_node + len_of_sib_len + sib_len + (count << 1);
            int ptr_pos = util::get_int(ptr_loc);
            if (ptr_pos < get_trie_len()) {
                uint8_t *child_node = ptr_loc + ptr_pos;
                while ((*child_node & 0x03) && child_node < trie + get_trie_len()) {
                    if (*child_node == 0x02)
                        child_node += 3;
                    if (*child_node & 0x01) {
                        int len_of_len;
                        int len = get_pfx_len(child_node, &len_of_len);
                        child_node += len + len_of_len;
                    }
                }
                int len_of_child_len;
                int child_sib_len = get_pfx_len(child_node, &len_of_child_len);
                return find_last_child_rec(child_node, child_sib_len, len_of_child_len, child_sib_len);
            }
        }
        return last_node + len_of_sib_len + (sib_len * 3);
    }

    uint8_t *find_child_pos_to_be(int sib_len, int len_of_sib_len) {
        int count = trie_pos - orig_pos - len_of_sib_len;
        return find_last_child_rec(orig_pos, sib_len, len_of_sib_len, count);
    }

    int insert_current() {
        uint8_t key_char;
        int ret, len;
        ret = 0;

        key_char = key[key_pos - 1];
        switch (insert_state) {
        case INSERT_AFTER: {
            if (change_len(orig_pos, len, 1))
                trie_pos++;
            update_ptrs(trie_pos, 3);
            ins_b(trie_pos++, key_char);
            ret = (trie_pos - trie) + (len << 1) - 2;
            ins_b2(trie + ret, 0x00, 0x00);
            update_sibling_ptrs(ret, len);
            break; }
        case INSERT_BEFORE: {
            if (change_len(orig_pos, len, 1))
                trie_pos++;
            update_ptrs(trie_pos, 3);
            ins_b(trie_pos, key_char);
            uint8_t *t = orig_pos + (*orig_pos & 0x04 ? 2 : 1);
            ret = (trie_pos - t);
            ret = (t - trie) + len + (ret << 1);
            ins_b2(trie + ret, 0x00, 0x00);
            update_sibling_ptrs(ret, trie_pos - t);
            break; }
        case INSERT_CHILD_LEAF: {
            int len_of_len;
            len = get_pfx_len(orig_pos, &len_of_len);
            uint8_t *t = orig_pos + len_of_len + len;
            t += ((trie_pos - orig_pos - len_of_len) << 1);
            int ptr_pos = util::get_int(t);
            ins_b3(t + ptr_pos, 0x02, 0x00, 0x00);
            update_ptrs(t + ptr_pos + 1, 3);
            ret = (t - trie) + ptr_pos + 1;
            break; }
        case INSERT_THREAD: {
            int p, min;
            uint8_t c1, c2;
            uint8_t *t;
            int ptr, pos;
            ret = pos = 0;

            t = orig_pos;
            int sib_len = get_pfx_len(orig_pos, &len);
            t += len;
            while (*t != key_char)
                t++;
            t = orig_pos + len + sib_len + ((t - orig_pos - len) << 1);
            ptr = util::get_int(t);
            c1 = c2 = key_char;
            p = key_pos;
            min = util::min_b(key_len, key_pos + key_at_len);
#if BFT_MIDDLE_PREFIX == 1
            int cmp = need_count - (2 + 7 + 3 + 1); // restore cmp from search_current_block
            need_count = 0;
#else
            int cmp = need_count - 3; // restore cmp from search_current_block
            cmp /= 4;
            cmp--;
#endif
            if (p + cmp == min && cmp) {
                cmp--;
#if BFT_MIDDLE_PREFIX == 1
                need_count = 4;
#endif
            }
#if BFT_MIDDLE_PREFIX == 1
            need_count += (cmp + (cmp ? 1 : 0) + 7);
#endif

            uint8_t *child_ins_pos = find_child_pos_to_be(sib_len, len);
            // uint8_t *child_ins_pos = orig_pos + len + (sib_len * 3);
            if (child_ins_pos != trie + get_trie_len()) {
                ins_at(child_ins_pos, trie + get_trie_len() + need_count, need_count);
                // update_sibling_ptrs(t - trie, trie_pos - orig_pos - len);
                update_ptrs(child_ins_pos, need_count);
            } else
                set_trie_len(get_trie_len() + need_count);
            util::set_int(t, child_ins_pos - t);
            //util::set_int(t, get_trie_len() - (t - trie));
            t = child_ins_pos;

            // util::set_int(t, get_trie_len() - (t - trie));
            // t = trie + get_trie_len();
#if BFT_MIDDLE_PREFIX == 1
            if (cmp) {
                *t++ = (cmp << 3) | (cmp > 31 ? 0x05 : 0x01);
                if (cmp > 31)
                    *t++ = cmp >> 5;
                memcpy(t, key + key_pos, cmp);
                t += cmp;
            }
            p += cmp;
#endif
            // Insert thread
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
#if BFT_MIDDLE_PREFIX == 1
                switch (c1 == c2 ? 3 : 1) {
#else
                switch (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1) {
                case 2:
                    *t++ = 0x08;
                    *t++ = c1;
                    util::set_int(t, 2);
                    t += 2;
                    break;
#endif
                case 1:
                    *t++ = 0x10;
                    *t++ = c1;
                    *t++ = c2;
                    ret = is_swapped ? ret : (t - trie);
                    pos = is_swapped ? (t - trie) : pos;
                    util::set_int(t, is_swapped ? ptr : 0);
                    t += 2;
                    ret = is_swapped ? (t - trie) : ret;
                    pos = is_swapped ? pos : (t - trie);
                    util::set_int(t, is_swapped ? 0 : ptr);
                    t += 2;
                    break;
                case 3:
                    *t++ = 0x08;
                    *t++ = c1;
                    util::set_int(t, 2);
                    t += 2;
                    break;
                }
                if (c1 != c2)
                    break;
                p++;
            }
            // child_leaf
            if (c1 == c2) {
                    *t++ = 0x02;
                    ret = (p == key_len) ? (t - trie) : ret;
                    pos = (p == key_len) ? pos : (t - trie);
                    util::set_int(t, ptr);
                    t += 2;
                c2 = (p == key_len ? key_at[p - key_pos] : key[p]);
                *t++ = x08;
                *t++ = c2;
                ret = (p == key_len) ? ret : (t - trie);
                pos = (p == key_len) ? (t - trie) : pos;
                    util::set_int(t, ptr);
                t += 2;
            }
            if (t - child_ins_pos != need_count)
                std::cout << "MISMATCH NEED_COUNT: " << key << " " << cmp << " " << (t - child_ins_pos) << " " << (int) need_count << std::endl;
            // set_trie_len(t - trie);
            int diff = p - key_pos;
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
            break; }
#if BFT_MIDDLE_PREFIX == 1
        case INSERT_CONVERT: {
            int diff = trie_pos - orig_pos;
            int len_of_len;
            int pfx_len = get_pfx_len(orig_pos, &len_of_len);
            uint8_t c1 = (*trie_pos > key_char ? key_char : *trie_pos);
            uint8_t c2 = (*trie_pos > key_char ? *trie_pos : key_char);
            uint8_t *t = orig_pos + len_of_len;
            diff = trie_pos - t;
            int sw = (pfx_len == 1 ? 0 : (diff == 0 ? 1 : (diff == (pfx_len - 1) ? 2 : 3)));
            int to_ins;
            // if (key_pos == key_len && c1 == c2) {
                to_ins = (sw == 0 ? 5 : (sw == 3 ? 7 : 6));
                //std::cout << "IC: " << sw << " " << to_ins << std::endl;
                if (sw == 2 || sw == 3) {
                    if (change_len(orig_pos, len_of_len, diff - pfx_len))
                        trie_pos++;
                }
            // } else {
            //     to_ins = (sw == 0 ? 5 : (sw == 3 ? 7 : 6));
            //     if (sw == 2 || sw == 3) {
            //         if (change_len(orig_pos, len_of_len, diff - pfx_len))
            //             trie_pos++;
            //     }
            // }
            ins_bytes(trie_pos, to_ins);
            update_ptrs(orig_pos + 1, to_ins);
            t = (sw == 0 || sw == 1 ? orig_pos : trie_pos);
            if (key_pos == key_len && c1 == c2) {
                *t++ = 0x08;
                *t++ = c1;
                util::set_int(t, 2);
                t += 2;
                *t++ = 0x02;
                ret = t - trie;
                t += 2;
            } else {
                *t++ = 0x10;
                *t++ = c1;
                *t++ = c2;
                util::set_int(t, 4);
                t += 2;
                util::set_int(t, 2);
                ret = t - trie - (c1 == key_char ? 2 : 0);
                t += 2;
            }
            if (sw == 1 || sw == 3) {
                *t = 0x01;
                change_len(t, len, pfx_len - diff - 1);
            }
            break; }
#endif
        case INSERT_EMPTY:
            key_char = *key;
            append(0x08);
            append(key_char);
            ret = get_trie_len();
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
