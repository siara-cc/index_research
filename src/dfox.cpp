#include <iostream>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include "dfox.h"
#include "GenTree.h"

#define FAVOUR_SIZE

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
            maxKeyCount += node->filledSize();
            int16_t brk_idx;
            byte *b = node->split(&brk_idx);
            dfox_node_handler new_block(b);
            new_block.initVars();
            new_block.isPut = true;
            blockCount++;
            if (root_data == node->buf) {
                root_data = util::alignedAlloc(DFOX_NODE_SIZE);
                dfox_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                blockCount++;
                int16_t first_len;
                char *first_key;
                char addr[5];
                util::ptrToFourBytes((unsigned long) node->buf, addr);
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = addr;
                root.value_len = sizeof(char *);
                root.addData(0);
                first_key = (char *) new_block.getKey(0, &first_len);
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                root.initVars();
                root.key = first_key;
                root.key_len = first_len;
                root.value = addr;
                root.value_len = sizeof(char *);
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
                parent.key = (char *) new_block.getKey(0, &parent.key_len);
                parent.value = addr;
                parent.value_len = sizeof(char *);
                parent.locate(prev_level);
                recursiveUpdate(&parent, ~(lastSearchPos[prev_level] + 1),
                        lastSearchPos, node_paths, prev_level);
            }
            brk_idx++;
            if (idx > brk_idx) {
                node->setBuf(new_block.buf);
                idx -= brk_idx;
            }
            node->initVars();
            idx = ~node->locate(level);
        }
        node->addData(idx);
    } else {
        //if (node->isLeaf) {
        //    int16_t vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

byte *dfox_node_handler::split(int16_t *pbrk_idx) {
    int16_t orig_filled_size = filledSize();
    byte *b = util::alignedAlloc(DFOX_NODE_SIZE);
    dfox_node_handler old_block(this->buf);
    old_block.isPut = true;
    dfox_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    if (!isLeaf())
        new_block.setLeaf(false);
    int16_t kv_last_pos = old_block.getKVLastPos();
    int16_t halfKVLen = DFOX_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int16_t new_idx;
    int16_t brk_idx = -1;
    int16_t brk_kv_pos = 0;
    int16_t tot_len = 0;
    // (1) move all data to new_block in order
    for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
        int16_t src_idx = old_block.getPtr(new_idx);
        int16_t key_len = old_block.buf[src_idx];
        key_len++;
        int16_t value_len = old_block.buf[src_idx + key_len];
        value_len++;
        int16_t kv_len = key_len;
        kv_len += value_len;
        tot_len += kv_len;
        memcpy(new_block.buf + kv_last_pos, old_block.buf + src_idx, kv_len);
        new_block.insPtr(new_idx, kv_last_pos);
        kv_last_pos += kv_len;
        if (tot_len > halfKVLen) {
            if (brk_idx == -1) {
                brk_idx = new_idx;
                brk_kv_pos = kv_last_pos;
            }
        }
    }
    // (2) move top half of new_block to bottom half of old_block
    kv_last_pos = old_block.getKVLastPos();
    int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
    memcpy(old_block.buf + DFOX_NODE_SIZE - old_blk_new_len,
            new_block.buf + kv_last_pos, old_blk_new_len);
    // (3) Insert pointers to the moved data in old block
    old_block.FILLED_SIZE= 0;
    old_block.TRIE_LEN= 0;
    for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
        int16_t src_idx = new_block.getPtr(new_idx);
        src_idx += (DFOX_NODE_SIZE - brk_kv_pos);
        old_block.insPtr(new_idx, src_idx);
        if (new_idx == 0) // (5) Set Last data position for old block
            old_block.setKVLastPos(src_idx);
        old_block.initVars();
        old_block.key = (char *) (old_block.buf + src_idx + 1);
        old_block.key_len = old_block.buf[src_idx];
        if (new_idx)
            old_block.locate(0);
        old_block.insertCurrent();
    }
    int16_t new_size = orig_filled_size - brk_idx - 1;
    /*
     // (4) move new block pointers to front
     byte *new_kv_idx = new_block->buf + IDX_HDR_SIZE - orig_filled_size;
     memmove(new_kv_idx + new_idx, new_kv_idx, new_size); */
    // (4) and set corresponding bits
    byte high_bits[MAX_PTR_BITMAP_BYTES];
    memcpy(high_bits, new_block.DATA_PTR_HIGH_BITS, MAX_PTR_BITMAP_BYTES);
    memset(new_block.DATA_PTR_HIGH_BITS, 0, MAX_PTR_BITMAP_BYTES);
    for (int16_t i = 0; i < new_size; i++) {
        int16_t from_bit = i + new_idx;
        if (high_bits[from_bit >> 3] & pos_mask[from_bit])
            new_block.DATA_PTR_HIGH_BITS[i >> 3] |= pos_mask[i];
    }
    if (high_bits[MAX_PTR_BITMAP_BYTES - 1] & 0x01)
        new_block.setLeaf(1);
    new_block.setKVLastPos(brk_kv_pos); // (5) New Block Last position
    //filled_size = brk_idx + 1;
    //setFilledSize(filled_size); // (7) Filled Size for Old block
    new_block.TRIE_LEN= 0;
    new_block.setFilledSize(new_size); // (8) Filled Size for New Block
    for (int16_t i = 0; i < new_size; i++) {
        new_block.initVars();
        new_block.key = (char *) new_block.getKey(i, &new_block.key_len);
        if (i)
            new_block.locate(0);
        new_block.insertCurrent();
    }
    *pbrk_idx = brk_idx;
    return new_block.buf;
}

dfox::dfox() {
    root_data = util::alignedAlloc(DFOX_NODE_SIZE);
    dfox_node_handler root(root_data);
    root.initBuf();
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 0; //9999;
    blockCount = 1;
    maxThread = 9999;
}

dfox::~dfox() {
    delete root_data;
}

dfox_node_handler::dfox_node_handler(byte *m) {
    setBuf(m);
    isPut = false;
}

void dfox_node_handler::initBuf() {
    memset(buf, '\0', DFOX_NODE_SIZE);
    setLeaf(1);
    FILLED_SIZE= 0;
    TRIE_LEN= 0;
    threads = 0;
    setKVLastPos(DFOX_NODE_SIZE);
}

void dfox_node_handler::setBuf(byte *m) {
    buf = m;
    trie = buf + IDX_HDR_SIZE;
}

void dfox_node_handler::setKVLastPos(int16_t val) {
    util::setInt(LAST_DATA_PTR, val);
}

int16_t dfox_node_handler::getKVLastPos() {
    return util::getInt(LAST_DATA_PTR);
}

void dfox_node_handler::addData(int16_t pos) {

    insertCurrent();
    int16_t kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_len;
    memcpy(buf + kv_last_pos + 1, key, key_len);
    buf[kv_last_pos + key_len + 1] = value_len;
    memcpy(buf + kv_last_pos + key_len + 2, value, value_len);

    insPtr(pos, kv_last_pos);

}

void dfox_node_handler::insPtr(int16_t pos, int16_t kv_pos) {
    int16_t filledSz = filledSize();
    filledSz++;
    byte *kvIdx = buf + IDX_BLK_SIZE;
    kvIdx -= filledSz;
    if (pos > 0) {
        memmove(kvIdx, kvIdx + 1, pos);
    }
    kvIdx[pos] = (byte) (kv_pos & 0xFF);
    byte offset = pos & 0x07;
    byte carry = 0;
    if (kv_pos >= 256)
        carry = 0x80;
    byte is_leaf = isLeaf();
    for (int16_t i = (pos >> 3); i < MAX_PTR_BITMAP_BYTES; i++) {
        byte bitmap = DATA_PTR_HIGH_BITS[i];
        byte new_carry = 0;
        if (DATA_PTR_HIGH_BITS[i] & 0x01)
            new_carry = 0x80;
        DATA_PTR_HIGH_BITS[i] = bitmap & left_mask[offset];
        DATA_PTR_HIGH_BITS[i] |= ((bitmap & ryte_incl_mask[offset]) >> 1);
        DATA_PTR_HIGH_BITS[i] |= (carry >> offset);
        offset = 0;
        carry = new_carry;
    }
    setLeaf(is_leaf);
    FILLED_SIZE= filledSz;
}

bool dfox_node_handler::isLeaf() {
    return (IS_LEAF_BYTE& 0x01);
}

void dfox_node_handler::setLeaf(char isLeaf) {
    if (isLeaf)
        IS_LEAF_BYTE|= 0x01;
        else
        IS_LEAF_BYTE &= 0xFE;
    }

void dfox_node_handler::setFilledSize(int16_t filledSize) {
    FILLED_SIZE= filledSize;
}

bool dfox_node_handler::isFull(int16_t kv_len) {
#ifndef FAVOUR_SIZE
    need_count++;
#endif
    if (TRIE_LEN+ need_count + FILLED_SIZE >= TRIE_PTR_AREA_SIZE)
    return true;
    if (FILLED_SIZE> MAX_PTRS)
    return true;
    if ((getKVLastPos() - kv_len - 2) < IDX_BLK_SIZE)
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

int16_t dfox_node_handler::getPtr(int16_t pos) {
    int16_t idx = buf[IDX_BLK_SIZE - FILLED_SIZE+ pos];
    if (DATA_PTR_HIGH_BITS[pos >> 3] & pos_mask[pos])
    idx |= 256;
    return idx;
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
    byte child;
    byte origTC;
    byte leafPos;
    byte childPos;
    int16_t pos, min;

    if (TRIE_LEN== 0) {
        byte kc = key[0];
        byte offset = (kc & 0x07);
#ifdef FAVOUR_SIZE
        append((kc & 0xF8) | 0x03);
#else
        append((kc & 0xF8) | 0x07);
        append(0x00);
#endif
        append(0x80 >> offset);
        insertState = 0;
        return;
    }

    switch (insertState) {
    case INSERT_MIDDLE1:
        tc &= 0xFE;
        setAt(origPos, tc);
#ifdef FAVOUR_SIZE
        insAt(triePos, (msb5 | 0x03), mask);
        triePos += 2;
#else
        insAt(triePos, (msb5 | 0x07), 0, mask);
        triePos += 3;
#endif
        break;
    case INSERT_MIDDLE2:
#ifdef FAVOUR_SIZE
        insAt(triePos, (msb5 | 0x02), mask);
        triePos += 2;
#else
        insAt(triePos, (msb5 | 0x06), 0, mask);
        triePos += 3;
#endif
        break;
    case INSERT_LEAF:
#ifdef FAVOUR_SIZE
        origTC = getAt(origPos);
        leafPos = origPos + 1;
        if (origTC & 0x04)
        leafPos++;
        if (origTC & 0x02) {
            leaves |= mask;
            setAt(leafPos, leaves);
        } else {
            insAt(leafPos, mask);
            triePos++;
            setAt(origPos, (origTC | 0x02));
        }
#else
        leafPos = origPos + 2;
        leaves |= mask;
        setAt(leafPos, leaves);
#endif
        break;
    case INSERT_THREAD:
        threads++;
#ifdef FAVOUR_SIZE
        childPos = origPos + 1;
        child = mask;
        origTC = getAt(origPos);
        if (origTC & 0x04) {
            child |= getAt(childPos);
            setAt(childPos, child);
        } else {
            insAt(childPos, child);
            triePos++;
            origTC |= 0x04;
            setAt(origPos, origTC);
        }
#else
        origTC = getAt(origPos);
        childPos = origPos + 1;
        child = mask;
        child |= getAt(childPos);
        setAt(childPos, child);
#endif
        pos = keyPos - 1;
        min = util::min(key_len, key_at_len);
        byte c1, c2;
        c1 = key[pos];
        c2 = key_at[pos];
        pos++;
        if (pos < min) {
            leafPos = childPos;
            if (origTC & 0x02) {
                leafPos++;
                byte leaf = getAt(leafPos);
                leaf &= ~mask;
#ifdef FAVOUR_SIZE
                if (leaf)
                setAt(leafPos, leaf);
                else {
                    delAt(leafPos);
                    triePos--;
                    origTC &= 0xFD;
                    setAt(origPos, origTC);
                }
#else
                setAt(leafPos, leaf);
#endif
            }
            do {
                c1 = key[pos];
                c2 = key_at[pos];
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
                        msb5c1 |= 0x05;
                } else {
                    if (msb5c1 == msb5c2) {
                        msb5c1 |= 0x03;
                        c1leaf = ((0x80 >> offc1) | (0x80 >> offc2));
                    } else {
                        c1leaf = (0x80 >> offc1);
                        c2leaf = (0x80 >> offc2);
                        msb5c2 |= 0x03;
                        msb5c1 |= 0x02;
#ifdef FAVOUR_SIZE
                        insAt(triePos, msb5c2, c2leaf);
#else
                        msb5c2 |= 0x06;
                        insAt(triePos, msb5c2, 0, c2leaf);
#endif
                    }
                }
#ifdef FAVOUR_SIZE
                if (c1leaf != 0 && c1child != 0) {
#else
                msb5c1 |= 0x06;
#endif
                insAt(triePos, msb5c1, c1child, c1leaf);
                triePos += 3;
#ifdef FAVOUR_SIZE
            } else if (c1leaf != 0) {
                insAt(triePos, msb5c1, c1leaf);
                triePos += 2;
            } else {
                insAt(triePos, msb5c1, c1child);
                triePos += 2;
            }
#endif
                if (c1 != c2)
                    break;
                pos++;
            } while (pos < min);
        }
        if (c1 == c2) {
            c2 = (pos == key_at_len ? key[pos] : key_at[pos]);
            int16_t msb5c2 = (c2 & 0xF8);
            int16_t offc2 = (c2 & 0x07);
#ifdef FAVOUR_SIZE
            msb5c2 |= 0x03;
            insAt(triePos, msb5c2, (0x80 >> offc2));
            triePos += 2;
#else
            msb5c2 |= 0x07;
            insAt(triePos, msb5c2, 0, (0x80 >> offc2));
            triePos += 3;
#endif
        }
        break;
    }
    insertState = 0;
}

int16_t dfox_node_handler::locate(int16_t level) {
    register int16_t r_keyPos = 0;
    register byte kc = key[r_keyPos++];
    register int16_t pos = 0;
    register int32_t i = 0;
    register byte len = TRIE_LEN;
    do {
        register byte tc = trie[i];
        if ((kc ^ tc) < 0x08) {
            register byte r_leaves;
            register byte children;
            register int16_t to_skip = 0;
            register byte offset = (kc & 0x07);
            register byte r_mask = (0x80 >> offset);
            if (isPut)
                origPos = i;
            i++;
#ifdef FAVOUR_SIZE
            children = 0;
            r_leaves = 0;
            if (tc & 0x04)
#endif
            children = trie[i++];
#ifdef FAVOUR_SIZE
            if (tc & 0x02)
#endif
            r_leaves = trie[i++];
//            switch (tc & 0x06) {
//            case 0x02:
//                leaves = trie[i++];
//                children = 0;
//                break;
//            case 0x04:
//                children = trie[i++];
//                leaves = 0;
//                break;
//            case 0x06:
//                children = trie[i++];
//                leaves = trie[i++];
//                break;
//            }
            //if (children > mask)
            to_skip = GenTree::bit_count[children & left_mask[offset]];
            //if (leaves > mask)
            pos += GenTree::bit_count[r_leaves & left_mask[offset]];
            while (to_skip) {
                register byte child_tc = trie[i++];
#ifdef FAVOUR_SIZE
                if (child_tc & 0x04)
#endif
                to_skip += GenTree::bit_count[trie[i++]];
#ifdef FAVOUR_SIZE
                if (child_tc & 0x02)
#endif
                pos += GenTree::bit_count[trie[i++]];
                if (child_tc & 0x01)
                    to_skip--;
            }
            if (children & r_mask) {
                if (r_leaves & r_mask) {
                    if (r_keyPos == key_len)
                        return pos;
                    else
                        pos++;
                } else {
                    if (r_keyPos == key_len) {
                        if (isPut) {
                            this->leaves = r_leaves;
                            this->mask = r_mask;
                            triePos = i;
                            insertState = INSERT_LEAF;
                            need_count = 2;
                        }
                        return ~pos;
                    }
                }
            } else {
                if (r_leaves & r_mask) {
                    //pos++;
                    int16_t key_at_len;
                    const char *key_at = (char *) getKey(pos, &key_at_len);
                    int16_t cmp = util::compare(key, key_len, key_at,
                            key_at_len, r_keyPos);
                    if (cmp == 0)
                        return pos;
                    if (cmp > 0)
                        pos++;
                    if (isPut) {
                        triePos = i;
                        this->mask = r_mask;
                        this->keyPos = r_keyPos;
                        this->key_at = key_at;
                        this->key_at_len = key_at_len;
                        insertState = INSERT_THREAD;
                        if (cmp < 0)
                            cmp = -cmp;
                        if (r_keyPos < cmp)
                            cmp -= r_keyPos;
#ifdef FAVOUR_SIZE
                        need_count = cmp * 2;
#else
                        need_count = cmp * 3;
#endif
                        need_count += 4;
                    }
                    return ~pos;
                } else {
                    if (isPut) {
                        triePos = i;
                        this->mask = r_mask;
                        this->leaves = r_leaves;
                        insertState = INSERT_LEAF;
                        need_count = 2;
                    }
                    return ~pos;
                }
            }
            kc = key[r_keyPos++];
        } else if (kc > tc) {
            register int16_t to_skip = 0;
            if (isPut)
                origPos = i;
            i++;
#ifdef FAVOUR_SIZE
            if (tc & 0x04)
#endif
            to_skip += GenTree::bit_count[trie[i++]];
#ifdef FAVOUR_SIZE
            if (tc & 0x02)
#endif
            pos += GenTree::bit_count[trie[i++]];
            while (to_skip) {
                register byte child_tc = trie[i++];
#ifdef FAVOUR_SIZE
                if (child_tc & 0x04)
#endif
                to_skip += GenTree::bit_count[trie[i++]];
#ifdef FAVOUR_SIZE
                if (child_tc & 0x02)
#endif
                pos += GenTree::bit_count[trie[i++]];
                if (child_tc & 0x01)
                    to_skip--;
            }
            if (tc & 0x01) {
                if (isPut) {
                    triePos = i;
                    this->tc = tc;
                    this->mask = (0x80 >> (kc & 0x07));
                    msb5 = (kc & 0xF8);
                    insertState = INSERT_MIDDLE1;
                    need_count = 2;
                }
                return ~pos;
            }
        } else {
            if (isPut) {
                msb5 = (kc & 0xF8);
                this->mask = (0x80 >> (kc & 0x07));
                insertState = INSERT_MIDDLE2;
                need_count = 2;
                this->tc = tc;
                triePos = i;
            }
            return ~pos;
        }
    } while (i < TRIE_LEN);
    if (isPut) {
        triePos = i;
        this->tc = tc;
        this->mask = (0x80 >> (kc & 0x07));
        msb5 = (kc & 0xF8);
        insertState = INSERT_MIDDLE1;
        need_count = 2;
    }
    return ~pos;
}

void dfox_node_handler::delAt(byte pos) {
    TRIE_LEN--;
    byte *ptr = trie + pos;
    memmove(ptr, ptr + 1, TRIE_LEN - pos);
}

void dfox_node_handler::delAt(byte pos, int16_t count) {
    TRIE_LEN-= count;
    byte *ptr = trie + pos;
    memmove(ptr, ptr + count, TRIE_LEN - pos);
}

void dfox_node_handler::insAt(byte pos, byte b) {
    byte *ptr = trie + pos;
    memmove(ptr + 1, ptr, TRIE_LEN- pos);
    trie[pos] = b;
    TRIE_LEN++;
}

void dfox_node_handler::insAt(byte pos, byte b1, byte b2) {
    byte *ptr = trie + pos;
    memmove(ptr + 2, ptr, TRIE_LEN- pos);
    trie[pos++] = b1;
    trie[pos] = b2;
    TRIE_LEN+= 2;
}

void dfox_node_handler::insAt(byte pos, byte b1, byte b2, byte b3) {
    byte *ptr = trie + pos;
    memmove(ptr + 3, ptr, TRIE_LEN- pos);
    trie[pos++] = b1;
    trie[pos++] = b2;
    trie[pos] = b3;
    TRIE_LEN+= 3;
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
    keyPos = 0;
    triePos = 0;
    need_count = 0;
    insertState = 0;
}

byte dfox_node_handler::left_mask[8] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8,
        0xFC, 0xFE };
byte dfox_node_handler::ryte_mask[8] = { 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03,
        0x01, 0x00 };
byte dfox_node_handler::ryte_incl_mask[8] = { 0xFF, 0x7F, 0x3F, 0x1F, 0x0F,
        0x07, 0x03, 0x01 };
byte dfox_node_handler::pos_mask[48] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04,
        0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40,
        0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04,
        0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40,
        0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
