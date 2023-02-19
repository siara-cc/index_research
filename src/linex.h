#ifndef LINEX_H
#define LINEX_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define LX_BLK_HDR_SIZE 6
#define LX_PREFIX_CODING 1
#define LX_DATA_AREA 0

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class linex : public bplus_tree_handler<linex> {
private:
    int linear_search() {
        int idx = 0;
        int filled_sz = filled_size();
        key_at = current_block + LX_BLK_HDR_SIZE;
        prev_key_at = key_at;
        prefix_len = prev_prefix_len = 0;
        while (idx < filled_sz) {
            int cmp;
            key_at_len = *(key_at + 1);
    #if LX_PREFIX_CODING == 1
            uint8_t *p_cur_prefix_len = key_at;
            for (int pctr = 0; prefix_len < (*p_cur_prefix_len & 0x7F); pctr++)
                prefix[prefix_len++] = prev_key_at[pctr + 2];
            if (prefix_len > *key_at)
                prefix_len = *key_at;
            cmp = memcmp(prefix, key, prefix_len);
            if (cmp == 0) {
                cmp = util::compare(key_at + 2, key_at_len, key + prefix_len, key_len - prefix_len);
            }
    #if LX_DATA_AREA == 1
            if (cmp == 0) {
                int partial_key_len = prefix_len + key_at_len;
                uint8_t *data_key_at = current_block + key_at_len + *key_at + 1;
                if (*p_cur_prefix_len & 0x80)
                    data_key_at += 256;
                cmp = util::compare(data_key_at + 1, *data_key_at, key + partial_key_len, key_len - partial_key_len);
            }
    #endif
    #else
            cmp = util::compare(key_at + 2, key_at_len, key, key_len);
    #endif
            if (cmp > 0)
                return ~idx;
            else if (cmp == 0) {
                key_at += 2;
                return idx;
            }
            prev_key_at = key_at;
            prev_prefix_len = prefix_len;
            key_at += key_at_len;
            key_at += 2;
            key_at += *key_at;
            key_at++;
    #if LX_DATA_AREA == 1
            key_at++;
    #endif
            idx++;
        }
        return ~idx;
    }
public:
    int pos;
    uint8_t *prev_key_at;
    uint8_t prefix[60];
    int prefix_len;
    int prev_prefix_len;

    linex(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
       bplus_tree_handler<linex>(leaf_block_sz, parent_block_sz, cache_sz, fname) {
        init_current_block();
    }

    linex(uint32_t block_sz, uint8_t *block, bool ls_leaf) :
      bplus_tree_handler<linex>(block_sz, block, ls_leaf) {
        init_stats();
    }

    void add_first_data() {
        set_kv_last_pos(LX_BLK_HDR_SIZE);
        add_data(0);
    }

    void add_data(int search_result) {
        int prev_plen = 0;
        int filled_sz = filled_size();
        set_filled_size(filled_sz + 1);
        if (search_result < 0)
            key_at -= 2;
    #if LX_PREFIX_CODING == 1
        if (filled_sz && key_at != prev_key_at) {
            while (prev_plen < key_len) {
                if (prev_plen < prev_prefix_len) {
                    if (prefix[prev_plen] != key[prev_plen])
                        break;
                } else {
                    if (prev_key_at[prev_plen - prev_prefix_len + 2]
                            != key[prev_plen])
                        break;
                }
                prev_plen++;
            }
        }
    #endif
        int kv_last_pos = get_kv_last_pos();
        int key_left = key_len - prev_plen;
        int tot_len = key_left + value_len + 3;
        int len_to_move = kv_last_pos - (key_at - current_block);
        if (len_to_move) {
    #if LX_PREFIX_CODING == 1
            int next_plen = prefix_len;
            while (key_at[next_plen - prefix_len + 2] == key[next_plen]
                    && next_plen < key_len)
                next_plen++;
            int diff = next_plen - prefix_len;
            if (diff) {
                int next_key_len = key_at[1];
                int next_value_len = key_at[next_key_len + 2];
                *key_at = next_plen;
                key_at[1] -= diff;
                memmove(key_at + 2, key_at + diff + 2,
                        next_key_len + next_value_len + 1 - diff);
                memmove(key_at + diff, key_at,
                        next_key_len + next_value_len + 3 - diff);
                tot_len -= diff;
            }
    #endif
            memmove(key_at + tot_len, key_at, len_to_move);
        }
        *key_at++ = prev_plen;
        *key_at++ = key_left;
        memcpy(key_at, key + prev_plen, key_left);
        key_at += key_left;
        *key_at++ = value_len;
        memcpy(key_at, value, value_len);
        kv_last_pos += tot_len;
        set_kv_last_pos(kv_last_pos);

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int filled_sz = filled_size();
        const uint32_t LINEX_NODE_SIZE = is_leaf() ? leaf_block_size : parent_block_size;
        uint8_t *b = allocate_block(LINEX_NODE_SIZE, is_leaf(), current_block[0] & 0x1F);
        linex new_block(LINEX_NODE_SIZE, b, is_leaf());
        new_block.set_kv_last_pos(LX_BLK_HDR_SIZE);
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        int kv_last_pos = get_kv_last_pos();
        int half_kVLen = kv_last_pos / 2;

        int brk_idx = -1;
        int brk_kv_pos;
        int tot_len;
        brk_kv_pos = tot_len = 0;
        // Copy all data to new block in ascending order
        int new_idx;
        int src_idx = LX_BLK_HDR_SIZE;
        int dst_idx = src_idx;
        prefix_len = 0;
        prev_key_at = current_block + src_idx;
        for (new_idx = 0; new_idx < filled_sz; new_idx++) {
            for (int pctr = 0; prefix_len < current_block[src_idx]; pctr++)
                prefix[prefix_len++] = prev_key_at[pctr + 2];
            if (prefix_len > current_block[src_idx])
                prefix_len = current_block[src_idx];
            src_idx++;
            int kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            tot_len += kv_len;
            if (brk_idx == new_idx) {
                new_block.current_block[dst_idx++] = 0;
                new_block.current_block[dst_idx++] = current_block[src_idx] + prefix_len;
                memcpy(new_block.current_block + dst_idx, prefix, prefix_len);
                dst_idx += prefix_len;
                memcpy(new_block.current_block + dst_idx, current_block + src_idx + 1, kv_len - 1);
                dst_idx += kv_len;
                dst_idx--;
    #if LX_PREFIX_CODING == 0
                if (is_leaf()) {
                    prefix_len = 0;
                    for (int i = 0; current_block[src_idx + i + 1] == prev_key_at[i + 2]; i++) {
                        prefix[i] = prev_key_at[i + 2];
                        prefix_len++;
                    }
                }
    #endif
                memcpy(first_key, prefix, prefix_len);
                if (is_leaf()) {
    #if LX_PREFIX_CODING == 0
                    first_key[prefix_len] = current_block[src_idx + prefix_len + 1];
    #else
                    first_key[prefix_len] = current_block[src_idx + 1];
    #endif
                    *first_len_ptr = prefix_len + 1;
                } else {
                    *first_len_ptr = current_block[src_idx];
                    memcpy(first_key + prefix_len, current_block + src_idx + 1,
                            *first_len_ptr);
                    *first_len_ptr += prefix_len;
                }
                prefix_len = 0;
            } else {
                new_block.current_block[dst_idx++] = prefix_len;
                memcpy(new_block.current_block + dst_idx, current_block + src_idx, kv_len);
                dst_idx += kv_len;
            }
            if (brk_idx == -1) {
                if (tot_len > half_kVLen || new_idx == (filled_sz / 2)) {
                    brk_idx = new_idx + 1;
                    brk_kv_pos = dst_idx;
                }
            }
            prev_key_at = current_block + src_idx - 1;
            src_idx += kv_len;
            kv_last_pos = dst_idx;
        }

        int old_blk_new_len = brk_kv_pos - LX_BLK_HDR_SIZE;
        memcpy(current_block + LX_BLK_HDR_SIZE, new_block.current_block + LX_BLK_HDR_SIZE,
                old_blk_new_len); // Copy back first half to old block
        set_kv_last_pos(LX_BLK_HDR_SIZE + old_blk_new_len);
        set_filled_size(brk_idx); // Set filled upto for old block

        int new_size = filled_sz - brk_idx;
        // Move index of second half to first half in new block
        uint8_t *new_kv_idx = new_block.current_block + brk_kv_pos;
        memmove(new_block.current_block + LX_BLK_HDR_SIZE, new_kv_idx,
                kv_last_pos - brk_kv_pos); // Move first block to front
        // Set KV Last pos for new block
        new_block.set_kv_last_pos(LX_BLK_HDR_SIZE + kv_last_pos - brk_kv_pos);
        new_block.set_filled_size(new_size); // Set filled upto for new block

        return b;

    }

    bool is_full(int search_result) {
        int LINEX_NODE_SIZE = is_leaf() ? leaf_block_size : parent_block_size;
        if ((get_kv_last_pos() + key_len + value_len + 5) >= LINEX_NODE_SIZE)
            return true;
        return false;
    }

    inline int search_current_block() {
        pos = linear_search();
        return pos;
    }

    uint8_t *get_child_ptr_pos(int search_result) {
        if (search_result >= 0)
            key_at -= 2;
        if (search_result < 0)
            key_at = prev_key_at;
        return key_at + 1;
    }

    int get_header_size() {
        return LX_BLK_HDR_SIZE;
    }

    inline uint8_t *get_ptr_pos() {
        return NULL;
    }

    void set_current_block_root() {
        current_block = root_block;
        key_at = current_block + LX_BLK_HDR_SIZE;
        prefix_len = 0;
    }

    void set_current_block(uint8_t *m) {
        current_block = m;
        key_at = m + LX_BLK_HDR_SIZE;
        prefix_len = 0;
    }

    void init_current_block() {
        bplus_tree_handler<linex>::init_current_block();
        set_kv_last_pos(LX_BLK_HDR_SIZE);
    }

    void init_derived() {
    }

    void cleanup() {
    }

};

#endif
