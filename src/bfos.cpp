#include <iostream>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include "bfos.h"
#include "GenTree.h"

char *bfos::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    int16_t pos = -1;
    byte *node_data = root_data;
    bfos_node_handler node(node_data);
    node.key = key;
    node.key_len = key_len;
    recursiveSearchForGet(&node, &pos);
    if (pos < 0)
        return null;
    return (char *) node.getData(pos, pValueLen);
}

void bfos::recursiveSearchForGet(bfos_node_handler *node, int16_t *pIdx) {
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
            /*            do {
             node_data = node->getChild(pos);
             node->setBuf(node_data);
             level++;
             pos = 0;
             } while (!node->isLeaf());
             *pIdx = pos;
             return;*/
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

void bfos::recursiveSearch(bfos_node_handler *node, int16_t lastSearchPos[],
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
            /*            do {
             node_data = node->getChild(lastSearchPos[level]);
             node->setBuf(node_data);
             level++;
             node_paths[level] = node->buf;
             lastSearchPos[level] = 0;
             } while (!node->isLeaf());
             *pIdx = lastSearchPos[level];
             return;*/
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

void bfos::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    int16_t lastSearchPos[10];
    byte *block_paths[10];
    bfos_node_handler node(root_data);
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

void bfos::recursiveUpdate(bfos_node_handler *node, int16_t pos,
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
            if (node->isLeaf())
                maxKeyCount += node->filledSize();
            //maxKeyCount += node->TRIE_LEN;
            int16_t brk_idx;
            byte first_key[40];
            int16_t first_len;
            byte *b = node->split(&brk_idx, first_key, &first_len);
            bfos_node_handler new_block(b);
            new_block.isPut = true;
            int16_t cmp = util::compare((char *) first_key, first_len,
                    node->key, node->key_len);
            if (cmp <= 0) {
                new_block.initVars();
                new_block.key = node->key;
                new_block.key_len = node->key_len;
                new_block.value = node->value;
                new_block.value_len = node->value_len;
                idx = ~new_block.locate(level);
                new_block.addData(idx);
            } else {
                node->initVars();
                idx = ~node->locate(level);
                node->addData(idx);
            }
            if (node->isLeaf())
                blockCount++;
            if (root_data == node->buf) {
                //blockCount++;
                root_data = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
                bfos_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                root.setLeaf(false);
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
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                bfos_node_handler parent(parent_data);
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

int16_t bfos_node_handler::nextKey(bfos_iterator_status& s) {
    register byte tc, children, leaves;
    if (s.t == trie) {
        keyPos = 0;
        s.offset_a[keyPos] = x08;
    } else {
        while (s.offset_a[keyPos] == x08 && (tc & x04))
            s.offset_a[--keyPos]++;
    }
    do {
        if (s.offset_a[keyPos] > x07) {
            s.tp[keyPos] = s.t - trie;
            tc = *s.t++;
            children = *s.t++;
            leaves = *s.t++;
            s.offset_a[keyPos] = GenTree::first_bit_offset[children | leaves];
        }
        register byte mask = x80 >> s.offset_a[keyPos];
        if (leaves & mask) {
            if (children & mask) // !!!
                leaves &= ~mask;
            return keyPos;
        }
        if (children & mask)
            s.offset_a[++keyPos] = x08;
        while (s.offset_a[keyPos] == x07 && (tc & x04)) // reload tc, c, l
            keyPos--;
        s.offset_a[keyPos]++;
    } while (1); // (s.t - trie) < TRIE_LEN);
    return -1;
}

byte *bfos_node_handler::split(int16_t *pbrk_idx, byte *first_key, // invalid
        int16_t *first_len_ptr) {
    int16_t orig_filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
    bfos_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    if (!isLeaf())
        new_block.setLeaf(false);
    register int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = BFOS_NODE_SIZE - kv_last_pos + 1;
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
#if BFOS_9_BIT_PTR == 1
    memcpy(buf, new_block.buf, BFOS_HDR_SIZE + brk_idx);
#else
    memcpy(buf, new_block.buf, BFOS_HDR_SIZE + (brk_idx << 1));
#endif

    {
        bfos_iterator_status s;
        register int16_t brk_key_len = 0; //findPos(s, brk_idx);
        deleteTrieLastHalf(brk_key_len, s);

        brk_key_len = nextKey(s) + 1;
        new_block.key_at = (char *) new_block.getKey(brk_idx,
                &new_block.key_at_len);
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
            first_key[brk_key_len] = (s.tp[brk_key_len] & xF8) // !!!
            | s.offset_a[brk_key_len];
        } while (brk_key_len--);
    }

    {
        int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
        memcpy(buf + BFOS_NODE_SIZE - old_blk_new_len,
                new_block.buf + kv_last_pos, old_blk_new_len); // Copy back first half to old block
        register int16_t diff = BFOS_NODE_SIZE - brk_kv_pos;
        idx = brk_idx;
        while (idx--)
            setPtr(idx, getPtr(idx) + diff);
#if BFOS_9_BIT_PTR == 1
        byte *block_ptrs = buf + BFOS_HDR_SIZE + brk_idx;
#else
        byte *block_ptrs = buf + BFOS_HDR_SIZE + (brk_idx << 1);
#endif
        memmove(block_ptrs, trie, TRIE_LEN);
        trie = block_ptrs;
        setKVLastPos(BFOS_NODE_SIZE - old_blk_new_len);
        setFilledSize(brk_idx);
    }

    {
#if BFOS_9_BIT_PTR == 1
#if defined(BFOS_INT64MAP)
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
        byte *block_ptrs = new_block.buf + BFOS_HDR_SIZE;
#if BFOS_9_BIT_PTR == 1
        memmove(block_ptrs, block_ptrs + brk_idx,
                new_size + new_block.TRIE_LEN);
        new_block.trie = block_ptrs + new_size;
#else
        memmove(block_ptrs, block_ptrs + (brk_idx << 1),
                (new_size << 1) + new_block.TRIE_LEN);
        new_block.trie = block_ptrs + (new_size * 2);
#endif
        new_block.setKVLastPos(brk_kv_pos);
        new_block.setFilledSize(new_size);
    }

    *pbrk_idx = brk_idx;
    return new_block.buf;
}

void bfos_node_handler::deleteTrieLastHalf(int16_t brk_key_len, // invalid
        bfos_iterator_status& s) {
    register byte *t = 0;
    register byte children = 0;
    for (register int idx = 0; idx <= brk_key_len; idx++) {
        register byte offset = s.offset_a[idx];
        t = trie + s.tp[idx];
        register byte tc = *t;
        *t++ = (tc | x04);
#if BFOS_UNIT_SZ_3 == 0
        children = 0;
        if (tc & x02) {
#endif
        children = *t;
        children &= (
                (idx == brk_key_len) ?
                        left_mask[offset] : left_incl_mask[offset]);
        *t++ = children;
#if BFOS_UNIT_SZ_3 == 0
    }
    if (tc & x01)
#endif
        *t++ &= left_incl_mask[offset];
    }
    register byte to_skip = GenTree::bit_count[children];
    while (to_skip) {
        register byte tc = *t++;
#if BFOS_UNIT_SZ_3 == 0
        if (tc & x02)
#endif
        to_skip += GenTree::bit_count[*t++];
#if BFOS_UNIT_SZ_3 == 0
        if (tc & x01)
#endif
        t++;
        if (tc & x04)
            to_skip--;
    }
    TRIE_LEN = t - trie;

}

void bfos_node_handler::deleteTrieFirstHalf(int16_t brk_key_len, // invalid
        bfos_iterator_status& s) {
    register byte *delete_start = trie;
    register int tot_del = 0;
    for (register int idx = 0; idx <= brk_key_len; idx++) {
        register byte offset = s.offset_a[idx];
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
#if BFOS_UNIT_SZ_3 == 0
        if (tc & x02) {
#endif
        register byte children = *t;
        count = GenTree::bit_count[children & left_mask[offset]];
        children &= ryte_incl_mask[offset];
        *t++ = children;
#if BFOS_UNIT_SZ_3 == 0
    }
    if (tc & x01) {
#endif
        *t++ &= (
                (idx == brk_key_len) ?
                        ryte_incl_mask[offset] : ryte_mask[offset]);
#if BFOS_UNIT_SZ_3 == 0
    }
#endif
        delete_start = t;
        while (count) {
            tc = *t++;
#if BFOS_UNIT_SZ_3 == 0
            if (tc & x02)
#endif
            count += GenTree::bit_count[*t++];
#if BFOS_UNIT_SZ_3 == 0
            if (tc & x01)
#endif
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

bfos::bfos() {
    root_data = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
    bfos_node_handler root(root_data);
    root.initBuf();
    total_size = maxKeyCount = 0;
    numLevels = blockCount = 1;
    maxThread = 9999;
}

bfos::~bfos() {
    delete root_data;
}

bfos_node_handler::bfos_node_handler(byte * m) {
    setBuf(m);
    isPut = false;
}

void bfos_node_handler::initBuf() {
    //memset(buf, '\0', BFOS_NODE_SIZE);
    setLeaf(1);
    setFilledSize(0);
    TRIE_LEN = 0;
    setKVLastPos(BFOS_NODE_SIZE);
    trie = buf + BFOS_HDR_SIZE;
}

void bfos_node_handler::setBuf(byte *m) {
    buf = m;
    trie = buf + BFOS_HDR_SIZE;
}

void bfos_node_handler::setKVLastPos(int16_t val) {
    util::setInt(LAST_DATA_PTR, val);
}

int16_t bfos_node_handler::getKVLastPos() {
    return util::getInt(LAST_DATA_PTR);
}

void bfos_node_handler::addData(int16_t pos) {

    int16_t ptr = insertCurrent();

    int16_t key_left = key_len - keyPos;
    int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
    setKVLastPos(kv_last_pos);
    util::setInt(buf + ptr, kv_last_pos);
    buf[kv_last_pos] = key_left;
    if (key_left)
        memcpy(buf + kv_last_pos + 1, key + keyPos, key_left);
    buf[kv_last_pos + key_left + 1] = value_len;
    memcpy(buf + kv_last_pos + key_left + 2, value, value_len);
    setFilledSize(filledSize() + 1);
    //insPtr(pos, kv_last_pos);

}

void bfos_node_handler::insPtr(int16_t pos, int16_t kv_pos) {
    int16_t filledSz = filledSize();
#if BFOS_9_BIT_PTR == 1
    byte *kvIdx = buf + BFOS_HDR_SIZE + pos;
    memmove(kvIdx + 1, kvIdx, filledSz - pos + TRIE_LEN);
    *kvIdx = kv_pos;
    trie++;
#if defined(BFOS_INT64MAP)
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
#else
    byte *kvIdx = buf + BFOS_HDR_SIZE + (pos << 1);
    memmove(kvIdx + 2, kvIdx, (filledSz - pos) * 2 + TRIE_LEN);
    util::setInt(kvIdx, kv_pos);
    trie += 2;
#endif
    setFilledSize(filledSz + 1);

}

bool bfos_node_handler::isLeaf() { // ok
    return IS_LEAF_BYTE;
}

void bfos_node_handler::setLeaf(char isLeaf) { // ok
    IS_LEAF_BYTE = isLeaf;
}

void bfos_node_handler::setFilledSize(int16_t filledSize) { // ok
    util::setInt(FILLED_SIZE, filledSize);
}

bool bfos_node_handler::isFull(int16_t kv_len) { // ok
    if ((getKVLastPos() - kv_len - 2) < (BFOS_HDR_SIZE + TRIE_LEN + need_count))
        return true;
    if (TRIE_LEN > 240)
        return true;
    return false;
}

int16_t bfos_node_handler::filledSize() { // ok
    return util::getInt(FILLED_SIZE);
}

byte *bfos_node_handler::getChild(int16_t pos) { // ok
    byte *idx = getData(pos, &pos);
    unsigned long addr_num = util::fourBytesToPtr(idx);
    byte *ret = (byte *) addr_num;
    return ret;
}

void bfos_node_handler::setPtr(int16_t pos, int16_t ptr) { // invalid
    byte *kvIdx = buf + BFOS_HDR_SIZE + (pos << 1);
    util::setInt(kvIdx, ptr);
}

int16_t bfos_node_handler::getPtr(int16_t pos) { // invalid
    byte *kvIdx = buf + BFOS_HDR_SIZE + (pos << 1);
    return util::getInt(kvIdx);
}

byte *bfos_node_handler::getKey(int16_t ptr, int16_t *plen) { // ok
    byte *kvIdx = buf + ptr;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *bfos_node_handler::getData(int16_t ptr, int16_t *plen) { // ok
    byte *kvIdx = buf + ptr;
    *plen = kvIdx[0];
    kvIdx++;
    kvIdx += *plen;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

void bfos_node_handler::updatePtrs(byte *upto, int diff) {
    byte *t = trie;
    //upto--;
    while (t <= upto) {
        int count;
        t++;
        count = GenTree::bit_count[*t++];
        while (count--) {
            if (t < upto && (t + *t + 1) >= upto)
                *t += diff;
            t++;
        }
        t += (GenTree::bit_count[*t++] << 1);
    }
}

int16_t bfos_node_handler::insertCurrent() {
    register byte key_char;
    register byte mask;
    register byte *leafPos;
    int16_t ret = 0;
    int16_t ptr;
    int16_t pos;

    switch (insertState) {
    case INSERT_AFTER:
        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x07);
        updatePtrs(triePos, 5);
        *origPos &= xFB;
        insAt(triePos, ((key_char & xF8) | x05), 0, mask, 0, 0);
        ret = triePos - buf + 3;
        break;
    case INSERT_BEFORE:
        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x07);
        updatePtrs(origPos, 5);
        if (keyPos > 1)
            trie[last_child_pos] -= 5;
        insAt(origPos, ((key_char & xF8) | x01), 0, mask, 0, 0);
        ret = origPos - buf + 3;
        break;
    case INSERT_LEAF:
        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x07);
        leafPos = origPos + 1;
        leafPos += GenTree::bit_count[*leafPos];
        leafPos++;
        updatePtrs(triePos, 2);
        *leafPos |= mask;
        insAt(triePos, 0, 0);
        ret = triePos - buf;
        break;
    case INSERT_THREAD:
        int16_t p, min;
        byte c1, c2;
        byte *childPos;
        key_char = key[keyPos - 1];
        c1 = c2 = key_char;
        key_char &= x07;
        mask = x80 >> key_char;
        childPos = origPos + 1;
        *childPos |= mask;
        childPos += GenTree::bit_count[*childPos++ & left_mask[key_char]];
        insAt(childPos, (byte) (TRIE_LEN - (childPos - trie)));
        updatePtrs(childPos, 1);
        p = keyPos;
        min = util::min(key_len, keyPos + key_at_len);
        if (p < min) {
            leafPos = origPos + 1;
            leafPos += GenTree::bit_count[*leafPos++];
            (*leafPos) &= ~mask;
            leafPos += (GenTree::bit_count[*leafPos++ & left_mask[key_char]]
                    << 1);
            ptr = util::getInt(leafPos);
            delAt(leafPos, 2);
            updatePtrs(leafPos, -2);
        } else {
            leafPos = origPos + 1;
            leafPos += GenTree::bit_count[*leafPos++];
            leafPos += (GenTree::bit_count[*leafPos++ & left_mask[key_char]]
                    << 1);
            ptr = util::getInt(leafPos);
            pos = leafPos - buf;
            ret = pos;
        }
        triePos = trie + TRIE_LEN;
        while (p < min) {
            bool isSwapped = false;
            c1 = key[p];
            c2 = key_at[p - keyPos];
            if (c1 > c2) {
                byte swap = c1;
                c1 = c2;
                c2 = swap;
                isSwapped = true;
            }
            switch ((c1 ^ c2) > x07 ?
                    0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
            case 0:
                append((c1 & xF8) | x01);
                append(0);
                append(x80 >> (c1 & x07));
                ret = isSwapped ? ret : BFOS_HDR_SIZE + TRIE_LEN;
                pos = isSwapped ? BFOS_HDR_SIZE + TRIE_LEN : pos;
                appendPtr(isSwapped ? ptr : 0);
                append((c2 & xF8) | x05);
                append(0);
                append(x80 >> (c2 & x07));
                ret = isSwapped ? BFOS_HDR_SIZE + TRIE_LEN : ret;
                pos = isSwapped ? pos : BFOS_HDR_SIZE + TRIE_LEN;
                appendPtr(isSwapped ? 0 : ptr);
                break;
            case 1:
                append((c1 & xF8) | x05);
                append(0);
                append((x80 >> (c1 & x07)) | (x80 >> (c2 & x07)));
                ret = isSwapped ? ret : BFOS_HDR_SIZE + TRIE_LEN;
                pos = isSwapped ? BFOS_HDR_SIZE + TRIE_LEN : pos;
                appendPtr(isSwapped ? ptr : 0);
                ret = isSwapped ? BFOS_HDR_SIZE + TRIE_LEN : ret;
                pos = isSwapped ? pos : BFOS_HDR_SIZE + TRIE_LEN;
                appendPtr(isSwapped ? 0 : ptr);
                break;
            case 2:
                append((c1 & xF8) | x06);
                append(x80 >> (c1 & x07));
                append(1);
                append(0);
                break;
            case 3:
                append((c1 & xF8) | x07);
                append(x80 >> (c1 & x07));
                append(3);
                append(x80 >> (c1 & x07));
                ret = (p + 1 == key_len) ? BFOS_HDR_SIZE + TRIE_LEN : ret;
                pos = (p + 1 == key_len) ? pos : BFOS_HDR_SIZE + TRIE_LEN;
                appendPtr((p + 1 == key_len) ? 0 : ptr);
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
            append((c2 & xF8) | x05);
            append(0);
            append(x80 >> (c2 & x07));
//            ret = (p == key_len) ? BFOS_HDR_SIZE + TRIE_LEN : ret;
//            pos = (p == key_len) ? pos : BFOS_HDR_SIZE + TRIE_LEN;
//            appendPtr((p == key_len) ? 0 : ptr);
            ret = (p == key_len) ? ret : BFOS_HDR_SIZE + TRIE_LEN;
            pos = (p == key_len) ? BFOS_HDR_SIZE + TRIE_LEN : pos;
            appendPtr((p == key_len) ? ptr : 0);
            if (p == key_len)
                keyPos--;
        }
        if (diff < key_at_len)
            diff++;
        if (diff) {
            p = ptr;
            key_at_len -= diff;
            p += diff;
            if (key_at_len >= 0) {
                buf[p] = key_at_len;
                util::setInt(buf + pos, p);
            }
        }
        break;
    case INSERT_EMPTY:
        key_char = *key;
        mask = x80 >> (key_char & x07);
        append((key_char & xF8) | x05);
        append(0);
        append(mask);
        ret = BFOS_HDR_SIZE + TRIE_LEN;
        append(0);
        append(0);
        keyPos = 1;
        break;
    }
    return ret;
}

int16_t bfos_node_handler::locate(int16_t level) {
    keyPos = 1;
    byte key_char = *key;
    byte *t = trie;
    int16_t ptr = 0;
    do {
        byte trie_char = *t;
        origPos = t++;
        switch ((key_char ^ trie_char) > x07 ?
                (key_char > trie_char ? 0 : 2) : 1) {
        case 0:
            t += GenTree::bit_count[*t++];
            t += (GenTree::bit_count[*t++] << 1);
            if (trie_char & x04) {
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_AFTER;
                    need_count = 5;
                }
                return -1;
            }
            continue;
        case 1:
            byte r_leaves, r_children, r_mask;
            r_children = *t++;
            t += GenTree::bit_count[r_children];
            r_leaves = *t++;
            key_char &= x07;
            r_mask = x80 >> key_char;
            switch (r_leaves & r_mask ?
                    (r_children & r_mask ? (keyPos == key_len ? 3 : 4) : 2) :
                    (r_children & r_mask ? (keyPos == key_len ? 0 : 1) : 0)) {
            case 0:
                if (isPut) {
                    t += (GenTree::bit_count[r_leaves & left_mask[key_char]]
                            << 1);
                    triePos = t;
                    insertState = INSERT_LEAF;
                    need_count = 3;
                }
                return -1;
            case 1:
                break;
            case 2:
                register int16_t cmp;
                t += (GenTree::bit_count[r_leaves & left_mask[key_char]] << 1);
                ptr = util::getInt(t);
                key_at = (char *) getKey(ptr, &key_at_len);
                cmp = util::compare(key + keyPos, key_len - keyPos, key_at,
                        key_at_len);
                if (cmp == 0)
                    return ptr;
                if (isPut) {
                    triePos = origPos + 1
                            + GenTree::bit_count[r_children
                                    & left_mask[key_char]];
                    insertState = INSERT_THREAD;
                    if (cmp < 0)
                        cmp = -cmp;
                    need_count = (cmp * 4) + 6;
                }
                return -1;
            case 3:
                t += (GenTree::bit_count[r_leaves & left_mask[key_char]] << 1);
                ptr = util::getInt(t);
                return ptr;
            case 4:
                break;
            }
            t = origPos + 1;
            t += GenTree::bit_count[*t++ & left_mask[key_char]];
            last_child_pos = t - trie;
            t += *t;
            t++;
            key_char = key[keyPos++];
            continue;
        case 2:
            if (isPut) {
                insertState = INSERT_BEFORE;
                need_count = 5;
            }
            return -1;
        }
    } while (1);
    return -1;
}

void bfos_node_handler::delAt(byte *ptr) {
    TRIE_LEN--;
    memmove(ptr, ptr + 1, trie + TRIE_LEN - ptr);
}

void bfos_node_handler::delAt(byte *ptr, int16_t count) {
    TRIE_LEN -= count;
    memmove(ptr, ptr + count, trie + TRIE_LEN - ptr);
}

void bfos_node_handler::insAt(byte *ptr, byte b) {
    memmove(ptr + 1, ptr, trie + TRIE_LEN - ptr);
    *ptr = b;
    TRIE_LEN++;
}

byte bfos_node_handler::insAt(byte *ptr, byte b1, byte b2) {
    memmove(ptr + 2, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr = b2;
    TRIE_LEN += 2;
    return 2;
}

byte bfos_node_handler::insAt(byte *ptr, byte b1, byte b2, byte b3) {
    memmove(ptr + 3, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr = b3;
    TRIE_LEN += 3;
    return 3;
}

byte bfos_node_handler::insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4) {
    memmove(ptr + 4, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr++ = b3;
    *ptr = b4;
    TRIE_LEN += 4;
    return 4;
}

byte bfos_node_handler::insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4,
        byte b5) {
    memmove(ptr + 5, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr++ = b3;
    *ptr++ = b4;
    *ptr = b5;
    TRIE_LEN += 5;
    return 5;
}

byte bfos_node_handler::insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4,
        byte b5, byte b6) {
    memmove(ptr + 6, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr++ = b3;
    *ptr++ = b4;
    *ptr++ = b5;
    *ptr = b6;
    TRIE_LEN += 6;
    return 6;
}

byte bfos_node_handler::insChildAndLeafAt(byte *ptr, byte b1, byte b2) {
    memmove(ptr + 3, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr = b2;
    TRIE_LEN += 3;
    return 3;
}

void bfos_node_handler::setAt(byte pos, byte b) {
    trie[pos] = b;
}

void bfos_node_handler::append(byte b) {
    trie[TRIE_LEN++] = b;
}

void bfos_node_handler::appendPtr(int16_t p) {
    util::setInt(trie + TRIE_LEN, p);
    TRIE_LEN += 2;
}

byte bfos_node_handler::getAt(byte pos) {
    return trie[pos];
}

void bfos_node_handler::initVars() {
}

byte bfos_node_handler::left_mask[8] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8,
        0xFC, 0xFE };
byte bfos_node_handler::left_incl_mask[8] = { 0x80, 0xC0, 0xE0, 0xF0, 0xF8,
        0xFC, 0xFE, 0xFF };
byte bfos_node_handler::ryte_mask[8] = { 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03,
        x01, 0x00 };
byte bfos_node_handler::ryte_incl_mask[8] = { 0xFF, 0x7F, 0x3F, 0x1F, 0x0F,
        0x07, 0x03, x01 };
