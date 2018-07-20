#include "linex.h"

byte *linex::getChildPtrPos(int16_t idx) {
    if (idx < 0) {
        idx++;
        idx = ~idx;
        key_at = prev_key_at;
    }
    return current_block + getPtr(idx);
}

int linex::getHeaderSize() {
    return LX_BLK_HDR_SIZE;
}

void linex::addData(int16_t idx) {
    int16_t prev_plen = 0;
    int16_t filled_size = filledSize();
    setFilledSize(filled_size + 1);
#if LX_PREFIX_CODING == 1
    if (filled_size && key_at != prev_key_at) {
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
    int16_t kv_last_pos = getKVLastPos();
    int16_t key_left = key_len - prev_plen;
    int16_t tot_len = key_left + value_len + 3;
    int16_t len_to_move = kv_last_pos - (key_at - current_block);
    if (len_to_move) {
#if LX_PREFIX_CODING == 1
        int16_t next_plen = prefix_len;
        while (key_at[next_plen - prefix_len + 2] == key[next_plen]
                && next_plen < key_len)
            next_plen++;
        int16_t diff = next_plen - prefix_len;
        if (diff) {
            int16_t next_key_len = key_at[1];
            int16_t next_value_len = key_at[next_key_len + 2];
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
    setKVLastPos(kv_last_pos);

    if (BPT_MAX_KEY_LEN < key_len)
        BPT_MAX_KEY_LEN = key_len;

}

byte *linex::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(LINEX_NODE_SIZE);
    linex new_block(b);
    new_block.initBuf();
    if (!isLeaf())
        new_block.setLeaf(false);
    new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = kv_last_pos / 2;

    int16_t brk_idx = -1;
    int16_t brk_kv_pos;
    int16_t tot_len;
    brk_kv_pos = tot_len = 0;
    // Copy all data to new block in ascending order
    int16_t new_idx;
    int16_t src_idx = LX_BLK_HDR_SIZE;
    int16_t dst_idx = src_idx;
    prefix_len = 0;
    prev_key_at = current_block + src_idx;
    for (new_idx = 0; new_idx < filled_size; new_idx++) {
        for (int16_t pctr = 0; prefix_len < current_block[src_idx]; pctr++)
            prefix[prefix_len++] = prev_key_at[pctr + 2];
        if (prefix_len > current_block[src_idx])
            prefix_len = current_block[src_idx];
        src_idx++;
        int16_t kv_len = current_block[src_idx];
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
            if (isLeaf()) {
                prefix_len = 0;
                for (int i = 0; current_block[src_idx + i + 1] == prev_key_at[i + 2]; i++) {
                    prefix[i] = prev_key_at[i + 2];
                    prefix_len++;
                }
            }
#endif
            memcpy(first_key, prefix, prefix_len);
            if (isLeaf()) {
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
            if (tot_len > halfKVLen || new_idx == (filled_size / 2)) {
                brk_idx = new_idx + 1;
                brk_kv_pos = dst_idx;
            }
        }
        prev_key_at = current_block + src_idx - 1;
        src_idx += kv_len;
        kv_last_pos = dst_idx;
    }

    int16_t old_blk_new_len = brk_kv_pos - LX_BLK_HDR_SIZE;
    memcpy(current_block + LX_BLK_HDR_SIZE, new_block.current_block + LX_BLK_HDR_SIZE,
            old_blk_new_len); // Copy back first half to old block
    setKVLastPos(LX_BLK_HDR_SIZE + old_blk_new_len);
    setFilledSize(brk_idx); // Set filled upto for old block

    int16_t new_size = filled_size - brk_idx;
    // Move index of second half to first half in new block
    byte *new_kv_idx = new_block.current_block + brk_kv_pos;
    memmove(new_block.current_block + LX_BLK_HDR_SIZE, new_kv_idx,
            kv_last_pos - brk_kv_pos); // Move first block to front
    // Set KV Last pos for new block
    new_block.setKVLastPos(LX_BLK_HDR_SIZE + kv_last_pos - brk_kv_pos);
    new_block.setFilledSize(new_size); // Set filled upto for new block

    return b;

}

int16_t linex::linearSearch() {
    int16_t idx = 0;
    int16_t filled_size = filledSize();
    key_at = current_block + LX_BLK_HDR_SIZE;
    prev_key_at = key_at;
    prefix_len = prev_prefix_len = 0;
    while (idx < filled_size) {
        int16_t cmp;
        key_at_len = *(key_at + 1);
#if LX_PREFIX_CODING == 1
        byte *p_cur_prefix_len = key_at;
        for (int16_t pctr = 0; prefix_len < (*p_cur_prefix_len & 0x7F); pctr++)
            prefix[prefix_len++] = prev_key_at[pctr + 2];
        if (prefix_len > *key_at)
            prefix_len = *key_at;
        cmp = memcmp(prefix, key, prefix_len);
        if (cmp == 0) {
            cmp = util::compare((char *) key_at + 2, key_at_len,
                    key + prefix_len, key_len - prefix_len);
        }
#if LX_DATA_AREA == 1
        if (cmp == 0) {
            int16_t partial_key_len = prefix_len + key_at_len;
            byte *data_key_at = current_block + key_at_len + *key_at + 1;
            if (*p_cur_prefix_len & 0x80)
                data_key_at += 256;
            cmp = util::compare((char *) data_key_at + 1, *data_key_at,
                    key + partial_key_len, key_len - partial_key_len);
        }
#endif
#else
        cmp = util::compare((char *) key_at + 2, key_at_len, key, key_len);
#endif
        if (cmp > 0)
            return ~idx;
        else if (cmp == 0)
            return idx;
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

int16_t linex::searchCurrentNode() {
    pos = linearSearch();
    return pos;
}

bool linex::isFull(int16_t kv_len) {
    if ((getKVLastPos() + kv_len + 3) >= LINEX_NODE_SIZE)
        return true;
    return false;
}

byte *linex::getChildPtr(byte *ptr) {
    ptr++;
    ptr += (*ptr + 1);
    return (byte *) util::bytesToPtr(ptr);
}

char *linex::getValueAt(int16_t *vlen) {
    key_at++;
    key_at += *key_at;
    key_at++;
    *vlen = *key_at;
    key_at++;
    return (char *) key_at;
}

byte *linex::getPtrPos() {
    return NULL;
}
