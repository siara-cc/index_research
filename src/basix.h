#ifndef BASIX_H
#define BASIX_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#if BPT_9_BIT_PTR == 1
#define BLK_HDR_SIZE 14
#define BITMAP_POS 6
#else
#define BLK_HDR_SIZE 6
#endif

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class basix : public bplus_tree_handler<basix> {
public:
    int16_t pos;
    basix(uint16_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint16_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE) :
        bplus_tree_handler<basix>(leaf_block_sz, parent_block_sz) {
    }

    inline int16_t searchCurrentBlock() {
        int middle, first, filled_size;
        first = 0;
        filled_size = filledSize();
        while (first < filled_size) {
            middle = (first + filled_size) >> 1;
            key_at = getKey(middle, &key_at_len);
            int16_t cmp = util::compare((char *) key_at, key_at_len, key, key_len);
            if (cmp < 0)
                first = middle + 1;
            else if (cmp > 0)
                filled_size = middle;
            else
                return middle;
        }
        return ~filled_size;
    }

    inline byte *getChildPtrPos(int16_t idx) {
        if (idx < 0) {
            idx++;
            idx = ~idx;
        }
        return current_block + getPtr(idx);
    }

    inline byte *getPtrPos() {
        return current_block + BLK_HDR_SIZE;
    }

    inline int getHeaderSize() {
        return BLK_HDR_SIZE;
    }

    bool isFull() {
        int16_t ptr_size = filledSize() + 1;
    #if BPT_9_BIT_PTR == 0
        ptr_size <<= 1;
    #endif
        if (getKVLastPos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2))
            return true;
    #if BPT_9_BIT_PTR == 1
        if (filledSize() > 62)
        return true;
    #endif
        return false;
    }

    byte *split(byte *first_key, int16_t *first_len_ptr) {
        int16_t orig_filled_size = filledSize();
        const uint16_t BASIX_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
        byte *b = (byte *) util::alignedAlloc(BASIX_NODE_SIZE);
        basix new_block;
        new_block.setCurrentBlock(b);
        new_block.initCurrentBlock();
        //new_block.setKVLastPos(BASIX_NODE_SIZE);
        if (!isLeaf())
            new_block.setLeaf(false);
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
            int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + BASIX_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            setKVLastPos(BASIX_NODE_SIZE - old_blk_new_len);
            setFilledSize(brk_idx);
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
            int16_t new_size = orig_filled_size - brk_idx;
            byte *block_ptrs = new_block.current_block + BLK_HDR_SIZE;
    #if BPT_9_BIT_PTR == 1
            memmove(block_ptrs, block_ptrs + brk_idx, new_size);
    #else
            memmove(block_ptrs, block_ptrs + (brk_idx << 1), new_size << 1);
    #endif
            new_block.setKVLastPos(brk_kv_pos);
            new_block.setFilledSize(new_size);
        }

        return b;
    }

    void addData(int16_t idx) {

        uint16_t kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
        setKVLastPos(kv_last_pos);
        current_block[kv_last_pos] = key_len & 0xFF;
        memcpy(current_block + kv_last_pos + 1, key, key_len);
        current_block[kv_last_pos + key_len + 1] = value_len & 0xFF;
        memcpy(current_block + kv_last_pos + key_len + 2, value, value_len);
        insPtr(idx, kv_last_pos);
        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

    }

    void addFirstData() {
        addData(0);
    }

};

#endif
