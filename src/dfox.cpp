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
    if (node_paths)
        *node_paths = buf;
    while (!isLeaf()) {
        int16_t idx = locate();
        if (idx < 0) {
            idx++;
            idx = ~idx;
        }
        key_at = buf + getPtr(idx);
        setBuf(getChildPtr(key_at));
        if (node_paths)
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
        node.keyPos = 1;
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
                root_data = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
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
                root.keyPos = 1;
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
    do {
        while (ctr > x07) {
            if (tc & x04) {
                keyPos--;
                tc = trie[tp[keyPos]];
                while (tc & x01) {
                    keyPos--;
                    tc = trie[tp[keyPos]];
                }
                child = (tc & x02 ? trie[tp[keyPos] + 3] : 0);
                leaf = trie[tp[keyPos] + (tc & x02 ? 4 : 1)];
                ctr = first_key[keyPos] & 0x07;
                ctr++;
            } else {
                tp[keyPos] = t - trie;
                tc = *t++;
                if (tc & x01) {
                    byte len = tc >> 1;
                    memset(tp + keyPos, t - trie - 1, len);
                    memcpy(first_key + keyPos, t, len);
                    t += len;
                    keyPos += len;
                    tp[keyPos] = t - trie;
                    tc = *t++;
                }
                t += (tc & x02 ? 2 : 0);
                child = (tc & x02 ? *t++ : 0);
                leaf = *t++;
                ctr = 0;
            }
        }
        first_key[keyPos] = (tc & xF8) | ctr;
        byte mask = x01 << ctr;
        if (leaf & mask) {
            leaf &= ~mask;
            if (0 == (child & mask))
                ctr++;
            return t;
        }
        if (child & mask) {
            keyPos++;
            ctr = x08;
            tc = 0;
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
    memcpy(new_block.trie, trie, BPT_TRIE_LEN);
    new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = DFOX_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int16_t brk_idx = -1;
    int16_t brk_kv_pos;
    int16_t tot_len;
    brk_kv_pos = tot_len = 0;
    char ctr = x08;
    byte tp[DFOX_MAX_KEY_PREFIX_LEN];
    byte *t = trie;
    byte tc, child, leaf;
    tc = child = leaf = 0;
    //if (!isLeaf())
    //   cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled size:" << orig_filled_size << endl;
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
            //memcpy(first_key + keyPos + 1, buf + src_idx + 1, buf[src_idx]);
            //first_key[keyPos+1+buf[src_idx]] = 0;
            //cout << first_key << endl;
            if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                //memcpy(first_key + keyPos + 1, buf + src_idx + 1, buf[src_idx]);
                //first_key[keyPos+1+buf[src_idx]] = 0;
                //cout << first_key << ":";
                brk_idx = idx + 1;
                brk_kv_pos = kv_last_pos;
                deleteTrieLastHalf(keyPos, first_key, tp);
                new_block.keyPos = keyPos;
                t = new_block.trie + (t - trie);
                t = new_block.nextKey(first_key, tp, t, ctr, tc, child, leaf);
                keyPos = new_block.keyPos;
                //src_idx = getPtr(idx + 1);
                //memcpy(first_key + keyPos + 1, buf + src_idx + 1, buf[src_idx]);
                //first_key[keyPos+1+buf[src_idx]] = 0;
                //cout << first_key << endl;
            }
        }
    }
    kv_last_pos = getKVLastPos();
#if DX_9_BIT_PTR == 1
    memcpy(buf + DFOX_HDR_SIZE, new_block.buf + DFOX_HDR_SIZE, DX_MAX_PTR_BITMAP_BYTES);
    memcpy(trie + BPT_TRIE_LEN, new_block.trie + new_block.BPT_TRIE_LEN, brk_idx);
#else
    memcpy(trie + BPT_TRIE_LEN, new_block.trie + new_block.BPT_TRIE_LEN, (brk_idx << 1));
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
        byte *block_ptrs = new_block.trie + new_block.BPT_TRIE_LEN;
#if DX_9_BIT_PTR == 1
        memmove(block_ptrs, block_ptrs + brk_idx, new_size);
#else
        memmove(block_ptrs, block_ptrs + (brk_idx << 1), new_size << 1);
#endif
        new_block.setKVLastPos(brk_kv_pos);
        new_block.setFilledSize(new_size);
    }

    return new_block.buf;
}

void dfox_node_handler::movePtrList(byte orig_trie_len) {
#if DX_9_BIT_PTR == 1
    memmove(trie + BPT_TRIE_LEN, trie + orig_trie_len, filledSize());
#else
    memmove(trie + BPT_TRIE_LEN, trie + orig_trie_len, filledSize() << 1);
#endif
}

void dfox_node_handler::deleteTrieLastHalf(int16_t brk_key_len, byte *first_key, byte *tp) {
    byte orig_trie_len = BPT_TRIE_LEN;
    for (int idx = brk_key_len; idx >= 0; idx--) {
        byte *t = trie + tp[idx];
        byte tc = *t;
        if (tc & x01)
            continue;
        byte offset = first_key[idx] & x07;
        *t++ = (tc | x04);
        if (tc & x02) {
            byte children = t[2];
            children &= ~((idx == brk_key_len ? xFF : xFE) << offset);
                            // ryte_mask[offset] : ryte_incl_mask[offset]);
            t[2] = children;
            byte leaves = t[3] & ~(xFE << offset);
            t[3] = leaves; // ryte_incl_mask[offset];
            pos = BIT_COUNT(leaves);
            byte *new_t = skipChildren(t + 4, BIT_COUNT(children));
            *t++ = pos;
            *t = new_t - t;
            t = new_t;
        } else
            *t++ &= ~(xFE << offset); // ryte_incl_mask[offset];
        if (idx == brk_key_len)
            BPT_TRIE_LEN = t - trie;
    }
    movePtrList(orig_trie_len);
}

#ifndef _MSC_VER
__attribute__((always_inline))
#endif
byte *dfox_node_handler::skipChildren(byte *t, int16_t count) {
    while (count) {
        byte tc = *t++;
        if (tc & x01) {
            t += (tc >> 1);
            continue;
        }
        if (tc & x04)
            count--;
        if (tc & x02) {
            pos += *t++;
            t += *t;
        } else
            pos += BIT_COUNT(*t++);
    }
    return t;
}

int dfox_node_handler::deleteSegment(byte *delete_end, byte *delete_start) {
    int count = delete_end - delete_start;
    if (count) {
        BPT_TRIE_LEN -= count;
        memmove(delete_start, delete_end, trie + BPT_TRIE_LEN - delete_start);
    }
    return count;
}

void dfox_node_handler::deleteTrieFirstHalf(int16_t brk_key_len, byte *first_key, byte *tp) {
    byte orig_trie_len = BPT_TRIE_LEN;
    for (int idx = brk_key_len; idx >= 0; idx--) {
        byte *t = trie + tp[idx];
        byte tc = *t++;
        if (tc & x01) {
            byte len = tc >> 1;
            deleteSegment(trie + tp[idx + 1], t + len);
            idx -= len;
            idx++;
            continue;
        }
        byte offset = first_key[idx] & 0x07;
        if (tc & x02) {
            byte children = t[2];
            int16_t count = BIT_COUNT(children & ~(xFF << offset)); // ryte_mask[offset];
            children &= (xFF << offset); // left_incl_mask[offset];
            t[2] = children;
            byte leaves = t[3] & ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
            t[3] = leaves;
            byte *new_t = skipChildren(t + 4, count);
            deleteSegment((idx < brk_key_len) ? (trie + tp[idx + 1]) : new_t, t + 4);
            pos = BIT_COUNT(leaves);
            new_t = skipChildren(t + 4, BIT_COUNT(children));
            *t++ = pos;
            *t = new_t - t;
        } else
            *t &= ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
    }
    deleteSegment(trie + tp[0], trie);
    movePtrList(orig_trie_len);
}

dfox::dfox() {
    root_data = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
    dfox_node_handler root(root_data);
    root.initBuf();
    total_size = maxKeyCountLeaf = maxKeyCountNode = blockCountNode = 0;
    numLevels = blockCountLeaf = 1;
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
    trie = buf + DFOX_HDR_SIZE + DX_MAX_PTR_BITMAP_BYTES;
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
    byte *kvIdx = trie + BPT_TRIE_LEN + pos;
    memmove(kvIdx + 1, kvIdx, filledSz - pos);
    *kvIdx = kv_pos;
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
    byte *kvIdx = trie + BPT_TRIE_LEN + (pos << 1);
    memmove(kvIdx + 2, kvIdx, (filledSz - pos) * 2);
    util::setInt(kvIdx, kv_pos);
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
    if (BPT_TRIE_LEN > (240 - need_count))
        return true;
    return false;
}

void dfox_node_handler::setPtr(int16_t pos, int16_t ptr) {
#if DX_9_BIT_PTR == 1
    byte *kvIdx = trie + BPT_TRIE_LEN + pos;
    *kvIdx = ptr;
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
    byte *kvIdx = trie + BPT_TRIE_LEN + (pos << 1);
    util::setInt(kvIdx, ptr);
#endif
}

int16_t dfox_node_handler::getPtr(int16_t pos) {
#if DX_9_BIT_PTR == 1
    int16_t ptr = trie[BPT_TRIE_LEN + pos];
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
    byte *kvIdx = trie + BPT_TRIE_LEN + (pos << 1);
    return util::getInt(kvIdx);
#endif
}

void dfox_node_handler::updatePtrs(byte *loop_upto, byte *covering_upto, int diff) {
    byte *t = trie;
    byte tc = *t++;
    loop_upto++;
    while (t <= loop_upto) {
        if (tc & x01) {
            t += (tc >> 1);
        } else if (tc & x02) {
            t++;
            if ((t + *t) > covering_upto) {
                *t = *t + diff;
                (*(t-1))++;
                t += 3;
            } else
                t += *t;
        } else
            t++;
        tc = *t++;
    }
}

void dfox_node_handler::insertCurrent() {
    byte key_char, mask;
    int16_t diff;

    key_char = key[keyPos - 1];
    mask = x01 << (key_char & x07);
    switch (insertState) {
    case INSERT_AFTER:
        *origPos &= xFB;
        insAt(triePos, ((key_char & xF8) | x04), mask);
        if (keyPos > 1)
            updatePtrs(origPos - 1, triePos - 1, 2);
        break;
    case INSERT_BEFORE:
        insAt(triePos, key_char & xF8, mask);
        if (keyPos > 1)
            updatePtrs(origPos, triePos + 1, 2);
        break;
    case INSERT_LEAF:
        if (*origPos & x02)
            origPos[4] |= mask;
        else
            origPos[1] |= mask;
        updatePtrs(origPos, origPos + 1, 0);
        break;
#if DX_MIDDLE_PREFIX == 1
    case INSERT_CONVERT:
        byte b, c;
        char cmp_rel;
        diff = triePos - origPos;
        // 3 possible relationships between key_char and *triePos, 4 possible positions of triePos
        c = *triePos;
        cmp_rel = ((c ^ key_char) > x07 ? (c < key_char ? 0 : 1) : 2);
        if (cmp_rel == 0)
            insAt(triePos + key_at_pos, (key_char & xF8) | 0x04, 1 << (key_char & x07));
        if (diff == 1)
            triePos = origPos;
        b = (cmp_rel == 2 ? x04 : x00) | (cmp_rel == 1 ? x00 : x02);
        need_count = (*origPos >> 1) - diff;
        diff--;
        *triePos++ = ((cmp_rel == 0 ? c : key_char) & xF8) | b;
        b = (cmp_rel == 1 ? (diff ? 6 : 5) : (diff ? 4 : 3));
        if (diff)
            *origPos = (diff << 1) | x01;
        if (need_count)
            b++;
        // this just inserts b number of bytes - buf is dummy
        insAt(triePos + (diff ? 0 : 1), (const char *) buf, b);
        updatePtrs(origPos, triePos, b + (cmp_rel == 0 ? 2 : 0));
        if (cmp_rel == 1) {
            *triePos++ = 1 << (key_char & x07);
            *triePos++ = (c & xF8) | x06;
        }
        b = pos;
        pos = (cmp_rel == 2 ? 1 : 0);
        triePos[1] = skipChildren(triePos + need_count + (need_count ? 5 : 4), 1) - triePos - 1;
        *triePos++ = pos;
        triePos++;
        pos = b;
        *triePos++ = 1 << (c & x07);
        if (cmp_rel == 2)
            *triePos++ = 1 << (key_char & x07);
        else
            *triePos++ = 0;
        if (need_count)
            *triePos = (need_count << 1) | x01;
        break;
#endif
    case INSERT_THREAD:
        int16_t p, min;
        byte c1, c2;
        byte *fromPos;
        if (*origPos & x02) {
            origPos[3] |= mask;
            origPos[1]++;
            fromPos = triePos;
        } else {
            insAt(origPos + 1, BIT_COUNT(origPos[1]) + 1, x00, mask);
            triePos += 3;
            fromPos = triePos - 3;
            *origPos |= x02;
        }
        c1 = c2 = key_char;
        p = keyPos;
        min = util::min16(key_len, keyPos + key_at_len);
        if (p < min)
            origPos[4] &= ~mask;
#if DX_MIDDLE_PREFIX == 1
        need_count -= 9;
        if (p + need_count == min && need_count)
            need_count--;
        if (need_count) {
            insAt(triePos, (need_count << 1) | x01, key + keyPos, need_count);
            triePos += need_count;
            triePos++;
            p += need_count;
            //dfox::count1 += need_count;
        }
#else
        need_count -= 10;
        need_count /= 5;
        if (need_count) {
            need_count = 0;
            while (p < min) {
                c1 = key[p];
                c2 = key_at[p - keyPos];
                need_count += ((c1 ^ c2) > x07 ? 4 : (c1 == c2 ? (p + 1 == min ? 7 : 5) : 2));
                if (c1 != c2)
                    break;
                p++;
            }
            p = keyPos;
        }
#endif
        while (p < min) {
            c1 = key[p];
            c2 = key_at[p - keyPos];
            if (c1 > c2) {
                byte swap = c1;
                c1 = c2;
                c2 = swap;
            }
#if DX_MIDDLE_PREFIX == 1
            switch ((c1 ^ c2) > x07 ? 0 : (c1 == c2 ? 3 : 1)) {
#else
            switch ((c1 ^ c2) > x07 ?
                    0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
            case 2:
                need_count -= 5;
                triePos += insAt(triePos, (c1 & xF8) | x06, 2, need_count + 3,
                        x01 << (c1 & x07), 0);
                break;
#endif
            case 0:
                triePos += insAt(triePos, c1 & xF8, x01 << (c1 & x07),
                        (c2 & xF8) | x04, x01 << (c2 & x07));
                break;
            case 1:
                triePos += insAt(triePos, (c1 & xF8) | x04,
                        (x01 << (c1 & x07)) | (x01 << (c2 & x07)));
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
        diff = p - keyPos;
        keyPos = p + 1;
        if (c1 == c2) {
            c2 = (p == key_len ? key_at[diff] : key[p]);
            triePos += insAt(triePos, (c2 & xF8) | x04, (x01 << (c2 & x07)));
            if (p == key_len)
                keyPos--;
        }
        p = triePos - fromPos;
        updatePtrs(origPos - 2, origPos, p + (origPos[2] ? 0 : 0));
        origPos[2] += p;
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
        append((key_char & xF8) | x04);
        append(mask);
        break;
    }

}

#ifndef _MSC_VER
__attribute__((aligned(32)))
__attribute__((hot))
#endif
int16_t dfox_node_handler::locate() {
    byte key_char;
    byte *t = trie;
    pos = 0;
    keyPos = 1;
    key_char = *key;
    do {
        byte trie_char = *t;
        switch ((key_char ^ trie_char) > x07 ? (key_char > trie_char ? 0 : 2) : 1) {
        case 0:
            origPos = t++;
            if (trie_char & x02) {
                pos += *t++;
                t += *t;
            } else
                pos += BIT_COUNT(*t++);
            if (trie_char & x04) {
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_AFTER;
                    need_count = 3;
                }
                return ~pos;
            }
            break;
        case 1:
            origPos = t++;
            if (trie_char & x02) {
                t += 2;
                byte r_children = *t++;
                byte r_leaves = *t++;
                byte r_mask = ~(xFF << (key_char & x07));
                pos += BIT_COUNT(r_leaves & r_mask);
                t = skipChildren(t, BIT_COUNT(r_children & r_mask++));
                key_char = (r_leaves & r_mask ? x01 : x00)
                        | (r_children & r_mask ? x02 : x00)
                        | (keyPos == key_len ? x04 : x00);
            } else {
                byte r_leaves = *t++;
                byte r_mask = ~(xFF << (key_char & x07));
                pos += BIT_COUNT(r_leaves & r_mask);
                key_char = (r_leaves & (r_mask + 1) ? x01 : x00);
            }
            switch (key_char) {
            case x01:
            case x05:
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
#if DX_MIDDLE_PREFIX == 1
                    need_count = cmp + 8;
#else
                    need_count = (cmp * 5) + 5;
#endif
                }
                return ~pos;
            case x02:
                break;
            case x07:
                key_at = getKey(pos, &key_at_len);
                return pos;
            case x03:
                pos++;
                break;
            case x00:
            case x04:
            case x06:
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_LEAF;
                    need_count = 3;
                }
                return ~pos;
            }
            key_char = key[keyPos++];
#if DX_MIDDLE_PREFIX == 1
            if (*t & x01) {
                origPos = t;
                byte pfx_len = (*t++ >> 1);
                while (pfx_len && key_char == *t && keyPos < key_len) {
                    key_char = key[keyPos++];
                    t++;
                    pfx_len--;
                }
                if (!pfx_len)
                    continue;
                triePos = t;
                if (key_char > *t) {
                    t = skipChildren(t + pfx_len, 1);
                    key_at_pos = t - triePos;
                }
                if (isPut) {
                    insertState = INSERT_CONVERT;
                    need_count = 6;
                }
                return ~pos;
            }
#endif
            break;
        case 2:
            if (isPut) {
                triePos = t;
                insertState = INSERT_BEFORE;
                need_count = 3;
            }
            return ~pos;
            break;
        }
    } while (1);
    return ~pos;
}

byte dfox_node_handler::insChildAndLeafAt(byte *ptr, byte b1, byte b2) {
#if DX_9_BIT_PTR == 1
    memmove(ptr + 5, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
#else
    memmove(ptr + 5, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
#endif
    *ptr++ = b1;
    *ptr++ = x02;
    *ptr++ = x05;
    *ptr++ = b2;
    *ptr = b2;
    BPT_TRIE_LEN += 5;
    return 5;
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
    key_at += key_at_len;
    *vlen = *key_at;
    key_at++;
    return (char *) key_at;
}

void dfox_node_handler::initVars() {
}

long dfox::count1, dfox::count2;
