#include <math.h>
#include <stdint.h>
#include "dfox.h"
#include "GenTree.h"

char *dfox::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    dfox_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    node.traverseToLeaf();
    if (node.locate() < 0)
        return null;
    char * ret = node.getValueAt(pValueLen);
    return ret;
}

void dfox_node_handler::traverseToLeaf(byte *node_paths[]) {
    byte level = 1;
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

void dfox::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    byte *node_paths[10];
    dfox_node_handler node(root_data);
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
        node.traverseToLeaf(node_paths);
        node.locate();
        recursiveUpdate(&node, -1, node_paths, numLevels - 1);
    }
}

void dfox::recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
        byte *node_paths[], int16_t level) {
    //int16_t idx = pos; // lastSearchPos[level];
    if (pos < 0) {
        pos = ~pos;
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
                //maxKeyCount += node->BPT_TRIE_LEN;
            //maxKeyCount += node->PREFIX_LEN;
            byte first_key[DFOX_MAX_KEY_PREFIX_LEN];
            int16_t first_len;
            byte *b = node->split(first_key, &first_len);
            dfox_node_handler new_block(b);
            new_block.isPut = true;
            int16_t cmp = util::compare((char *) first_key, first_len,
                    node->key, node->key_len);
            if (cmp <= 0) {
                new_block.initVars();
                new_block.key = node->key;
                new_block.key_len = node->key_len;
                new_block.value = node->value;
                new_block.value_len = node->value_len;
                pos = ~new_block.locate();
                new_block.addData();
            } else {
                node->initVars();
                pos = ~node->locate();
                node->addData();
            }
            if (root_data == node->buf) {
                blockCountNode++;
                root_data = (byte *) util::alignedAlloc(node_size);
                dfox_node_handler root(root_data);
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
                dfox_node_handler parent(parent_data);
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

byte *dfox_node_handler::nextKey(byte *first_key, byte *tp, byte *t, char& ctr, byte& tc, byte& child, byte& leaf) {
    if (t == trie) {
        keyPos = 0;
        ctr = x08;
    } else {
        while (ctr == x08 && (tc & x04)) {
            keyPos--;
            tc = trie[tp[keyPos]];
            child = (tc & x02 ? trie[tp[keyPos] + 1] : x00);
            leaf = trie[tp[keyPos] + (tc & x02 ? 2 : 1)];
            ctr = first_key[keyPos] & 0x07;
            ctr++;
        }
    }
    do {
        if (ctr > x07) {
            tp[keyPos] = t - trie;
            tc = *t++;
            child = (tc & x02 ? *t++ : x00);
            leaf = *t++;
            ctr = util::first_bit_offset[child | leaf];
        }
        first_key[keyPos] = (tc & xF8) | ctr;
        byte mask = (x01 << ctr);
        if (leaf & mask) {
            if (child & mask)
                leaf &= ~mask;
            else
                ctr++;
            return t;
        }
        if (child & mask) {
            keyPos++;
            ctr = x08;
        }
        while (ctr == x07 && (tc & x04)) {
            keyPos--;
            tc = trie[tp[keyPos]];
            child = (tc & x02 ? trie[tp[keyPos] + 1] : x00);
            leaf = trie[tp[keyPos] + (tc & x02 ? 2 : 1)];
            ctr = first_key[keyPos] & 0x07;
        }
        ctr++;
    } while (1); // (t - trie) < BPT_TRIE_LEN);
    return t;
}

byte *dfox_node_handler::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t orig_filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
    dfox_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    if (!isLeaf())
        new_block.setLeaf(false);
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = DFOX_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int16_t brk_idx = -1;
    int16_t brk_kv_pos;
    int16_t tot_len;
    brk_kv_pos = tot_len = 0;
    char ctr = 0;
    byte tp[DFOX_MAX_KEY_PREFIX_LEN];
    byte *t = trie;
    byte tc, child, leaf;
    tc = child = leaf = 0;
    //cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled size:" << orig_filled_size << endl;
    keyPos = 0;
    // (1) move all data to new_block in order
    int16_t idx;
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
            t = nextKey(first_key, tp, t, ctr, tc, child, leaf);
            //brk_key_len = nextKey(s);
            //if (tot_len > halfKVLen) {
            if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                brk_idx = idx + 1;
                brk_kv_pos = kv_last_pos;
                memcpy(new_block.trie, trie, BPT_TRIE_LEN);
                new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
                deleteTrieLastHalf(keyPos, first_key, tp);
                new_block.keyPos = keyPos;
                t = new_block.trie + (t - trie);
                t = new_block.nextKey(first_key, tp, t, ctr, tc, child, leaf);
                keyPos = new_block.keyPos;
            }
        }
    }
    kv_last_pos = getKVLastPos();
#if DX_9_BIT_PTR == 1
    memcpy(buf + DFOX_HDR_SIZE, new_block.buf + DFOX_HDR_SIZE, DX_MAX_PTR_BITMAP_BYTES + brk_idx);
#else
    memcpy(buf + DFOX_HDR_SIZE, new_block.buf + DFOX_HDR_SIZE, (brk_idx << 1));
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
        memcpy(buf + DFOX_NODE_SIZE - old_blk_new_len,
                new_block.buf + kv_last_pos, old_blk_new_len); // Copy back first half to old block
        int16_t diff = DFOX_NODE_SIZE - brk_kv_pos;
        idx = brk_idx;
        while (idx--)
            setPtr(idx, getPtr(idx) + diff);
#if DX_9_BIT_PTR == 1
        byte *block_ptrs = buf + DFOX_HDR_SIZE + DX_MAX_PTR_BITMAP_BYTES + brk_idx;
#else
        byte *block_ptrs = buf + DFOX_HDR_SIZE + (brk_idx << 1);
#endif
        memmove(block_ptrs, trie, BPT_TRIE_LEN);
        trie = block_ptrs;
        setKVLastPos(DFOX_NODE_SIZE - old_blk_new_len);
        setFilledSize(brk_idx);
    }

    {
#if DX_9_BIT_PTR == 1
#if DX_INT64MAP == 1
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
        byte *block_ptrs = new_block.buf + DFOX_HDR_SIZE
                + DX_MAX_PTR_BITMAP_BYTES;
#if DX_9_BIT_PTR == 1
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

void dfox_node_handler::deleteTrieLastHalf(int16_t brk_key_len, byte *first_key, byte *tp) {
    byte *t = 0;
    byte children = 0;
    for (int idx = 0; idx <= brk_key_len; idx++) {
        byte offset = first_key[idx] & 0x07;
        t = trie + tp[idx];
        byte tc = *t;
        *t++ = (tc | x04);
        children = 0;
        if (tc & x02) {
            children = *t;
            children &= ~((idx == brk_key_len ? xFF : xFE) << offset);
                            // ryte_mask[offset] : ryte_incl_mask[offset]);
            *t++ = children;
        }
        *t++ &= ~(xFE << offset); // ryte_incl_mask[offset];
    }
    byte to_skip = BIT_COUNT(children);
    while (to_skip) {
        byte tc = *t++;
        if (tc & x02)
            to_skip += BIT_COUNT(*t++);
        t++;
        if (tc & x04)
            to_skip--;
    }
    BPT_TRIE_LEN = t - trie;

}

void dfox_node_handler::deleteTrieFirstHalf(int16_t brk_key_len, byte *first_key, byte *tp) {
    byte *delete_start = trie;
    int tot_del = 0;
    for (int idx = 0; idx <= brk_key_len; idx++) {
        byte offset = first_key[idx] & 0x07;
        byte *t = trie + tp[idx] - tot_del;
        int count = t - delete_start;
        if (count) {
            BPT_TRIE_LEN -= count;
            memmove(delete_start, t, trie + BPT_TRIE_LEN - delete_start);
            t -= count;
            tot_del += count;
        }
        byte tc = *t++;
        count = 0;
        if (tc & x02) {
            byte children = *t;
            count = BIT_COUNT(children & ~(xFF << offset)); // ryte_mask[offset]];
            children &= (xFF << offset); // left_incl_mask[offset];
            *t++ = children;
        }
        *t++ &= ((idx == brk_key_len ? xFF : xFE) << offset);
                            // left_incl_mask[offset] : left_mask[offset]);
        delete_start = t;
        while (count) {
            tc = *t++;
            if (tc & x02)
                count += BIT_COUNT(*t++);
            t++;
            if (tc & x04)
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

dfox::dfox() {
    root_data = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
    dfox_node_handler root(root_data);
    root.initBuf();
    total_size = maxKeyCountLeaf = maxKeyCountNode = 0;
    numLevels = blockCountLeaf = blockCountNode = 1;
    maxThread = 9999;
    count1 = 0;
}

dfox::~dfox() {
    delete root_data;
}

dfox_node_handler::dfox_node_handler(byte * m) {
    setBuf(m);
    isPut = false;
}

void dfox_node_handler::initBuf() {
    //memset(buf, '\0', DFOX_NODE_SIZE);
    setLeaf(1);
    setFilledSize(0);
    BPT_TRIE_LEN = 0;
    //MID_KEY_LEN = 0;
    setKVLastPos(DFOX_NODE_SIZE);
    trie = buf + DFOX_HDR_SIZE + DX_MAX_PTR_BITMAP_BYTES;
#if DX_INT64MAP == 1
    bitmap = (uint64_t *) (buf + DFOX_HDR_SIZE);
#else
    bitmap1 = (uint32_t *) (buf + DFOX_HDR_SIZE);
    bitmap2 = bitmap1 + 1;
#endif
}

void dfox_node_handler::setBuf(byte *m) {
    buf = m;
    trie = buf + DFOX_HDR_SIZE + DX_MAX_PTR_BITMAP_BYTES
            + filledSize() * (DX_9_BIT_PTR == 1 ? 1 : 2);
#if DX_INT64MAP == 1
    bitmap = (uint64_t *) (buf + DFOX_HDR_SIZE);
#else
    bitmap1 = (uint32_t *) (buf + DFOX_HDR_SIZE);
    bitmap2 = bitmap1 + 1;
#endif
}

void dfox_node_handler::addData() {

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

void dfox_node_handler::insPtr(int16_t pos, int16_t kv_pos) {
    int16_t filledSz = filledSize();
#if DX_9_BIT_PTR == 1
    byte *kvIdx = buf + DFOX_HDR_SIZE + DX_MAX_PTR_BITMAP_BYTES + pos;
    memmove(kvIdx + 1, kvIdx, filledSz - pos + BPT_TRIE_LEN);
    *kvIdx = kv_pos;
    trie++;
#if DX_INT64MAP == 1
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
    byte *kvIdx = buf + DFOX_HDR_SIZE + (pos << 1);
    memmove(kvIdx + 2, kvIdx, (filledSz - pos) * 2 + BPT_TRIE_LEN);
    util::setInt(kvIdx, kv_pos);
    trie += 2;
#endif
    setFilledSize(filledSz + 1);

}

void dfox_node_handler::insBit(uint32_t *ui32, int pos, int16_t kv_pos) {
    uint32_t ryte_part = (*ui32) & util::ryte_mask32[pos];
    ryte_part >>= 1;
    if (kv_pos >= 256)
        ryte_part |= util::mask32[pos];
    (*ui32) = (ryte_part | ((*ui32) & util::left_mask32[pos]));
}

void dfox_node_handler::insBit(uint64_t *ui64, int pos, int16_t kv_pos) {
    uint64_t ryte_part = (*ui64) & util::ryte_mask64[pos];
    ryte_part >>= 1;
    if (kv_pos >= 256)
        ryte_part |= util::mask64[pos];
    (*ui64) = (ryte_part | ((*ui64) & util::left_mask64[pos]));
}

bool dfox_node_handler::isFull(int16_t kv_len) {
    int16_t ptr_size = filledSize() + 1;
#if DX_9_BIT_PTR == 0
    ptr_size <<= 1;
#endif
    if ((getKVLastPos() - kv_len - 2)
            < (DFOX_HDR_SIZE + DX_MAX_PTR_BITMAP_BYTES + BPT_TRIE_LEN
                    + need_count + ptr_size))
        return true;
    if (filledSize() > DX_MAX_PTRS)
        return true;
    if (BPT_TRIE_LEN + need_count > 240)
        return true;
    return false;
}

void dfox_node_handler::setPtr(int16_t pos, int16_t ptr) {
#if DX_9_BIT_PTR == 1
    buf[DFOX_HDR_SIZE + DX_MAX_PTR_BITMAP_BYTES + pos] = ptr;
#if DX_INT64MAP == 1
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
    byte *kvIdx = buf + DFOX_HDR_SIZE + (pos << 1);
    util::setInt(kvIdx, ptr);
#endif
}

int16_t dfox_node_handler::getPtr(int16_t pos) {
#if DX_9_BIT_PTR == 1
    int16_t ptr = buf[DFOX_HDR_SIZE + DX_MAX_PTR_BITMAP_BYTES + pos];
#if DX_INT64MAP == 1
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
    byte *kvIdx = buf + DFOX_HDR_SIZE + (pos << 1);
    return util::getInt(kvIdx);
#endif
}

void dfox_node_handler::updatePtrs(byte *upto, int diff) {
    byte *t = trie;
    byte tc = *t++;
    while (t <= upto) {
        if (tc & x01) {
            t += *t;
        } else if (tc & x02) {
            t += 2;
            if (t < upto && (t + t[1]) >= upto) {
                (*t)++;
                t[1] += diff;
            }
            t += 2;
        } else
            t++;
        tc = *t++;
    }
}

void dfox_node_handler::insertCurrent() {
    byte origTC;
    byte key_char;
    byte mask;
    byte *leafPos;
    int16_t diff;

    switch (insertState) {
    case INSERT_MIDDLE1:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        *origPos &= xFB;
        insAt(triePos, ((key_char & xF8) | x04), mask);
        break;
    case INSERT_MIDDLE2:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        insAt(triePos, key_char & xF8, mask);
        break;
    case INSERT_LEAF:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        origTC = *origPos;
        leafPos = origPos + (origTC & x02 ? 2 : 1);
        *leafPos |= mask;
        break;
    case INSERT_CONVERT:
        byte b, c;
        char cmp_rel;
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        diff = triePos - origPos;
        // 3 possible relationships between key_char and *triePos, 4 possible positions of triePos
        cmp_rel = ((*triePos ^ key_char) > x07 ? (*triePos < key_char ? 0 : 1) : 2);
        if (cmp_rel == 0) {
            triePos = origPos + 1 + (*origPos >> 1);
            char to_skip = 1;
            while (to_skip) {
                byte child_tc = *triePos++;
                if (child_tc & x01) {
                    triePos += (child_tc >> 1);
                    continue;
                }
                if (child_tc & x02)
                    to_skip += BIT_COUNT(*triePos++);
                pos += BIT_COUNT(*triePos++);
                if (child_tc & x04)
                    to_skip--;
            }
            insAt(triePos, (key_char & xF8) | 0x04, 1 << (key_char & x07));
            triePos = origPos + diff;
        }
        diff--;
        c = *triePos;
        if (diff == 0)
            triePos = origPos;
        b = (cmp_rel == 2 ? x04 : x00) | (cmp_rel == 1 ? x00 : x02);
        need_count = *origPos >> 1; // save original count
        *triePos++ = ((cmp_rel == 0 ? c : key_char) & xF8) | b;
        b = (cmp_rel == 1 ? (need_count == 1 ? 3 : 4) : (need_count == 1 ? 1 : 2));
        if (diff) {
            triePos = origPos + diff + 2;
            *origPos = (diff << 1) | x01;
        }
        need_count--;
        need_count -= diff;
        if (need_count)
            b++;
        // this just inserts b number of bytes - buf is dummy
        insAt(triePos + (diff ? 0 : 1), (const char *) buf, b);
        *triePos++ = 1 << ((cmp_rel == 0 ? c : key_char) & x07);
        if (diff && cmp_rel != 1)
            *triePos++ = 1 << (c & x07);
        if (cmp_rel == 1) {
            *triePos++ = (c & xF8) | x06;
            *triePos++ = 1 << (c & x07);
            *triePos++ = 0;
        } else if (diff == 0)
            *triePos++ = ((cmp_rel == 0) ? 0 : (1 << (c & x07)));
        if (need_count)
            *triePos = (need_count << 1) | x01;
        break;
    case INSERT_THREAD:
        int16_t p, min;
        byte c1, c2;
        byte *childPos;
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        origTC = *origPos;
        childPos = origPos + 1;
        if (origTC & x02) {
            *childPos |= mask;
        } else {
            insAt(childPos, mask);
            triePos++;
            origTC |= x02;
            *origPos = origTC;
        }
        c1 = c2 = key_char;
        p = keyPos;
        min = util::min16(key_len, keyPos + key_at_len);
        if (p < min) {
            leafPos = childPos + 1;
            (*leafPos) &= ~mask;
        }
        need_count -= 6;
        if (p + need_count == min && need_count)
            need_count--;
        if (p < min) {
        //while (p < min) {
            if (need_count) {
                insAt(triePos, (need_count << 1) | x01, key + keyPos, need_count);
                triePos += need_count;
                triePos++;
                p += need_count;
            }
            c1 = key[p];
            c2 = key_at[p - keyPos];
            if (c1 > c2) {
                byte swap = c1;
                c1 = c2;
                c2 = swap;
            }
            //switch ((c1 ^ c2) > x07 ?
            //        0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
            switch ((c1 ^ c2) > x07 ? 0 : (c1 == c2 ? 3 : 1)) {
            case 0:
                triePos += insAt(triePos, c1 & xF8, x01 << (c1 & x07),
                        (c2 & xF8) | x04, x01 << (c2 & x07));
                break;
            case 1:
                triePos += insAt(triePos, (c1 & xF8) | x04,
                        (x01 << (c1 & x07)) | (x01 << (c2 & x07)));
                break;
            case 2:
                dfox::count1++;
                triePos += insAt(triePos, (c1 & xF8) | x06, x01 << (c1 & x07),
                        0);
                break;
            case 3:
                triePos += insChildAndLeafAt(triePos, (c1 & xF8) | x06,
                        x01 << (c1 & x07));
                break;
            }
            //if (c1 != c2)
            //    break;
            //p++;
            if (c1 == c2)
                p++;
        }
        diff = p - keyPos;
        keyPos = p + 1;
        if (c1 == c2) {
            c2 = (p == key_len ? key_at[diff] : key[p]);
            insAt(triePos, (c2 & xF8) | x04, (x01 << (c2 & x07)));
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
        mask = x01 << (key_char & x07);
        append((key_char & xF8) | x04);
        append(mask);
        keyPos = 1;
        break;
    }

}

void dfox_node_handler::insThreadAt(byte *ptr, byte b1, byte b2, byte b3, const char *s, byte len) {
    memmove(ptr + 3 + len, ptr, trie + BPT_TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr++ = b3;
    memcpy(ptr, s, len);
    BPT_TRIE_LEN += len;
    BPT_TRIE_LEN += 3;
}

int16_t dfox_node_handler::locate() {
    byte key_char;
    byte *t = trie;
    pos = 0;
    keyPos = 1;
    key_char = *key;
    do {
        byte trie_char = *t;
        int to_skip;
        switch ((trie_char & x01) ? 3 : ((key_char ^ trie_char) > x07 ?
                (key_char > trie_char ? 0 : 2) : 1)) {
        case 0:
            origPos = t++;
            to_skip = (trie_char & x02 ? BIT_COUNT(*t++) : x00);
            pos += BIT_COUNT(*t++);
            while (to_skip) {
                byte child_tc = *t++;
                if (child_tc & x01) {
                    t += (child_tc >> 1);
                    continue;
                }
                if (child_tc & x02)
                    to_skip += BIT_COUNT(*t++);
                pos += BIT_COUNT(*t++);
                if (child_tc & x04)
                    to_skip--;
            }
            if (trie_char & x04) {
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_MIDDLE1;
                    need_count = 3;
                }
                return ~pos;
            }
            break;
        case 1:
            byte r_leaves, r_children, r_mask;
            uint16_t to_skip;
            origPos = t++;
            r_children = (trie_char & x02 ? *t++ : x00);
            r_leaves = *t++;
            key_char &= x07;
            r_mask = ~(xFF << key_char);
            pos += BIT_COUNT(r_leaves & r_mask);
            to_skip = BIT_COUNT(r_children & r_mask);
            while (to_skip) {
                trie_char = *t++;
                if (trie_char & x01) {
                    t += (trie_char >> 1);
                    continue;
                }
                if (trie_char & x02)
                    to_skip += BIT_COUNT(*t++);
                pos += BIT_COUNT(*t++);
                if (trie_char & x04)
                    to_skip--;
            }
            r_mask = (x01 << key_char);
            switch (r_leaves & r_mask ?
                    (r_children & r_mask ? (keyPos == key_len ? 3 : 4) : 2) :
                    (r_children & r_mask ? (keyPos == key_len ? 0 : 1) : 0)) {
            case 0:
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_LEAF;
                    need_count = 3;
                }
                return ~pos;
            case 1:
                break;
            case 2:
                int16_t cmp;
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
                    need_count = cmp + 5;
                    //need_count = (cmp * 3) + 4;
                }
                return ~pos;
            case 3:
                return pos;
            case 4:
                pos++;
                break;
            }
            key_char = key[keyPos++];
            break;
        case 3:
            byte pfx_len;
            origPos = t++;
            pfx_len = (trie_char >> 1);
            while (pfx_len--) {
                if (key_char != *t) {
                    if (isPut) {
                        triePos = t;
                        insertState = INSERT_CONVERT;
                        need_count = 5;
                    }
                    return ~pos;
                }
                t++;
                key_char = key[keyPos++];
            }
            continue;
        case 2:
            if (isPut) {
                triePos = t;
                insertState = INSERT_MIDDLE2;
                need_count = 3;
            }
            return ~pos;
            break;
        }
    } while (1);
    return ~pos;
}

byte dfox_node_handler::insChildAndLeafAt(byte *ptr, byte b1, byte b2) {
    memmove(ptr + 3, ptr, trie + BPT_TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr = b2;
    BPT_TRIE_LEN += 3;
    return 3;
}

void dfox_node_handler::append(byte b) {
    trie[BPT_TRIE_LEN++] = b;
}

byte *dfox_node_handler::getKey(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + getPtr(pos);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *dfox_node_handler::getChildPtr(byte *ptr) {
    ptr += (*ptr + 1);
    return (byte *) util::bytesToPtr(ptr);
}

char *dfox_node_handler::getValueAt(int16_t *vlen) {
    key_at = buf + getPtr(pos);
    key_at += *key_at;
    key_at++;
    *vlen = *key_at;
    key_at++;
    return (char *) key_at;
}

void dfox_node_handler::initVars() {
}

long dfox::count1, dfox::count2;
