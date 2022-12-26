#ifndef SQLITE_H
#define SQLITE_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define BLK_HDR_SIZE 6

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class sqlite : public bplus_tree_handler<sqlite> {
public:
    int16_t pos;
    sqlite(uint16_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint16_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, uint8_t *block = NULL) :
        bplus_tree_handler<sqlite>(leaf_block_sz, parent_block_sz, cache_sz, fname, block) {
    }

    inline void setCurrentBlockRoot() {
        setCurrentBlock(root_block);
    }

    inline void setCurrentBlock(uint8_t *m) {
        current_block = m;
    }

    inline int16_t searchCurrentBlock() {
        int middle, first, filled_size;
        first = 0;
        filled_size = filledSize();
        while (first < filled_size) {
            middle = (first + filled_size) >> 1;
            key_at = getKey(middle, &key_at_len);
            int16_t cmp = util::compare(key_at, key_at_len, key, key_len);
            if (cmp < 0)
                first = middle + 1;
            else if (cmp > 0)
                filled_size = middle;
            else
                return middle;
        }
        return ~filled_size;
    }

    inline uint8_t *getChildPtrPos(int16_t search_result) {
        if (search_result < 0) {
            search_result++;
            search_result = ~search_result;
        }
        return current_block + getPtr(search_result);
    }

    inline uint8_t *getPtrPos() {
        return current_block + BLK_HDR_SIZE;
    }

    inline int getHeaderSize() {
        return BLK_HDR_SIZE;
    }

    void remove_entry(int16_t pos) {
        delPtr(pos);
    }

    void makeSpace() {
        int block_size = (isLeaf() ? leaf_block_size : parent_block_size);
        int lvl = current_block[0] & 0x1F;
        const uint16_t data_size = block_size - getKVLastPos();
        uint8_t data_buf[data_size];
        uint16_t new_data_len = 0;
        int16_t new_idx;
        int16_t orig_filled_size = filledSize();
        for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
            uint16_t src_idx = getPtr(new_idx);
            uint16_t kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            new_data_len += kv_len;
            memcpy(data_buf + data_size - new_data_len, current_block + src_idx, kv_len);
            setPtr(new_idx, block_size - new_data_len);
        }
        uint16_t new_kv_last_pos = block_size - new_data_len;
        memcpy(current_block + new_kv_last_pos, data_buf + data_size - new_data_len, new_data_len);
        //printf("%d, %d\n", data_size, new_data_len);
        setKVLastPos(new_kv_last_pos);
        searchCurrentBlock();
    }

    bool isFull(int16_t search_result) {
        int16_t ptr_size = filledSize() + 1;
        if (getKVLastPos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2)) {
            makeSpace();
            if (getKVLastPos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2))
                return true;
        }
        return false;
    }

    uint8_t *split(uint8_t *first_key, int16_t *first_len_ptr) {
        int16_t orig_filled_size = filledSize();
        uint32_t BASIX_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
        int lvl = current_block[0] & 0x1F;
        uint8_t *b = allocateBlock(BASIX_NODE_SIZE, isLeaf(), lvl);
        sqlite new_block(this->leaf_block_size, this->parent_block_size, 0, NULL, b);
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        uint16_t kv_last_pos = getKVLastPos();
        uint16_t halfKVLen = BASIX_NODE_SIZE - kv_last_pos + 1;
        halfKVLen /= 2;

        int16_t brk_idx = -1;
        uint16_t brk_kv_pos;
        uint16_t tot_len;
        brk_kv_pos = tot_len = 0;
        // Copy all data to new block in ascending order
        int16_t new_idx;
        for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
            uint16_t src_idx = getPtr(new_idx);
            uint16_t kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            tot_len += kv_len;
            memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
            new_block.insPtr(new_idx, kv_last_pos);
            kv_last_pos += kv_len;
            if (brk_idx == -1) {
                if (tot_len > halfKVLen || new_idx == (orig_filled_size / 2)) {
                    brk_idx = new_idx + 1;
                    brk_kv_pos = kv_last_pos;
                    uint16_t first_idx = getPtr(new_idx + 1);
                    if (isLeaf()) {
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
        kv_last_pos = getKVLastPos();
        uint16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
        memcpy(current_block + BASIX_NODE_SIZE - old_blk_new_len, new_block.current_block + kv_last_pos,
                old_blk_new_len); // Copy back first half to old block
        //memset(new_block.current_block + kv_last_pos, '\0', old_blk_new_len);
        int diff = (BASIX_NODE_SIZE - brk_kv_pos);
        for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
            setPtr(new_idx, new_block.getPtr(new_idx) + diff);
        } // Set index of copied first half in old block

        {
            uint16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + BASIX_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            setKVLastPos(BASIX_NODE_SIZE - old_blk_new_len);
            setFilledSize(brk_idx);
        }

        {
            int16_t new_size = orig_filled_size - brk_idx;
            uint8_t *block_ptrs = new_block.current_block + BLK_HDR_SIZE;
            new_block.setKVLastPos(brk_kv_pos);
            new_block.setFilledSize(new_size);
        }

        return b;
    }

    void addData(int16_t search_result) {

        uint16_t kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
        if ((*current_block & 0x1F) == BPT_STAGING_LVL)
            kv_last_pos--;
        setKVLastPos(kv_last_pos);
        uint8_t *ptr = current_block + kv_last_pos;
        *ptr++ = key_len;
        memcpy(ptr, key, key_len);
        ptr += key_len;
        *ptr++ = value_len;
        memcpy(ptr, value, value_len);
        if ((*current_block & 0x1F) == BPT_STAGING_LVL) {
            ptr += value_len;
            *ptr = 1;
        }
        insPtr(search_result, kv_last_pos);
        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

    }

    void addFirstData() {
        addData(0);
    }

};

#endif