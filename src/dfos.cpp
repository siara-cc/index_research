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
        }/* else {
         do {
         node_data = node->getChild(pos);
         node->setBuf(node_data);
         level++;
         pos = node->getFirstPtr();
         } while (!node->isLeaf());
         *pIdx = pos;
         return;
         }*/
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
        }/* else {
         do {
         node_data = node->getChild(lastSearchPos[level]);
         node->setBuf(node_data);
         level++;
         node_paths[level] = node->buf;
         lastSearchPos[level] = node->getFirstPtr();
         } while (!node->isLeaf());
         *pIdx = lastSearchPos[level];
         return;
         }*/
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
            //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->DFOS_TRIE_LEN);
            maxKeyCount += node->filledSize();
            byte *b = node->split();
            dfos_node_handler new_block(b);
            new_block.initVars();
            new_block.isPut = true;
            if (node->isLeaf())
                blockCount++;
            bool isRoot = false;
            if (root_data == node->buf)
                isRoot = true;
            int16_t first_key_len = node->key_len;
            byte *old_buf = node->buf;
            const char *new_block_first_key = (char *) new_block.getFirstKey(
                    &first_key_len);
            int16_t cmp = util::compare(new_block_first_key, first_key_len,
                    node->key, node->key_len, 0);
            if (cmp <= 0)
                node->setBuf(new_block.buf);
            if (isRoot) {
                root_data = (byte *) util::alignedAlloc(DFOS_NODE_SIZE);
                dfos_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                char addr[5];
                util::ptrToFourBytes((unsigned long) old_buf, addr);
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = addr;
                root.value_len = sizeof(char *);
                root.addData(0);
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                root.initVars();
                root.key = new_block_first_key;
                root.key_len = first_key_len;
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
                parent.key = new_block_first_key;
                parent.key_len = first_key_len;
                parent.value = addr;
                parent.value_len = sizeof(char *);
                parent.locate(prev_level);
                recursiveUpdate(&parent, ~(lastSearchPos[prev_level] + 1),
                        lastSearchPos, node_paths, prev_level);
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

int dfos_node_handler::copyToNewBlock(int i, int depth,
        dfos_node_handler *new_block) {
    byte tc;
    do {
        tc = trie[i];
        new_block->trie[i++] = tc;
        byte children = 0;
        byte leaves = 0;
        if (tc & 0x02) {
            children = trie[i];
            new_block->trie[i++] = children;
        }
        byte ptr_high_bits = 0;
        byte ptr_pos = 0;
        byte high_bits_pos = 0;
        if (tc & 0x01) {
            leaves = trie[i];
            new_block->trie[i++] = leaves;
            if (DFOS_NODE_SIZE == 512) {
                ptr_high_bits = trie[i];
                high_bits_pos = i;
                new_block->trie[i++] = 0;
            }
            ptr_pos = i;
            if (DFOS_NODE_SIZE != 512)
                i += GenTree::bit_count[leaves];
            i += GenTree::bit_count[leaves];
        }
        for (int bit = 0; bit < 8; bit++) {
            byte mask = 0x80 >> bit;
            if (leaves & mask) {
                int16_t ptr = 0;
                if (DFOS_NODE_SIZE == 512) {
                    ptr = trie[ptr_pos];
                    if (ptr_high_bits & mask)
                        ptr += 256;
                } else {
                    ptr = util::getInt(trie + ptr_pos);
                }
                int key_len = buf[ptr];
                int value_len = buf[ptr + key_len + 1];
                int kv_len = (key_len + value_len + 2);
                int16_t kv_last_pos = new_block->getKVLastPos();
                memcpy(new_block->buf + kv_last_pos, buf + ptr, kv_len);
                if ((DFOS_NODE_SIZE - kv_last_pos) < half_kv_len) {
                    if (brk_kv_pos == 0) {
                        brk_idx++;
                        brk_kv_pos = kv_last_pos;
                        new_block->brk_idx = brk_idx;
                        new_block->brk_kv_pos = kv_last_pos;
                    }
                } else
                    brk_idx++;
                if (DFOS_NODE_SIZE == 512) {
                    if (kv_last_pos >= 256)
                        new_block->trie[high_bits_pos] |= mask;
                    new_block->trie[ptr_pos] = kv_last_pos & 0xFF;
                    ptr_pos++;
                } else {
                    util::setInt(new_block->trie + ptr_pos, kv_last_pos);
                    ptr_pos += 2;
                }
                kv_last_pos += kv_len;
                new_block->setKVLastPos(kv_last_pos);
            }
            if (children & mask)
                i = copyToNewBlock(i, depth + 1, new_block);
        }
    } while (0 == (tc & 0x04));
    return i;

}

int dfos_node_handler::splitTrie(int i, int depth, dfos_node_handler *old_block,
        byte *old_trie) {
    byte tc;
    do {
        tc = old_trie[i++];
        byte children = 0;
        byte leaves = 0;
        if (tc & 0x02)
            children = old_trie[i++];
        byte ptr_high_bits = 0;
        byte ptr_pos = 0;
        if (tc & 0x01) {
            leaves = old_trie[i++];
            if (DFOS_NODE_SIZE == 512) {
                ptr_high_bits = old_trie[i++];
            }
            ptr_pos = i;
            if (DFOS_NODE_SIZE != 512)
                i += GenTree::bit_count[leaves];
            i += GenTree::bit_count[leaves];
        }
        for (int bit = 0; bit < 8; bit++) {
            byte mask = 0x80 >> bit;
            if (leaves & mask) {
                int16_t ptr = 0;
                if (DFOS_NODE_SIZE == 512) {
                    ptr = old_trie[ptr_pos];
                    if (ptr_high_bits & mask)
                        ptr += 256;
                    ptr_pos++;
                } else {
                    ptr = util::getInt(old_trie + ptr_pos);
                    ptr_pos += 2;
                }
                if (ptr < brk_kv_pos) {
                    ptr += DFOS_NODE_SIZE;
                    ptr -= old_block->half_kv_len;
                    ptr -= old_block->getKVLastPos();
                    old_block->key_len = old_block->buf[ptr];
                    old_block->key = (const char *) old_block->buf + ptr + 1;
                    old_block->value_len = old_block->key[old_block->key_len];
                    old_block->value = old_block->key + old_block->key_len + 1;
                    old_block->locate(0);
                    old_block->insertCurrent(ptr);
                    old_block->DFOS_FILLED_SIZE++;
                } else {
                    key_len = buf[ptr];
                    key = (const char *) buf + ptr + 1;
                    value_len = key[key_len];
                    value = key + key_len + 1;
                    locate(0);
                    insertCurrent(ptr);
                    DFOS_FILLED_SIZE++;
                }
            }
            if (children & mask)
                i = splitTrie(i, depth + 1, old_block, old_trie);
        }
    } while (0 == (tc & 0x04));
    return i;

}

byte *dfos_node_handler::split() {
    byte *b = (byte *) util::alignedAlloc(DFOS_NODE_SIZE);
    dfos_node_handler old_block(this->buf);
    old_block.isPut = true;
    dfos_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    if (!isLeaf())
        new_block.setLeaf(false);
    int16_t trie_len = old_block.DFOS_TRIE_LEN;
    int16_t kv_last_pos = old_block.getKVLastPos();
    old_block.half_kv_len = DFOS_NODE_SIZE - kv_last_pos + 1;
    old_block.half_kv_len /= 2;

    old_block.brk_idx = 0;
    old_block.brk_kv_pos = 0;
    new_block.setKVLastPos(kv_last_pos);
    old_block.copyToNewBlock(0, 0, &new_block);
    memcpy(old_block.buf, new_block.buf, DFOS_NODE_SIZE);
    old_block.half_kv_len = old_block.brk_kv_pos - kv_last_pos;
    memmove(old_block.buf + DFOS_NODE_SIZE - old_block.half_kv_len,
            old_block.buf + kv_last_pos, old_block.half_kv_len);
    // assumption: trie is shorter than half the data
    memcpy(old_block.buf + kv_last_pos, new_block.trie, trie_len);
    old_block.DFOS_TRIE_LEN = 0;
    old_block.DFOS_FILLED_SIZE = 0;
    new_block.DFOS_TRIE_LEN = 0;
    new_block.DFOS_FILLED_SIZE = 0;
    new_block.setKVLastPos(new_block.brk_kv_pos);
    old_block.setKVLastPos(kv_last_pos);
    new_block.splitTrie(0, 0, &old_block, old_block.buf + kv_last_pos);
    old_block.setKVLastPos(DFOS_NODE_SIZE - old_block.half_kv_len);

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
    DFOS_FILLED_SIZE = 0;
    DFOS_TRIE_LEN = 0;
    setKVLastPos(DFOS_NODE_SIZE);
}

void dfos_node_handler::setBuf(byte *m) {
    buf = m;
    trie = buf + DFOS_IDX_HDR_SIZE;
}

void dfos_node_handler::setKVLastPos(int16_t val) {
    util::setInt(DFOS_LAST_DATA_PTR, val);
}

int16_t dfos_node_handler::getKVLastPos() {
    return util::getInt(DFOS_LAST_DATA_PTR);
}

void dfos_node_handler::addData(int16_t ptr) {

    int16_t kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_len;
    memcpy(buf + kv_last_pos + 1, key, key_len);
    buf[kv_last_pos + key_len + 1] = value_len;
    memcpy(buf + kv_last_pos + key_len + 2, value, value_len);

    insertCurrent(kv_last_pos);
    DFOS_FILLED_SIZE++;

}

bool dfos_node_handler::isLeaf() {
    return DFOS_IS_LEAF;
}

void dfos_node_handler::setLeaf(char isLeaf) {
    DFOS_IS_LEAF = isLeaf;
}

void dfos_node_handler::setFilledSize(int16_t filledSize) {
    DFOS_FILLED_SIZE = filledSize;
}

bool dfos_node_handler::isFull(int16_t kv_len) {
    if ((getKVLastPos() - kv_len - 2)
            < (DFOS_TRIE_LEN + need_count + DFOS_IDX_HDR_SIZE))
        return true;
    return false;
}

int16_t dfos_node_handler::filledSize() {
    return DFOS_FILLED_SIZE;
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

int16_t dfos_node_handler::getFirstPtr(int i) {
    int16_t ptr = 0;
    while (i < DFOS_TRIE_LEN) {
        byte tc = trie[i++];
        byte leaves = 0;
        if (tc & 0x02)
            i++;
        if (tc & 0x01)
            leaves = trie[i++];
        if (leaves) {
            byte mask = 0x80;
            while (mask) {
                if (leaves & mask) {
                    if (DFOS_NODE_SIZE == 512) {
                        byte bitmap = trie[i];
                        if (bitmap & mask)
                            ptr += 256;
                        ptr += trie[i + 1];
                    } else
                        ptr = util::getInt(trie + i);
                    break;
                }
                mask >>= 1;
            }
            if (DFOS_NODE_SIZE == 512)
                i++;
            else
                i += GenTree::bit_count[leaves];
            i += GenTree::bit_count[leaves];
            if (ptr)
                break;
        }
    }
    return ptr;
}

byte *dfos_node_handler::getFirstKey(int16_t *plen) {
    int16_t ptr = getFirstPtr();
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

    if (DFOS_TRIE_LEN == 0) {
        byte kc = key[0];
        byte offset = (kc & 0x07);
        byte mask = 0x80 >> offset;
        insAt(0, (kc & 0xF8) | 0x05, mask, kv_pos);
        insertState = 0;
        return;
    }

    switch (insertState) {
    case DFOS_INSERT_MIDDLE1:
        tc &= 0xFB;
        setAt(origPos, tc);
        insAt(triePos, (msb5 | 0x05), mask, kv_pos);
        triePos += 2;
        break;
    case DFOS_INSERT_MIDDLE2:
        insAt(triePos, (msb5 | 0x01), mask, kv_pos);
        triePos += 2;
        break;
    case DFOS_INSERT_LEAF:
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
                insAt(leafPos, mask, (kv_pos >= 256 ? mask : 0),
                        (byte) (kv_pos & 0xFF));
            else
                insAt(leafPos, mask, kv_pos);
            triePos += 3;
            setAt(origPos, (origTC | 0x01));
        }
        break;
    case DFOS_INSERT_THREAD:
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
                        c2_kv_pos = util::getInt(
                                trie + leafPos + ptr_count * 2);
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
                        c2_kv_pos = util::getInt(
                                trie + leafPos + ptr_count * 2);
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
    int16_t ptr = -1;
    keyPos = 0;
    locate(level, &ptr);
    return ptr;
}

int16_t dfos_node_handler::locate(int16_t level, int16_t *ptr, int i) {
    byte tc;
    byte kc = (level < 0 ? 0xFF : key[keyPos++]);
    do {
        if (level >= 0)
            origPos = i;
        tc = trie[i++];
        byte children = 0;
        byte r_leaves = 0;
        if (tc & 0x02)
            children = trie[i++];
        byte ptr_high_bits = 0;
        byte ptr_pos = 0;
        if (tc & 0x01) {
            r_leaves = trie[i++];
            if (DFOS_NODE_SIZE == 512) {
                ptr_high_bits = trie[i++];
            }
            ptr_pos = i;
            if (DFOS_NODE_SIZE != 512)
                i += GenTree::bit_count[r_leaves];
            i += GenTree::bit_count[r_leaves];
        }
        if ((kc ^ tc) < 0x08) {
            byte offset = kc & 0x07;
            byte r_mask = 0x80 >> offset;
            for (int bit = 0; bit < 8; bit++) {
                byte mask = 0x80 >> bit;
                int16_t cur_ptr = 0;
                if (r_leaves & mask) {
                    if (DFOS_NODE_SIZE == 512) {
                        cur_ptr = trie[ptr_pos];
                        if (ptr_high_bits & r_mask)
                            cur_ptr += 256;
                        ptr_pos++;
                    } else {
                        cur_ptr = util::getInt(trie + ptr_pos);
                        ptr_pos += 2;
                    }
                }
                if (offset == bit) {
                    if (children & r_mask) {
                        if (r_leaves & r_mask) {
                            if (keyPos == key_len) {
                                *ptr = cur_ptr;
                                return i;
                            }
                        } else {
                            if (keyPos == key_len) {
                                if (isPut) {
                                    this->leaves = r_leaves;
                                    this->mask = r_mask;
                                    triePos = i;
                                    insertState = DFOS_INSERT_LEAF;
                                    need_count = 1;
                                    if (DFOS_NODE_SIZE != 512)
                                        need_count++;
                                }
                                *ptr = last_ptr;
                                return i;
                            }
                        }
                        i = locate(level + 1, ptr, i);
                        if (*ptr > 0)
                            return i;
                    } else {
                        if (r_leaves & r_mask) {
                            int16_t key_at_len;
                            const char *key_at = (char *) getKey(cur_ptr,
                                    &key_at_len);
                            int16_t cmp = util::compare(key, key_len, key_at,
                                    key_at_len, keyPos);
                            if (cmp == 0) {
                                *ptr = cur_ptr;
                                return i;
                            }
                            if (isPut) {
                                triePos = i;
                                this->mask = r_mask;
                                this->keyPos = keyPos;
                                this->key_at = key_at;
                                this->key_at_len = key_at_len;
                                insertState = DFOS_INSERT_THREAD;
                                need_count = cmp;
                                if (need_count < 0)
                                    need_count = -need_count;
                                if (keyPos < need_count)
                                    need_count -= keyPos;
                                need_count *= 2;
                                need_count += 8;
                            }
                            *ptr = ~(cmp > 0 ? cur_ptr : last_ptr);
                            return i;
                        } else {
                            if (isPut) {
                                triePos = i;
                                this->tc = tc;
                                this->mask = r_mask;
                                this->leaves = r_leaves;
                                insertState = DFOS_INSERT_LEAF;
                                need_count = 1;
                                if (DFOS_NODE_SIZE != 512)
                                    need_count++;
                            }
                            *ptr = ~last_ptr;
                            return i;
                        }
                    }
                    last_ptr = cur_ptr;
                } else {
                    if (children & mask) {
                        i = locate(-1, ptr, i);
                        *ptr = -1;
                    }
                }
            }
        } else if (kc > tc || level < 0) {
            if (children) {
                for (int bit = 0; bit < 8; bit++) {
                    byte r_mask = 0x80 >> bit;
                    if (children & r_mask)
                        i = locate(-1, ptr, i);
                    *ptr = -1;
                }
            }
            if (level >= 0 && (tc & 0x04)) {
                if (isPut) {
                    triePos = origPos;
                    this->tc = tc;
                    this->mask = (0x80 >> (kc & 0x07));
                    msb5 = (kc & 0xF8);
                    insertState = DFOS_INSERT_MIDDLE1;
                    need_count = 4;
                }
                *ptr = ~last_ptr;
                return i;
            }
        } else {
            if (isPut) {
                msb5 = (kc & 0xF8);
                this->mask = (0x80 >> (kc & 0x07));
                insertState = DFOS_INSERT_MIDDLE2;
                need_count = 4;
                this->tc = tc;
                triePos = origPos;
            }
            *ptr = ~last_ptr;
            return i;
        }
    } while (0 == (tc & 0x04));
    return i;

}

//int16_t dfos_node_handler::locate(int16_t level) {
//    register int keyPos = 0;
//    int i = 0;
//    register byte kc = key[keyPos++];
//    int16_t last_ptr = getFirstPtr();
//    do {
//        byte tc = trie[i];
//        if ((kc ^ tc) < 0x08) {
//            byte r_leaves;
//            byte children;
//            register byte offset = (kc & 0x07);
//            register byte r_mask = (0x80 >> offset);
//            if (isPut)
//                origPos = i;
//            i++;
//            children = 0;
//            r_leaves = 0;
//            if (tc & 0x02)
//                children = trie[i++];
//            if (tc & 0x01) {
//                r_leaves = trie[i++];
//            }
//            byte left_leaf = r_leaves & left_mask[offset];
//            int leaf_count = GenTree::bit_count[left_leaf];
//            if (leaf_count) {
//                if (DFOS_NODE_SIZE == 512) {
//                    register byte bitmap = trie[i];
//                    if (bitmap & GenTree::last_bit_mask[left_leaf])
//                        last_ptr = 256 + trie[i + leaf_count];
//                    else
//                        last_ptr = trie[i + leaf_count];
//                } else
//                    last_ptr = util::getInt(trie + i + leaf_count * 2);
//            }
//            if (children & r_mask) {
//                if (r_leaves & r_mask) {
//                    if (keyPos == key_len) {
//                        int leaf_count = GenTree::bit_count[r_leaves
//                                & left_incl_mask[offset]];
//                        if (leaf_count) {
//                            leaf_count--;
//                            if (DFOS_NODE_SIZE == 512) {
//                                register byte bitmap = trie[i++];
//                                if (bitmap & r_mask)
//                                    last_ptr = 256 + trie[i + leaf_count];
//                                else
//                                    last_ptr = trie[i + leaf_count];
//                            } else
//                                last_ptr = util::getInt(
//                                        trie + i + leaf_count * 2);
//                        }
//                        return last_ptr;
//                    }
//                } else {
//                    if (keyPos == key_len) {
//                        if (isPut) {
//                            this->leaves = r_leaves;
//                            this->mask = r_mask;
//                            triePos = i;
//                            insertState = DFOS_INSERT_LEAF;
//                            need_count = 1;
//                            if (DFOS_NODE_SIZE != 512)
//                                need_count++;
//                        }
//                        return ~last_ptr;
//                    }
//                }
//                int leaf_count = GenTree::bit_count[r_leaves];
//                if (DFOS_NODE_SIZE == 512) {
//                    if (leaf_count)
//                        leaf_count++;
//                } else
//                    leaf_count <<= 1;
//                i += leaf_count;
//                register int to_skip = GenTree::bit_count[children
//                        & left_mask[offset]];
//                while (to_skip) {
//                    register byte child_tc = trie[i++];
//                    if (child_tc & 0x02)
//                        to_skip += GenTree::bit_count[trie[i++]];
//                    if (child_tc & 0x01) {
//                        byte leaf = trie[i++];
//                        byte leaf_count = GenTree::bit_count[leaf];
//                        if (leaf_count) {
//                            if (DFOS_NODE_SIZE == 512) {
//                                last_ptr = trie[i + leaf_count];
//                                if (trie[i] & GenTree::last_bit_mask[leaf])
//                                    last_ptr += 256;
//                                leaf_count++;
//                            } else {
//                                leaf_count--;
//                                leaf_count <<= 1;
//                                last_ptr = trie[i + leaf_count];
//                                leaf_count += 2;
//                            }
//                            i += leaf_count;
//                        }
//                    }
//                    if (child_tc & 0x04)
//                        to_skip--;
//                }
//            } else {
//                if (r_leaves & r_mask) {
//                    int16_t ptr;
//                    register int leaf_count = GenTree::bit_count[r_leaves
//                            & left_incl_mask[offset]];
//                    if (DFOS_NODE_SIZE == 512) {
//                        register byte bitmap = trie[i];
//                        ptr = trie[i + leaf_count];
//                        if (bitmap & r_mask)
//                            ptr += 256;
//                        i++;
//                    } else {
//                        leaf_count--;
//                        ptr = util::getInt(trie + i + leaf_count * 2);
//                        i += GenTree::bit_count[r_leaves];
//                    }
//                    i += GenTree::bit_count[r_leaves];
//                    int16_t key_at_len;
//                    const char *key_at = (char *) getKey(ptr, &key_at_len);
//                    int16_t cmp = util::compare(key, key_len, key_at,
//                            key_at_len, keyPos);
//                    if (cmp == 0)
//                        return ptr;
//                    if (isPut) {
//                        register int to_skip = GenTree::bit_count[children
//                                & left_mask[offset]];
//                        while (to_skip) {
//                            register byte child_tc = trie[i++];
//                            if (child_tc & 0x02)
//                                to_skip += GenTree::bit_count[trie[i++]];
//                            if (child_tc & 0x01) {
//                                byte leaf = trie[i++];
//                                byte leaf_count = GenTree::bit_count[leaf];
//                                if (leaf_count) {
//                                    if (DFOS_NODE_SIZE == 512) {
//                                        last_ptr = trie[i + leaf_count];
//                                        if (trie[i]
//                                                & GenTree::last_bit_mask[leaf])
//                                            last_ptr += 256;
//                                        leaf_count++;
//                                    } else {
//                                        leaf_count--;
//                                        leaf_count <<= 1;
//                                        last_ptr = trie[i + leaf_count];
//                                        leaf_count += 2;
//                                    }
//                                    i += leaf_count;
//                                }
//                            }
//                            if (child_tc & 0x04)
//                                to_skip--;
//                        }
//                        triePos = i;
//                        this->mask = r_mask;
//                        this->keyPos = keyPos;
//                        this->key_at = key_at;
//                        this->key_at_len = key_at_len;
//                        insertState = DFOS_INSERT_THREAD;
//                        need_count = cmp;
//                        if (need_count < 0)
//                            need_count = -need_count;
//                        if (keyPos < need_count)
//                            need_count -= keyPos;
//                        need_count *= 2;
//                        need_count += 8;
//                    }
//                    return ~(cmp > 0 ? ptr : last_ptr);
//                } else {
//                    if (isPut) {
//                        triePos = GenTree::bit_count[r_leaves];
//                        if (DFOS_NODE_SIZE == 512)
//                            triePos++;
//                        else
//                            triePos <<= 1;
//                        triePos += i;
//                        this->tc = tc;
//                        this->mask = r_mask;
//                        this->leaves = r_leaves;
//                        insertState = DFOS_INSERT_LEAF;
//                        need_count = 1;
//                        if (DFOS_NODE_SIZE != 512)
//                            need_count++;
//                    }
//                    return ~last_ptr;
//                }
//            }
//            kc = key[keyPos++];
//        } else if (kc > tc) {
//            register int to_skip = 0;
//            if (isPut)
//                origPos = i;
//            i++;
//            if (tc & 0x02)
//                to_skip += GenTree::bit_count[trie[i++]];
//            if (tc & 0x01) {
//                byte leaf = trie[i++];
//                byte leaf_count = GenTree::bit_count[leaf];
//                if (leaf_count) {
//                    if (DFOS_NODE_SIZE == 512) {
//                        last_ptr = trie[i + leaf_count];
//                        if (trie[i] & GenTree::last_bit_mask[leaf])
//                            last_ptr += 256;
//                        leaf_count++;
//                    } else {
//                        leaf_count--;
//                        leaf_count <<= 1;
//                        last_ptr = trie[i + leaf_count];
//                        leaf_count += 2;
//                    }
//                    i += leaf_count;
//                }
//            }
//            while (to_skip) {
//                register byte child_tc = trie[i++];
//                if (child_tc & 0x02)
//                    to_skip += GenTree::bit_count[trie[i++]];
//                if (child_tc & 0x01) {
//                    byte leaf = trie[i++];
//                    byte leaf_count = GenTree::bit_count[leaf];
//                    if (leaf_count) {
//                        if (DFOS_NODE_SIZE == 512) {
//                            last_ptr = trie[i + leaf_count];
//                            if (trie[i] & GenTree::last_bit_mask[leaf])
//                                last_ptr += 256;
//                            leaf_count++;
//                        } else {
//                            leaf_count--;
//                            leaf_count <<= 1;
//                            last_ptr = trie[i + leaf_count];
//                            leaf_count += 2;
//                        }
//                        i += leaf_count;
//                    }
//                }
//                if (child_tc & 0x04)
//                    to_skip--;
//            }
//            if (tc & 0x04) {
//                if (isPut) {
//                    triePos = i;
//                    this->tc = tc;
//                    this->mask = (0x80 >> (kc & 0x07));
//                    msb5 = (kc & 0xF8);
//                    insertState = DFOS_INSERT_MIDDLE1;
//                    need_count = 4;
//                }
//                return ~last_ptr;
//            }
//        } else {
//            if (isPut) {
//                msb5 = (kc & 0xF8);
//                this->mask = (0x80 >> (kc & 0x07));
//                insertState = DFOS_INSERT_MIDDLE2;
//                need_count = 4;
//                this->tc = tc;
//                triePos = i;
//            }
//            return ~last_ptr;
//        }
//    } while (1);
//    return ~last_ptr;
//}

void dfos_node_handler::delAt(byte pos) {
    DFOS_TRIE_LEN--;
    byte *ptr = trie + pos;
    memmove(ptr, ptr + 1, DFOS_TRIE_LEN - pos);
}

void dfos_node_handler::delAt(byte pos, int16_t count) {
    DFOS_TRIE_LEN -= count;
    byte *ptr = trie + pos;
    memmove(ptr, ptr + count, DFOS_TRIE_LEN - pos);
}

void dfos_node_handler::insAt(byte pos, byte b) {
    byte *ptr = trie + pos;
    memmove(ptr + 1, ptr, DFOS_TRIE_LEN - pos);
    trie[pos] = b;
    DFOS_TRIE_LEN++;
}

void dfos_node_handler::insAt(byte pos, int16_t i) {
    byte *ptr = trie + pos;
    memmove(ptr + 2, ptr, DFOS_TRIE_LEN - pos);
    util::setInt(trie + pos, i);
    DFOS_TRIE_LEN += 2;
}

void dfos_node_handler::insAt(byte pos, byte b1, byte b2) {
    byte *ptr = trie + pos;
    memmove(ptr + 2, ptr, DFOS_TRIE_LEN - pos);
    trie[pos++] = b1;
    trie[pos] = b2;
    DFOS_TRIE_LEN += 2;
}

void dfos_node_handler::insAt(byte pos, byte b1, byte b2, byte b3) {
    byte *ptr = trie + pos;
    memmove(ptr + 3, ptr, DFOS_TRIE_LEN - pos);
    trie[pos++] = b1;
    trie[pos++] = b2;
    trie[pos] = b3;
    DFOS_TRIE_LEN += 3;
}

void dfos_node_handler::insAt(byte pos, byte b1, int16_t i1) {
    byte *ptr = trie + pos;
    memmove(ptr + 3, ptr, DFOS_TRIE_LEN - pos);
    trie[pos++] = b1;
    util::setInt(trie + pos, i1);
    DFOS_TRIE_LEN += 3;
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
    memmove(ptr + size, ptr, DFOS_TRIE_LEN - pos);
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
    DFOS_TRIE_LEN += size;
    return size;
}

byte dfos_node_handler::insAt(byte pos, byte tc, byte leaf, int16_t c1_kv_pos) {
    byte *ptr = trie + pos;
    int size = 4;
    memmove(ptr + size, ptr, DFOS_TRIE_LEN - pos);
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
    DFOS_TRIE_LEN += size;
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
byte dfos_node_handler::left_incl_mask[8] = { 0x80, 0xC0, 0xE0, 0xF0, 0xF8,
        0xFC, 0xFE, 0xFF };
byte dfos_node_handler::ryte_mask[8] = { 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03,
        0x01, 0x00 };
byte dfos_node_handler::ryte_incl_mask[8] = { 0xFF, 0x7F, 0x3F, 0x1F, 0x0F,
        0x07, 0x03, 0x01 };
byte dfos_node_handler::pos_mask[48] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04,
        0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40,
        0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04,
        0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40,
        0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
