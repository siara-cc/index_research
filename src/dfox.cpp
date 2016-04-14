#include <math.h>
#include "dfox.h"
#include "GenTree.h"

dfox_block *dfox::recursiveSearch(const char *key, int key_len,
        dfox_block *node, int lastSearchPos[], dfox_block *block_paths[],
        int *pIdx, dfox_var *v) {
    int level = 0;
    while (!node->isLeaf()) {
        lastSearchPos[level] = node->locateInTrie(key, key_len, v);
        block_paths[level] = node;
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
            lastSearchPos[level]--;
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
        v->init();
        level++;
    }
    block_paths[level] = node;
    *pIdx = node->locateInTrie(key, key_len, v);
    lastSearchPos[level] = *pIdx; // required?
    return node;
}

char *dfox::get(const char *key, int key_len, int *pValueLen) {
    int lastSearchPos[numLevels];
    dfox_block *block_paths[numLevels];
    int pos = -1;
    dfox_var *v = new dfox_var();
    v->key = key;
    v->key_len = key_len;
    dfox_block *foundNode = recursiveSearch(key, key_len, root, lastSearchPos,
            block_paths, &pos, v);
    if (pos < 0)
        return null;
    return (char *) foundNode->getData(pos, pValueLen);
}

void dfox::put(const char *key, int key_len, const char *value, int value_len) {
    // isPut = true;
    int lastSearchPos[numLevels];
    dfox_block *block_paths[numLevels];
    dfox_var *v = new dfox_var();
    v->key = key;
    v->key_len = key_len;
    if (root->filledSize() == 0) {
        root->addData(0, key, key_len, value, value_len, v);
        total_size++;
    } else {
        int pos = 0;
        dfox_block *foundNode = recursiveSearch(key, key_len, root,
                lastSearchPos, block_paths, &pos, v);
        recursiveUpdate(foundNode, pos, key, key_len, value, value_len,
                lastSearchPos, block_paths, numLevels - 1, v);
    }
    // isPut = false;
}

void dfox::recursiveUpdate(dfox_block *block, int pos, const char *key,
        int key_len, const char *value, int value_len, int lastSearchPos[],
        dfox_block *blocks_path[], int level, dfox_var *v) {
    int idx = pos; // lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (block->isFull(key_len + value_len, v)) {
            //printf("Full\n");
            if (maxKeyCount > block->filledSize())
                maxKeyCount = block->filledSize();
            int brk_idx;
            dfox_block *new_block = block->split(&brk_idx);
            dfox_var *rv = new dfox_var();
            if (root == block) {
                root = new dfox_block();
                int first_len;
                char *first_key; // = (char *) block->getKey(0, &first_len);
                char addr[5];
                util::ptrToFourBytes((unsigned long) block, addr);
                rv->key = "";
                rv->key_len = 1;
                root->addData(0, rv->key, 1, addr, sizeof(char *), rv);
                first_key = (char *) new_block->getKey(0, &first_len);
                rv->key = first_key;
                rv->key_len = first_len;
                util::ptrToFourBytes((unsigned long) new_block, addr);
                rv->init();
                root->locateInTrie(first_key, first_len, rv);
                root->addData(1, first_key, first_len, addr, sizeof(char *),
                        rv);
                root->setLeaf(false);
                numLevels++;
            } else {
                int prev_level = level - 1;
                dfox_block *parent = blocks_path[prev_level];
                rv->key = (char *) new_block->getKey(0, &rv->key_len);
                char addr[5];
                util::ptrToFourBytes((unsigned long) new_block, addr);
                parent->locateInTrie(rv->key, rv->key_len, rv);
                recursiveUpdate(parent, ~(lastSearchPos[prev_level] + 1),
                        rv->key, rv->key_len, addr,
                        sizeof(char *), lastSearchPos, blocks_path, prev_level,
                        rv);
            }
            if (idx > brk_idx) {
                block = new_block;
                idx -= brk_idx;
            }
            v->init();
            block->locateInTrie(key, key_len, v);
            idx = ~v->lastSearchPos;
        }
        block->addData(idx, key, key_len, value, value_len, v);
    } else {
        //if (node->isLeaf) {
        //    int vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

dfox_block *dfox_block::split(int *pbrk_idx) {
    int orig_filled_size = filledSize();
    dfox_block *new_block = new dfox_block();
    if (!isLeaf())
        new_block->setLeaf(false);
    int kv_last_pos = getKVLastPos();
    int halfKVLen = BLK_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int new_idx;
    int brk_idx = -1;
    int brk_kv_pos = 0;
    int tot_len = 0;
    dfox_var *v = new dfox_var();
    // (1) move all data to new_block in order
    for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
        int src_idx = getPtr(new_idx);
        int key_len = buf[src_idx];
        key_len++;
        int value_len = buf[src_idx + key_len];
        value_len++;
        int kv_len = key_len;
        kv_len += value_len;
        tot_len += kv_len;
        memcpy(new_block->buf + kv_last_pos, buf + src_idx, kv_len);
        new_block->insPtr(new_idx, kv_last_pos);
        kv_last_pos += kv_len;
        if (tot_len > halfKVLen) {
            if (brk_idx == -1) {
                brk_idx = new_idx;
                brk_kv_pos = kv_last_pos;
            }
        }
    }
    // (2) move top half of new_block to bottom half of old_block
    kv_last_pos = getKVLastPos();
    int old_blk_new_len = brk_kv_pos - kv_last_pos;
    memcpy(buf + BLK_SIZE - old_blk_new_len, new_block->buf + kv_last_pos,
            old_blk_new_len);
    // (3) Insert pointers to the moved data in old block
    FILLED_SIZE = 0;
    TRIE_LEN = 0;
    for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
        int src_idx = new_block->getPtr(new_idx);
        src_idx += (BLK_SIZE - brk_kv_pos);
        insPtr(new_idx, src_idx);
        if (new_idx == 0) // (5) Set Last data position for old block
            setKVLastPos(src_idx);
        char *key = (char *) (buf + src_idx + 1);
        v->init();
        v->key = key;
        v->key_len = buf[src_idx];
        locateInTrie(key, buf[src_idx], v);
        insertCurrent(v);
    }
    int new_size = orig_filled_size - brk_idx - 1;
    /*
     // (4) move new block pointers to front
     byte *new_kv_idx = new_block->buf + IDX_HDR_SIZE - orig_filled_size;
     memmove(new_kv_idx + new_idx, new_kv_idx, new_size); */
    // (4) and set corresponding bits
    byte high_bits[4];
    memcpy(high_bits, new_block->DATA_PTR_HIGH_BITS, 4);
    memset(new_block->DATA_PTR_HIGH_BITS, 0, 4);
    for (int i = 0; i < new_size; i++) {
        int from_bit = i + new_idx;
        if (high_bits[from_bit / 8] & (0x80 >> (from_bit % 8)))
            new_block->DATA_PTR_HIGH_BITS[i / 8] |= (0x80 >> (i % 8));
    }
    if (high_bits[3] & 0x01)
        new_block->setLeaf(1);
    new_block->setKVLastPos(brk_kv_pos); // (5) New Block Last position
    //filled_size = brk_idx + 1;
    //setFilledSize(filled_size); // (7) Filled Size for Old block
    new_block->TRIE_LEN = 0;
    new_block->setFilledSize(new_size); // (8) Filled Size for New Block
    for (int i = 0; i < new_size; i++) {
        v->init();
        v->key = (char *) new_block->getKey(i, &v->key_len);
        new_block->locateInTrie(v->key, v->key_len, v);
        new_block->insertCurrent(v);
    }
    *pbrk_idx = (brk_idx + 1);
    return new_block;
}

long dfox::size() {
    return root->TRIE_LEN;
}

dfox::dfox() {
    root = new dfox_block();
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 9999;
}

dfox_block::dfox_block() {
    memset(buf, '\0', sizeof(buf));
    setLeaf(1);
    FILLED_SIZE = 0;
    TRIE_LEN = 0;
    setKVLastPos(sizeof(buf));
    trie = buf + IDX_HDR_SIZE;
}

void dfox_block::setKVLastPos(int val) {
    util::setInt(LAST_DATA_PTR, val);
}

int dfox_block::getKVLastPos() {
    return util::getInt(LAST_DATA_PTR);
}

void dfox_block::addData(int pos, const char *key, int key_len,
        const char *value, int value_len, dfox_var *v) {

    insertCurrent(v);
    int kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_len;
    memcpy(buf + kv_last_pos + 1, key, key_len);
    buf[kv_last_pos + key_len + 1] = value_len;
    memcpy(buf + kv_last_pos + key_len + 2, value, value_len);

    insPtr(pos, kv_last_pos);

}

void dfox_block::insPtr(int pos, int kv_pos) {
    int filledSz = filledSize();
    filledSz++;
    byte *kvIdx = buf + IDX_BLK_SIZE;
    kvIdx -= filledSz;
    if (pos > 0) {
        memmove(kvIdx, kvIdx + 1, pos);
    }
    kvIdx[pos] = (byte) kv_pos & 0xFF;
    byte offset = pos % 8;
    byte carry = 0;
    if (kv_pos > 256)
        carry = 0x80;
    byte is_leaf = isLeaf();
    for (int i = pos / 8; i < 4; i++) {
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
    FILLED_SIZE = filledSz;
}

bool dfox_block::isLeaf() {
    return (IS_LEAF_BYTE & 0x01);
}

void dfox_block::setLeaf(char isLeaf) {
    if (isLeaf)
        IS_LEAF_BYTE |= 0x01;
    else
        IS_LEAF_BYTE &= 0xFE;
}

void dfox_block::setFilledSize(int filledSize) {
    FILLED_SIZE = filledSize;
}

bool dfox_block::isFull(int kv_len, dfox_var *v) {
    if (TRIE_LEN + v->need_count + FILLED_SIZE >= TRIE_PTR_AREA_SIZE)
        return true;
    if (FILLED_SIZE > 26)
        return true;
    if ((getKVLastPos()-kv_len-2) < (IDX_BLK_SIZE + 10))
        return true;
    return false;
}

int dfox_block::filledSize() {
    return FILLED_SIZE;
}

dfox_block *dfox_block::getChild(int pos) {
    byte *idx = getData(pos, &pos);
    unsigned long addr_num = util::fourBytesToPtr(idx);
    dfox_block *ret = (dfox_block *) addr_num;
    return ret;
}

int dfox_block::getPtr(int pos) {
    byte *kvIdx = buf + IDX_BLK_SIZE;
    kvIdx -= FILLED_SIZE;
    kvIdx += pos;
    int idx = *kvIdx;
    if (DATA_PTR_HIGH_BITS[pos / 8] & (0x80 >> (pos % 8)))
        idx += 256;
    return idx;
}

byte *dfox_block::getKey(int pos, int *plen) {
    byte *kvIdx = buf + getPtr(pos);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *dfox_block::getData(int pos, int *plen) {
    byte *kvIdx = buf + getPtr(pos);
    *plen = kvIdx[0];
    kvIdx++;
    kvIdx += *plen;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

void dfox_block::insertCurrent(dfox_var *v) {
    byte leaf;
    byte child;
    byte origTC;
    byte leafPos;
    byte childPos;
    int pos, min;

    if (TRIE_LEN == 0) {
        byte kc = v->key[0];
        byte msb5 = kc >> 3;
        byte base = (msb5 << 3);
        byte offset = kc - base;
        append(msb5 | 0xC0);
        append(0x80 >> offset);
        v->insertState = 0;
        return;
    }

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
        pos = v->lastLevel;
        min = util::min(v->key_len, v->key_at_len);
        char c1, c2;
        c1 = v->key[pos];
        c2 = v->key_at[pos];
        pos++;
        if (pos < min) {
            leafPos = childPos;
            if (0 == (origTC & 0x20)) {
                leafPos++;
                int leaf = getAt(leafPos);
                leaf &= ~v->mask;
                setAt(leafPos, leaf);
            }
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
        }
        if (c1 == c2) {
            c2 = (pos == v->key_at_len ? v->key[pos] : v->key_at[pos]);
            int msb5c2 = c2 >> 3;
            int offc2 = c2 & 0x07;
            msb5c2 |= 0xC0;
            insAt(v->triePos, (0x80 >> offc2));
            insAt(v->triePos, msb5c2);
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
    if (v->triePos >= TRIE_LEN)
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
    if (v->csPos >= v->key_len)
        return false;
    int kc = v->key[v->csPos++];
    v->msb5 = kc >> 3;
    int offset = kc & 0x07;
    v->mask = 0x80 >> offset;
    v->tc = getAt(v->triePos);
    int msb5tc = v->tc & 0x1F;
    v->origPos = v->triePos;
    while (msb5tc < v->msb5) {
        v->origPos = v->triePos;
        v->tc = recurseSkip(v);
        if ((v->tc & 0x80) == 0x80 || v->triePos == TRIE_LEN) {
            v->insertState = INSERT_MIDDLE1;
            v->need_count = 2;
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
                    v->need_count = 2;
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
                int cmp = util::compare(v->key, v->key_len, v->key_at,
                        v->key_at_len);
                if (cmp == 0)
                    return true;
                if (cmp < 0)
                    v->lastSearchPos--;
                v->insertState = INSERT_THREAD;
                if (cmp < 0)
                    cmp = -cmp;
                cmp -= level;
                cmp--;
                v->need_count = cmp * 2;
                v->need_count += 4;
                v->lastLevel = level;
                return true;
            } else {
                v->insertState = INSERT_LEAF2;
                v->need_count = 2;
                return true;
            }
        }
    } else {
        v->insertState = INSERT_MIDDLE2;
        v->need_count = 2;
        return true;
    }
    return false;
}

int dfox_block::locateInTrie(const char *key, int key_len, dfox_var *v) {
    if (TRIE_LEN == 0)
        return -1;
    while (v->triePos < TRIE_LEN) {
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
    memmove(trie + pos + 1, trie + pos, TRIE_LEN - pos);
    trie[pos] = b;
    TRIE_LEN++;
}

void dfox_block::setAt(byte pos, byte b) {
    trie[pos] = b;
}

void dfox_block::append(byte b) {
    trie[TRIE_LEN++] = b;
}

byte dfox_block::getAt(byte pos) {
    return trie[pos];
}

void dfox_var::init() {
    tc = 0;
    mask = 0;
    msb5 = 0;
    csPos = 0;
    leaves = 0;
    triePos = 0;
    origPos = 0;
    lastLevel = 0;
    need_count = 0;
    insertState = 0;
    lastSearchPos = -1;
}

byte dfox_block::left_mask[8] =
        { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
byte dfox_block::ryte_mask[8] =
        { 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00 };
byte dfox_block::ryte_incl_mask[8] = { 0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03,
        0x01 };
