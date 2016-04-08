#include "dfox.h"
#include "GenTree.h"

void dfox::put(const char *key, int key_len, const char *value, int value_len) {
    // isPut = true;
    int lastSearchPos[numLevels];
    dfox_block *block_paths[numLevels];
    if (root->filledSize() == 0) {
        root->addData(0, key, key_len, value, value_len);
        total_size++;
    } else {
        int pos = 0;
        dfox_block *foundNode = recursiveSearch(key, key_len, root,
                lastSearchPos, block_paths, &pos);
        recursiveUpdate(foundNode, pos, key, key_len, value, value_len,
                lastSearchPos, block_paths, numLevels - 1);
    }
    // isPut = false;
}

dfox_block *dfox::recursiveSearch(const char *key, int key_len,
        dfox_block *node, int lastSearchPos[], dfox_block *block_paths[],
        int *pIdx) {
    int level = 0;
    while (!node->isLeaf()) {
        lastSearchPos[level] = node->locateInTrie(key, key_len);
        block_paths[level] = node;
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
            if (lastSearchPos[level] >= node->filledSize()) {
                lastSearchPos[level] -= node->filledSize();
                do {
                    node = node->getChild(lastSearchPos[level]);
                    level++;
                    block_paths[level] = node;
                    lastSearchPos[level] = 0;
                } while (!node->isLeaf());
                *pIdx = lastSearchPos[level];
                return node;
            }
        }
        node = node->getChild(lastSearchPos[level]);
        level++;
    }
    block_paths[level] = node;
    *pIdx = node->locateInTrie(key, key_len);
    lastSearchPos[level] = *pIdx; // required?
    return node;
}

char *dfox::get(const char *key, int key_len, int *pValueLen) {
    int lastSearchPos[numLevels];
    dfox_block *block_paths[numLevels];
    int pos = -1;
    dfox_block *foundNode = recursiveSearch(key, key_len, root, lastSearchPos,
            block_paths, &pos);
    if (pos < 0)
        return null;
    return (char *) foundNode->getData(pos, pValueLen);
}

void dfox_block::setKVLastPos(int val) {
    util::setInt(&buf[3], val);
}

int dfox_block::getKVLastPos() {
    return util::getInt(buf + 3);
}

void dfox_block::addData(int idx, const char *key, int key_len,
        const char *value, int value_len) {

    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += idx;
    kvIdx += idx;
    int filledSz = filledSize();
    if (idx < filledSz)
        memmove(kvIdx + 2, kvIdx, (filledSz - idx) * 2);

    filledSz++;
    util::setInt(buf + 1, filledSz);
    int kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_len;
    memcpy(buf + kv_last_pos + 1, key, key_len);
    buf[kv_last_pos + key_len + 1] = value_len;
    memcpy(buf + kv_last_pos + key_len + 2, value, value_len);
    util::setInt(kvIdx, kv_last_pos);
}

dfox_block *dfox_block::split(int *pbrk_idx) {
    int filled_size = filledSize();
    linex_block *new_block = new linex_block();
    if (!isLeaf())
        new_block->setLeaf(false);
    int kv_last_pos = getKVLastPos();
    int halfKVLen = BLK_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;
    byte *kv_idx = buf + BLK_HDR_SIZE;
    byte *new_kv_idx = new_block->buf + BLK_HDR_SIZE;
    int new_idx;
    int brk_idx = -1;
    int brk_kv_pos = 0;
    int new_pos = 0;
    int tot_len = 0;
    for (new_idx = 0; new_idx < filled_size; new_idx++) {
        int src_idx = util::getInt(kv_idx + new_pos);
        int key_len = buf[src_idx];
        key_len++;
        int value_len = buf[src_idx + key_len];
        value_len++;
        int kv_len = key_len;
        kv_len += value_len;
        tot_len += kv_len;
        memcpy(new_block->buf + kv_last_pos, buf + src_idx, kv_len);
        util::setInt(new_kv_idx + new_pos, kv_last_pos);
        kv_last_pos += kv_len;
        new_pos += 2;
        if (tot_len > halfKVLen && brk_idx == -1) {
            brk_idx = new_idx;
            brk_kv_pos = kv_last_pos;
        }
    }
    kv_last_pos = getKVLastPos();
    int old_blk_new_len = brk_kv_pos - kv_last_pos;
    memcpy(buf + BLK_SIZE - old_blk_new_len, new_block->buf + kv_last_pos,
            old_blk_new_len); // (3)
    new_pos = 0;
    for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
        int src_idx = util::getInt(new_kv_idx + new_pos);
        src_idx += (BLK_SIZE - brk_kv_pos);
        util::setInt(kv_idx + new_pos, src_idx);
        if (new_idx == 0)
            setKVLastPos(src_idx); // (6)
        new_pos += 2;
    } // (4)
    int new_size = filled_size - brk_idx - 1;
    memcpy(new_kv_idx, new_kv_idx + new_pos, new_size * 2); // (5)
    new_block->setKVLastPos(util::getInt(new_kv_idx)); // (7)
    filled_size = brk_idx + 1;
    setFilledSize(filled_size); // (8)
    new_block->setFilledSize(new_size); // (9)
    *pbrk_idx = brk_idx;
    return new_block;
}

void dfox::recursiveUpdate(dfox_block *block, int pos, const char *key,
        int key_len, const char *value, int value_len, int lastSearchPos[],
        dfox_block *blocks_path[], int level) {
    int idx = pos; // lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (block->isFull(key_len + value_len)) {
            //printf("Full\n");
            if (maxKeyCount < block->filledSize())
                maxKeyCount = block->filledSize();
            int brk_idx;
            linex_block *new_block = block->split(&brk_idx);
            if (root == block) {
                root = new linex_block();
                int first_len;
                char *first_key = (char *) block->getKey(0, &first_len);
                char addr[5];
                util::ptrToFourBytes((unsigned long) block, addr);
                root->addData(0, "", 0, addr, sizeof(char *));
                first_key = (char *) new_block->getKey(0, &first_len);
                util::ptrToFourBytes((unsigned long) new_block, addr);
                root->addData(1, first_key, first_len, addr, sizeof(char *));
                root->setLeaf(false);
                numLevels++;
            } else {
                int prev_level = level - 1;
                linex_block *parent = blocks_path[prev_level];
                int first_key_len;
                char *new_block_first_key = (char *) new_block->getKey(0,
                        &first_key_len);
                char addr[5];
                util::ptrToFourBytes((unsigned long) new_block, addr);
                recursiveUpdate(parent, ~(lastSearchPos[prev_level] + 1),
                        new_block_first_key, first_key_len, addr,
                        sizeof(char *), lastSearchPos, blocks_path, prev_level);
            }
            if (idx > brk_idx) {
                block = new_block;
                idx -= brk_idx;
            }
        }
        block->addData(idx, key, key_len, value, value_len);
    } else {
        //if (node->isLeaf) {
        //    int vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

long dfox::size() {
    return total_size;
}

dfox::dfox() {
    root = new dfox_block();
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 0;
}

dfox_block::dfox_block() {
    memset(buf, '\0', sizeof(buf));
    setLeaf(1);
    setFilledSize(0);
    setKVLastPos(sizeof(buf));
    triePos = buf + BLK_HDR_SIZE;
    trieLen = 0;
    left_mask[0] = 0x00;
    left_mask[1] = 0x80;
    left_mask[2] = 0xC0;
    left_mask[3] = 0xE0;
    left_mask[4] = 0xF0;
    left_mask[5] = 0xF8;
    left_mask[6] = 0xFC;
    left_mask[7] = 0xFE;
    ryte_mask[0] = 0x7F;
    ryte_mask[1] = 0x3F;
    ryte_mask[2] = 0x1F;
    ryte_mask[3] = 0x0F;
    ryte_mask[4] = 0x07;
    ryte_mask[5] = 0x03;
    ryte_mask[6] = 0x01;
    ryte_mask[7] = 0x00;
}

bool dfox_block::isLeaf() {
    return (buf[0] == 1);
}

void dfox_block::setLeaf(char isLeaf) {
    buf[0] = isLeaf;
}

void dfox_block::setFilledSize(char filledSize) {
    util::setInt(buf + 1, filledSize);
}

bool dfox_block::isFull(int kv_len) {
}

int dfox_block::filledSize() {
    return util::getInt(buf + 1);
}

dfox_block *dfox_block::getChild(int pos) {
    unsigned long addr_num = util::fourBytesToPtr(idx);
    dfox_block *ret = (dfox_block *) addr_num;
    return ret;
}

byte *dfox_block::getKey(int pos, int *plen) {
}

byte *dfox_block::getData(int pos, int *plen) {
}

void dfox_block::insertCurrent(dfox_var *v) {
    byte leaf;
    byte child;
    byte origTC;
    byte leafPos;
    byte childPos;
    int pos, min;
    switch (v->insertState) {
    case INSERT_MIDDLE1:
        v->tc &= 0x7F;
        setAt(v->origPos, v->tc);
        insAt(v->triePos, v->mask);
        // trie.insert(triePos, (char) 0);
        insAt(v->triePos, (v->msb5 | 0xC0));
        break;
    case INSERT_LEAF1:
        origTC = getAt(v->origPos);
        leafPos = v->origPos + 1;
        if (0 == (origTC & 0x40))
            leafPos++;
        if (0 == (origTC & 0x20)) {
            v->leaves |= v->mask;
            setAt(leafPos, v->leaves);
        } else {
            insAt(leafPos, v->mask);
            setAt(v->origPos, (origTC & 0xDF));
        }
        break;
    case INSERT_THREAD:
        childPos = v->origPos + 1;
        child = v->mask;
        origTC = getAt(v->origPos);
        if (0 == (origTC & 0x40)) {
            child |= getAt(childPos);
            setAt(childPos, child);
        } else {
            insAt(childPos, child);
            v->triePos++;
            origTC &= 0xBF; // ~0x40
            setAt(v->origPos, origTC);
        }
        pos = v->lastLevel + 1;
        min = util::min(v->key_len, v->key_at_len);
        if (pos < min) {
            leafPos = childPos;
            if (0 == (origTC & 0x20)) {
                leafPos++;
                int leaf = getAt(leafPos);
                leaf &= ~v->mask;
                setAt(leafPos, leaf);
            }
            char c1, c2;
            do {
                c1 = v->key[pos];
                c2 = v->key_at[pos];
                if (c1 > c2) {
                    byte swap = c1;
                    c1 = c2;
                    c2 = swap;
                }
                byte msb5c1 = c1 >> 3;
                byte offc1 = c1 & 0x07;
                byte msb5c2 = c2 >> 3;
                byte offc2 = c2 & 0x07;
                byte c1leaf = 0;
                byte c2leaf = 0;
                byte c1child = 0;
                if (c1 == c2) {
                    c1child = (0x80 >> offc1);
                    if (pos + 1 == min) {
                        c1leaf = (0x80 >> offc1);
                        msb5c1 |= 0x80;
                    } else
                        msb5c1 |= 0xA0;
                } else {
                    if (msb5c1 == msb5c2) {
                        msb5c1 |= 0xC0;
                        c1leaf = ((0x80 >> offc1) | (0x80 >> offc2));
                    } else {
                        c1leaf = (0x80 >> offc1);
                        c2leaf = (0x80 >> offc2);
                        msb5c2 |= 0xC0;
                        msb5c1 |= 0x40;
                        insAt(v->triePos, c2leaf);
                        insAt(v->triePos, msb5c2);
                    }
                }
                if (c1leaf != 0)
                    insAt(v->triePos, c1leaf);
                if (c1child != 0)
                    insAt(v->triePos, c1child);
                insAt(v->triePos, msb5c1);
                if (c1leaf != 0 && c1child != 0)
                    v->triePos += 3;
                else
                    v->triePos += 2;
                if (c1 != c2)
                    break;
                pos++;
            } while (pos < min);
            if (c1 == c2) {
                c2 = (pos == v->key_at_len ? v->key[pos] : v->key_at[pos]);
                int msb5c2 = c2 >> 3;
                int offc2 = c2 & 0x07;
                msb5c2 |= 0xC0;
                insAt(v->triePos, (0x80 >> offc2));
                insAt(v->triePos, msb5c2);
            }
        }
        break;
    case INSERT_LEAF2:
        origTC = getAt(v->origPos);
        leafPos = v->origPos + 1;
        leaf = v->mask;
        if (0 == (origTC & 0x40))
            leafPos++;
        if (0 == (origTC & 0x20)) {
            leaf |= getAt(leafPos);
            setAt(leafPos, leaf);
        } else {
            insAt(leafPos, leaf);
            origTC &= 0xDF; // ~0x20
            setAt(v->origPos, origTC);
        }
        break;
    case INSERT_MIDDLE2:
        insAt(v->triePos, v->mask);
        insAt(v->triePos, (v->msb5 | 0x40));
        break;
    }
    v->insertState = 0;
}

byte dfox_block::recurseSkip(dfox_var *v) {
    if (v->triePos >= trieLen)
        return 0x80;
    byte tc = getAt(v->triePos++);
    byte children = 0;
    byte leaves = 0;
    if (0 == (tc & 0x40))
        children = getAt(v->triePos++);
    if (0 == (tc & 0x20))
        leaves = getAt(v->triePos++);
    v->lastSearchPos += GenTree::bit_count[leaves];
    for (int i = 0; i < GenTree::bit_count[children]; i++) {
        int ctc;
        do {
            ctc = recurseSkip(v);
        } while ((ctc & 0x80) != 0x80);
    }
    return tc;
}

bool dfox_block::recurseTrie(int level, dfox_var *v) {
    if (v->csPos >= v->key_len) {
        return false;
    }
    int kc = v->key[v->csPos++];
    v->msb5 = kc >> 3;
    int offset = kc & 0x07;
    v->mask = 0x80 >> offset;
    v->tc = getAt(v->triePos);
    int msb5tc = v->tc & 0x1F;
    v->origPos = v->triePos;
    while (msb5tc < v->msb5) {
        v->origPos = v->triePos;
        recurseSkip(v);
        if ((v->tc & 0x80) == 0x80 || v->triePos == trieLen) {
            v->insertState = INSERT_MIDDLE1;
            return true;
        }
        v->tc = getAt(v->triePos);
        msb5tc = v->tc & 0x1F;
    }
    v->origPos = v->triePos;
    if (msb5tc == v->msb5) {
        v->tc = getAt(v->triePos++);
        int children = 0;
        v->leaves = 0;
        if (0 == (v->tc & 0x40)) {
            children = getAt(v->triePos++);
        }
        if (0 == (v->tc & 0x20)) {
            v->leaves = getAt(v->triePos++);
            v->lastSearchPos +=
                    GenTree::bit_count[v->leaves & left_mask[offset]];
        }
        int lCount = GenTree::bit_count[children & left_mask[offset]];
        while (lCount-- > 0) {
            int ctc;
            do {
                ctc = recurseSkip(v);
            } while ((ctc & 0x80) != 0x80);
        }
        if (v->mask == (children & v->mask)) {
            if (v->mask == (v->leaves & v->mask)) {
                v->lastSearchPos++;
                if (v->csPos == v->key_len) {
                    return true;
                }
            } else {
                if (v->csPos == v->key_len) {
                    v->insertState = INSERT_LEAF1;
                    return true;
                }
            }
            if (recurseTrie(level + 1, v)) {
                return true;
            }
        } else {
            if (v->mask == (v->leaves & v->mask)) {
                v->lastSearchPos++;
                v->key_at = (char *) getKey(v->lastSearchPos, &v->key_at_len);
                int cmp = util::compare(v->key, v->key_len, v->key_at, v->key_at_len);
                if (cmp == 0) {
                    return true;
                }
                if (cmp < 0)
                    v->lastSearchPos--;
                v->insertState = INSERT_THREAD;
                v->lastLevel = level;
                return true;
            } else {
                v->insertState = INSERT_LEAF2;
                return true;
            }
        }
    } else {
        v->insertState = INSERT_MIDDLE2;
        return true;
    }
    return false;
}

int dfox_block::locateInTrie(const char *key, int key_len) {
    dfox_var *v = new dfox_var();
    v->key = key;
    v->key_len = key_len;
    v->csPos = 0;
    v->insertState = 0;
    byte kc = key[v->csPos];
    byte msb5 = kc >> 3;
    byte base = (msb5 << 3);
    byte offset = kc - base;
    if (trieLen == 0) {
        append(msb5 | 0xC0);
        append(0x80 >> offset);
        return -1;
    }
    v->lastSearchPos = -1;
    v->triePos = 0;
    while (v->triePos < trieLen) {
        if (recurseTrie(0, v)) {
            if (v->insertState != 0)
                v->lastSearchPos = ~(v->lastSearchPos + 1);
            return v->lastSearchPos;
        }
    }
    v->lastSearchPos = ~(v->lastSearchPos + 1);
    return v->lastSearchPos;
}

void dfox_block::insAt(byte pos, byte b) {
    memmove(triePos + pos + 1, triePos + pos, 1);
    triePos[pos] = b;
    trieLen++;
}

void dfox_block::setAt(byte pos, byte b) {
    triePos[pos] = b;
}

void dfox_block::append(byte b) {
    triePos[trieLen++] = b;
}

byte dfox_block::getAt(byte pos) {
    return triePos[pos];
}

dfox_var::dfox_var() {
    tc = 0;
    origPos = 0;
    lastLevel = 0;
    insertState = 0;
    csPos = 0;
    triePos = 0;
}

// insert pointer
// split
// isFull
// getData, getKey
