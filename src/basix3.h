#ifndef BASIX3_H
#define BASIX3_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define BASIX3_HDR_SIZE 7

#define MAX_KEY_LEN current_block[6]

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class basix3 : public bplus_tree_handler<basix3> {
public:
    int pos;
    basix3(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
        bplus_tree_handler<basix3>(leaf_block_sz, parent_block_sz, cache_sz, fname) {
    }

    basix3(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bplus_tree_handler<basix3>(block_sz, block, is_leaf) {
        init_stats();
    }

    inline void set_current_block_root() {
        set_current_block(root_block);
    }

    inline void set_current_block(uint8_t *m) {
        current_block = m;
    }

    inline uint8_t *get_key(int pos, int *plen) {
        uint8_t *kv_idx = current_block + get_ptr(pos);
        *plen = *kv_idx;
        return kv_idx + 1;
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

    inline int get_header_size() {
        return BASIX3_HDR_SIZE;
    }

    void remove_entry(int pos) {
        del_ptr(pos);
    }

    void make_space() {
        int block_size = (is_leaf() ? leaf_block_size : parent_block_size);
        int orig_filled_size = filled_size();
        if (orig_filled_size == 0) {
            set_kv_last_pos(block_size == 65536 ? 65535 : block_size);
            return;
        }
        const uint32_t data_size = block_size - get_kv_last_pos();
        if (data_size < 3) {
            cout << "data size 0" << endl;
            return;
        }
        uint8_t *data_buf = (uint8_t *) malloc(data_size);
        uint32_t new_data_len = 0;
        int new_idx;
        for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
            uint32_t src_idx = get_ptr(new_idx);
            int kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            new_data_len += kv_len;
            memcpy(data_buf + data_size - new_data_len, current_block + src_idx, kv_len);
            set_ptr(new_idx, block_size - new_data_len);
        }
        uint32_t new_kv_last_pos = block_size - new_data_len;
        memcpy(current_block + new_kv_last_pos, data_buf + data_size - new_data_len, new_data_len);
        //printf("%d, %d\n", data_size, new_data_len);
        free(data_buf);
        set_kv_last_pos(new_kv_last_pos);
        search_current_block();
    }

    bool is_full(int search_result) {
        uint32_t ptr_size = filled_size() + 1;
        ptr_size *= 3;
        if (get_kv_last_pos() <= (BASIX3_HDR_SIZE + ptr_size + key_len + value_len + 2)) {
            //make_space();
            //if (get_kv_last_pos() <= (BASIX3_HDR_SIZE + ptr_size + key_len + value_len + 2))
                return true;
        }
        return false;
    }

    uint8_t *find_split_source(int search_result) {
        return NULL;
    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int orig_filled_size = filled_size();
        uint32_t BASIX_NODE_SIZE = is_leaf() ? leaf_block_size : parent_block_size;
        int lvl = current_block[0] & 0x1F;
        uint8_t *b = allocate_block(BASIX_NODE_SIZE, is_leaf(), lvl);
        basix3 new_block(BASIX_NODE_SIZE, b, is_leaf());
        new_block.MAX_KEY_LEN = MAX_KEY_LEN;
        new_block.set_kv_last_pos(BASIX_NODE_SIZE);
        uint32_t kv_last_pos = get_kv_last_pos();
        uint32_t half_kVLen = BASIX_NODE_SIZE - kv_last_pos + 1;
        half_kVLen /= 2;

        int brk_idx = -1;
        uint32_t brk_kv_pos;
        uint32_t tot_len;
        brk_kv_pos = tot_len = 0;
        // Copy all data to new block in ascending order
        int new_idx;
        for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
            uint32_t src_idx = get_ptr(new_idx);
            int kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            tot_len += kv_len;
            memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
            new_block.ins_ptr(new_idx, kv_last_pos);
            kv_last_pos += kv_len;
            if (brk_idx == -1) {
                if (tot_len > half_kVLen || new_idx == (orig_filled_size / 2)) {
                    brk_idx = new_idx + 1;
                    brk_kv_pos = kv_last_pos;
                    uint32_t first_idx = get_ptr(new_idx + 1);
                    if (is_leaf()) {
                        int len = 0;
                        while (current_block[first_idx + len + 1] == current_block[src_idx + len + 1])
                            len++;
                        *first_len_ptr = len + 1;
                        memcpy(first_key, current_block + first_idx + 1, *first_len_ptr);
                    } else {
                        *first_len_ptr = current_block[first_idx];
                        memcpy(first_key, current_block + first_idx + 1, *first_len_ptr);
                    }
                }
            }
        }
        //memset(current_block + BASIX3_HDR_SIZE, '\0', BASIX_NODE_SIZE - BASIX3_HDR_SIZE);
        kv_last_pos = get_kv_last_pos();
        //memset(new_block.current_block + kv_last_pos, '\0', old_blk_new_len);
        int diff = (BASIX_NODE_SIZE - brk_kv_pos);
        for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
            set_ptr(new_idx, new_block.get_ptr(new_idx) + diff);
        } // Set index of copied first half in old block

        {
            uint32_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + BASIX_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            set_kv_last_pos(BASIX_NODE_SIZE - old_blk_new_len);
            set_filled_size(brk_idx);
        }

        {
            int new_size = orig_filled_size - brk_idx;
            uint8_t *block_ptrs = new_block.current_block + BASIX3_HDR_SIZE;
            memmove(block_ptrs, block_ptrs + (brk_idx * 3), new_size * 3);
            new_block.set_kv_last_pos(brk_kv_pos);
            new_block.set_filled_size(new_size);
        }

        return b;
    }

    void add_data(int search_result) {

        if (search_result < 0)
            search_result = ~search_result;
        uint32_t kv_last_pos = get_kv_last_pos() - (key_len + value_len + 2);
        set_kv_last_pos(kv_last_pos);
        uint8_t *ptr = current_block + kv_last_pos;
        *ptr++ = key_len;
        memcpy(ptr, key, key_len);
        ptr += key_len;
        *ptr++ = value_len;
        memcpy(ptr, value, value_len);
        ins_ptr(search_result, kv_last_pos);
        if (MAX_KEY_LEN < key_len)
            MAX_KEY_LEN = key_len;

    }

    void add_first_data() {
        set_kv_last_pos(is_leaf() ? leaf_block_size : parent_block_size);
        MAX_KEY_LEN = 1;
        add_data(0);
    }

    inline uint32_t get_kv_last_pos() {
        return util::get_int3(BPT_LAST_DATA_PTR);
    }

    inline void set_kv_last_pos(uint32_t val) {
        util::set_int3(BPT_LAST_DATA_PTR, val);
    }

    inline uint8_t *get_ptr_pos() {
        return current_block + BASIX3_HDR_SIZE;
    }

    inline uint32_t get_ptr(int pos) {
        return util::get_int3(get_ptr_pos() + (pos * 3));
    }

    inline void set_ptr(int pos, uint32_t ptr) {
        uint8_t *kv_idx = get_ptr_pos() + (pos * 3);
        return util::set_int3(kv_idx, ptr);
    }

    inline void ins_ptr(int pos, uint32_t kv_pos) {
        int filled_sz = filled_size();
        uint8_t *kv_idx = get_ptr_pos() + (pos * 3);
        memmove(kv_idx + 3, kv_idx, (filled_sz - pos) * 3);
        util::set_int3(kv_idx, kv_pos);
        set_filled_size(filled_sz + 1);
    }

    inline void del_ptr(int pos) {
        int filled_sz = filled_size() - 1;
        uint8_t *kv_idx = get_ptr_pos() + (pos * 3);
        memmove(kv_idx, kv_idx + 3, (filled_sz - pos) * 3);
        set_filled_size(filled_sz);
    }

    void init_derived() {
    }

    void cleanup() {
    }

};

#endif
