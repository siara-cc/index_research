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
            maxKeyCount += node->filledSize();
            int16_t brk_idx;
            byte *b = node->split(&brk_idx);
            node->trie = node->buf + IDX_HDR_SIZE + node->FILLED_SIZE;
            dfox_node_handler new_block(b);
            new_block.initVars();
            new_block.isPut = true;
            if (node->isLeaf())
                blockCount++;
            if (root_data == node->buf) {
                root_data = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
                dfox_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
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
    byte *b = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
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
    old_block.FILLED_SIZE = 0;
    old_block.TRIE_LEN = 0;
    old_block.trie = buf + IDX_HDR_SIZE;
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

    if (brk_idx > 31) {
        (*new_block.bitmap1) = (*new_block.bitmap2);
        (*new_block.bitmap1) <<= (brk_idx - 32);
    } else {
        (*new_block.bitmap1) <<= brk_idx;
        (*new_block.bitmap1) |= ((*new_block.bitmap2) >> (32 - brk_idx));
    }

    if (IS_LEAF_BYTE)
        new_block.setLeaf(1);
    new_block.setKVLastPos(brk_kv_pos); // (5) New Block Last position
    //filled_size = brk_idx + 1;
    //setFilledSize(filled_size); // (7) Filled Size for Old block
    new_block.TRIE_LEN = 0;
    memmove(new_block.buf + IDX_HDR_SIZE,
            new_block.buf + IDX_HDR_SIZE + brk_idx + 1, new_size);
    new_block.setFilledSize(new_size); // (8) Filled Size for New Block
    new_block.trie = new_block.buf + IDX_HDR_SIZE + new_size;
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
    root_data = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
    dfox_node_handler root(root_data);
    root.initBuf();
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 0; //9999;
    blockCount = 1;
    maxThread = 9999;
    uint32_t ui32 = 1;
    for (int i = 0; i < 32; i++) {
        mask32[i] = (0x80000000 >> i);
        if (i == 0) {
            ryte_mask32[i] = 0xFFFFFFFF;
            left_mask32[i] = 0;
        } else {
            ryte_mask32[i] = (ui32 << (32 - i));
            ryte_mask32[i]--;
            left_mask32[i] = ~(ryte_mask32[i]);
        }
    }
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
    FILLED_SIZE = 0;
    TRIE_LEN = 0;
    setKVLastPos(DFOX_NODE_SIZE);
    trie = buf + IDX_HDR_SIZE;
    bitmap1 = (uint32_t *) buf;
    bitmap2 = bitmap1 + 1;
}

void dfox_node_handler::setBuf(byte *m) {
    buf = m;
    trie = buf + IDX_HDR_SIZE + FILLED_SIZE;
    bitmap1 = (uint32_t *) buf;
    bitmap2 = bitmap1 + 1;
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
    int16_t key_left = key_len - keyPos;
    register int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_left;
    if (key_left)
        memcpy(buf + kv_last_pos + 1, key + keyPos, key_left);
    buf[kv_last_pos + key_left + 1] = value_len;
    memcpy(buf + kv_last_pos + key_left + 2, value, value_len);

    insPtr(pos, kv_last_pos);

}

void dfox_node_handler::insBit(uint32_t *ui32, int pos, int16_t kv_pos) {
    register uint32_t ryte_part = (*ui32) & dfox::ryte_mask32[pos];
    ryte_part >>= 1;
    if (kv_pos >= 256)
        ryte_part |= dfox::mask32[pos];
    (*ui32) = (ryte_part | ((*ui32) & dfox::left_mask32[pos]));

}

void dfox_node_handler::insPtr(int16_t pos, int16_t kv_pos) {
    register int16_t filledSz = filledSize();
    register byte *kvIdx = buf + IDX_HDR_SIZE + pos;
    memmove(kvIdx + 1, kvIdx, filledSz - pos + TRIE_LEN);
    kvIdx[0] = (byte) (kv_pos & 0xFF);
    filledSz++;
    trie++;
    if (pos > 31) {
        int idx = pos - 32;
        insBit(bitmap2, idx, kv_pos);
    } else {
        byte last_bit = (*bitmap1 & 0x01);
        insBit(bitmap1, pos, kv_pos);
        *bitmap2 >>= 1;
        if (last_bit)
            *bitmap2 |= *dfox::mask32;
    }
    FILLED_SIZE = filledSz;
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
    if ((getKVLastPos() - kv_len - 3)
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
    if (pos & 0xFFE0) {
        pos -= 32;
        if (ptr >= 256)
            *bitmap2 |= dfox::mask32[pos];
        else
            *bitmap2 &= ~dfox::mask32[pos];
    } else {
        if (ptr >= 256)
            *bitmap1 |= dfox::mask32[pos];
        else
            *bitmap1 &= ~dfox::mask32[pos];
    }
}

int16_t dfox_node_handler::getPtr(register int16_t pos) {
    int16_t ptr = buf[IDX_HDR_SIZE + pos];
    if (pos & 0xFFE0) {
        if (*bitmap2 & dfox::mask32[pos - 32])
            ptr |= 256;
    } else {
        if (*bitmap1 & dfox::mask32[pos])
            ptr |= 256;
    }
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

    if (TRIE_LEN == 0) {
        byte kc = key[0];
        byte offset = (kc & 0x07);
        append((kc & 0xF8) | 0x05);
        append(0x80 >> offset);
        return;
    }

    register byte origTC;
    register byte leafPos;

    switch (insertState) {
    case INSERT_MIDDLE1:
        tc &= 0xFB;
        setAt(origPos, tc);
        insAt(triePos, (msb5 | 0x05), mask);
        break;
    case INSERT_MIDDLE2:
        insAt(triePos, (msb5 | 0x01), mask);
        break;
    case INSERT_LEAF:
        origTC = getAt(origPos);
        leafPos = origPos + 1;
        if (origTC & 0x02)
            leafPos++;
        if (origTC & 0x01) {
            leaves |= mask;
            setAt(leafPos, leaves);
        } else {
            insAt(leafPos, mask);
            setAt(origPos, (origTC | 0x01));
        }
        break;
    case INSERT_THREAD:
        register byte child;
        register byte childPos;
        register int16_t c_pos, c_pos_at, min;
        childPos = origPos + 1;
        child = mask;
        origTC = getAt(origPos);
        if (origTC & 0x02) {
            child |= getAt(childPos);
            setAt(childPos, child);
        } else {
            insAt(childPos, child);
            triePos++;
            origTC |= 0x02;
            setAt(origPos, origTC);
        }
        c_pos = keyPos; // - 1;
        min = util::min(key_len, c_pos + key_at_len);
        byte c1, c2;
        c1 = key[c_pos];
        c2 = key_at[0];
        //c_pos++;
        c_pos_at = c_pos;
        if (c_pos < min) {
            leafPos = childPos;
            if (origTC & 0x01) {
                leafPos++;
                byte leaf = getAt(leafPos);
                leaf &= ~mask;
                if (leaf)
                    setAt(leafPos, leaf);
                else {
                    delAt(leafPos);
                    triePos--;
                    origTC &= 0xFE;
                    setAt(origPos, origTC);
                }
            }
            do {
                c1 = key[c_pos];
                c2 = key_at[c_pos - keyPos];
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
                    if (c_pos + 1 == min) {
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
                        insAt(triePos, msb5c2, c2leaf);
                    }
                }
                if (c1leaf != 0 && c1child != 0) {
                    insAt(triePos, msb5c1, c1child, c1leaf);
                    triePos += 3;
                } else if (c1leaf != 0) {
                    insAt(triePos, msb5c1, c1leaf);
                    triePos += 2;
                } else {
                    insAt(triePos, msb5c1, c1child);
                    triePos += 2;
                }
                c_pos++;
                if (c1 != c2)
                    break;
            } while (c_pos < min);
        }
        if (keyPos == key_len)
            c_pos++;
        if (c_pos > c_pos_at) {
            int16_t key_at_ptr = getPtr(key_at_pos);
            int16_t new_len = buf[key_at_ptr];
            int16_t diff = (c_pos - c_pos_at);
            new_len -= diff;
            key_at_ptr += diff;
            buf[key_at_ptr] = new_len;
            setPtr(key_at_pos, key_at_ptr);
        }
        if (c1 == c2 || keyPos == key_len) {
            if (keyPos == (key_len - 1))
                c2 = key_at[c_pos];
            else if (keyPos != key_len) {
                c2 = key[c_pos];
                c_pos++;
            }
            int16_t msb5c2 = (c2 & 0xF8);
            int16_t offc2 = (c2 & 0x07);
            msb5c2 |= 0x05;
            insAt(triePos, msb5c2, (0x80 >> offc2));
        }
        if (keyPos < key_len)
            keyPos = c_pos;
        break;
    }
}

int16_t dfox_node_handler::locate(int16_t level) {
    keyPos = 0;
    register int i = 0;
    register int skip_count = 0;
    byte kc = key[keyPos++];
    register int16_t pos = 0;
    memset(skip_list, 0, sizeof(skip_list));
    do {
        register byte tc = trie[i];
        if (tc & 0x03)
            skip_count = 0;
        else {
            skip_count = tc >> 2;
            if (isPut)
                skip_list[keyPos] = i;
            i++;
            continue;
        }
        if ((kc ^ tc) < eight) {
            register byte r_leaves = 0;
            register byte children = 0;
            register int to_skip = 0;
            register int offset = (kc & 0x07);
            register byte r_mask = (0x80 >> offset);
            if (isPut)
                origPos = i;
            i++;
            if (tc & 0x02) {
                children = trie[i++];
                if (isPut && !skip_count && !(tc & 0x04))
                    skip_list[keyPos] = i;
            }
            if (tc & 0x01)
                r_leaves = trie[i++];
            to_skip = GenTree::bit_count[children & left_mask[offset]];
            pos += GenTree::bit_count[r_leaves & left_mask[offset]];
            while (to_skip) {
                register byte child_tc = trie[i++];
                if (child_tc & 0x02)
                    to_skip += GenTree::bit_count[trie[i++]];
                if (child_tc & 0x01)
                    pos += GenTree::bit_count[trie[i++]];
                if (child_tc & 0x04)
                    to_skip--;
            }
            if (children & r_mask) {
                if (r_leaves & r_mask) {
                    if (keyPos == key_len)
                        return pos;
                    else
                        pos++;
                } else {
                    if (keyPos == key_len) {
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
                    key_at = (char *) getKey(pos, &key_at_len);
                    int16_t cmp = util::compare(key + keyPos, key_len - keyPos,
                            key_at, key_at_len);
                    if (cmp == 0)
                        return pos;
                    key_at_pos = pos;
                    if (cmp > 0)
                        pos++;
                    if (isPut) {
                        triePos = i;
                        this->leaves = r_leaves;
                        this->mask = r_mask;
                        insertState = INSERT_THREAD;
                        if (cmp < 0)
                            cmp = -cmp;
                        if (keyPos < cmp)
                            cmp -= keyPos;
                        need_count = cmp * 2;
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
            kc = key[keyPos++];
        } else if (kc > tc) {
            if (skip_count) {
                i += skip_count;
                continue;
            }
            register int to_skip = 0;
            if (isPut)
                origPos = i;
            i++;
            if (tc & 0x02)
                to_skip += GenTree::bit_count[trie[i++]];
            if (tc & 0x01)
                pos += GenTree::bit_count[trie[i++]];
            while (to_skip) {
                register byte child_tc = trie[i++];
                if (child_tc & 0x02)
                    to_skip += GenTree::bit_count[trie[i++]];
                if (child_tc & 0x01)
                    pos += GenTree::bit_count[trie[i++]];
                if (child_tc & 0x04)
                    to_skip--;
            }
            if (tc & 0x04) {
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

void dfox_node_handler::insAt(register byte pos, byte b) {
    register byte *ptr = trie + pos;
    memmove(ptr + 1, ptr, TRIE_LEN - pos);
    trie[pos] = b;
    TRIE_LEN++;
}

void dfox_node_handler::insAt(register byte pos, byte b1, byte b2) {
    register byte *ptr = trie + pos;
    memmove(ptr + 2, ptr, TRIE_LEN - pos);
    trie[pos++] = b1;
    trie[pos] = b2;
    TRIE_LEN += 2;
}

void dfox_node_handler::insAt(register byte pos, byte b1, byte b2, byte b3) {
    register byte *ptr = trie + pos;
    memmove(ptr + 3, ptr, TRIE_LEN - pos);
    trie[pos++] = b1;
    trie[pos++] = b2;
    trie[pos] = b3;
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
    //keyPos = 0;
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
uint32_t dfox::left_mask32[32];
uint32_t dfox::ryte_mask32[32];
uint32_t dfox::mask32[32];
