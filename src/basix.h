#ifndef BASIX_H
#define BASIX_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

#if BPT_9_BIT_PTR == 1
#define BLK_HDR_SIZE 14
#define BITMAP_POS 6
#else
#define BLK_HDR_SIZE 6
#endif

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class basix : public bplus_tree_handler<basix> {
public:
    basix(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, const uint8_t opts = 0) :
        bplus_tree_handler<basix>(leaf_block_sz, parent_block_sz, cache_sz, fname, 0, false, opts) {
    }

    basix(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bplus_tree_handler<basix>(block_sz, block, is_leaf) {
        init_stats();
    }

    basix(const char *filename, int blk_size, int page_resv_bytes, const uint8_t opts) :
       bplus_tree_handler<basix>(filename, blk_size, page_resv_bytes, opts) {
        init_stats();
    }

    inline void set_current_block_root() {
        set_current_block(root_block);
    }

    inline void set_current_block(uint8_t *m) {
        current_block = m;
#if BPT_9_BIT_PTR == 1
#if BPT_INT64MAP == 1
        bitmap = (uint64_t *) (current_block + get_header_size() - 8);
#else
        bitmap1 = (uint32_t *) (current_block + get_header_size() - 8);
        bitmap2 = bitmap1 + 1;
#endif
#endif
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

    inline uint8_t *get_ptr_pos() {
        return current_block + BLK_HDR_SIZE;
    }

    inline int get_header_size() {
        return BLK_HDR_SIZE;
    }

    void remove_entry(int pos) {
        del_ptr(pos);
        set_changed(1);
    }

    void remove_found_entry(bptree_iter_ctx *ctx) {
        if (ctx->found_page_pos[ctx->last_page_lvl] != -1) {
            del_ptr(ctx->found_page_pos[ctx->last_page_lvl]);
            set_changed(1);
        }
        total_size--;
    }

    void make_space() {
        int block_size = (is_leaf() ? block_size : parent_block_size);
        int orig_filled_size = filled_size();
        if (orig_filled_size == 0) {
            set_kv_last_pos(block_size == 65536 ? 65535 : block_size);
            return;
        }
        int lvl = current_block[0] & 0x1F;
        const int data_size = block_size - get_kv_last_pos();
        if (data_size < 3) {
            std::cout << "data size 0" << std::endl;
            return;
        }
        uint8_t *data_buf = (uint8_t *) malloc(data_size);
        int new_data_len = 0;
        int new_idx;
        for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
            int src_idx = get_ptr(new_idx);
            int kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            new_data_len += kv_len;
            memcpy(data_buf + data_size - new_data_len, current_block + src_idx, kv_len);
            set_ptr(new_idx, block_size - new_data_len);
        }
        int new_kv_last_pos = block_size - new_data_len;
        memcpy(current_block + new_kv_last_pos, data_buf + data_size - new_data_len, new_data_len);
        //printf("%d, %d\n", data_size, new_data_len);
        free(data_buf);
        set_kv_last_pos(new_kv_last_pos);
        search_current_block();
    }

    bool is_full(int search_result) {
        int ptr_size = filled_size() + 1;
    #if BPT_9_BIT_PTR == 0
        ptr_size <<= 1;
    #endif
        if (get_kv_last_pos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2)) {
            // if (!is_leaf() && to_demote_blocks)
            //     demote_blocks();
            // if (*current_block & 0x20) {
            //     make_space();
            //     *current_block &= 0xDF;
            //     if (get_kv_last_pos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2))
            //         return true;
            // } else
            	return true;
        }
    #if BPT_9_BIT_PTR == 1
        if (filled_size() > 62)
        return true;
    #endif
        return false;
    }

    void demote_blocks() {
        //if (current_block[5] % 8 > 0)
        //    return;
        if ((current_block[0] & 0x1F) <= BPT_PARENT0_LVL)
            return;
        int filled_sz = filled_size();
        int total = 0;
        int count = 0;
        int less_count = 0;
        int demoted_count = 0;
        std::cout << "Start: " << current_page << ": " << std::endl;
        for (int pos = 0; pos < filled_sz; pos++) {
            int src_idx = get_ptr(pos);
            uint8_t *kv = current_block + src_idx;
            kv += *kv;
            kv++;
            kv += *kv;
            //cout << (int) *kv << " ";
            int split_count = *kv;
            //cout << split_count << ", ";
            if (split_count == 255)
                continue;
            //total += split_count;
            //count++;
            //if (*kv == 0) {
                // *kv = 255;
            //    less_count++;
            //}// else
                // *kv = 0;
        }
            for (int pos = 0; pos < filled_sz; pos++) {
                int src_idx = get_ptr(pos);
                uint8_t *kv = current_block + src_idx;
                kv += *kv;
                kv++;
                kv += *kv;
                std::cout << (int) *kv << " ";
            }
            std::cout << std::endl;
        //cout << endl;
        if (total == 0)
            total = 1;
        if (count == 0)
            count = total;
        int mean = total / count;
        /*for (int pos = 0; pos < filled_size; pos++) {
            int src_idx = get_ptr(pos);
            uint8_t *kv = current_block + src_idx;
            kv += *kv;
            kv++;
            kv += *kv;
            if (*kv == 255)
                continue;
            if (*kv < 128) {
                *kv = 255;
                less_count++;
            } else
                *kv = 0;
        }*/
        if (less_count == 0) {
            /*for (int pos = 0; pos < filled_size; pos++) {
                int src_idx = get_ptr(pos);
                uint8_t *kv = current_block + src_idx;
                kv += *kv;
                kv++;
                kv += *kv;
                cout << (int) *kv << " ";
            }
            cout << endl;*/
        } else {
            std::cout << "Less count: " << less_count << ", tot: " << filled_sz << ", mean: " << mean
                 << ", dcount: " << demoted_count << ", lvl: " << (int) (current_block[0] & 0x1F) << std::endl;
        }
    }

    uint8_t *find_split_source(int search_result) {
        if (search_result < 0)
            search_result = ~search_result;
        if (search_result == 0) {
            std::cout << "New split block can never be at pos 0" << std::endl;
            return NULL;
        }
        int ptr_pos = get_ptr(search_result - 1);
        uint8_t *ret = current_block + ptr_pos;
        ret += *ret;
        ret++;
        ret += *ret;
        return ret;
    }

    uint8_t *split(uint8_t *first_key, int *first_len_ptr) {
        int orig_filled_size = filled_size();
        uint32_t BASIX_NODE_SIZE = is_leaf() ? block_size : parent_block_size;
        int lvl = current_block[0] & 0x1F;
        uint8_t *b = allocate_block(BASIX_NODE_SIZE, is_leaf(), lvl);
        basix new_block(BASIX_NODE_SIZE, b, is_leaf());
        if (BPT_MAX_KEY_LEN < 255)
            BPT_MAX_KEY_LEN++;
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        int kv_last_pos = get_kv_last_pos();
        int half_kVLen = BASIX_NODE_SIZE - kv_last_pos + 1;
        half_kVLen /= 2;

        int brk_idx = -1;
        int brk_kv_pos;
        int tot_len;
        brk_kv_pos = tot_len = 0;
        // Copy all data to new block in ascending order
        int new_idx;
        for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
            int src_idx = get_ptr(new_idx);
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
                    int first_idx = get_ptr(new_idx + 1);
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
        //memset(current_block + BLK_HDR_SIZE, '\0', BASIX_NODE_SIZE - BLK_HDR_SIZE);
        kv_last_pos = get_kv_last_pos();
        //memset(new_block.current_block + kv_last_pos, '\0', old_blk_new_len);
        int diff = (BASIX_NODE_SIZE - brk_kv_pos);
        for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
            set_ptr(new_idx, new_block.get_ptr(new_idx) + diff);
        } // Set index of copied first half in old block

        {
            int old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + BASIX_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            set_kv_last_pos(BASIX_NODE_SIZE - old_blk_new_len);
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
            uint8_t *block_ptrs = new_block.current_block + BLK_HDR_SIZE;
    #if BPT_9_BIT_PTR == 1
            memmove(block_ptrs, block_ptrs + brk_idx, new_size);
    #else
            memmove(block_ptrs, block_ptrs + (brk_idx << 1), new_size << 1);
    #endif
            new_block.set_kv_last_pos(brk_kv_pos);
            new_block.set_filled_size(new_size);
        }

        return b;
    }

    uint8_t *add_data(int search_result) {

        int kv_last_pos = get_kv_last_pos() - (key_len + value_len + 2);
        set_kv_last_pos(kv_last_pos);
        uint8_t *ptr = current_block + kv_last_pos;
        *ptr++ = key_len;
        memcpy(ptr, key, key_len);
        ptr += key_len;
        *ptr++ = value_len;
        memcpy(ptr, value, value_len);
        ins_ptr(search_result, kv_last_pos);
        //if (BPT_MAX_KEY_LEN < key_len)
        //    BPT_MAX_KEY_LEN = key_len;
        return ptr;

    }

    void add_first_data() {
        add_data(0);
    }

    void init_derived() {
    }

    void cleanup() {
    }

    uint8_t *next_rec(bptree_iter_ctx *ctx, uint8_t *val_buf, int *val_buf_len) {
        if (ctx->found_page_pos[ctx->last_page_lvl] < 0)
            ctx->found_page_pos[ctx->last_page_lvl] = ~ctx->found_page_pos[ctx->last_page_lvl];
        uint8_t *target_block;
        if (cache_size > 0)
            target_block = cache->get_disk_page_in_cache(ctx->pages[ctx->last_page_lvl].page);
        else
            target_block = ctx->pages[ctx->last_page_lvl].ptr;
        int filled_sz = util::get_int(target_block + 1);
        uint8_t *ret;
        int next_pos;
        if (ctx->found_page_pos[ctx->last_page_lvl] < filled_sz) {
            next_pos = ctx->found_page_pos[ctx->last_page_lvl];
            ret = target_block + util::get_int(target_block + BLK_HDR_SIZE + (next_pos << 1));
            ctx->found_page_pos[ctx->last_page_lvl]++;
        } else {
            int lvl = ctx->last_page_lvl - 1;
            do {
                if (ctx->found_page_pos[lvl] < 0)
                    ctx->found_page_pos[lvl] = ~ctx->found_page_pos[lvl];
                ctx->found_page_pos[lvl]++;
                if (cache_size > 0)
                    target_block = cache->get_disk_page_in_cache(ctx->pages[lvl].page);
                else
                    target_block = ctx->pages[lvl].ptr;
                filled_sz = util::get_int(target_block + 1);
                next_pos = ctx->found_page_pos[lvl];
                lvl--;
            } while (lvl >= 0 && next_pos >= filled_sz);
            if (lvl < 0 && next_pos >= filled_sz)
                return NULL;
            lvl++;
            while (lvl < ctx->last_page_lvl) {
                next_pos = ctx->found_page_pos[lvl];
                uint8_t *child_ptr_loc = target_block + util::get_int(target_block + BLK_HDR_SIZE + (next_pos << 1));
                lvl++;
                if (cache_size > 0) {
                    ctx->pages[lvl].page = get_child_page(child_ptr_loc);
                    target_block = cache->get_disk_page_in_cache(ctx->pages[lvl].page);
                } else {
                    ctx->pages[lvl].ptr = get_child_ptr(child_ptr_loc);
                    target_block = ctx->pages[lvl].ptr;
                }
                ctx->found_page_pos[lvl] = 0;
            }
            next_pos = 0;
            ret = target_block + util::get_int(target_block + BLK_HDR_SIZE + (next_pos << 1));
            ctx->found_page_pos[lvl] = 1;
        }
        return ret;
    }

};

#endif
