#include <math.h>
#include <stdint.h>
#include "dfqx.h"
#include "GenTree.h"

char *dfqx::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    dfqx_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    if (!node.isLeaf())
        node.traverseToLeaf();
    if (node.locate() < 0)
        return null;
    char * ret = node.getValueAt(pValueLen);
    return ret;
}

void dfqx_node_handler::traverseToLeaf(byte *node_paths[]) {
    byte level;
    level = 1;
    if (isPut)
        *node_paths = buf;
    while (!isLeaf()) {
        int16_t idx = locate();
        if (idx < 0) {
            idx = ~idx;
            if (idx)
                idx--;
        }
        key_at = buf + getPtr(idx);
        setBuf(getChildPtr(key_at));
        if (isPut)
            node_paths[level++] = buf;
    }
}

void dfqx::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    byte *node_paths[10];
    dfqx_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    node.value = value;
    node.value_len = value_len;
    node.isPut = true;
    if (node.filledSize() == 0) {
        node.pos = 0;
        node.insertState = INSERT_EMPTY;
        node.addData();
        total_size++;
    } else {
        if (!node.isLeaf())
            node.traverseToLeaf(node_paths);
        node.locate();
        recursiveUpdate(&node, -1, node_paths, numLevels - 1);
    }
}

void dfqx::recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
        byte *node_paths[], int16_t level) {
    int16_t idx = pos; // lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (node->isFull(node->key_len + node->value_len)) {
            //std::cout << "Full\n" << std::endl;
            //if (maxKeyCount < block->filledSize())
            //    maxKeyCount = block->filledSize();
            //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
            //cout << (int) node->TRIE_LEN << endl;
            if (node->isLeaf()) {
                maxKeyCountLeaf += node->filledSize();
                blockCountLeaf++;
            } else {
                maxKeyCountNode += node->filledSize();
                blockCountNode++;
            }
            //    maxKeyCount += node->TRIE_LEN;
            //maxKeyCount += node->PREFIX_LEN;
            byte first_key[DFQX_MAX_KEY_PREFIX_LEN];
            int16_t first_len;
            byte *b = node->split(first_key, &first_len);
            dfqx_node_handler new_block(b);
            new_block.isPut = true;
            int16_t cmp = util::compare((char *) first_key, first_len,
                    node->key, node->key_len);
            if (cmp <= 0) {
                new_block.initVars();
                new_block.key = node->key;
                new_block.key_len = node->key_len;
                new_block.value = node->value;
                new_block.value_len = node->value_len;
                idx = ~new_block.locate();
                new_block.addData();
            } else {
                node->initVars();
                idx = ~node->locate();
                node->addData();
            }
            if (root_data == node->buf) {
                blockCountNode++;
                root_data = (byte *) util::alignedAlloc(DFQX_NODE_SIZE);
                dfqx_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                root.setLeaf(0);
                byte addr[9];
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = (char *) addr;
                root.value_len = util::ptrToBytes((unsigned long) node->buf, addr);
                root.pos = 0;
                root.insertState = INSERT_EMPTY;
                root.addData();
                root.initVars();
                root.key = (char *) first_key;
                root.key_len = first_len;
                root.value = (char *) addr;
                root.value_len = util::ptrToBytes((unsigned long) new_block.buf, addr);
                root.locate();
                root.addData();
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                dfqx_node_handler parent(parent_data);
                byte addr[9];
                parent.initVars();
                parent.isPut = true;
                parent.key = (char *) first_key;
                parent.key_len = first_len;
                parent.value = (char *) addr;
                parent.value_len = util::ptrToBytes((unsigned long) new_block.buf, addr);
                parent.locate();
                recursiveUpdate(&parent, -1, node_paths, prev_level);
            }
        } else
            node->addData();
    } else {
        //if (node->isLeaf) {
        //    int16_t vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

byte *dfqx_node_handler::nextKey(byte *first_key, byte *tp, byte *t, char& ctr, byte& tc, byte& child_leaf) {
    if (t == trie) {
        keyPos = 0;
        ctr = x04;
    } else {
        while (ctr == x04 && (tc & x01)) {
            keyPos--;
            tc = trie[tp[keyPos]];
            child_leaf = trie[tp[keyPos] + 1];
            ctr = first_key[keyPos] & 0x03;
            ctr++;
        }
    }
    do {
        if (ctr > x03) {
            tp[keyPos] = t - trie;
            tc = *t++;
            child_leaf = *t++;
            ctr = 0;
        }
        first_key[keyPos] = (tc & xFC) | ctr;
        if (child_leaf & (x80 >> ctr)) {
            if (child_leaf & (x08 >> ctr))
                child_leaf &= ~(x80 >> ctr);
            else
                ctr++;
            return t;
        }
        if (child_leaf & (x08 >> ctr)) {
            keyPos++;
            ctr = x04;
        }
        while (ctr == x03 && (tc & x01)) {
            keyPos--;
            tc = trie[tp[keyPos]];
            child_leaf = trie[tp[keyPos] + 1];
            ctr = first_key[keyPos] & 0x03;
        }
        ctr++;
    } while (1); // (t - trie) < BPT_TRIE_LEN);
    return t;
}

byte *dfqx_node_handler::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t orig_filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(DFQX_NODE_SIZE);
    dfqx_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    if (!isLeaf())
        new_block.setLeaf(false);
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = DFQX_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int16_t brk_idx = -1;
    int16_t brk_kv_pos;
    int16_t tot_len;
    brk_kv_pos = tot_len = 0;
    // (1) move all data to new_block in order
    int16_t idx;
    char ctr = 0;
    byte tp[DFQX_MAX_KEY_PREFIX_LEN];
    byte *t = trie;
    byte tc, child_leaf;
    tc = child_leaf = 0;
    //cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled size:" << orig_filled_size << endl;
    keyPos = 0;
    for (idx = 0; idx < orig_filled_size; idx++) {
        int16_t src_idx = getPtr(idx);
        int16_t kv_len = buf[src_idx];
        kv_len++;
        kv_len += buf[src_idx + kv_len];
        kv_len++;
        tot_len += kv_len;
        memcpy(new_block.buf + kv_last_pos, buf + src_idx, kv_len);
        new_block.insPtr(idx, kv_last_pos);
        kv_last_pos += kv_len;
        if (brk_idx == -1) {
            t = nextKey(first_key, tp, t, ctr, tc, child_leaf);
            //if (tot_len > halfKVLen) {
            if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                brk_idx = idx + 1;
                brk_kv_pos = kv_last_pos;
                memcpy(new_block.trie, trie, BPT_TRIE_LEN);
                new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
                deleteTrieLastHalf(keyPos, first_key, tp);
                new_block.keyPos = keyPos;
                t = new_block.trie + (t - trie);
                t = new_block.nextKey(first_key, tp, t, ctr, tc, child_leaf);
                keyPos = new_block.keyPos;
            }
        }
    }
    kv_last_pos = getKVLastPos();
#if DQ_9_BIT_PTR == 1
    memcpy(buf + DFQX_HDR_SIZE, new_block.buf + DFQX_HDR_SIZE,
            DQ_MAX_PTR_BITMAP_BYTES + brk_idx);
#else
    memcpy(buf + DFQX_HDR_SIZE, new_block.buf + DFQX_HDR_SIZE, (brk_idx << 1));
#endif

    {
        new_block.key_at = new_block.getKey(brk_idx, &new_block.key_at_len);
        keyPos++;
        if (isLeaf())
            *first_len_ptr = keyPos;
        else {
            if (new_block.key_at_len) {
                memcpy(first_key + keyPos, new_block.key_at,
                        new_block.key_at_len);
                *first_len_ptr = keyPos + new_block.key_at_len;
            } else
                *first_len_ptr = keyPos;
        }
        keyPos--;
        new_block.deleteTrieFirstHalf(keyPos, first_key, tp);
    }

    {
        int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
        memcpy(buf + DFQX_NODE_SIZE - old_blk_new_len,
                new_block.buf + kv_last_pos, old_blk_new_len); // Copy back first half to old block
        int16_t diff = DFQX_NODE_SIZE - brk_kv_pos;
        idx = brk_idx;
        while (idx--)
            setPtr(idx, getPtr(idx) + diff);
#if DQ_9_BIT_PTR == 1
        byte *block_ptrs = buf + DFQX_HDR_SIZE + DQ_MAX_PTR_BITMAP_BYTES
                + brk_idx;
#else
        byte *block_ptrs = buf + DFQX_HDR_SIZE + (brk_idx << 1);
#endif
        memmove(block_ptrs, trie, BPT_TRIE_LEN);
        trie = block_ptrs;
        setKVLastPos(DFQX_NODE_SIZE - old_blk_new_len);
        setFilledSize(brk_idx);
    }

    {
#if DQ_9_BIT_PTR == 1
#if DQ_INT64MAP == 1
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
        byte *block_ptrs = new_block.buf + DFQX_HDR_SIZE
                + DQ_MAX_PTR_BITMAP_BYTES;
#if DQ_9_BIT_PTR == 1
        memmove(block_ptrs, block_ptrs + brk_idx,
                new_size + new_block.BPT_TRIE_LEN);
        new_block.trie = block_ptrs + new_size;
#else
        memmove(block_ptrs, block_ptrs + (brk_idx << 1),
                (new_size << 1) + new_block.BPT_TRIE_LEN);
        new_block.trie = block_ptrs + (new_size * 2);
#endif
        new_block.setKVLastPos(brk_kv_pos);
        new_block.setFilledSize(new_size);
    }

    return new_block.buf;
}

void dfqx_node_handler::deleteTrieLastHalf(int16_t brk_key_len,
        byte *first_key, byte *tp) {
    byte *t = 0;
    byte children = 0;
    for (int idx = 0; idx <= brk_key_len; idx++) {
        byte offset = first_key[idx] & 0x03;
        t = trie + tp[idx];
        byte tc = *t;
        *t++ = (tc | x01);
        children = *t & x0F;
        children &= (
                (idx == brk_key_len) ?
                        ryte_mask[offset] : ryte_incl_mask[offset]);
        byte child_leaf = (*t & (ryte_incl_mask[offset] << 4)) + children;
        *t++ = child_leaf;
    }
    byte to_skip = bit_count[children];
    while (to_skip) {
        byte tc = *t++;
        to_skip += bit_count[*t & x0F];
        t++;
        if (tc & x01)
            to_skip--;
    }
    BPT_TRIE_LEN = t - trie;

}

void dfqx_node_handler::deleteTrieFirstHalf(int16_t brk_key_len,
        byte *first_key, byte *tp) {
    byte *delete_start = trie;
    int tot_del = 0;
    for (int idx = 0; idx <= brk_key_len; idx++) {
        byte offset = first_key[idx] & 0x03;
        byte *t = trie + tp[idx] - tot_del;
        int count = t - delete_start;
        if (count) {
            BPT_TRIE_LEN -= count;
            memmove(delete_start, t, trie + BPT_TRIE_LEN - delete_start);
            t -= count;
            tot_del += count;
        }
        byte tc = *t++;
        byte children = *t & x0F;
        count = bit_count[children & ryte_mask[offset]];
        children &= left_incl_mask[offset];
        *t = (children)
                + (*t
                        & ((idx == brk_key_len ?
                                left_incl_mask[offset] : left_mask[offset]) << 4));
        t++;
        delete_start = t;
        while (count) {
            tc = *t++;
            count += bit_count[*t & x0F];
            t++;
            if (tc & x01)
                count--;
        }
        if (t != delete_start) {
            count = t - delete_start;
            BPT_TRIE_LEN -= count;
            memmove(delete_start, t, trie + BPT_TRIE_LEN - delete_start);
            t -= count;
            tot_del += count;
        }
    }

}

dfqx::dfqx() {
    root_data = (byte *) util::alignedAlloc(DFQX_NODE_SIZE);
    dfqx_node_handler root(root_data);
    root.initBuf();
    total_size = maxKeyCountLeaf = maxKeyCountNode = 0;
    numLevels = blockCountLeaf = blockCountNode = 1;
    maxThread = 9999;
    count1 = 0;
}

dfqx::~dfqx() {
    delete root_data;
}

dfqx_node_handler::dfqx_node_handler(byte * m) {
    setBuf(m);
    isPut = false;
}

void dfqx_node_handler::initBuf() {
    //memset(buf, '\0', DFQX_NODE_SIZE);
    setLeaf(1);
    setFilledSize(0);
    BPT_TRIE_LEN = 0;
    //MID_KEY_LEN = 0;
    setKVLastPos(DFQX_NODE_SIZE);
    trie = buf + DFQX_HDR_SIZE + DQ_MAX_PTR_BITMAP_BYTES;
#if DQ_INT64MAP == 1
    bitmap = (uint64_t *) (buf + DFQX_HDR_SIZE);
#else
    bitmap1 = (uint32_t *) (buf + DFQX_HDR_SIZE);
    bitmap2 = bitmap1 + 1;
#endif
}

void dfqx_node_handler::setBuf(byte *m) {
    buf = m;
    trie = buf + DFQX_HDR_SIZE + DQ_MAX_PTR_BITMAP_BYTES
            + filledSize() * (DQ_9_BIT_PTR == 1 ? 1 : 2);
#if DQ_INT64MAP == 1
    bitmap = (uint64_t *) (buf + DFQX_HDR_SIZE);
#else
    bitmap1 = (uint32_t *) (buf + DFQX_HDR_SIZE);
    bitmap2 = bitmap1 + 1;
#endif
}

void dfqx_node_handler::addData() {

    insertCurrent();

    int16_t key_left = key_len - keyPos;
    int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_left;
    if (key_left)
        memcpy(buf + kv_last_pos + 1, key + keyPos, key_left);
    buf[kv_last_pos + key_left + 1] = value_len;
    memcpy(buf + kv_last_pos + key_left + 2, value, value_len);

    insPtr(pos, kv_last_pos);

}

void dfqx_node_handler::insPtr(int16_t pos, int16_t kv_pos) {
    int16_t filledSz = filledSize();
#if DQ_9_BIT_PTR == 1
    byte *kvIdx = buf + DFQX_HDR_SIZE + DQ_MAX_PTR_BITMAP_BYTES + pos;
    memmove(kvIdx + 1, kvIdx, filledSz - pos + BPT_TRIE_LEN);
    *kvIdx = kv_pos;
    trie++;
#if DQ_INT64MAP == 1
    insBit(bitmap, pos, kv_pos);
#else
    if (pos & 0xFFE0) {
        insBit(bitmap2, pos - 32, kv_pos);
    } else {
        byte last_bit = (*bitmap1 & 0x01);
        insBit(bitmap1, pos, kv_pos);
        *bitmap2 >>= 1;
        if (last_bit)
        *bitmap2 |= *util::mask32;
    }
#endif
#else
    byte *kvIdx = buf + DFQX_HDR_SIZE + (pos << 1);
    memmove(kvIdx + 2, kvIdx, (filledSz - pos) * 2 + BPT_TRIE_LEN);
    util::setInt(kvIdx, kv_pos);
    trie += 2;
#endif
    setFilledSize(filledSz + 1);

}

void dfqx_node_handler::insBit(uint32_t *ui32, int pos, int16_t kv_pos) {
    uint32_t ryte_part = (*ui32) & util::ryte_mask32[pos];
    ryte_part >>= 1;
    if (kv_pos >= 256)
        ryte_part |= util::mask32[pos];
    (*ui32) = (ryte_part | ((*ui32) & util::left_mask32[pos]));
}

void dfqx_node_handler::insBit(uint64_t *ui64, int pos, int16_t kv_pos) {
    uint64_t ryte_part = (*ui64) & util::ryte_mask64[pos];
    ryte_part >>= 1;
    if (kv_pos >= 256)
        ryte_part |= util::mask64[pos];
    (*ui64) = (ryte_part | ((*ui64) & util::left_mask64[pos]));
}

bool dfqx_node_handler::isFull(int16_t kv_len) {
    int16_t ptr_size = filledSize() + 1;
#if DQ_9_BIT_PTR == 0
    ptr_size <<= 1;
#endif
    if ((getKVLastPos() - kv_len - 2)
            < (DFQX_HDR_SIZE + DQ_MAX_PTR_BITMAP_BYTES + BPT_TRIE_LEN
                    + need_count + ptr_size))
        return true;
    if (filledSize() > DQ_MAX_PTRS)
        return true;
    if (BPT_TRIE_LEN + need_count > 240)
        return true;
    return false;
}

void dfqx_node_handler::setPtr(int16_t pos, int16_t ptr) {
#if DQ_9_BIT_PTR == 1
    buf[DFQX_HDR_SIZE + DQ_MAX_PTR_BITMAP_BYTES + pos] = ptr;
#if DQ_INT64MAP == 1
    if (ptr >= 256)
        *bitmap |= util::mask64[pos];
    else
        *bitmap &= ~util::mask64[pos];
#else
    if (pos & 0xFFE0) {
        pos -= 32;
        if (ptr >= 256)
        *bitmap2 |= util::mask32[pos];
        else
        *bitmap2 &= ~util::mask32[pos];
    } else {
        if (ptr >= 256)
        *bitmap1 |= util::mask32[pos];
        else
        *bitmap1 &= ~util::mask32[pos];
    }
#endif
#else
    byte *kvIdx = buf + DFQX_HDR_SIZE + (pos << 1);
    util::setInt(kvIdx, ptr);
#endif
}

int16_t dfqx_node_handler::getPtr(int16_t pos) {
#if DQ_9_BIT_PTR == 1
    int16_t ptr = buf[DFQX_HDR_SIZE + DQ_MAX_PTR_BITMAP_BYTES + pos];
#if DQ_INT64MAP == 1
    if (*bitmap & util::mask64[pos])
        ptr |= 256;
#else
    if (pos & 0xFFE0) {
        if (*bitmap2 & util::mask32[pos - 32])
        ptr |= 256;
    } else {
        if (*bitmap1 & util::mask32[pos])
        ptr |= 256;
    }
#endif
    return ptr;
#else
    byte *kvIdx = buf + DFQX_HDR_SIZE + (pos << 1);
    return util::getInt(kvIdx);
#endif
}

void dfqx_node_handler::insertCurrent() {
    byte key_char;
    byte mask;
    byte *leafPos;

    switch (insertState) {
    case INSERT_MIDDLE1:
        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x03);
        *origPos &= xFE;
        insAt(triePos, ((key_char & xFC) | x01), mask);
        break;
    case INSERT_MIDDLE2:
        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x03);
        insAt(triePos, (key_char & xFC), mask);
        break;
    case INSERT_LEAF:
        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x03);
        leafPos = origPos + 1;
        *leafPos |= mask;
        break;
    case INSERT_THREAD:
        int16_t p, min;
        byte c1, c2;
        byte *childPos;
        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x03);
        childPos = origPos + 1;
        *childPos |= (x08 >> (key_char & x03));
        c1 = c2 = key_char;
        p = keyPos;
        min = util::min16(key_len, keyPos + key_at_len);
        if (p < min) {
            leafPos = childPos;
            (*leafPos) &= ~mask;
        }
        while (p < min) {
            c1 = key[p];
            c2 = key_at[p - keyPos];
            if (c1 > c2) {
                byte swap = c1;
                c1 = c2;
                c2 = swap;
            }
            switch ((c1 ^ c2) > x03 ?
                    0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
            case 0:
                triePos += insAt(triePos, (c1 & xFC), x80 >> (c1 & x03),
                        (c2 & xFC) | x01, x80 >> (c2 & x03));
                break;
            case 1:
                triePos += insAt(triePos, (c1 & xFC) | x01,
                        (x80 >> (c1 & x03)) | (x80 >> (c2 & x03)));
                break;
            case 2:
                dfqx::count1++;
                triePos += insAt(triePos, (c1 & xFC) | x01, x08 >> (c1 & x03));
                break;
            case 3:
                triePos += insAt(triePos, (c1 & xFC) | x01,
                        (0x88 >> (c1 & x03)));
                break;
            }
            if (c1 != c2)
                break;
            p++;
        }
        int16_t diff;
        diff = p - keyPos;
        keyPos = p + 1;
        if (c1 == c2) {
            c2 = (p == key_len ? key_at[diff] : key[p]);
            insAt(triePos, (c2 & xFC) | x01, (x80 >> (c2 & x03)));
            if (p == key_len)
                keyPos--;
        }
        if (diff < key_at_len)
            diff++;
        if (diff) {
            p = getPtr(key_at_pos);
            key_at_len -= diff;
            p += diff;
            if (key_at_len >= 0) {
                buf[p] = key_at_len;
                setPtr(key_at_pos, p);
            }
        }
        break;
    case INSERT_EMPTY:
        key_char = *key;
        mask = x80 >> (key_char & x03);
        append((key_char & xFC) | x01);
        append(mask);
        keyPos = 1;
        break;
    }

}

int16_t dfqx_node_handler::locate() {
    byte key_char;
    byte *t = trie;
    uint16_t to_skip = 0;
    keyPos = 1;
    key_char = *key;
    do {
        byte trie_char = *t;
        switch ((key_char ^ trie_char) > x03 ?
                (key_char > trie_char ? 0 : 2) : 1) {
        case 0:
            origPos = t++;
            to_skip += dbl_bit_count[*t++];
            while (to_skip & 0xFF) {
                to_skip -= (*t++ & x01);
                to_skip += dbl_bit_count[*t++];
            }
            if (trie_char & x01) {
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_MIDDLE1;
                    need_count = 2;
                }
                pos = to_skip >> 8;
                return ~pos;
            }
            break;
        case 1:
            byte r_leaves_children;
            origPos = t++;
            r_leaves_children = *t++;
            key_char &= x03;
            //to_skip += dbl_bit_count[r_leaves_children & dbl_ryte_mask[key_char]];
            to_skip += dbl_bit_count[r_leaves_children & ((0xEECC8800 >> (key_char << 3)) & xFF)];
            while (to_skip & 0xFF) {
                to_skip -= (*t++ & x01);
                to_skip += dbl_bit_count[*t++];
            }
            switch ((r_leaves_children << key_char) & 0x88) {
            case 0: // 00000000
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_LEAF;
                    need_count = 2;
                }
                pos = to_skip >> 8;
                return ~pos;
            case 0x08: // 00001000
                if (keyPos == key_len) {
                    if (isPut) {
                        triePos = t;
                        insertState = INSERT_LEAF;
                        need_count = 2;
                    }
                    pos = to_skip >> 8;
                    return ~pos;
                }
                break;
            case 0x80: // 10000000
                int16_t cmp;
                pos = to_skip >> 8;
                key_at = getKey(pos, &key_at_len);
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at, key_at_len);
                if (cmp == 0)
                    return pos;
                key_at_pos = pos;
                if (cmp > 0)
                    pos++;
                else
                    cmp = -cmp;
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_THREAD;
                    need_count = (cmp * 2) + 4;
                }
                return ~pos;
            case 0x88: // 10001000
                if (keyPos == key_len) {
                    pos = to_skip >> 8;
                    return pos;
                } else {
                    to_skip += 0x100;
                }
                break;
            }
            key_char = key[keyPos++];
            break;
        case 2:
            if (isPut) {
                triePos = t;
                insertState = INSERT_MIDDLE2;
                need_count = 2;
            }
            pos = to_skip >> 8;
            return ~pos;
            break;
        }
    } while (1);
    pos = to_skip >> 8;
    return ~pos;
}

void dfqx_node_handler::append(byte b) {
    trie[BPT_TRIE_LEN++] = b;
}

byte *dfqx_node_handler::getKey(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + getPtr(pos);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *dfqx_node_handler::getChildPtr(byte *ptr) {
    ptr += (*ptr + 1);
    return (byte *) util::bytesToPtr(ptr);
}

char *dfqx_node_handler::getValueAt(int16_t *vlen) {
    key_at = buf + getPtr(pos);
    key_at += *key_at;
    key_at++;
    *vlen = *key_at;
    key_at++;
    return (char *) key_at;
}

void dfqx_node_handler::initVars() {
}

byte dfqx_node_handler::left_mask[4] = { 0x07, 0x03, 0x01, 0x00 };
byte dfqx_node_handler::left_incl_mask[4] = { 0x0F, 0x07, 0x03, 0x01 };
byte dfqx_node_handler::ryte_mask[4] = { 0x00, 0x08, 0x0C, 0x0E };
byte dfqx_node_handler::dbl_ryte_mask[4] = { 0x00, 0x88, 0xCC, 0xEE };
byte dfqx_node_handler::ryte_incl_mask[4] = { 0x08, 0x0C, 0x0E, 0x0F };
byte dfqx_node_handler::first_bit_offset[16] = { 0x04, 0x03, 0x02, 0x02, 0x01,
        0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
byte dfqx_node_handler::bit_count[16] = { 0x00, 0x01, 0x01, 0x02, 0x01,
        0x02, 0x02, 0x03, 0x01, 0x02, 0x02, 0x03, 0x02, 0x03, 0x03, 0x04 };
uint16_t dfqx_node_handler::dbl_bit_count[256] = {
      //0000   0001   0010   0011   0100   0101   0110   0111   1000   1001   1010   1011   1100   1101   1110   1111
        0x000, 0x001, 0x001, 0x002, 0x001, 0x002, 0x002, 0x003, 0x001, 0x002, 0x002, 0x003, 0x002, 0x003, 0x003, 0x004, //0000
        0x100, 0x101, 0x101, 0x102, 0x101, 0x102, 0x102, 0x103, 0x101, 0x102, 0x102, 0x103, 0x102, 0x103, 0x103, 0x104, //0001
        0x100, 0x101, 0x101, 0x102, 0x101, 0x102, 0x102, 0x103, 0x101, 0x102, 0x102, 0x103, 0x102, 0x103, 0x103, 0x104, //0010
        0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //0011
        0x100, 0x101, 0x101, 0x102, 0x101, 0x102, 0x102, 0x103, 0x101, 0x102, 0x102, 0x103, 0x102, 0x103, 0x103, 0x104, //0100
        0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //0101
        0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //0110
        0x300, 0x301, 0x301, 0x302, 0x301, 0x302, 0x302, 0x303, 0x301, 0x302, 0x302, 0x303, 0x302, 0x303, 0x303, 0x304, //0111
        0x100, 0x101, 0x101, 0x102, 0x101, 0x102, 0x102, 0x103, 0x101, 0x102, 0x102, 0x103, 0x102, 0x103, 0x103, 0x104, //1000
        0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //1001
        0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //1010
        0x300, 0x301, 0x301, 0x302, 0x301, 0x302, 0x302, 0x303, 0x301, 0x302, 0x302, 0x303, 0x302, 0x303, 0x303, 0x304, //1011
        0x200, 0x201, 0x201, 0x202, 0x201, 0x202, 0x202, 0x203, 0x201, 0x202, 0x202, 0x203, 0x202, 0x203, 0x203, 0x204, //1100
        0x300, 0x301, 0x301, 0x302, 0x301, 0x302, 0x302, 0x303, 0x301, 0x302, 0x302, 0x303, 0x302, 0x303, 0x303, 0x304, //1101
        0x300, 0x301, 0x301, 0x302, 0x301, 0x302, 0x302, 0x303, 0x301, 0x302, 0x302, 0x303, 0x302, 0x303, 0x303, 0x304, //1110
        0x400, 0x401, 0x401, 0x402, 0x401, 0x402, 0x402, 0x403, 0x401, 0x402, 0x402, 0x403, 0x402, 0x403, 0x403, 0x404  //1111
};
long dfqx::count1, dfqx::count2;
