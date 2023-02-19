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
        current_block = (uint8_t *) util::aligned_alloc(block_size);
        set_leaf(1);
        set_filled_size(0);
        BPT_MAX_KEY_LEN = 1;
        set_kv_last_pos(block_size);
    }

    ~bas_blk() {
        free(current_block);
    }

    inline bool is_leaf() {
        return BPT_IS_LEAF_BYTE;
    }

    inline void set_leaf(char is_leaf) {
        if (is_leaf) {
            current_block[0] = 0x80;
        } else
            current_block[0] &= 0x7F;
    }

    inline uint8_t *get_key(int16_t pos, uint8_t *plen) {
        uint8_t *kv_idx = current_block + get_ptr(pos);
        *plen = *kv_idx;
        return kv_idx + 1;
    }

    inline int get_ptr(int16_t pos) {
#if BPT_9_BIT_PTR == 1
        uint16_t ptr = *(get_ptr_pos() + pos);
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
        return util::get_int(get_ptr_pos() + (pos << 1));
#endif
    }

    inline int16_t filled_size() {
        return util::get_int(BPT_FILLED_SIZE);
    }

    inline uint16_t get_kv_last_pos() {
        return util::get_int(BPT_LAST_DATA_PTR);
    }

    inline int16_t search_current_block() {
        int middle, first, filled_sz;
        first = 0;
        filled_sz = filled_size();
        while (first < filled_sz) {
            middle = (first + filled_sz) >> 1;
            key_at = get_key(middle, &key_at_len);
            int16_t cmp = util::compare(key_at, key_at_len, key, key_len);
            if (cmp < 0)
                first = middle + 1;
            else if (cmp > 0)
                filled_sz = middle;
            else
                return middle;
        }
        return ~filled_sz;
    }

    inline uint8_t *get_ptr_pos() {
        return current_block + BLK_HDR_SIZE;
    }

    inline int get_header_size() {
        return BLK_HDR_SIZE;
    }

    void remove_entry(int16_t pos) {
        del_ptr(pos);
    }

    void make_space() {
        const uint16_t data_size = block_size - get_kv_last_pos();
        uint8_t data_buf[data_size];
        uint16_t new_data_len = 0;
        int16_t new_idx;
        int16_t orig_filled_size = filled_size();
        for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
            uint16_t src_idx = get_ptr(new_idx);
            uint16_t kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            new_data_len += kv_len;
            memcpy(data_buf + data_size - new_data_len, current_block + src_idx, kv_len);
            set_ptr(new_idx, block_size - new_data_len);
        }
        uint16_t new_kv_last_pos = block_size - new_data_len;
        memcpy(current_block + new_kv_last_pos, data_buf + data_size - new_data_len, new_data_len);
        //printf("%d, %d\n", data_size, new_data_len);
        set_kv_last_pos(new_kv_last_pos);
        search_current_block();
    }

    bool is_full(int16_t search_result) {
        int16_t ptr_size = filled_size() + 1;
    #if BPT_9_BIT_PTR == 0
        ptr_size <<= 1;
    #endif
        if (get_kv_last_pos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2)) {
            make_space();
            if (get_kv_last_pos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2))
                return true;
        }
    #if BPT_9_BIT_PTR == 1
        if (filled_size() > 62)
        return true;
    #endif
        return false;
    }

    void add_data(int16_t search_result) {

        uint16_t kv_last_pos = get_kv_last_pos() - (key_len + value_len + 2);
        set_kv_last_pos(kv_last_pos);
        uint8_t *ptr = current_block + kv_last_pos;
        *ptr++ = key_len;
        memcpy(ptr, key, key_len);
        ptr += key_len;
        *ptr++ = value_len;
        memcpy(ptr, value, value_len);
        ins_ptr(search_result, kv_last_pos);
        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

    }

    void add_first_data() {
        add_data(0);
    }

    inline void set_filled_size(int16_t filled_size) {
        util::set_int(BPT_FILLED_SIZE, filled_size);
    }

    inline void ins_ptr(int16_t pos, uint16_t kv_pos) {
        int16_t filled_sz = filled_size();
#if BPT_9_BIT_PTR == 1
        uint8_t *kv_idx = get_ptr_pos() + pos;
        memmove(kv_idx + 1, kv_idx, filled_sz - pos);
        *kv_idx = kv_pos;
#if BPT_INT64MAP == 1
        ins_bit(bitmap, pos, kv_pos);
#else
        if (pos & 0xFFE0) {
            ins_bit(bitmap2, pos - 32, kv_pos);
        } else {
            uint8_t last_bit = (*bitmap1 & 0x01);
            ins_bit(bitmap1, pos, kv_pos);
            *bitmap2 >>= 1;
            if (last_bit)
            *bitmap2 |= MASK32(0);
        }
#endif
#else
        uint8_t *kv_idx = get_ptr_pos() + (pos << 1);
        memmove(kv_idx + 2, kv_idx, (filled_sz - pos) * 2);
        util::set_int(kv_idx, kv_pos);
#endif
        set_filled_size(filled_sz + 1);

    }

    inline void del_ptr(int16_t pos) {
        int16_t filled_sz = filled_size() - 1;
#if BPT_9_BIT_PTR == 1
        uint8_t *kv_idx = get_ptr_pos() + pos;
        memmove(kv_idx, kv_idx + 1, filled_sz - pos);
#if BPT_INT64MAP == 1
        del_bit(bitmap, pos);
#else
        if (pos & 0xFFE0) {
            del_bit(bitmap2, pos - 32);
        } else {
            uint8_t first_bit = (*bitmap2 >> 7);
            del_bit(bitmap1, pos);
            *bitmap2 <<= 1;
            if (first_bit)
                *bitmap1 |= 0x01;
        }
#endif
#else
        uint8_t *kv_idx = get_ptr_pos() + (pos << 1);
        memmove(kv_idx, kv_idx + 2, (filled_sz - pos) * 2);
#endif
        set_filled_size(filled_sz);

    }

    inline void set_ptr(int16_t pos, uint16_t ptr) {
#if BPT_9_BIT_PTR == 1
        *(get_ptr_pos() + pos) = ptr;
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
        uint8_t *kv_idx = get_ptr_pos() + (pos << 1);
        return util::set_int(kv_idx, ptr);
#endif
    }

    inline void set_kv_last_pos(uint16_t val) {
        util::set_int(BPT_LAST_DATA_PTR, val);
    }

    inline void ins_bit(uint32_t *ui32, int pos, uint16_t kv_pos) {
        uint32_t ryte_part = (*ui32) & RYTE_MASK32(pos);
        ryte_part >>= 1;
        if (kv_pos >= 256)
            ryte_part |= MASK32(pos);
        (*ui32) = (ryte_part | ((*ui32) & LEFT_MASK32(pos)));

    }

    inline void del_bit(uint32_t *ui32, int pos) {
        uint32_t ryte_part = (*ui32) & RYTE_MASK32(pos);
        ryte_part <<= 1;
        (*ui32) = (ryte_part | ((*ui32) & LEFT_MASK32(pos)));
    }

    inline uint8_t *get_value_at(uint8_t *key_ptr, int16_t *vlen) {
        key_ptr += *(key_ptr - 1);
        if (vlen != NULL)
          *vlen = *key_ptr;
        return (uint8_t *) key_ptr + 1;
    }

    inline uint8_t *get_value_at(int16_t *vlen) {
        if (vlen != NULL)
          *vlen = key_at[key_at_len];
        return (uint8_t *) key_at + key_at_len + 1;
    }

    uint8_t *put(const uint8_t *key, uint8_t key_len, const uint8_t *value,
            int16_t value_len, int16_t *p_value_len = NULL) {
        this->key = key;
        this->key_len = key_len;
        this->value = value;
        this->value_len = value_len;
        if (filled_size() == 0) {
            add_first_data();
        } else {
            int16_t search_result = search_current_block();
            if (search_result >= 0)
                return get_value_at(p_value_len);
            if (!is_full(search_result))
                add_data(~search_result);
            else
                cout << "Full" << endl;
        }
        return NULL;
    }

    uint8_t *get(const uint8_t *key, uint8_t key_len, int16_t *p_value_len) {
        this->key = key;
        this->key_len = key_len;
        if (search_current_block() < 0)
            return NULL;
        return get_value_at(p_value_len);
    }

};

#endif
