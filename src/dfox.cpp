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

int16_t dfox_node_handler::nextKey(dfox_iterator_status& s) {
    if (s.t == trie) {
        keyPos = 0;
        s.offset_a[keyPos] = 8;
    } else {
        while (s.offset_a[keyPos] == 8 && (s.tc_a[keyPos] & 0x04)) {
            keyPos--;
            s.offset_a[keyPos]++;
        }
    }
    do {
        if (s.offset_a[keyPos] > 0x07) {
            s.tc_a[keyPos] = *s.t++;
            s.leaf_a[keyPos] = 0;
            s.child_a[keyPos] = 0;
            if (s.tc_a[keyPos] & 0x02)
                s.child_a[keyPos] = *s.t++;
            if (s.tc_a[keyPos] & 0x01)
                s.leaf_a[keyPos] = *s.t++;
            s.offset_a[keyPos] = 0;
        }
        byte mask = 0x80 >> s.offset_a[keyPos];
        if (s.leaf_a[keyPos] & mask) {
            s.partial_key[keyPos] = (s.tc_a[keyPos] & 0xF8)
                    | s.offset_a[keyPos];
            if (s.child_a[keyPos] & mask)
                s.leaf_a[keyPos] &= ~mask;
            else
                s.offset_a[keyPos]++;
            return keyPos;
        }
        if (s.child_a[keyPos] & mask) {
            s.partial_key[keyPos] = (s.tc_a[keyPos] & 0xF8)
                    | s.offset_a[keyPos];
            keyPos++;
            s.offset_a[keyPos] = 8;
        }
        while (s.offset_a[keyPos] == 7 && (s.tc_a[keyPos] & 0x04))
            keyPos--;
        s.offset_a[keyPos]++;
    } while (1);
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
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = DFOX_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    register int16_t new_idx;
    int16_t brk_idx = -1;
    int16_t brk_kv_pos = 0;
    int16_t tot_len = 0;
    byte *old_block_ptrs = buf + IDX_HDR_SIZE;
    int16_t brk_key_len = 0;
    byte brk_key[40];
    dfox_iterator_status s;
    s.t = trie;
    // (1) move all data to new_block in order
    for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
        int16_t src_idx = old_block_ptrs[new_idx];
        if (*bitmap & GenTree::mask64[new_idx])
            src_idx |= 256;
        int16_t key_len = buf[src_idx];
        key_len++;
        int16_t value_len = buf[src_idx + key_len];
        value_len++;
        int16_t kv_len = key_len;
        kv_len += value_len;
        tot_len += kv_len;
        memcpy(new_block.buf + kv_last_pos, buf + src_idx, kv_len);
        new_block.insPtr(new_idx, kv_last_pos);
        kv_last_pos += kv_len;
        if (brk_idx == -1)
            brk_key_len = nextKey(s);
        if (tot_len > halfKVLen && brk_idx == -1) {
            brk_idx = new_idx + 1;
            brk_kv_pos = kv_last_pos;
            brk_key_len++;
            memcpy(brk_key, s.partial_key, brk_key_len);
        }
    }
    int16_t new_brk_key_len = nextKey(s) + 1;
    memcpy(first_key, s.partial_key, new_brk_key_len);
    new_block.key_at = (char *) new_block.getKey(brk_idx,
            &new_block.key_at_len);
    if (new_block.key_at_len) {
        memcpy(first_key + new_brk_key_len, new_block.key_at,
                new_block.key_at_len);
        *first_len_ptr = new_brk_key_len + new_block.key_at_len;
    } else
        *first_len_ptr = new_brk_key_len;
    kv_last_pos = getKVLastPos();
    memcpy(new_block.trie, trie, TRIE_LEN);
    new_block.TRIE_LEN = TRIE_LEN;
    memcpy(buf, new_block.buf, DFOX_NODE_SIZE);

    int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
    memmove(buf + IDX_HDR_SIZE + brk_idx, trie, TRIE_LEN);
    memmove(buf + DFOX_NODE_SIZE - old_blk_new_len, buf + kv_last_pos,
            old_blk_new_len);
    int diff = DFOX_NODE_SIZE - old_blk_new_len - kv_last_pos;
    for (int i = 0; i < brk_idx; i++)
        setPtr(i, getPtr(i) + diff);
    setKVLastPos(DFOX_NODE_SIZE - old_blk_new_len);
    setFilledSize(brk_idx);
    trie = buf + IDX_HDR_SIZE + brk_idx;

    int16_t new_size = orig_filled_size - brk_idx;
    (*new_block.bitmap) <<= brk_idx;
    if (IS_LEAF_BYTE)
        new_block.setLeaf(1);
    memmove(new_block.buf + IDX_HDR_SIZE,
            new_block.buf + IDX_HDR_SIZE + brk_idx, new_size + TRIE_LEN);
    new_block.setKVLastPos(brk_kv_pos);
    new_block.setFilledSize(new_size);
    new_block.trie = new_block.buf + IDX_HDR_SIZE + new_size;

    deleteTrieLastHalf(brk_key, brk_key_len);
    new_block.deleteTrieFirstHalf(first_key, new_brk_key_len);

    *pbrk_idx = brk_idx;
    return new_block.buf;
}

void dfox_node_handler::deleteTrieLastHalf(byte *brk_key, int16_t brk_key_len) {
    keyPos = 0;
    register byte *t = trie;
    register byte kc = brk_key[keyPos++];
    do {
        register int to_skip;
        register byte tc, leaves, children;
        byte *prev_t = t;
        tc = *t++;
        leaves = children = 0;
        if (tc & 0x02)
            children = *t++;
        if (tc & 0x01)
            leaves = *t++;
        while ((kc ^ tc) > 0x07) {
            to_skip = GenTree::bit_count[children];
            while (to_skip) {
                byte child_tc = *t++;
                if (child_tc & 0x02)
                    to_skip += GenTree::bit_count[*t++];
                if (child_tc & 0x01)
                    t++;
                if (child_tc & 0x04)
                    to_skip--;
            }
            prev_t = t;
            tc = *t++;
            leaves = children = 0;
            if (tc & 0x02)
                children = *t++;
            if (tc & 0x01)
                leaves = *t++;
        }
        byte offset = kc & 0x07;
        if (keyPos == brk_key_len)
            children &= left_mask[offset];
        else
            children &= left_incl_mask[offset];
        leaves &= left_incl_mask[offset];
        tc |= 0x04;
        to_skip = GenTree::bit_count[children];
        if (keyPos != brk_key_len)
            to_skip--;
        while (to_skip) {
            byte child_tc = *t++;
            if (child_tc & 0x02)
                to_skip += GenTree::bit_count[*t++];
            if (child_tc & 0x01)
                t++;
            if (child_tc & 0x04)
                to_skip--;
        }
        *prev_t++ = tc;
        if (tc & 0x02)
            *prev_t++ = children;
        if (tc & 0x01)
            *prev_t = leaves;
        if (keyPos == brk_key_len) {
            TRIE_LEN = t - trie;
            break;
        }
        kc = brk_key[keyPos++];
    } while (1);
}

void dfox_node_handler::deleteTrieFirstHalf(byte *brk_key,
        int16_t brk_key_len) {
    keyPos = 0;
    register byte *t = trie;
    register byte kc = brk_key[keyPos++];
    do {
        register int to_skip;
        register byte tc, leaves, children;
        byte *prev_t = t;
        tc = *t++;
        leaves = children = 0;
        if (tc & 0x02)
            children = *t++;
        if (tc & 0x01)
            leaves = *t++;
        byte *delete_start = 0;
        while ((kc ^ tc) > 0x07) {
            to_skip = GenTree::bit_count[children];
            if (!delete_start)
                delete_start = prev_t;
            while (to_skip) {
                byte child_tc = *t++;
                if (child_tc & 0x02)
                    to_skip += GenTree::bit_count[*t++];
                if (child_tc & 0x01)
                    t++;
                if (child_tc & 0x04)
                    to_skip--;
            }
            prev_t = t;
            tc = *t++;
            leaves = children = 0;
            if (tc & 0x02)
                children = *t++;
            if (tc & 0x01)
                leaves = *t++;
        }
        if (delete_start) {
            int to_delete = prev_t - delete_start;
            TRIE_LEN -= to_delete;
            t -= to_delete;
            memmove(delete_start, prev_t, trie + TRIE_LEN - delete_start);
            prev_t -= to_delete;
        }
        byte offset = kc & 0x07;
        to_skip = GenTree::bit_count[children & left_mask[offset]];
        delete_start = t;
        while (to_skip) {
            byte child_tc = *t++;
            if (child_tc & 0x02)
                to_skip += GenTree::bit_count[*t++];
            if (child_tc & 0x01)
                t++;
            if (child_tc & 0x04)
                to_skip--;
        }
        if (t > delete_start) {
            int to_delete = t - delete_start;
            TRIE_LEN -= to_delete;
            memmove(delete_start, t, trie + TRIE_LEN - delete_start);
            t -= to_delete;
        }
        if (keyPos == brk_key_len)
            leaves &= ryte_incl_mask[offset];
        else
            leaves &= ryte_mask[offset];
        children &= ryte_incl_mask[offset];
        *prev_t++ = tc;
        if (tc & 0x02)
            *prev_t++ = children;
        if (tc & 0x01)
            *prev_t = leaves;
        if (keyPos == brk_key_len)
            break;
        kc = brk_key[keyPos++];
    } while (1);
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
    bitmap = (uint64_t *) buf;
}

void dfox_node_handler::setBuf(byte *m) {
    buf = m;
    trie = buf + IDX_HDR_SIZE + FILLED_SIZE;
    bitmap = (uint64_t *) buf;
}

void dfox_node_handler::setKVLastPos(int16_t val) {
    util::setInt(LAST_DATA_PTR, val);
}

int16_t dfox_node_handler::getKVLastPos() {
    return util::getInt(LAST_DATA_PTR);
}

void dfox_node_handler::addData(int16_t pos) {

    if (TRIE_LEN == 0)
        keyPos = 1;

    insertCurrent();

    if (insertState == INSERT_THREAD) {
        int16_t diff = 0;
        register int i;
        for (i = keyPos; i < key_len && (i - keyPos) < key_at_len; i++) {
            if (key[i] == key_at[i - keyPos])
                diff++;
            else
                break;
        }
        keyPos += diff;
        keyPos++;
        if (i >= key_len)
            keyPos = key_len;
        if ((i - keyPos) < key_at_len)
            diff++;
        if (diff) {
            int16_t key_at_ptr = getPtr(key_at_pos);
            int16_t new_len = buf[key_at_ptr];
            new_len -= diff;
            key_at_ptr += diff;
            if (new_len >= 0) {
                buf[key_at_ptr] = new_len;
                setPtr(key_at_pos, key_at_ptr);
            }
        }
    }
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
    insBit(bitmap, pos, kv_pos);
    FILLED_SIZE = filledSz;
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
    if (FILLED_SIZE > MAX_PTRS)
        return true;
    if ((getKVLastPos() - kv_len - 2)
            < (IDX_HDR_SIZE + TRIE_LEN + need_count + FILLED_SIZE))
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
    buf[IDX_HDR_SIZE + pos] = (ptr & 0xFF);
    if (ptr >= 256)
        *bitmap |= GenTree::mask64[pos];
    else
        *bitmap &= ~GenTree::mask64[pos];
}

int16_t dfox_node_handler::getPtr(int16_t pos) {
    int16_t ptr = buf[IDX_HDR_SIZE + pos];
    if (*bitmap & GenTree::mask64[pos])
        ptr |= 256;
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
    byte origTC;
    byte key_char;
    byte mask;
    byte *leafPos;

    if (TRIE_LEN == 0) {
        byte kc = key[0];
        byte offset = (kc & 0x07);
        append((kc & 0xF8) | 0x05);
        append(0x80 >> offset);
        return;
    }

    key_char = key[keyPos - 1];
    mask = 0x80 >> (key_char & 0x07);
    switch (insertState) {
    case INSERT_MIDDLE1:
        origTC = *origPos;
        origTC &= 0xFB;
        *origPos = origTC;
        insAt(triePos - trie, ((key_char & 0xF8) | 0x05), mask);
        break;
    case INSERT_MIDDLE2:
        insAt(triePos - trie, ((key_char & 0xF8) | 0x01), mask);
        break;
    case INSERT_LEAF:
        origTC = *origPos;
        leafPos = origPos + 1;
        if (origTC & 0x02)
            leafPos++;
        if (origTC & 0x01) {
            *leafPos |= mask;
        } else {
            insAt(leafPos - trie, mask);
            *origPos = (origTC | 0x01);
        }
        break;
    case INSERT_THREAD:
        int16_t pos, min;
        byte child;
        byte c1, c2;
        byte *childPos = origPos + 1;
        child = mask;
        origTC = *origPos;
        if (origTC & 0x02) {
            child |= *childPos;
            *childPos = child;
        } else {
            insAt(childPos - trie, child);
            triePos++;
            origTC |= 0x02;
            *origPos = origTC;
        }
        leafPos = childPos;
        if (origTC & 0x01) {
            leafPos++;
            byte leaf = *leafPos;
            leaf &= ~mask;
            if (leaf)
                *leafPos = leaf;
            else {
                delAt(leafPos - trie);
                triePos--;
                origTC &= 0xFE;
                *origPos = origTC;
            }
        }
        pos = keyPos - 1;
        min = util::min(key_len, keyPos + key_at_len);
        c1 = key_char;
        c2 = c1;
        pos++;
        while (pos < min) {
            c1 = key[pos];
            c2 = key_at[pos - keyPos];
            if (c1 > c2) {
                byte swap = c1;
                c1 = c2;
                c2 = swap;
            }
            byte msb5c1 = (c1 & 0xF8);
            byte offc1 = (c1 & 0x07);
            byte msb5c2 = (c2 & 0xF8);
            byte offc2 = (c2 & 0x07);
            byte c1leaf = 0;
            byte c2leaf = 0;
            byte c1child = 0;
            if (c1 == c2) {
                c1child = (0x80 >> offc1);
                if (pos + 1 == min) {
                    c1leaf = (0x80 >> offc1);
                    msb5c1 |= 0x07;
                } else
                    msb5c1 |= 0x06;
            } else {
                if (msb5c1 == msb5c2) {
                    msb5c1 |= 0x05;
                    c1leaf = ((0x80 >> offc1) | (0x80 >> offc2));
                } else {
                    c1leaf = (0x80 >> offc1);
                    c2leaf = (0x80 >> offc2);
                    msb5c2 |= 0x05;
                    msb5c1 |= 0x01;
                    insAt(triePos - trie, msb5c2, c2leaf);
                }
            }
            if (c1leaf && c1child) {
                insAt(triePos - trie, msb5c1, c1child, c1leaf);
                triePos += 3;
            } else if (c1leaf) {
                insAt(triePos - trie, msb5c1, c1leaf);
                triePos += 2;
            } else {
                insAt(triePos - trie, msb5c1, c1child);
                triePos += 2;
            }
            if (c1 != c2)
                break;
            pos++;
        }
        if (c1 == c2) {
            if (pos == key_len)
                c2 = key_at[pos - keyPos];
            else
                c2 = key[pos];
            byte msb5c2 = (c2 & 0xF8);
            byte offc2 = (c2 & 0x07);
            msb5c2 |= 0x05;
            insAt(triePos - trie, msb5c2, (0x80 >> offc2));
        }
        break;
    }
}

int16_t dfox_node_handler::locate(int16_t level) {
    keyPos = 0;
    register byte key_char = key[keyPos++];
    register int16_t pos = 0;
    register byte *t = trie;
    do {
        register byte trie_char = *t;
        if ((key_char ^ trie_char) < 0x08) {
            register byte r_leaves;
            register byte r_children;
            register int r_offset = (key_char & 0x07);
            register byte r_mask = (0x80 >> r_offset);
            register int to_skip = 0;
            r_leaves = r_children = 0;
            origPos = t;
            t++;
            if (trie_char & 0x02)
                r_children = *t++;
            if (trie_char & 0x01)
                r_leaves = *t++;
            to_skip = GenTree::bit_count[r_children & left_mask[r_offset]];
            pos += GenTree::bit_count[r_leaves & left_mask[r_offset]];
            while (to_skip) {
                register byte child_tc = *t++;
                if (child_tc & 0x02)
                    to_skip += GenTree::bit_count[*t++];
                if (child_tc & 0x01)
                    pos += GenTree::bit_count[*t++];
                if (child_tc & 0x04)
                    to_skip--;
            }
            if (r_children & r_mask) {
                if (r_leaves & r_mask) {
                    if (keyPos == key_len)
                        return pos;
                    else
                        pos++;
                } else {
                    if (keyPos == key_len) {
                        if (isPut) {
                            insertState = INSERT_LEAF;
                            need_count = 2;
                        }
                        return ~pos;
                    }
                }
                key_char = key[keyPos++];
            } else {
                if (r_leaves & r_mask) {
                    key_at = (char *) getKey(pos, &key_at_len);
                    int16_t cmp = util::compare(key + keyPos, key_len - keyPos,
                            key_at, key_at_len);
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
                } else {
                    if (isPut) {
                        insertState = INSERT_LEAF;
                        need_count = 2;
                    }
                    return ~pos;
                }
            }
        } else if (key_char > trie_char) {
            register int to_skip = 0;
            origPos = t;
            t++;
            if (trie_char & 0x02)
                to_skip = GenTree::bit_count[*t++];
            if (trie_char & 0x01)
                pos += GenTree::bit_count[*t++];
            while (to_skip) {
                register byte child_tc = *t++;
                if (child_tc & 0x02)
                    to_skip += GenTree::bit_count[*t++];
                if (child_tc & 0x01)
                    pos += GenTree::bit_count[*t++];
                if (child_tc & 0x04)
                    to_skip--;
                child_tc = *t;
            }
            if (trie_char & 0x04) {
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_MIDDLE1;
                    need_count = 2;
                }
                return ~pos;
            }
        } else {
            if (isPut) {
                triePos = t;
                insertState = INSERT_MIDDLE2;
                need_count = 2;
            }
            return ~pos;
        }
    } while (1);
    return ~pos;
}

void dfox_node_handler::delAt(byte pos) {
    TRIE_LEN--;
    byte *ptr = trie + pos;
    memmove(ptr, ptr + 1, TRIE_LEN - pos);
}

void dfox_node_handler::delAt(byte pos, int16_t count) {
    TRIE_LEN -= count;
    byte *ptr = trie + pos;
    memmove(ptr, ptr + count, TRIE_LEN - pos);
}

void dfox_node_handler::insAt(byte pos, byte b) {
    byte *ptr = trie + pos;
    memmove(ptr + 1, ptr, TRIE_LEN - pos);
    *ptr = b;
    TRIE_LEN++;
}

void dfox_node_handler::insAt(byte pos, byte b1, byte b2) {
    byte *ptr = trie + pos;
    memmove(ptr + 2, ptr, TRIE_LEN - pos);
    *ptr++ = b1;
    *ptr = b2;
    TRIE_LEN += 2;
}

void dfox_node_handler::insAt(byte pos, byte b1, byte b2, byte b3) {
    byte *ptr = trie + pos;
    memmove(ptr + 3, ptr, TRIE_LEN - pos);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr = b3;
    TRIE_LEN += 3;
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
        0x01, 0x00 };
byte dfox_node_handler::ryte_incl_mask[8] = { 0xFF, 0x7F, 0x3F, 0x1F, 0x0F,
        0x07, 0x03, 0x01 };
