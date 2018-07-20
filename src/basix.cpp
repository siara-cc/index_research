#include "basix.h"

byte *basix::getChildPtrPos(int16_t idx) {
    if (idx < 0) {
        idx++;
        idx = ~idx;
    }
    return current_block + getPtr(idx);
}

int basix::getHeaderSize() {
    return BLK_HDR_SIZE;
}

//int16_t basix::binarySearch(const char *key, int16_t key_len) {
//    int middle, filled_size;
//    filled_size = filledSize() - 1;
//    middle = util::roots[filled_size];
//    do {
//        int16_t middle_key_len;
//        char *middle_key = (char *) getKey(middle, &middle_key_len);
//        int16_t cmp = util::compare(middle_key, middle_key_len, key, key_len);
//        if (cmp < 0) {
//            middle = util::ryte[middle];
//            while (middle > filled_size)
//                middle = util::left[middle];
//        } else if (cmp > 0)
//            middle = util::left[middle];
//        else
//            return middle;
//    } while (middle >= 0);
//    return middle;
//}

int16_t basix::binarySearch(const char *key, int16_t key_len) {
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

// branch ffree
//int16_t basix::binarySearchLeaf(const char *key, int16_t key_len) {
//    int middle, first, filled_size;
//    int16_t cmp = 1;
//    first = 0;
//    filled_size = filledSize() + 1;
//    while (cmp && first < filled_size) {
//        middle = (first + filled_size) / 2;
//        int16_t middle_key_len;
//        char *middle_key = (char *) getKey(middle, &middle_key_len);
//        cmp = util::compare(middle_key, middle_key_len, key, key_len);
//        first = ((cmp < 0) ? middle + 1 : first);
//        filled_size = ((cmp > 0) ? middle : filled_size);
//    }
//    return (first < filled_size) ? middle : ~filled_size;
//}
//int16_t basix::binarySearchLeaf(const char *key, int16_t key_len) {
//    int16_t n;
//    char cmp;
//    int16_t *base = (int16_t *) (current_block + BLK_HDR_SIZE);
//    int16_t *new_base = base;
//    n = filledSize() + 1;
//    int16_t half;
//    while (n > 1) {
//        half = n / 2;
//        char *middle_key = (char *) (current_block + new_base[half]);
//        int16_t middle_key_len = *middle_key;
//        middle_key++;
//        cmp = util::compare(middle_key, middle_key_len, key, key_len);
//        new_base = ((cmp <= 0) ? &new_base[half] : new_base);
//        n -= half;
//    }
//    n = (new_base - base);
//    return (cmp == 0) ? n : ~(n+1);
//}

int16_t basix::locate() {
    pos = binarySearch(key, key_len);
    return pos;
}

byte *basix::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t orig_filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(BASIX_NODE_SIZE);
    basix new_block(b);
    new_block.initBuf();
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

void basix::addData(int16_t idx) {

    uint16_t kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    current_block[kv_last_pos] = key_len & 0xFF;
    memcpy(current_block + kv_last_pos + 1, key, key_len);
    current_block[kv_last_pos + key_len + 1] = value_len & 0xFF;
    memcpy(current_block + kv_last_pos + key_len + 2, value, value_len);
    insPtr(pos, kv_last_pos);
    if (BPT_MAX_KEY_LEN < key_len)
        BPT_MAX_KEY_LEN = key_len;

}

bool basix::isFull(int16_t kv_len) {
    int16_t ptr_size = filledSize() + 2;
#if BPT_9_BIT_PTR == 0
    ptr_size <<= 1;
#endif
    if (getKVLastPos() <= (BLK_HDR_SIZE + ptr_size + kv_len + 2))
        return true;
#if BPT_9_BIT_PTR == 1
    if (filledSize() > 62)
    return true;
#endif
    return false;
}

byte *basix::getPtrPos() {
    return current_block + BLK_HDR_SIZE;
}
