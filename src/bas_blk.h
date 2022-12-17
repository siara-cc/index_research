#ifndef BAS_BLK_H
#define BAS_BLK_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "univix_util.h"

using namespace std;

#if BPT_9_BIT_PTR == 1
#define BLK_HDR_SIZE 14
#define BITMAP_POS 6
#else
#define BLK_HDR_SIZE 6
#endif

#define BPT_IS_LEAF_BYTE current_block[0] & 0x80
#define BPT_IS_CHANGED current_block[0] & 0x40
#define BPT_LEVEL (current_block[0] & 0x1F)
#define BPT_FILLED_SIZE current_block + 1
#define BPT_LAST_DATA_PTR current_block + 3
#define BPT_MAX_KEY_LEN current_block[5]
#define BPT_TRIE_LEN_PTR current_block + 6
#define BPT_TRIE_LEN current_block[7]
#define BPT_MAX_PFX_LEN current_block[8]

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class bas_blk {
public:
    const uint8_t *key;
    uint8_t key_len;
    uint8_t *key_at;
    uint8_t key_at_len;
    const uint8_t *value;
    int16_t value_len;
#if BPT_9_BIT_PTR == 1
#if BPT_INT64MAP == 1
    uint64_t *bitmap;
#else
    uint32_t *bitmap1;
    uint32_t *bitmap2;
#endif
#endif
    int16_t pos;
    uint8_t *current_block;
    int block_size;

    bas_blk(uint16_t block_sz) {
        block_size = block_sz;
        current_block = (uint8_t *) util::alignedAlloc(block_size);
        setLeaf(1);
        setFilledSize(0);
        BPT_MAX_KEY_LEN = 1;
        setKVLastPos(block_size);
    }

    ~bas_blk() {
        free(current_block);
    }

    inline bool isLeaf() {
        return BPT_IS_LEAF_BYTE;
    }

    inline void setLeaf(char isLeaf) {
        if (isLeaf) {
            current_block[0] = 0x80;
        } else
            current_block[0] &= 0x7F;
    }

    inline uint8_t *getKey(int16_t pos, uint8_t *plen) {
        uint8_t *kvIdx = current_block + getPtr(pos);
        *plen = *kvIdx;
        return kvIdx + 1;
    }

    inline int getPtr(int16_t pos) {
#if BPT_9_BIT_PTR == 1
        uint16_t ptr = *(getPtrPos() + pos);
#if BPT_INT64MAP == 1
        if (*bitmap & MASK64(pos))
        ptr |= 256;
#else
        if (pos & 0xFFE0) {
            if (*bitmap2 & MASK32(pos - 32))
            ptr |= 256;
        } else {
            if (*bitmap1 & MASK32(pos))
            ptr |= 256;
        }
#endif
        return ptr;
#else
        return util::getInt(getPtrPos() + (pos << 1));
#endif
    }

    inline int16_t filledSize() {
        return util::getInt(BPT_FILLED_SIZE);
    }

    inline uint16_t getKVLastPos() {
        return util::getInt(BPT_LAST_DATA_PTR);
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
    #if BPT_9_BIT_PTR == 0
        ptr_size <<= 1;
    #endif
        if (getKVLastPos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2)) {
            makeSpace();
            if (getKVLastPos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2))
                return true;
        }
    #if BPT_9_BIT_PTR == 1
        if (filledSize() > 62)
        return true;
    #endif
        return false;
    }

    void addData(int16_t search_result) {

        uint16_t kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
        setKVLastPos(kv_last_pos);
        uint8_t *ptr = current_block + kv_last_pos;
        *ptr++ = key_len;
        memcpy(ptr, key, key_len);
        ptr += key_len;
        *ptr++ = value_len;
        memcpy(ptr, value, value_len);
        insPtr(search_result, kv_last_pos);
        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

    }

    void addFirstData() {
        addData(0);
    }

    inline void setFilledSize(int16_t filledSize) {
        util::setInt(BPT_FILLED_SIZE, filledSize);
    }

    inline void insPtr(int16_t pos, uint16_t kv_pos) {
        int16_t filledSz = filledSize();
#if BPT_9_BIT_PTR == 1
        uint8_t *kvIdx = getPtrPos() + pos;
        memmove(kvIdx + 1, kvIdx, filledSz - pos);
        *kvIdx = kv_pos;
#if BPT_INT64MAP == 1
        insBit(bitmap, pos, kv_pos);
#else
        if (pos & 0xFFE0) {
            insBit(bitmap2, pos - 32, kv_pos);
        } else {
            uint8_t last_bit = (*bitmap1 & 0x01);
            insBit(bitmap1, pos, kv_pos);
            *bitmap2 >>= 1;
            if (last_bit)
            *bitmap2 |= MASK32(0);
        }
#endif
#else
        uint8_t *kvIdx = getPtrPos() + (pos << 1);
        memmove(kvIdx + 2, kvIdx, (filledSz - pos) * 2);
        util::setInt(kvIdx, kv_pos);
#endif
        setFilledSize(filledSz + 1);

    }

    inline void delPtr(int16_t pos) {
        int16_t filledSz = filledSize();
#if BPT_9_BIT_PTR == 1
        uint8_t *kvIdx = getPtrPos() + pos;
        memmove(kvIdx, kvIdx + 1, filledSz - pos);
#if BPT_INT64MAP == 1
        delBit(bitmap, pos);
#else
        if (pos & 0xFFE0) {
            delBit(bitmap2, pos - 32);
        } else {
            uint8_t first_bit = (*bitmap2 >> 7);
            delBit(bitmap1, pos);
            *bitmap2 <<= 1;
            if (first_bit)
                *bitmap1 |= 0x01;
        }
#endif
#else
        uint8_t *kvIdx = getPtrPos() + (pos << 1);
        memmove(kvIdx, kvIdx + 2, (filledSz - pos) * 2);
#endif
        setFilledSize(filledSz - 1);

    }

    inline void setPtr(int16_t pos, uint16_t ptr) {
#if BPT_9_BIT_PTR == 1
        *(getPtrPos() + pos) = ptr;
#if BPT_INT64MAP == 1
        if (ptr >= 256)
        *bitmap |= MASK64(pos);
        else
        *bitmap &= ~MASK64(pos);
#else
        if (pos & 0xFFE0) {
            pos -= 32;
            if (ptr >= 256)
            *bitmap2 |= MASK32(pos);
            else
            *bitmap2 &= ~MASK32(pos);
        } else {
            if (ptr >= 256)
            *bitmap1 |= MASK32(pos);
            else
            *bitmap1 &= ~MASK32(pos);
        }
#endif
#else
        uint8_t *kvIdx = getPtrPos() + (pos << 1);
        return util::setInt(kvIdx, ptr);
#endif
    }

    inline void setKVLastPos(uint16_t val) {
        util::setInt(BPT_LAST_DATA_PTR, val);
    }

    inline void insBit(uint32_t *ui32, int pos, uint16_t kv_pos) {
        uint32_t ryte_part = (*ui32) & RYTE_MASK32(pos);
        ryte_part >>= 1;
        if (kv_pos >= 256)
            ryte_part |= MASK32(pos);
        (*ui32) = (ryte_part | ((*ui32) & LEFT_MASK32(pos)));

    }

    inline void delBit(uint32_t *ui32, int pos) {
        uint32_t ryte_part = (*ui32) & RYTE_MASK32(pos);
        ryte_part <<= 1;
        (*ui32) = (ryte_part | ((*ui32) & LEFT_MASK32(pos)));
    }

    inline uint8_t *getValueAt(uint8_t *key_ptr, int16_t *vlen) {
        key_ptr += *(key_ptr - 1);
        *vlen = *key_ptr;
        return (uint8_t *) key_ptr + 1;
    }

    inline uint8_t *getValueAt(int16_t *vlen) {
        key_at += key_at_len;
        *vlen = *key_at;
        return (uint8_t *) key_at + 1;
    }

    uint8_t *put(const uint8_t *key, uint8_t key_len, const uint8_t *value,
            int16_t value_len, int16_t *pValueLen = NULL) {
        this->key = key;
        this->key_len = key_len;
        this->value = value;
        this->value_len = value_len;
        if (filledSize() == 0) {
            addFirstData();
        } else {
            int16_t search_result = searchCurrentBlock();
            if (search_result >= 0 && pValueLen != NULL)
                return getValueAt(pValueLen);
            if (!isFull(search_result))
                addData(~search_result);
        }
        return NULL;
    }

    uint8_t *get(const uint8_t *key, uint8_t key_len, int16_t *pValueLen) {
        this->key = key;
        this->key_len = key_len;
        if (searchCurrentBlock() < 0)
            return NULL;
        return getValueAt(pValueLen);
    }

};

#endif
