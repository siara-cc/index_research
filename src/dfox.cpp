#include <iostream>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include "dfox.h"
#include "GenTree.h"

char *dfox::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    int16_t pos = -1;
    byte *node_data = root_data;
    dfox_node_handler node(node_data);
    node.key = key;
    node.key_len = key_len;
    recursiveSearchForGet(&node, &pos);
    if (pos < 0)
        return null;
    return (char *) node.getData(pos, pValueLen);
}

void dfox::recursiveSearchForGet(dfox_node_handler *node, int16_t *pIdx) {
    int16_t level = 0;
    int16_t pos = -1;
    byte *node_data = node->buf;
    node->initVars();
    while (!node->isLeaf()) {
        pos = node->locate(level);
        if (pos < 0) {
            pos = ~pos;
            if (pos)
                pos--;
        } else {
            do {
                node_data = node->getChild(pos);
                node->setBuf(node_data);
                level++;
                pos = 0;
            } while (!node->isLeaf());
            *pIdx = pos;
            return;
        }
        node_data = node->getChild(pos);
        node->setBuf(node_data);
        node->initVars();
        level++;
    }
    pos = node->locate(level);
    *pIdx = pos;
    return;
}

void dfox::recursiveSearch(dfox_node_handler *node, int16_t lastSearchPos[],
        byte *node_paths[], int16_t *pIdx) {
    int16_t level = 0;
    byte *node_data = node->buf;
    node->initVars();
    while (!node->isLeaf()) {
        lastSearchPos[level] = node->locate(level);
        node_paths[level] = node->buf;
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
            if (lastSearchPos[level])
                lastSearchPos[level]--;
        } else {
            do {
                node_data = node->getChild(lastSearchPos[level]);
                node->setBuf(node_data);
                level++;
                node_paths[level] = node->buf;
                lastSearchPos[level] = 0;
            } while (!node->isLeaf());
            *pIdx = lastSearchPos[level];
            return;
        }
        node_data = node->getChild(lastSearchPos[level]);
        node->setBuf(node_data);
        node->initVars();
        level++;
    }
    node_paths[level] = node_data;
    lastSearchPos[level] = node->locate(level);
    *pIdx = lastSearchPos[level];
    return;
}

void dfox::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    int16_t lastSearchPos[10];
    byte *block_paths[10];
    dfox_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    node.value = value;
    node.value_len = value_len;
    node.isPut = true;
    if (node.filledSize() == 0) {
        node.insertState = INSERT_EMPTY;
        node.addData(0);
        total_size++;
    } else {
        int16_t pos = -1;
        recursiveSearch(&node, lastSearchPos, block_paths, &pos);
        recursiveUpdate(&node, pos, lastSearchPos, block_paths, numLevels - 1);
    }
}

void dfox::recursiveUpdate(dfox_node_handler *node, int16_t pos,
        int16_t lastSearchPos[], byte *node_paths[], int16_t level) {
    int16_t idx = pos; // lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (node->isFull(node->key_len + node->value_len)) {
            //std::cout << "Full\n" << std::endl;
            //if (maxKeyCount < block->filledSize())
            //    maxKeyCount = block->filledSize();
            //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
            //cout << (int) node->TRIE_LEN << endl;
            maxKeyCount += node->TRIE_LEN;
            //maxKeyCount += node->filledSize();
            int16_t brk_idx;
            byte first_key[40];
            int16_t first_len;
            byte *b = node->split(&brk_idx, first_key, &first_len);
            dfox_node_handler new_block(b);
            new_block.initVars();
            new_block.isPut = true;
            //if (node->isLeaf())
            blockCount++;
            if (root_data == node->buf) {
                blockCount++;
                root_data = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
                dfox_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                char addr[5];
                util::ptrToFourBytes((unsigned long) node->buf, addr);
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = addr;
                root.value_len = sizeof(char *);
                root.insertState = INSERT_EMPTY;
                root.addData(0);
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                root.initVars();
                root.key = (char *) first_key;
                root.key_len = first_len;
                root.value = addr;
                //root.value_len = sizeof(char *);
                root.locate(0);
                root.addData(1);
                root.setLeaf(false);
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                dfox_node_handler parent(parent_data);
                char addr[5];
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                parent.initVars();
                parent.isPut = true;
                parent.key = (char *) first_key;
                parent.key_len = first_len;
                parent.value = addr;
                parent.value_len = sizeof(char *);
                lastSearchPos[prev_level] = parent.locate(prev_level);
                recursiveUpdate(&parent, lastSearchPos[prev_level],
                        lastSearchPos, node_paths, prev_level);
            }
            int16_t cmp = util::compare((char *) first_key, first_len,
                    node->key, node->key_len);
            if (cmp <= 0)
                node->setBuf(new_block.buf);
            node->initVars();
            idx = ~node->locate(level);
            node->addData(idx);
        } else
            node->addData(idx);
    } else {
        //if (node->isLeaf) {
        //    int16_t vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

int16_t dfox_node_handler::findPos(dfox_iterator_status& s, int brk_idx) {
    register byte *t = trie;
    register int pos = 0;
    register int keyPos = 0;
    do {
        register byte tc = s.tc_a[keyPos] = *t;
        s.tp[keyPos] = t - trie;
        t++;
        register byte children = (tc & x02 ? *t++ : x00);
        register byte leaves = (tc & x01 ? *t++ : x00);
        if (children) {
            register byte b = GenTree::first_bit_offset[children];
            s.offset_a[keyPos] = b;
            b = left_incl_mask[b];
            pos += GenTree::bit_count[leaves & b];
            if (pos >= brk_idx) {
                do {
                    if (leaves & (x80 >> s.offset_a[keyPos])) {
                        if (pos == brk_idx)
                            break;
                        pos--;
                    }
                } while (s.offset_a[keyPos]--);
                s.t = t;
                s.child_a[keyPos] = children;
                s.leaf_a[keyPos] = leaves & ryte_mask[s.offset_a[keyPos]];
                if ((children & (x80 >> s.offset_a[keyPos])) == 0)
                    s.offset_a[keyPos]++;
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
            pos += GenTree::bit_count[leaves];
            if (pos >= brk_idx) {
                s.offset_a[keyPos] = 7;
                do {
                    if (leaves & (x80 >> s.offset_a[keyPos])) {
                        if (pos == brk_idx)
                            break;
                        pos--;
                    }
                } while (s.offset_a[keyPos]--);
                s.t = t;
                s.offset_a[keyPos]++;
                s.child_a[keyPos] = 0;
                s.leaf_a[keyPos] = leaves & ryte_incl_mask[s.offset_a[keyPos]];
                this->keyPos = keyPos;
                return keyPos;
            }
        }
        while (!children && (tc & 0x04)) {
            keyPos--;
            tc = s.tc_a[keyPos];
            children = s.child_a[keyPos];
            leaves = s.leaf_a[keyPos];
            register byte b =
                    children ? GenTree::first_bit_offset[children] : x07;
            s.offset_a[keyPos] = b;
            b = left_incl_mask[b];
            pos += GenTree::bit_count[leaves & b];
            if (pos >= brk_idx) {
                do {
                    if (leaves & (x80 >> s.offset_a[keyPos])) {
                        if (pos == brk_idx)
                            break;
                        pos--;
                    }
                } while (s.offset_a[keyPos]--);
                s.t = t;
                if (children & (x80 >> s.offset_a[keyPos]))
                    s.leaf_a[keyPos] &= ~(x80 >> s.offset_a[keyPos]);
                else
                    s.offset_a[keyPos]++;
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
    } while (1); // (t - trie) < TRIE_LEN);
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
            register byte tc, children, leaves;
            s.tp[keyPos] = s.t - trie;
            tc = s.tc_a[keyPos] = *s.t++;
            children = s.child_a[keyPos] = (tc & x02 ? *s.t++ : x00);
            leaves = s.leaf_a[keyPos] = (tc & x01 ? *s.t++ : x00);
            s.offset_a[keyPos] = GenTree::first_bit_offset[children | leaves];
        }
        register byte mask = x80 >> s.offset_a[keyPos];
        if (s.leaf_a[keyPos] & mask) {
            if (s.child_a[keyPos] & mask)
                s.leaf_a[keyPos] &= ~mask;
            else
                s.offset_a[keyPos]++;
            return keyPos;
        }
        if (s.child_a[keyPos] & mask)
            s.offset_a[++keyPos] = x08;
        while (s.offset_a[keyPos] == x07 && (s.tc_a[keyPos] & x04))
            keyPos--;
        s.offset_a[keyPos]++;
    } while (1); // (s.t - trie) < TRIE_LEN);
    return -1;
}

byte *dfox_node_handler::split(int16_t *pbrk_idx, byte *first_key,
        int16_t *first_len_ptr) {
    int16_t orig_filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
    dfox_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    if (!isLeaf())
        new_block.setLeaf(false);
    register int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = DFOX_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    register int16_t brk_idx = -1;
    int16_t brk_kv_pos;
    register int16_t tot_len;
    brk_kv_pos = tot_len = 0;
    // (1) move all data to new_block in order
    register int16_t idx;
    for (idx = 0; idx < orig_filled_size; idx++) {
        register int16_t src_idx = getPtr(idx);
        register int16_t kv_len = buf[src_idx];
        kv_len++;
        kv_len += buf[src_idx + kv_len];
        kv_len++;
        tot_len += kv_len;
        memcpy(new_block.buf + kv_last_pos, buf + src_idx, kv_len);
        new_block.insPtr(idx, kv_last_pos);
        kv_last_pos += kv_len;
        if (brk_idx == -1) {
            //brk_key_len = nextKey(s);
            if (tot_len > halfKVLen) {
                brk_idx = idx + 1;
                brk_kv_pos = kv_last_pos;
            }
        }
    }
    kv_last_pos = getKVLastPos();
    memcpy(new_block.trie, trie, TRIE_LEN);
    new_block.TRIE_LEN = TRIE_LEN;
    memcpy(buf, new_block.buf, IDX_HDR_SIZE + brk_idx);

    {
        dfox_iterator_status s;
        register int16_t brk_key_len = findPos(s, brk_idx);
        deleteTrieLastHalf(brk_key_len, s);

        brk_key_len = nextKey(s) + 1;
        new_block.key_at = (char *) new_block.getKey(brk_idx,
                &new_block.key_at_len);
        if (new_block.key_at_len) {
            memcpy(first_key + brk_key_len, new_block.key_at,
                    new_block.key_at_len);
            *first_len_ptr = brk_key_len + new_block.key_at_len;
        } else
            *first_len_ptr = brk_key_len;
        brk_key_len--;
        new_block.deleteTrieFirstHalf(brk_key_len, s);
        s.offset_a[brk_key_len]--;
        do {
            first_key[brk_key_len] = (s.tc_a[brk_key_len] & xF8)
                    | s.offset_a[brk_key_len];
        } while (brk_key_len--);
    }

    {
        int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
        memcpy(buf + DFOX_NODE_SIZE - old_blk_new_len,
                new_block.buf + kv_last_pos, old_blk_new_len); // Copy back first half to old block
        register int16_t diff = DFOX_NODE_SIZE - brk_kv_pos;
        idx = brk_idx;
        while (idx--)
            setPtr(idx, getPtr(idx) + diff);
        byte *block_ptrs = buf + IDX_HDR_SIZE + brk_idx;
        memmove(block_ptrs, trie, TRIE_LEN);
        setKVLastPos(DFOX_NODE_SIZE - old_blk_new_len);
        setFilledSize(brk_idx);
        trie = block_ptrs;
    }

    {
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

        int16_t new_size = orig_filled_size - brk_idx;
        byte *block_ptrs = new_block.buf + IDX_HDR_SIZE;
        memmove(block_ptrs, block_ptrs + brk_idx,
                new_size + new_block.TRIE_LEN);
        new_block.setKVLastPos(brk_kv_pos);
        new_block.setFilledSize(new_size);
        new_block.trie = block_ptrs + new_size;
    }

    *pbrk_idx = brk_idx;
    return new_block.buf;
}

void dfox_node_handler::deleteTrieLastHalf(int16_t brk_key_len,
        dfox_iterator_status& s) {
    register byte *t = 0;
    register byte children = 0;
    for (register int idx = 0; idx <= brk_key_len; idx++) {
        register byte offset = s.offset_a[idx];
        if (idx == brk_key_len)
            offset--;
        t = trie + s.tp[idx];
        register byte tc = *t;
        *t++ = (tc | x04);
        children = 0;
        if (tc & x02) {
            children = *t;
            children &= (
                    (idx == brk_key_len) ?
                            left_mask[offset] : left_incl_mask[offset]);
            *t++ = children;
        }
        if (tc & x01)
            *t++ &= left_incl_mask[offset];
    }
    register byte to_skip = GenTree::bit_count[children];
    while (to_skip) {
        register byte tc = *t++;
        if (tc & 0x02)
            to_skip += GenTree::bit_count[*t++];
        if (tc & x01)
            t++;
        if (tc & x04)
            to_skip--;
    }
    TRIE_LEN = t - trie;

}

void dfox_node_handler::deleteTrieFirstHalf(int16_t brk_key_len,
        dfox_iterator_status& s) {
    register byte *delete_start = trie;
    register int tot_del = 0;
    for (register int idx = 0; idx <= brk_key_len; idx++) {
        register byte offset = s.offset_a[idx];
        if (idx == brk_key_len)
            offset--;
        register byte *t = trie + s.tp[idx] - tot_del;
        register int count = t - delete_start;
        if (count) {
            TRIE_LEN -= count;
            memmove(delete_start, t, trie + TRIE_LEN - delete_start);
            t -= count;
            tot_del += count;
        }
        register byte tc = *t++;
        count = 0;
        if (tc & x02) {
            register byte children = *t;
            count = GenTree::bit_count[children & left_mask[offset]];
            children &= ryte_incl_mask[offset];
            *t++ = children;
        }
        if (tc & x01) {
            *t++ &= (
                    (idx == brk_key_len) ?
                            ryte_incl_mask[offset] : ryte_mask[offset]);
        }
        delete_start = t;
        while (count) {
            tc = *t++;
            if (tc & 0x02)
                count += GenTree::bit_count[*t++];
            if (tc & x01)
                t++;
            if (tc & x04)
                count--;
        }
        if (t != delete_start) {
            count = t - delete_start;
            TRIE_LEN -= count;
            memmove(delete_start, t, trie + TRIE_LEN - delete_start);
            t -= count;
            tot_del += count;
        }
    }

}

dfox::dfox() {
    root_data = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
    dfox_node_handler root(root_data);
    root.initBuf();
    total_size = maxKeyCount = 0;
    numLevels = blockCount = 1;
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
    memset(buf, '\0', DFOX_NODE_SIZE);
    setLeaf(1);
    FILLED_SIZE = TRIE_LEN = 0;
    setKVLastPos(DFOX_NODE_SIZE);
    trie = buf + IDX_HDR_SIZE;
#if defined(DX_INT64MAP)
    bitmap = (uint64_t *) buf;
#else
    bitmap1 = (uint32_t *) buf;
    bitmap2 = bitmap1 + 1;
#endif
}

void dfox_node_handler::setBuf(byte *m) {
    buf = m;
    trie = buf + IDX_HDR_SIZE + FILLED_SIZE;
#if defined(DX_INT64MAP)
    bitmap = (uint64_t *) buf;
#else
    bitmap1 = (uint32_t *) buf;
    bitmap2 = bitmap1 + 1;
#endif
}

void dfox_node_handler::setKVLastPos(int16_t val) {
    util::setInt(LAST_DATA_PTR, val);
}

int16_t dfox_node_handler::getKVLastPos() {
    return util::getInt(LAST_DATA_PTR);
}

void dfox_node_handler::addData(int16_t pos) {

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
    byte *kvIdx = buf + IDX_HDR_SIZE + pos;
    memmove(kvIdx + 1, kvIdx, filledSz - pos + TRIE_LEN);
    *kvIdx = kv_pos;
    filledSz++;
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
        *bitmap2 |= *GenTree::mask32;
    }
#endif
    FILLED_SIZE = filledSz;
}

void dfox_node_handler::insBit(uint32_t *ui32, int pos, int16_t kv_pos) {
    uint32_t ryte_part = (*ui32) & GenTree::ryte_mask32[pos];
    ryte_part >>= 1;
    if (kv_pos >= 256)
        ryte_part |= GenTree::mask32[pos];
    (*ui32) = (ryte_part | ((*ui32) & GenTree::left_mask32[pos]));

}

void dfox_node_handler::insBit(uint64_t *ui64, int pos, int16_t kv_pos) {
    uint64_t ryte_part = (*ui64) & GenTree::ryte_mask64[pos];
    ryte_part >>= 1;
    if (kv_pos >= 256)
        ryte_part |= GenTree::mask64[pos];
    (*ui64) = (ryte_part | ((*ui64) & GenTree::left_mask64[pos]));
}

bool dfox_node_handler::isLeaf() {
    return IS_LEAF_BYTE;
}

void dfox_node_handler::setLeaf(char isLeaf) {
    IS_LEAF_BYTE = isLeaf;
}

void dfox_node_handler::setFilledSize(int16_t filledSize) {
    FILLED_SIZE = filledSize;
}

bool dfox_node_handler::isFull(int16_t kv_len) {
    //if (TRIE_LEN + need_count + FILLED_SIZE >= TRIE_PTR_AREA_SIZE)
    //    return true;
    if ((getKVLastPos() - kv_len - 2)
            < (IDX_HDR_SIZE + TRIE_LEN + need_count + FILLED_SIZE))
        return true;
    if (FILLED_SIZE > MAX_PTRS)
        return true;
    return false;
}

int16_t dfox_node_handler::filledSize() {
    return FILLED_SIZE;
}

byte *dfox_node_handler::getChild(int16_t pos) {
    byte *idx = getData(pos, &pos);
    unsigned long addr_num = util::fourBytesToPtr(idx);
    byte *ret = (byte *) addr_num;
    return ret;
}

void dfox_node_handler::setPtr(int16_t pos, int16_t ptr) {
    buf[IDX_HDR_SIZE + pos] = ptr;
#if defined(DX_INT64MAP)
    if (ptr >= 256)
        *bitmap |= GenTree::mask64[pos];
    else
        *bitmap &= ~GenTree::mask64[pos];
#else
    if (pos & 0xFFE0) {
        pos -= 32;
        if (ptr >= 256)
        *bitmap2 |= GenTree::mask32[pos];
        else
        *bitmap2 &= ~GenTree::mask32[pos];
    } else {
        if (ptr >= 256)
        *bitmap1 |= GenTree::mask32[pos];
        else
        *bitmap1 &= ~GenTree::mask32[pos];
    }
#endif
}

int16_t dfox_node_handler::getPtr(int16_t pos) {
    int16_t ptr = buf[IDX_HDR_SIZE + pos];
#if defined(DX_INT64MAP)
    if (*bitmap & GenTree::mask64[pos])
        ptr |= 256;
#else
    if (pos & 0xFFE0) {
        if (*bitmap2 & GenTree::mask32[pos - 32])
        ptr |= 256;
    } else {
        if (*bitmap1 & GenTree::mask32[pos])
        ptr |= 256;
    }
#endif
    return ptr;
}

byte *dfox_node_handler::getKey(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + getPtr(pos);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *dfox_node_handler::getData(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + getPtr(pos);
    *plen = kvIdx[0];
    kvIdx++;
    kvIdx += *plen;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

void dfox_node_handler::insertCurrent() {
    register byte origTC;
    register byte key_char;
    register byte mask;
    register byte *leafPos;

    switch (insertState) {
    case INSERT_MIDDLE1:
        *origPos &= xFB;
        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x07);
        insAt(triePos, ((key_char & xF8) | x05), mask);
        break;
    case INSERT_MIDDLE2:
        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x07);
        insAt(triePos, ((key_char & xF8) | x01), mask);
        break;
    case INSERT_LEAF:
        origTC = *origPos;
        leafPos = (origTC & x02 ? origPos + 2 : origPos + 1);
        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x07);
        if (origTC & x01) {
            *leafPos |= mask;
        } else {
            insAt(leafPos, mask);
            *origPos = (origTC | x01);
        }
        break;
    case INSERT_THREAD:
        int16_t p, min;
        byte c1, c2;
        byte *childPos;
        childPos = origPos + 1;
        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x07);
        origTC = *origPos;
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
        min = util::min(key_len, keyPos + key_at_len);
        if ((origTC & x01) && (p < min)) {
            leafPos = childPos;
            leafPos++;
            byte leaf = *leafPos;
            leaf &= ~mask;
            if (leaf)
                *leafPos = leaf;
            else {
                delAt(leafPos);
                triePos--;
                origTC &= xFE;
                *origPos = origTC;
            }
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
                triePos += ins4BytesAt(triePos, (c1 & xF8) | x01,
                        x80 >> (c1 & x07), (c2 & xF8) | x05, x80 >> (c2 & x07));
                break;
            case 1:
                triePos += insAt(triePos, (c1 & xF8) | x05,
                        (x80 >> (c1 & x07)) | (x80 >> (c2 & x07)));
                break;
            case 2:
                triePos += insAt(triePos, (c1 & xF8) | x06, x80 >> (c1 & x07));
                break;
            case 3:
                triePos += insChildAndLeafAt(triePos, (c1 & xF8) | x07,
                        x80 >> (c1 & x07));
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
            insAt(triePos, (c2 & xF8) | x05, (x80 >> (c2 & x07)));
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
        mask = x80 >> (key_char & x07);
        append((key_char & xF8) | x05);
        append(mask);
        keyPos = 1;
        break;
    }
}

int16_t dfox_node_handler::locate(int16_t level) {
    keyPos = 1;
    register byte key_char = *key;
    register int16_t pos = 0;
    register byte *t = trie;
    do {
        register byte trie_char = *t;
        register int to_skip;
        switch ((key_char ^ trie_char) > x07 ?
                (key_char > trie_char ? 0 : 2) : 1) {
        case 0:
            origPos = t++;
            to_skip = (trie_char & x02 ? GenTree::bit_count[*t++] : x00);
            if (trie_char & x01)
                pos += GenTree::bit_count[*t++];
            while (to_skip) {
                register byte child_tc = *t++;
                if (child_tc & x02)
                    to_skip += GenTree::bit_count[*t++];
                if (child_tc & x01)
                    pos += GenTree::bit_count[*t++];
                if (child_tc & x04)
                    to_skip--;
            }
            if (trie_char & x04) {
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_MIDDLE1;
                    need_count = 2;
                }
                return ~pos;
            }
            break;
        case 1:
            register byte r_leaves, r_children, r_mask;
            origPos = t++;
            r_children = (trie_char & x02 ? *t++ : x00);
            r_leaves = (trie_char & x01 ? *t++ : x00);
            key_char &= x07;
            r_mask = left_mask[key_char];
            pos += GenTree::bit_count[r_leaves & r_mask];
            to_skip = GenTree::bit_count[r_children & r_mask];
            while (to_skip) {
                trie_char = *t++;
                if (trie_char & x02)
                    to_skip += GenTree::bit_count[*t++];
                if (trie_char & x01)
                    pos += GenTree::bit_count[*t++];
                if (trie_char & x04)
                    to_skip--;
            }
            r_mask = (x80 >> key_char);
            switch (r_leaves & r_mask ?
                    (r_children & r_mask ? (keyPos == key_len ? 3 : 4) : 2) :
                    (r_children & r_mask ? (keyPos == key_len ? 0 : 1) : 0)) {
            //switch (r_children & r_mask ?
            //        (r_leaves & r_mask ?
            //                ((keyPos ^ key_len) ? 4 : 3) :
            //                ((keyPos ^ key_len) ? 1 : 0)) :
            //        (r_leaves & r_mask ? 2 : 0)) {
            case 0:
                if (isPut) {
                    insertState = INSERT_LEAF;
                    need_count = 2;
                }
                return ~pos;
            case 1:
                break;
            case 2:
                register int16_t cmp;
                key_at = (char *) getKey(pos, &key_at_len);
                cmp = util::compare(key + keyPos, key_len - keyPos, key_at,
                        key_at_len);
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
                    need_count = (cmp << 1) + 4;
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
                need_count = 2;
            }
            return ~pos;
            break;
        }
    } while (1);
    return ~pos;
}

void dfox_node_handler::delAt(byte *ptr) {
    TRIE_LEN--;
    memmove(ptr, ptr + 1, trie + TRIE_LEN - ptr);
}

void dfox_node_handler::delAt(byte *ptr, int16_t count) {
    TRIE_LEN -= count;
    memmove(ptr, ptr + count, trie + TRIE_LEN - ptr);
}

void dfox_node_handler::insAt(byte *ptr, byte b) {
    memmove(ptr + 1, ptr, trie + TRIE_LEN - ptr);
    *ptr = b;
    TRIE_LEN++;
}

byte dfox_node_handler::insAt(byte *ptr, byte b1, byte b2) {
    memmove(ptr + 2, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr = b2;
    TRIE_LEN += 2;
    return 2;
}

byte dfox_node_handler::insAt(byte *ptr, byte b1, byte b2, byte b3) {
    memmove(ptr + 3, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr = b3;
    TRIE_LEN += 3;
    return 3;
}

byte dfox_node_handler::ins4BytesAt(byte *ptr, byte b1, byte b2, byte b3,
        byte b4) {
    memmove(ptr + 4, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr++ = b3;
    *ptr = b4;
    TRIE_LEN += 4;
    return 4;
}

byte dfox_node_handler::insChildAndLeafAt(byte *ptr, byte b1, byte b2) {
    memmove(ptr + 3, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr = b2;
    TRIE_LEN += 3;
    return 3;
}

void dfox_node_handler::setAt(byte pos, byte b) {
    trie[pos] = b;
}

void dfox_node_handler::append(byte b) {
    trie[TRIE_LEN++] = b;
}

byte dfox_node_handler::getAt(byte pos) {
    return trie[pos];
}

void dfox_node_handler::initVars() {
}

byte dfox_node_handler::left_mask[8] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8,
        0xFC, 0xFE };
byte dfox_node_handler::left_incl_mask[8] = { 0x80, 0xC0, 0xE0, 0xF0, 0xF8,
        0xFC, 0xFE, 0xFF };
byte dfox_node_handler::ryte_mask[8] = { 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03,
        x01, 0x00 };
byte dfox_node_handler::ryte_incl_mask[8] = { 0xFF, 0x7F, 0x3F, 0x1F, 0x0F,
        0x07, 0x03, x01 };
