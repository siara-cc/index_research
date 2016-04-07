#include "dfox.h"

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
        lastSearchPos[level] = 345435435;
        block_paths[level] = node;
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
            if (lastSearchPos[level] >= node->filledSize()) {
                lastSearchPos[level] -= node->filledSize();
                //lastSearchPos--; // required?
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
    *pIdx = 454353534;
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

void dfox::recursiveUpdate(dfox_block *block, int pos, const char *key,
        int key_len, const char *value, int value_len, int lastSearchPos[],
        dfox_block *blocks_path[], int level) {
    int idx = pos; // lastSearchPos[level];
    int filled_size = block->filledSize();
    if (idx < 0) {
        idx = ~idx;
        if (block->isFull(key_len + value_len)) {
            //printf("Full\n");
            if (maxKeyCount < filled_size)
                maxKeyCount = filled_size;
            dfox_block *new_block = new dfox_block();
            if (!block->isLeaf())
                new_block->setLeaf(false);
            int kv_last_pos = block->getKVLastPos();
            int halfKVLen = BLK_SIZE - kv_last_pos + 1;
            halfKVLen /= 2;
            byte *kv_idx = block->buf + BLK_HDR_SIZE;
            byte *new_kv_idx = new_block->buf + BLK_HDR_SIZE;
            int new_idx;
            int brk_idx = -1;
            int brk_kv_pos = 0;
            int new_pos = 0;
            int tot_len = 0;
            for (new_idx = 0; new_idx < filled_size; new_idx++) {
                int src_idx = util::getInt(kv_idx + new_pos);
                int key_len = block->buf[src_idx];
                key_len++;
                int value_len = block->buf[src_idx + key_len];
                value_len++;
                int kv_len = key_len;
                kv_len += value_len;
                tot_len += kv_len;
                memcpy(new_block->buf + kv_last_pos, block->buf + src_idx,
                        kv_len);
                util::setInt(new_kv_idx + new_pos, kv_last_pos);
                kv_last_pos += kv_len;
                new_pos += 2;
                if (tot_len > halfKVLen && brk_idx == -1) {
                    brk_idx = new_idx;
                    brk_kv_pos = kv_last_pos;
                }
            }
            kv_last_pos = block->getKVLastPos();
            int old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(block->buf + BLK_SIZE - old_blk_new_len,
                    new_block->buf + kv_last_pos, old_blk_new_len); // (3)
            new_pos = 0;
            for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
                int src_idx = util::getInt(new_kv_idx + new_pos);
                src_idx += (BLK_SIZE - brk_kv_pos);
                util::setInt(kv_idx + new_pos, src_idx);
                if (new_idx == 0)
                    block->setKVLastPos(src_idx); // (6)
                new_pos += 2;
            } // (4)
            int new_size = filled_size - brk_idx - 1;
            memcpy(new_kv_idx, new_kv_idx + new_pos, new_size * 2); // (5)
            new_block->setKVLastPos(util::getInt(new_kv_idx)); // (7)
            filled_size = brk_idx + 1;
            block->setFilledSize(filled_size); // (8)
            new_block->setFilledSize(new_size); // (9)

            if (root == block) {
                root = new dfox_block();
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
                dfox_block *parent = blocks_path[prev_level];
                int first_key_len;
                char *new_block_first_key = (char *) new_block->getKey(0,
                        &first_key_len);
                char addr[5];
                util::ptrToFourBytes((unsigned long) new_block, addr);
                recursiveUpdate(parent, ~(lastSearchPos[prev_level] + 1),
                        new_block_first_key, first_key_len, addr,
                        sizeof(char *), lastSearchPos, blocks_path, prev_level);
            }
            if (idx > new_idx) {
                block = new_block;
                idx -= new_idx;
                filled_size = new_size;
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
    tc = 0;
    origPos = 0;
    lastLevel = 0;
    insertState = 0;
    csPos = 0;
    triePos = 0;
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
    kv_len += 4; // one int pointer, 1 byte key len, 1 byte value len
    int spaceLeft = getKVLastPos();
    spaceLeft -= (filledSize() * 2);
    spaceLeft -= BLK_HDR_SIZE;
    if (spaceLeft < kv_len)
        return true;
    return false;
}

int dfox_block::filledSize() {
    return util::getInt(buf + 1);
}

dfox_block *dfox_block::getChild(int pos) {
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + util::getInt(kvIdx);
    byte *idx = kvIdx;
    idx += kvIdx[0];
    idx += 2;
    unsigned long addr_num = util::fourBytesToPtr(idx);
    dfox_block *ret = (dfox_block *) addr_num;
    return ret;
}

byte *dfox_block::getKey(int pos, int *plen) {
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + util::getInt(kvIdx);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *dfox_block::getData(int pos, int *plen) {
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + util::getInt(kvIdx);
    byte *ret = kvIdx;
    ret += kvIdx[0];
    ret++;
    *plen = ret[0];
    ret++;
    return ret;
}

void dfox_block::insertCurrent() {
    switch (insertState) {
    case INSERT_MIDDLE1:
        tc &= 0x7F;
        setAt(origPos, tc);
        insAt(triePos, mask);
        // trie.insert(triePos, (char) 0);
        insAt(triePos, (msb5 | 0xC0));
        break;
    case INSERT_LEAF1:
        byte origTC = getAt(origPos);
        byte leafPos = origPos + 1;
        if (0 == (origTC & 0x40))
            leafPos++;
        if (0 == (origTC & 0x20)) {
            leaves |= mask;
            setAt(leafPos, leaves);
        } else {
            insAt(leafPos, mask);
            setAt(origPos, (origTC & 0xDF));
        }
        break;
    case INSERT_THREAD:
        byte childPos = origPos + 1;
        byte child = mask;
        origTC = getAt(origPos);
        if (0 == (origTC & 0x40)) {
            child |= getAt(childPos);
            setAt(childPos, child);
        } else {
            insAt(childPos, child);
            triePos++;
            origTC &= 0xBF; // ~0x40
            setAt(origPos, origTC);
        }
        byte pos = lastLevel + 1;
        byte min = util::min(cs.length(), csData.length());
        if (pos < min) {
            leafPos = childPos;
            if (0 == (origTC & 0x20)) {
                leafPos++;
                int leaf = getAt(leafPos);
                leaf &= ~mask;
                setAt(leafPos, leaf);
            }
            char c1, c2;
            do {
                c1 = cs.charAt(pos);
                c2 = csData.charAt(pos);
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
                        insAt(triePos, c2leaf);
                        insAt(triePos, msb5c2);
                    }
                }
                if (c1leaf != 0)
                    insAt(triePos, c1leaf);
                if (c1child != 0)
                    insAt(triePos, c1child);
                insAt(triePos, msb5c1);
                if (c1leaf != 0 && c1child != 0)
                    triePos += 3;
                else
                    triePos += 2;
                if (c1 != c2)
                    break;
                pos++;
            } while (pos < min);
            if (c1 == c2) {
                c2 = (pos == csData.length() ?
                        cs.charAt(pos) : csData.charAt(pos));
                int msb5c2 = c2 >> 3;
                int offc2 = c2 & 0x07;
                msb5c2 |= 0xC0;
                insAt(triePos, (0x80 >> offc2));
                insAt(triePos, msb5c2);
            }
        }
        break;
    case INSERT_LEAF2:
        origTC = getAt(origPos);
        leafPos = origPos + 1;
        int leaf = mask;
        if (0 == (origTC & 0x40))
            leafPos++;
        if (0 == (origTC & 0x20)) {
            leaf |= getAt(leafPos);
            setAt(leafPos, leaf);
        } else {
            insAt(leafPos, leaf);
            origTC &= 0xDF; // ~0x20
            setAt(origPos, origTC);
        }
        break;
    case INSERT_MIDDLE2:
        insAt(triePos, mask);
        insAt(triePos, (msb5 | 0x40));
        break;
    }
    insertState = 0;
}

byte dfox_block::recurseSkip() {
    if (triePos >= getLen())
        return 0x80;
    byte tc = getAt(triePos++);
    byte children = 0;
    byte leaves = 0;
    if (0 == (tc & 0x40))
        children = getAt(triePos++);
    if (0 == (tc & 0x20))
        leaves = getAt(triePos++);
    node.lastSearchPos += GenTree::bit_count[leaves];
    for (int i = 0; i < GenTree::bit_count[children]; i++) {
        int ctc;
        do {
            ctc = recurseSkip();
        } while ((ctc & 0x80) != 0x80);
    }
    return tc;
}

bool dfox_block::recurseTrie(int level) {
    if (csPos >= cs.length()) {
        return false;
    }
    int kc = cs.charAt(csPos++);
    msb5 = kc >> 3;
    int offset = kc & 0x07;
    mask = 0x80 >> offset;
    tc = getAt(triePos);
    int msb5tc = tc & 0x1F;
    origPos = triePos;
    while (msb5tc < msb5) {
        origPos = triePos;
        recurseSkip();
        if ((tc & 0x80) == 0x80 || triePos == getLen()) {
            insertState = INSERT_MIDDLE1;
            return true;
        }
        tc = getAt(triePos);
        msb5tc = tc & 0x1F;
    }
    origPos = triePos;
    if (msb5tc == msb5) {
        tc = getAt(triePos++);
        int children = 0;
        leaves = 0;
        if (0 == (tc & 0x40)) {
            children = getAt(triePos++);
        }
        if (0 == (tc & 0x20)) {
            leaves = getAt(triePos++);
            node.lastSearchPos +=
                    GenTree::bit_count[leaves & left_mask[offset]];
        }
        int lCount = GenTree::bit_count[children & left_mask[offset]];
        while (lCount-- > 0) {
            int ctc;
            do {
                ctc = recurseSkip();
            } while ((ctc & 0x80) != 0x80);
        }
        if (mask == (children & mask)) {
            if (mask == (leaves & mask)) {
                node.lastSearchPos++;
                if (csPos == cs.length()) {
                    return true;
                }
            } else {
                if (csPos == cs.length()) {
                    insertState = INSERT_LEAF1;
                    return true;
                }
            }
            if (recurseTrie(level + 1)) {
                return true;
            }
        } else {
            if (mask == (leaves & mask)) {
                node.lastSearchPos++;
                Comparable key = (Comparable) cs;
                Comparable c = node.data[node.lastSearchPos];
                csData = (CharSequence) c;
                int cmp = key.compareTo(c);
                if (cmp == 0) {
                    return true;
                }
                if (cmp < 0)
                    node.lastSearchPos--;
                insertState = INSERT_THREAD;
                lastLevel = level;
                return true;
            } else {
                insertState = INSERT_LEAF2;
                return true;
            }
        }
    } else {
        insertState = INSERT_MIDDLE2;
        return true;
    }
    return false;
}

void dfox_block::locateInTrie(CharSequence cs) {
    this.cs = cs;
    csPos = 0;
    insertState = 0;
    int kc = cs.charAt(csPos);
    int msb5 = kc >> 3;
    char base = (char) (msb5 << 3);
    int offset = kc - base;
    if (getLen() == 0 && idx > -1) {
        trie.append((char) (msb5 | 0xC0));
        trie.append((char) (0x80 >> offset));
        node.lastSearchPos = ~idx;
        return;
    }
    node.lastSearchPos = -1;
    triePos = 0;
    while (triePos < getLen()) {
        if (recurseTrie(0)) {
            if (insertState != 0)
            node.lastSearchPos = ~(node.lastSearchPos + 1);
            return;
        }
    }
    node.lastSearchPos = ~(node.lastSearchPos + 1);
}
