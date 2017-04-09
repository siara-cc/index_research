#include <math.h>
#ifndef ARDUINO
#include <malloc.h>
#endif
#include <stdint.h>
#include "dfox.h"
#include "GenTree.h"

char *dfox::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    dfox_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    if (!node.isLeaf())
        node.traverseToLeaf();
    if (node.locate() < 0)
        return null;
    char * ret = node.getValueAt(pValueLen);
    return ret;
}

void dfox_node_handler::traverseToLeaf(byte *node_paths[]) {
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
        if (!node.isLeaf())
            node.traverseToLeaf(node_paths);
        node.locate();
        recursiveUpdate(&node, -1, node_paths, numLevels - 1);
    }
}

void dfox::recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
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
                //maxKeyCount += node->BPT_TRIE_LEN;
            //maxKeyCount += node->PREFIX_LEN;
            byte first_key[64];
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
                idx = ~new_block.locate();
                new_block.addData();
            } else {
                node->initVars();
                idx = ~node->locate();
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
                util::ptrToBytes((unsigned long) node->buf, addr);
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = (char *) addr;
#if defined(ENV64BIT)
                root.value_len = 5;
#else
                root.value_len = 4;
#endif
                root.pos = 0;
                root.insertState = INSERT_EMPTY;
                root.addData();
                util::ptrToBytes((unsigned long) new_block.buf, addr);
                root.initVars();
                root.key = (char *) first_key;
                root.key_len = first_len;
                root.value = (char *) addr;
                //root.value_len = sizeof(char *);
                root.locate();
                root.addData();
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                dfox_node_handler parent(parent_data);
                byte addr[9];
                util::ptrToBytes((unsigned long) new_block.buf, addr);
                parent.initVars();
                parent.isPut = true;
                parent.key = (char *) first_key;
                parent.key_len = first_len;
                parent.value = (char *) addr;
#if defined(ENV64BIT)
                parent.value_len = 5;
#else
                parent.value_len = 4;
#endif
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

int16_t dfox_node_handler::findPos(dfox_iterator_status& s, int brk_idx) {
    byte *t = trie;
    int pos = 0;
    int keyPos = 0;
    do {
        byte tc = s.tc_a[keyPos] = *t;
        s.tp[keyPos] = t - trie;
        t++;
        byte children = (tc & x02 ? *t++ : x00);
        byte leaves = *t++;
        if (children) {
            byte b = util::first_bit_offset[children];
            s.offset_a[keyPos] = b;
            b = ryte_incl_mask[b];
            pos += util::bit_count[leaves & b];
            if (pos >= brk_idx) {
                do {
                    if (leaves & (x01 << s.offset_a[keyPos])) {
                        if (pos == brk_idx)
                            break;
                        pos--;
                    }
                } while (s.offset_a[keyPos]--);
                s.t = t;
                s.child_a[keyPos] = children;
                s.leaf_a[keyPos] = leaves & left_mask[s.offset_a[keyPos]];
                this->keyPos = keyPos;
                return keyPos;
            }
            b = ~b;
            children &= b;
            s.child_a[keyPos] = children;
            s.leaf_a[keyPos] = leaves & b;
            keyPos++;
            if (!children)
                continue;
        } else {
            pos += util::bit_count[leaves];
            if (pos >= brk_idx) {
                s.offset_a[keyPos] = 7;
                do {
                    if (leaves & (x01 << s.offset_a[keyPos])) {
                        if (pos == brk_idx)
                            break;
                        pos--;
                    }
                } while (s.offset_a[keyPos]--);
                s.t = t;
                s.child_a[keyPos] = 0;
                s.leaf_a[keyPos] = leaves & left_mask[s.offset_a[keyPos]];
                this->keyPos = keyPos;
                return keyPos;
            }
        }
        while (!children && (tc & 0x04)) {
            keyPos--;
            tc = s.tc_a[keyPos];
            children = s.child_a[keyPos];
            leaves = s.leaf_a[keyPos];
            byte b = children ? util::first_bit_offset[children] : x07;
            s.offset_a[keyPos] = b;
            b = ryte_incl_mask[b];
            pos += util::bit_count[leaves & b];
            if (pos >= brk_idx) {
                do {
                    if (leaves & (x01 << s.offset_a[keyPos])) {
                        if (pos == brk_idx)
                            break;
                        pos--;
                    }
                } while (s.offset_a[keyPos]--);
                s.t = t;
                s.leaf_a[keyPos] = leaves & left_mask[s.offset_a[keyPos]];
                this->keyPos = keyPos;
                return keyPos;
            }
            if (children) {
                b = ~b;
                children &= b;
                s.child_a[keyPos] = children;
                s.leaf_a[keyPos] = leaves & b;
                keyPos++;
                break;
            }
        }
    } while (1); // (t - trie) < BPT_TRIE_LEN);
    return -1;
}

int16_t dfox_node_handler::nextKey(dfox_iterator_status& s) {
    if (s.t == trie) {
        keyPos = 0;
        s.offset_a[keyPos] = x08;
    } else {
        while (s.offset_a[keyPos] == x08 && (s.tc_a[keyPos] & x04))
            s.offset_a[--keyPos]++;
    }
    do {
        if (s.offset_a[keyPos] > x07) {
            byte tc, children, leaves;
            s.tp[keyPos] = s.t - trie;
            tc = s.tc_a[keyPos] = *s.t++;
            children = s.child_a[keyPos] = (tc & x02 ? *s.t++ : x00);
            leaves = s.leaf_a[keyPos] = *s.t++;
            s.offset_a[keyPos] = util::first_bit_offset[children | leaves];
        }
        byte mask = x01 << s.offset_a[keyPos];
        if (s.leaf_a[keyPos] & mask) {
            if (s.child_a[keyPos] & mask)
                s.leaf_a[keyPos] &= ~mask;
            return keyPos;
        }
        if (s.child_a[keyPos] & mask)
            s.offset_a[++keyPos] = x08;
        while (s.offset_a[keyPos] == x07 && (s.tc_a[keyPos] & x04))
            keyPos--;
        s.offset_a[keyPos]++;
    } while (1); // (s.t - trie) < BPT_TRIE_LEN);
    return -1;
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
    // (1) move all data to new_block in order
    int16_t idx;
    for (idx = 0; idx < orig_filled_size; idx++) {
        int16_t src_idx = getPtr(idx);
        int16_t kv_len = buf[src_idx];
        kv_len++;
        if (isLeaf()) {
            kv_len += buf[src_idx + kv_len];
            kv_len++;
        } else
#if defined(ENV64BIT)
            kv_len += 5;
#else
            kv_len += 4;
#endif
        tot_len += kv_len;
        memcpy(new_block.buf + kv_last_pos, buf + src_idx, kv_len);
        new_block.insPtr(idx, kv_last_pos);
        kv_last_pos += kv_len;
        if (brk_idx == -1) {
            //brk_key_len = nextKey(s);
            //if (tot_len > halfKVLen) {
            if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                brk_idx = idx + 1;
                brk_kv_pos = kv_last_pos;
            }
        }
    }
    kv_last_pos = getKVLastPos();
    memcpy(new_block.trie, trie, BPT_TRIE_LEN);
    new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
#if DX_9_BIT_PTR == 1
    memcpy(buf, new_block.buf, DFOX_HDR_SIZE + DX_MAX_PTR_BITMAP_BYTES + brk_idx);
#else
    memcpy(buf, new_block.buf, DFOX_HDR_SIZE + (brk_idx << 1));
#endif

    {
        dfox_iterator_status s;
        int16_t brk_key_len = findPos(s, brk_idx);
        deleteTrieLastHalf(brk_key_len, s);

        brk_key_len = nextKey(s) + 1;
        new_block.key_at = new_block.getKey(brk_idx, &new_block.key_at_len);
        if (isLeaf())
            *first_len_ptr = brk_key_len;
        else {
            if (new_block.key_at_len) {
                memcpy(first_key + brk_key_len, new_block.key_at,
                        new_block.key_at_len);
                *first_len_ptr = brk_key_len + new_block.key_at_len;
            } else
                *first_len_ptr = brk_key_len;
        }
        brk_key_len--;
        new_block.deleteTrieFirstHalf(brk_key_len, s);
        do {
            first_key[brk_key_len] = (s.tc_a[brk_key_len] & xF8)
                    | s.offset_a[brk_key_len];
        } while (brk_key_len--);
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
#if defined(DX_INT64MAP)
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

void dfox_node_handler::deleteTrieLastHalf(int16_t brk_key_len,
        dfox_iterator_status& s) {
    byte *t = 0;
    byte children = 0;
    for (int idx = 0; idx <= brk_key_len; idx++) {
        byte offset = s.offset_a[idx];
        t = trie + s.tp[idx];
        byte tc = *t;
        *t++ = (tc | x04);
        children = 0;
        if (tc & x02) {
            children = *t;
            children &= (
                    (idx == brk_key_len) ?
                            ryte_mask[offset] : ryte_incl_mask[offset]);
            *t++ = children;
        }
        *t++ &= ryte_incl_mask[offset];
    }
    byte to_skip = util::bit_count[children];
    while (to_skip) {
        byte tc = *t++;
        if (tc & x02)
            to_skip += util::bit_count[*t++];
        t++;
        if (tc & x04)
            to_skip--;
    }
    BPT_TRIE_LEN = t - trie;

}

void dfox_node_handler::deleteTrieFirstHalf(int16_t brk_key_len,
        dfox_iterator_status& s) {
    byte *delete_start = trie;
    int tot_del = 0;
    for (int idx = 0; idx <= brk_key_len; idx++) {
        byte offset = s.offset_a[idx];
        byte *t = trie + s.tp[idx] - tot_del;
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
            count = util::bit_count[children & ryte_mask[offset]];
            children &= left_incl_mask[offset];
            *t++ = children;
        }
        *t++ &= (
                    (idx == brk_key_len) ?
                            left_incl_mask[offset] : left_mask[offset]);
        delete_start = t;
        while (count) {
            tc = *t++;
            if (tc & x02)
                count += util::bit_count[*t++];
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
#if defined(DX_INT64MAP)
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
#if defined(DX_INT64MAP)
    bitmap = (uint64_t *) (buf + DFOX_HDR_SIZE);
#else
    bitmap1 = (uint32_t *) (buf + DFOX_HDR_SIZE);
    bitmap2 = bitmap1 + 1;
#endif
}

void dfox_node_handler::addData() {

    insertCurrent();

    int16_t key_left = key_len - keyPos;
    int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 1);
    if (isLeaf())
        kv_last_pos--;
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_left;
    if (key_left)
        memcpy(buf + kv_last_pos + 1, key + keyPos, key_left);
    if (isLeaf()) {
        buf[kv_last_pos + key_left + 1] = value_len;
        memcpy(buf + kv_last_pos + key_left + 2, value, value_len);
    } else {
        memcpy(buf + kv_last_pos + key_left + 1, value, value_len);
    }

    insPtr(pos, kv_last_pos);

}

void dfox_node_handler::insPtr(int16_t pos, int16_t kv_pos) {
    int16_t filledSz = filledSize();
#if DX_9_BIT_PTR == 1
    byte *kvIdx = buf + DFOX_HDR_SIZE + DX_MAX_PTR_BITMAP_BYTES + pos;
    memmove(kvIdx + 1, kvIdx, filledSz - pos + BPT_TRIE_LEN);
    *kvIdx = kv_pos;
    trie++;
#if defined(DX_INT64MAP)
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
#if defined(DX_INT64MAP)
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
#if defined(DX_INT64MAP)
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

void dfox_node_handler::insertCurrent() {
    byte origTC;
    byte key_char;
    byte mask;
    byte *leafPos;

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
        while (p < min) {
            c1 = key[p];
            c2 = key_at[p - keyPos];
            if (c1 > c2) {
                byte swap = c1;
                c1 = c2;
                c2 = swap;
            }
            switch ((c1 ^ c2) > x07 ?
                    0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
            case 0:
                triePos += insAt(triePos, c1 & xF8, x01 << (c1 & x07),
                        (c2 & xF8) | x04, x01 << (c2 & x07));
                break;
            case 1:
                triePos += insAt(triePos, (c1 & xF8) | x04,
                        (x01 << (c1 & x07)) | (x01 << (c2 & x07)));
                break;
            case 2:
                triePos += insAt(triePos, (c1 & xF8) | x06, x01 << (c1 & x07),
                        0);
                break;
            case 3:
                triePos += insChildAndLeafAt(triePos, (c1 & xF8) | x06,
                        x01 << (c1 & x07));
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

int16_t dfox_node_handler::locate() {
    byte key_char;
    byte *t = trie;
    pos = 0;
    keyPos = 1;
    key_char = *key;
    do {
        byte trie_char = *t;
        int to_skip;
        switch ((key_char ^ trie_char) > x07 ?
                (key_char > trie_char ? 0 : 2) : 1) {
        case 0:
            origPos = t++;
            to_skip = (trie_char & x02 ? util::bit_count[*t++] : x00);
            pos += util::bit_count[*t++];
            while (to_skip) {
                byte child_tc = *t++;
                if (child_tc & x02)
                    to_skip += util::bit_count[*t++];
                pos += util::bit_count[*t++];
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
            origPos = t++;
            r_children = (trie_char & x02 ? *t++ : x00);
            r_leaves = *t++;
            key_char &= x07;
            r_mask = ryte_mask[key_char];
            pos += util::bit_count[r_leaves & r_mask];
            to_skip = util::bit_count[r_children & r_mask];
            while (to_skip) {
                trie_char = *t++;
                if (trie_char & x02)
                    to_skip += util::bit_count[*t++];
                pos += util::bit_count[*t++];
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
                    need_count = (cmp * 3) + 4;
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

byte dfox_node_handler::left_mask[8] = { 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0,
        x80, 0x00 };
byte dfox_node_handler::left_incl_mask[8] = { 0xFF, 0xFE, 0xFC, 0xF8, 0xF0,
        0xE0, 0xC0, 0x80 };
byte dfox_node_handler::ryte_mask[8] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F,
        0x3F, 0x7F };
byte dfox_node_handler::ryte_incl_mask[8] = { 0x01, 0x03, 0x07, 0x0F, 0x1F,
        0x3F, 0x7F, 0xFF };
