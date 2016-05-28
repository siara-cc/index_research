#include <iostream>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include "dfos.h"
#include "GenTree.h"

char *dfos::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    int16_t pos = -1;
    byte *node_data = root_data;
    dfos_node_handler node(node_data);
    node.key = key;
    node.key_len = key_len;
    recursiveSearchForGet(&node, &pos);
    if (pos < 0)
        return null;
    return (char *) node.getData(pos, pValueLen);
}

void dfos::recursiveSearchForGet(dfos_node_handler *node, int16_t *pIdx) {
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

void dfos::recursiveSearch(dfos_node_handler *node, int16_t lastSearchPos[],
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

void dfos::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    int16_t lastSearchPos[10];
    byte *block_paths[10];
    dfos_node_handler node(root_data);
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

void dfos::recursiveUpdate(dfos_node_handler *node, int16_t pos,
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
            dfos_node_handler new_block(b);
            new_block.initVars();
            new_block.isPut = true;
            if (node->isLeaf())
                blockCount++;
            if (root_data == node->buf) {
                root_data = (byte *) util::alignedAlloc(DFOS_NODE_SIZE);
                dfos_node_handler root(root_data);
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
                dfos_node_handler parent(parent_data);
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

byte *dfos_node_handler::split(int16_t *pbrk_idx) {
    int16_t orig_filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(DFOS_NODE_SIZE);
    dfos_node_handler old_block(this->buf);
    old_block.isPut = true;
    dfos_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    if (!isLeaf())
        new_block.setLeaf(false);
    int16_t kv_last_pos = old_block.getKVLastPos();
    int16_t halfKVLen = DFOS_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int16_t new_idx;
    int16_t brk_idx = -1;
    int16_t brk_kv_pos = 0;
    int16_t tot_len = 0;
    /*    // (1) move all data to new_block in order
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
     memcpy(old_block.buf + DFOS_NODE_SIZE - old_blk_new_len,
     new_block.buf + kv_last_pos, old_blk_new_len);
     // (3) Insert pointers to the moved data in old block
     old_block.FILLED_SIZE = 0;
     old_block.TRIE_LEN = 0;
     for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
     int16_t src_idx = new_block.getPtr(new_idx);
     src_idx += (DFOS_NODE_SIZE - brk_kv_pos);
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
     // (4) move new block pointers to front
     //byte *new_kv_idx = new_block->buf + IDX_HDR_SIZE - orig_filled_size;
     //memmove(new_kv_idx + new_idx, new_kv_idx, new_size);
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
     new_block.TRIE_LEN = 0;
     new_block.setFilledSize(new_size); // (8) Filled Size for New Block
     for (int16_t i = 0; i < new_size; i++) {
     new_block.initVars();
     new_block.key = (char *) new_block.getKey(i, &new_block.key_len);
     if (i)
     new_block.locate(0);
     new_block.insertCurrent();
     }*/
    *pbrk_idx = brk_idx;
    return new_block.buf;
}

dfos::dfos() {
    root_data = (byte *) util::alignedAlloc(DFOS_NODE_SIZE);
    dfos_node_handler root(root_data);
    root.initBuf();
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 0; //9999;
    blockCount = 1;
    maxThread = 9999;
}

dfos::~dfos() {
    delete root_data;
}

dfos_node_handler::dfos_node_handler(byte *m) {
    setBuf(m);
    isPut = false;
}

void dfos_node_handler::initBuf() {
    memset(buf, '\0', DFOS_NODE_SIZE);
    setLeaf(1);
    FILLED_SIZE = 0;
    TRIE_LEN = 0;
    setKVLastPos(DFOS_NODE_SIZE);
}

void dfos_node_handler::setBuf(byte *m) {
    buf = m;
    trie = buf + IDX_HDR_SIZE;
}

void dfos_node_handler::setKVLastPos(int16_t val) {
    util::setInt(LAST_DATA_PTR, val);
}

int16_t dfos_node_handler::getKVLastPos() {
    return util::getInt(LAST_DATA_PTR);
}

void dfos_node_handler::addData(int16_t ptr) {

    int16_t kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_len;
    memcpy(buf + kv_last_pos + 1, key, key_len);
    buf[kv_last_pos + key_len + 1] = value_len;
    memcpy(buf + kv_last_pos + key_len + 2, value, value_len);

    insertCurrent(kv_last_pos);
    FILLED_SIZE++;

}

bool dfos_node_handler::isLeaf() {
    return IS_LEAF_BYTE;
}

void dfos_node_handler::setLeaf(char isLeaf) {
    IS_LEAF_BYTE = isLeaf;
}

void dfos_node_handler::setFilledSize(int16_t filledSize) {
    FILLED_SIZE = filledSize;
}

bool dfos_node_handler::isFull(int16_t kv_len) {
    if ((getKVLastPos() - kv_len - 2) < (TRIE_LEN + need_count + IDX_HDR_SIZE))
        return true;
    return false;
}

int16_t dfos_node_handler::filledSize() {
    return FILLED_SIZE;
}

byte *dfos_node_handler::getChild(int16_t ptr) {
    byte *idx = getData(ptr, &ptr);
    unsigned long addr_num = util::fourBytesToPtr(idx);
    byte *ret = (byte *) addr_num;
    return ret;
}

byte *dfos_node_handler::getKey(int16_t ptr, int16_t *plen) {
    byte *kvIdx = buf + ptr;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *dfos_node_handler::getData(int16_t ptr, int16_t *plen) {
    byte *kvIdx = buf + ptr;
    *plen = kvIdx[0];
    kvIdx++;
    kvIdx += *plen;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

void dfos_node_handler::insertCurrent(int16_t kv_pos) {
    byte child;
    byte origTC;
    byte leafPos;
    byte childPos;
    int16_t pos, min;

    if (TRIE_LEN == 0) {
        byte kc = key[0];
        byte offset = (kc & 0x07);
        byte mask = 0x80 >> offset;
        insAt(0, (kc & 0xF8) | 0x05, mask, kv_pos);
        insertState = 0;
        return;
    }

    switch (insertState) {
    case INSERT_MIDDLE1:
        tc &= 0xFB;
        setAt(origPos, tc);
        insAt(triePos, (msb5 | 0x05), mask, kv_pos);
        triePos += 2;
        break;
    case INSERT_MIDDLE2:
        insAt(triePos, (msb5 | 0x01), mask, kv_pos);
        triePos += 2;
        break;
    case INSERT_LEAF:
        origTC = getAt(origPos);
        leafPos = origPos + 1;
        if (origTC & 0x02)
            leafPos++;
        if (origTC & 0x01) {
            byte ptr_count = GenTree::bit_count[leaves & ~(mask - 1)];
            leaves |= mask;
            setAt(leafPos, leaves);
            leafPos++;
            if (DFOS_NODE_SIZE == 512) {
                if (kv_pos >= 256) {
                    byte high_bits = getAt(leafPos);
                    setAt(leafPos, high_bits | mask);
                }
                leafPos++;
                insAt(leafPos + ptr_count, (byte) (kv_pos & 0xFF));
                triePos++;
            } else {
                insAt(leafPos + ptr_count * 2, kv_pos);
                triePos += 2;
            }
        } else {
            if (DFOS_NODE_SIZE == 512)
                insAt(leafPos, mask, (kv_pos >= 256 ? mask : 0), (byte) (kv_pos & 0xFF));
            else
                insAt(leafPos, mask, (byte) (kv_pos >> 8), (byte) (kv_pos & 0xFF));
            triePos += 3;
            setAt(origPos, (origTC | 0x01));
        }
        break;
    case INSERT_THREAD:
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
        pos = keyPos - 1;
        min = util::min(key_len, key_at_len);
        byte c1, c2;
        c1 = key[pos];
        c2 = key_at[pos];
        int16_t c2_kv_pos = 0;
        pos++;
        if (pos < min) {
            leafPos = childPos;
            if (origTC & 0x01) {
                leafPos++;
                byte leaf = getAt(leafPos);
                leaf &= ~mask;
                if (leaf) {
                    setAt(leafPos, leaf);
                    byte ptr_count = GenTree::bit_count[leaf & ~(mask - 1)];
                    leafPos++;
                    if (DFOS_NODE_SIZE == 512) {
                        if (getAt(leafPos) & mask)
                            c2_kv_pos = 256;
                        leafPos++;
                        c2_kv_pos += getAt(leafPos + ptr_count);
                        delAt(leafPos + ptr_count);
                        triePos--;
                    } else {
                        c2_kv_pos = util::getInt(trie + leafPos + ptr_count * 2);
                        delAt(leafPos + ptr_count * 2, 2);
                        triePos -= 2;
                    }
                } else {
                    if (DFOS_NODE_SIZE == 512) {
                        c2_kv_pos = getAt(leafPos + 2);
                        if (getAt(leafPos + 1) & mask)
                            c2_kv_pos += 256;
                    } else {
                        c2_kv_pos = util::getInt(trie + leafPos + 1);
                    }
                    delAt(leafPos, 3);
                    triePos -= 3;
                    origTC &= 0xFE;
                    setAt(origPos, origTC);
                }
            }
            do {
                c1 = key[pos];
                c2 = key_at[pos];
                if (c1 > c2) {
                    int16_t swap = c1;
                    c1 = c2;
                    c2 = swap;
                    swap = kv_pos;
                    kv_pos = c2_kv_pos;
                    c2_kv_pos = swap;
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
                        insAt(triePos, msb5c2, c2leaf, c2_kv_pos);
                    }
                }
                if (c1leaf != 0) {
                    if (((c1 ^ c2) < 0x08) && c1 != c2)
                        triePos += insAt(triePos, msb5c1, c1child, c1leaf,
                                kv_pos, c2_kv_pos, (0x80 >> offc2));
                    else {
                        int16_t c1_kv_pos = kv_pos;
                        if (c1 == c2 && (pos + 1) == key_at_len)
                            c1_kv_pos = c2_kv_pos;
                        triePos += insAt(triePos, msb5c1, c1child, c1leaf,
                                c1_kv_pos);
                    }
                } else {
                    insAt(triePos, msb5c1, c1child);
                    triePos += 2;
                }
                if (c1 != c2)
                    break;
                pos++;
            } while (pos < min);
        }
        if (c1 == c2) {
            if (pos == key_at_len) {
                c2 = key[pos];
            } else {
                leafPos = childPos;
                if (origTC & 0x01) {
                    leafPos++;
                    byte ptr_count = GenTree::bit_count[leaves & ~(mask - 1)];
                    leafPos++;
                    if (DFOS_NODE_SIZE == 512) {
                        c2_kv_pos = getAt(leafPos + ptr_count);
                        byte bitmap = getAt(leafPos);
                        if (bitmap & mask)
                            c2_kv_pos += 256;
                        if (kv_pos >= 256)
                            bitmap |= mask;
                        else
                            bitmap &= ~mask;
                        setAt(leafPos, bitmap);
                        setAt(leafPos + ptr_count, kv_pos & 0xFF);
                    } else {
                        ptr_count--; // re-evaluate
                        c2_kv_pos = util::getInt(trie + leafPos + ptr_count * 2);
                        setAt(leafPos + ptr_count * 2, kv_pos);
                    }
                }
                c2 = key_at[pos];
                kv_pos = c2_kv_pos;
            }
            int16_t msb5c2 = (c2 & 0xF8);
            int16_t offc2 = (c2 & 0x07);
            msb5c2 |= 0x05;
            insAt(triePos, msb5c2, (0x80 >> offc2), kv_pos);
            triePos += 4;
        }
        break;
    }
    insertState = 0;
}

int16_t dfos_node_handler::locate(int16_t level) {
    register int keyPos = 0;
    register int i = 0;
    register byte kc = key[keyPos++];
    do {
        register byte tc = trie[i];
        if ((kc ^ tc) < 0x08) {
            register byte r_leaves;
            register byte children;
            register byte offset = (kc & 0x07);
            register byte r_mask = (0x80 >> offset);
            if (isPut)
                origPos = i;
            i++;
            children = 0;
            r_leaves = 0;
            if (tc & 0x02)
                children = trie[i++];
            if (tc & 0x01)
                r_leaves = trie[i++];
            if (children & r_mask) {
                if (r_leaves & r_mask) {
                    if (keyPos == key_len) {
                        register int leaf_count = GenTree::bit_count[r_leaves
                                & left_mask[offset]];
                        int16_t ptr;
                        if (DFOS_NODE_SIZE == 512) {
                            register byte bitmap = trie[i++];
                            if (bitmap & r_mask)
                                ptr = 256 + trie[i + leaf_count];
                            else
                                ptr = trie[i + leaf_count];
                        } else
                            ptr = util::getInt(trie + i + leaf_count * 2);
                        return ptr;
                    }
                } else {
                    if (keyPos == key_len) {
                        if (isPut) {
                            this->leaves = r_leaves;
                            this->mask = r_mask;
                            triePos = i;
                            insertState = INSERT_LEAF;
                            need_count = 1;
                            if (DFOS_NODE_SIZE != 512)
                                need_count++;
                        }
                        return -1;
                    }
                }
                byte leaf_count = GenTree::bit_count[r_leaves];
                if (DFOS_NODE_SIZE == 512) {
                    if (leaf_count)
                        leaf_count++;
                } else
                    leaf_count <<= 1;
                i += leaf_count;
                register int to_skip = GenTree::bit_count[children
                        & left_mask[offset]];
                while (to_skip) {
                    register byte child_tc = trie[i++];
                    if (child_tc & 0x02)
                        to_skip += GenTree::bit_count[trie[i++]];
                    if (child_tc & 0x01) {
                        byte leaf_count = GenTree::bit_count[trie[i++]];
                        if (DFOS_NODE_SIZE == 512) {
                            if (leaf_count)
                                leaf_count++;
                        } else
                            leaf_count <<= 1;
                        i += leaf_count;
                    }
                    if (child_tc & 0x04)
                        to_skip--;
                }
            } else {
                if (r_leaves & r_mask) {
                    int16_t ptr;
                    register int leaf_count = GenTree::bit_count[r_leaves
                            & left_mask[offset]];
                    if (DFOS_NODE_SIZE == 512) {
                        register byte bitmap = trie[i++];
                        ptr = trie[i + leaf_count];
                        if (bitmap & r_mask)
                            ptr += 256;
                    } else {
                        ptr = util::getInt(trie + i + leaf_count * 2);
                        i += GenTree::bit_count[r_leaves];
                    }
                    i += GenTree::bit_count[r_leaves];
                    int16_t key_at_len;
                    const char *key_at = (char *) getKey(ptr, &key_at_len);
                    int16_t cmp = util::compare(key, key_len, key_at,
                            key_at_len, keyPos);
                    if (cmp == 0)
                        return ptr;
                    if (isPut) {
                        register int to_skip = GenTree::bit_count[children
                                & left_mask[offset]];
                        while (to_skip) {
                            register byte child_tc = trie[i++];
                            if (child_tc & 0x02)
                                to_skip += GenTree::bit_count[trie[i++]];
                            if (child_tc & 0x01) {
                                byte leaf_count = GenTree::bit_count[trie[i++]];
                                if (DFOS_NODE_SIZE == 512) {
                                    if (leaf_count)
                                        leaf_count++;
                                } else
                                    leaf_count <<= 1;
                                i += leaf_count;
                            }
                            if (child_tc & 0x04)
                                to_skip--;
                        }
                        triePos = i;
                        this->mask = r_mask;
                        this->keyPos = keyPos;
                        this->key_at = key_at;
                        this->key_at_len = key_at_len;
                        insertState = INSERT_THREAD;
                        if (cmp < 0)
                            cmp = -cmp;
                        if (keyPos < cmp)
                            cmp -= keyPos;
                        need_count = cmp * 2;
                        need_count += 8;
                    }
                    return -1;
                } else {
                    if (isPut) {
                        triePos = GenTree::bit_count[r_leaves];
                        if (DFOS_NODE_SIZE == 512)
                            triePos++;
                        else
                            triePos <<= 1;
                        triePos += i;
                        this->tc = tc;
                        this->mask = r_mask;
                        this->leaves = r_leaves;
                        insertState = INSERT_LEAF;
                        need_count = 1;
                        if (DFOS_NODE_SIZE != 512)
                            need_count++;
                    }
                    return -1;
                }
            }
            kc = key[keyPos++];
        } else if (kc > tc) {
            register int to_skip = 0;
            if (isPut)
                origPos = i;
            i++;
            if (tc & 0x02)
                to_skip += GenTree::bit_count[trie[i++]];
            if (tc & 0x01) {
                byte leaf_count = GenTree::bit_count[trie[i++]];
                if (DFOS_NODE_SIZE == 512) {
                    if (leaf_count)
                        leaf_count++;
                } else
                    leaf_count <<= 1;
                i += leaf_count;
            }
            while (to_skip) {
                register byte child_tc = trie[i++];
                if (child_tc & 0x02)
                    to_skip += GenTree::bit_count[trie[i++]];
                if (child_tc & 0x01) {
                    byte leaf_count = GenTree::bit_count[trie[i++]];
                    if (DFOS_NODE_SIZE == 512) {
                        if (leaf_count)
                            leaf_count++;
                    } else
                        leaf_count <<= 1;
                    i += leaf_count;
                }
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
                    need_count = 4;
                }
                return -1;
            }
        } else {
            if (isPut) {
                msb5 = (kc & 0xF8);
                this->mask = (0x80 >> (kc & 0x07));
                insertState = INSERT_MIDDLE2;
                need_count = 4;
                this->tc = tc;
                triePos = i;
            }
            return -1;
        }
    } while (1);
    return -1;
}

void dfos_node_handler::delAt(byte pos) {
    TRIE_LEN--;
    byte *ptr = trie + pos;
    memmove(ptr, ptr + 1, TRIE_LEN - pos);
}

void dfos_node_handler::delAt(byte pos, int16_t count) {
    TRIE_LEN -= count;
    byte *ptr = trie + pos;
    memmove(ptr, ptr + count, TRIE_LEN - pos);
}

void dfos_node_handler::insAt(byte pos, byte b) {
    byte *ptr = trie + pos;
    memmove(ptr + 1, ptr, TRIE_LEN - pos);
    trie[pos] = b;
    TRIE_LEN++;
}

void dfos_node_handler::insAt(byte pos, int16_t i) {
    byte *ptr = trie + pos;
    memmove(ptr + 2, ptr, TRIE_LEN - pos);
    util::setInt(trie + pos, i);
    TRIE_LEN += 2;
}

void dfos_node_handler::insAt(byte pos, byte b1, byte b2) {
    byte *ptr = trie + pos;
    memmove(ptr + 2, ptr, TRIE_LEN - pos);
    trie[pos++] = b1;
    trie[pos] = b2;
    TRIE_LEN += 2;
}

void dfos_node_handler::insAt(byte pos, byte b1, byte b2, byte b3) {
    byte *ptr = trie + pos;
    memmove(ptr + 3, ptr, TRIE_LEN - pos);
    trie[pos++] = b1;
    trie[pos++] = b2;
    trie[pos] = b3;
    TRIE_LEN += 3;
}

byte dfos_node_handler::insAt(byte pos, byte tc, byte child, byte leaf,
        int16_t c1_kv_pos) {
    return insAt(pos, tc, child, leaf, c1_kv_pos, 0, 0);
}

byte dfos_node_handler::insAt(byte pos, byte tc, byte child, byte leaf,
        int16_t c1_kv_pos, int16_t c2_kv_pos, byte c2_mask) {
    byte *ptr = trie + pos;
    int size = 4;
    if (c2_kv_pos) {
        if (DFOS_NODE_SIZE == 512)
            size = 5;
        else
            size = 6;
    }
    if (child)
        size++;
    memmove(ptr + size, ptr, TRIE_LEN - pos);
    trie[pos++] = tc;
    if (child)
        trie[pos++] = child;
    trie[pos++] = leaf;
    if (DFOS_NODE_SIZE == 512) {
        byte bitmap = 0;
        if (c1_kv_pos >= 256 && c2_kv_pos >= 256)
            bitmap = leaf;
        else if (c1_kv_pos >= 256)
            bitmap = leaf & ~c2_mask;
        else if (c2_kv_pos >= 256)
            bitmap = c2_mask;
        trie[pos++] = bitmap;
        trie[pos++] = (c1_kv_pos & 0xFF);
        if (c2_kv_pos)
            trie[pos] = (c2_kv_pos & 0xFF);
    } else {
        util::setInt(trie + pos, c1_kv_pos);
        if (c2_kv_pos)
            util::setInt(trie + pos + 2, c2_kv_pos);
    }
    TRIE_LEN += size;
    return size;
}

byte dfos_node_handler::insAt(byte pos, byte tc, byte leaf, int16_t c1_kv_pos) {
    byte *ptr = trie + pos;
    int size = 4;
    memmove(ptr + size, ptr, TRIE_LEN - pos);
    trie[pos++] = tc;
    trie[pos++] = leaf;
    if (DFOS_NODE_SIZE == 512) {
        if (c1_kv_pos >= 256)
            trie[pos++] = leaf;
        else
            trie[pos++] = 0x00;
        trie[pos] = (c1_kv_pos & 0xFF);
    } else
        util::setInt(trie + pos, c1_kv_pos);
    TRIE_LEN += size;
    return size;
}

void dfos_node_handler::setAt(byte pos, byte b) {
    trie[pos] = b;
}

byte dfos_node_handler::getAt(byte pos) {
    return trie[pos];
}

void dfos_node_handler::initVars() {
    //keyPos = 0;
    triePos = 0;
    need_count = 0;
    insertState = 0;
}

byte dfos_node_handler::left_mask[8] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8,
        0xFC, 0xFE };
byte dfos_node_handler::ryte_mask[8] = { 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03,
        0x01, 0x00 };
byte dfos_node_handler::ryte_incl_mask[8] = { 0xFF, 0x7F, 0x3F, 0x1F, 0x0F,
        0x07, 0x03, 0x01 };
byte dfos_node_handler::pos_mask[48] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04,
        0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40,
        0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04,
        0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40,
        0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
