#ifndef HASHPLUS_H
#define HASHPLUS_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

#define BLK_HDR_SIZE 6

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class hashplus : public bplus_tree_handler<hashplus> {
public:
    hashplus(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, const uint8_t opts = 0) :
        bplus_tree_handler<hashplus>(leaf_block_sz, parent_block_sz, cache_sz, fname, 0, false, opts) {
    }

    hashplus(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bplus_tree_handler<hashplus>(block_sz, block, is_leaf) {
        init_stats();
    }

    hashplus(const char *filename, int blk_size, int page_resv_bytes, const uint8_t opts) :
       bplus_tree_handler<hashplus>(filename, blk_size, page_resv_bytes, opts) {
        init_stats();
    }

    inline void set_current_block_root() {
        set_current_block(root_block);
    }

    inline void set_current_block(uint8_t *m) {
        current_block = m;
    }

    int search_current_block(bptree_iter_ctx *ctx = NULL) {
        int middle, first, filled_sz;
        first = 0;
        filled_sz = filled_size();
        while (first < filled_sz) {
            middle = (first + filled_sz) >> 1;
            key_at = get_key(middle, &key_at_len);
            int cmp = util::compare(key_at, key_at_len, key, key_len);
            if (cmp < 0)
                first = middle + 1;
            else if (cmp > 0)
                filled_sz = middle;
            else
                return middle;
        }
        return ~filled_sz;
    }

    inline uint8_t *get_child_ptr_pos(int search_result) {
        if (search_result < 0) {
            search_result++;
            search_result = ~search_result;
        }
        return current_block + get_ptr(search_result);
    }

    uint8_t *get_key(int pos, int *plen) {
        *plen = key_len;
        return get_ptr_pos() + pos * (2 + key_len);
    }

    int get_ptr(int pos) {
        return util::get_int(current_block + pos * (2 + key_len) + key_len);
    }

    void set_ptr(int pos, int ptr) {
        util::set_int(current_block + pos * (2 + key_len) + key_len, ptr);
    }

    inline uint8_t *get_ptr_pos() {
        return current_block + BLK_HDR_SIZE;
    }

    inline int get_header_size() {
        return BLK_HDR_SIZE;
    }

    void make_space() {
    }

    bool is_full(int search_result) {
        int ptr_size = filled_size() + 1;
        ptr_size *= (2 + key_len);
        if (get_kv_last_pos() <= (BLK_HDR_SIZE + ptr_size + value_len + 2)) {
            // if (*current_block & 0x20) {
            //     make_space();
            //     *current_block &= 0xDF;
            //     if (get_kv_last_pos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2))
            //         return true;
            // } else
            	return true;
        }
        return false;
    }

    uint8_t *find_split_source(int search_result) {
        return NULL;
    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int orig_filled_size = filled_size();
        uint32_t HASHPLUS_NODE_SIZE = is_leaf() ? block_size : parent_block_size;
        int lvl = current_block[0] & 0x1F;
        uint8_t *b = allocate_block(HASHPLUS_NODE_SIZE, is_leaf(), lvl);
        hashplus new_block(HASHPLUS_NODE_SIZE, b, is_leaf());
        if (BPT_MAX_KEY_LEN < 255)
            BPT_MAX_KEY_LEN++;
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        int kv_last_pos = get_kv_last_pos();
        int half_kVLen = HASHPLUS_NODE_SIZE - kv_last_pos + 1;
        half_kVLen /= 2;

        int brk_idx = -1;
        int brk_kv_pos;
        int tot_len;
        brk_kv_pos = tot_len = 0;
        // Copy all data to new block in ascending order
        int new_idx;
        for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
            int src_idx = get_ptr(new_idx);
            int v_len = current_block[src_idx];
            v_len++;
            tot_len += v_len;
            int k_at_len;
            uint8_t *k_at = get_key(new_idx, &k_at_len);
            memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, v_len);
            new_block.ins_key_and_ptr(new_idx, k_at, k_at_len, kv_last_pos);
            kv_last_pos += v_len;
            if (brk_idx == -1) {
                if (tot_len > half_kVLen || new_idx == (orig_filled_size / 2)) {
                    brk_idx = new_idx + 1;
                    brk_kv_pos = kv_last_pos;
                    //int first_idx = get_ptr(new_idx + 1);
                    *first_len_ptr = k_at_len;
                    memcpy(first_key, k_at, k_at_len);
                    // if (is_leaf()) {
                    //     int len = 0;
                    //     while (current_block[first_idx + len + 1] == current_block[src_idx + len + 1])
                    //         len++;
                    //     *first_len_ptr = len + 1;
                    //     memcpy(first_key, current_block + first_idx + 1, *first_len_ptr);
                    // } else {
                    //     *first_len_ptr = current_block[first_idx];
                    //     memcpy(first_key, current_block + first_idx + 1, *first_len_ptr);
                    // }
                }
            }
        }
        //memset(current_block + BLK_HDR_SIZE, '\0', HASHPLUS_NODE_SIZE - BLK_HDR_SIZE);
        kv_last_pos = get_kv_last_pos();
        //memset(new_block.current_block + kv_last_pos, '\0', old_blk_new_len);
        int diff = (HASHPLUS_NODE_SIZE - brk_kv_pos);
        for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
            set_ptr(new_idx, new_block.get_ptr(new_idx) + diff);
        } // Set index of copied first half in old block

        {
            int old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + HASHPLUS_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            set_kv_last_pos(HASHPLUS_NODE_SIZE - old_blk_new_len);
            set_filled_size(brk_idx);
        }

        {
            int new_size = orig_filled_size - brk_idx;
            uint8_t *block_ptrs = new_block.current_block + BLK_HDR_SIZE;
            memmove(block_ptrs, block_ptrs + (brk_idx * (2 + key_len)), new_size * (2 + key_len));
            new_block.set_kv_last_pos(brk_kv_pos);
            new_block.set_filled_size(new_size);
        }

        return b;
    }

    inline void ins_key_and_ptr(int pos, const uint8_t *k, int k_len, int kv_pos) {
        int filled_sz = filled_size();
        int unit_size = 2 + k_len;
        uint8_t *kv_idx = get_ptr_pos() + (pos * unit_size);
        memmove(kv_idx + unit_size, kv_idx, (filled_sz - pos) * unit_size);
        memcpy(kv_idx, k, k_len);
        util::set_int(kv_idx + k_len, kv_pos);
        set_filled_size(filled_sz + 1);
    }

    uint8_t *add_data(int search_result) {

        int kv_last_pos = get_kv_last_pos();
        // if (is_leaf()) {
        // } else {
            kv_last_pos -= (value_len + 1);
            set_kv_last_pos(kv_last_pos);
            uint8_t *ptr = current_block + kv_last_pos;
            *ptr++ = value_len;
            memcpy(ptr, value, value_len);
            ins_key_and_ptr(search_result, key, key_len, kv_last_pos);
            if (BPT_MAX_KEY_LEN < key_len)
               BPT_MAX_KEY_LEN = key_len;
        // }
        return ptr;

    }

    void add_first_data() {
        add_data(0);
    }

    void init_derived() {
    }

    void cleanup() {
    }

    void copy_value(uint8_t *val, int *vlen) {
        uint8_t *val_at = current_block + util::get_int(key_at + key_len);
        if (vlen != NULL)
            *vlen = *val_at;
        memcpy(val, val_at + 1, *val_at);
    }

    uint8_t *next_rec(bptree_iter_ctx *ctx, uint8_t *val_buf, int *val_buf_len) {
        return NULL;
    }

};

#endif
