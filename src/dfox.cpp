#include <iostream>
#include <math.h>
#include <malloc.h>
#include "dfox.h"
#include "GenTree.h"

char *dfox::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    int16_t pos = -1;
    byte *foundNode = recursiveSearchForGet(key, key_len, &pos);
    if (pos < 0)
        return null;
    dfox_node node(foundNode);
    return (char *) node.getData(pos, pValueLen);
}

byte *dfox::recursiveSearchForGet(const char *key, int16_t key_len,
        int16_t *pIdx) {
    int16_t level = 0;
    int16_t pos = -1;
    byte *node_data = root->buf;
    dfox_node node(node_data);
    while (!node.isLeaf()) {
        pos = node.locate(key, key_len, level, null);
        if (pos < 0) {
            pos = ~pos;
            if (pos)
                pos--;
        } else {
            do {
                node_data = node.getChild(pos);
                node.setBuf(node_data);
                level++;
                pos = 0;
            } while (!node.isLeaf());
            *pIdx = pos;
            return node_data;
        }
        node_data = node.getChild(pos);
        node.setBuf(node_data);
        level++;
    }
    pos = node.locate(key, key_len, level, null);
    *pIdx = pos;
    return node_data;
}

byte *dfox::recursiveSearch(const char *key, int16_t key_len, byte *node_data,
        int16_t lastSearchPos[], byte *node_paths[], int16_t *pIdx,
        dfox_var *v) {
    int16_t level = 0;
    dfox_node node(node_data);
    while (!node.isLeaf()) {
        lastSearchPos[level] = node.locate(key, key_len, level, v);
        node_paths[level] = node_data;
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
            if (lastSearchPos[level])
                lastSearchPos[level]--;
        } else {
            do {
                node_data = node.getChild(lastSearchPos[level]);
                node.setBuf(node_data);
                level++;
                node_paths[level] = node.buf;
                lastSearchPos[level] = 0;
            } while (!node.isLeaf());
            *pIdx = lastSearchPos[level];
            return node_data;
        }
        node_data = node.getChild(lastSearchPos[level]);
        node.setBuf(node_data);
        v->init();
        level++;
    }
    node_paths[level] = node_data;
    lastSearchPos[level] = node.locate(key, key_len, level, v);
    *pIdx = lastSearchPos[level];
    return node_data;
}

void dfox::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    int16_t lastSearchPos[numLevels];
    byte *block_paths[numLevels];
    dfox_var v;
    v.isPut = true;
    v.key = key;
    v.key_len = key_len;
    if (root->filledSize() == 0) {
        root->addData(0, value, value_len, &v);
        total_size++;
    } else {
        int16_t pos = -1;
        byte *foundNode = recursiveSearch(key, key_len, root->buf,
                lastSearchPos, block_paths, &pos, &v);
        recursiveUpdate(key, key_len, foundNode, pos, value, value_len,
                lastSearchPos, block_paths, numLevels - 1, &v);
    }
}

void dfox::recursiveUpdate(const char *key, int16_t key_len, byte *foundNode,
        int16_t pos, const char *value, int16_t value_len,
        int16_t lastSearchPos[], byte *node_paths[], int16_t level,
        dfox_var *v) {
    dfox_node node(foundNode);
    int16_t idx = pos; // lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (node.isFull(v->key_len + value_len, v)) {
            //std::cout << "Full\n" << std::endl;
            //if (maxKeyCount < block->filledSize())
            //    maxKeyCount = block->filledSize();
            //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
            maxKeyCount += node.filledSize();
            int16_t brk_idx;
            byte *b = node.split(&brk_idx);
            dfox_node new_block(b);
            blockCount++;
            dfox_var rv;
            if (root->buf == node.buf) {
                byte *buf = util::alignedAlloc(DFOX_NODE_SIZE);
                root->setBuf(buf);
                root->init();
                blockCount++;
                int16_t first_len;
                char *first_key;
                char addr[5];
                util::ptrToFourBytes((unsigned long) foundNode, addr);
                rv.key = "";
                rv.key_len = 1;
                root->addData(0, addr, sizeof(char *), &rv);
                first_key = (char *) new_block.getKey(0, &first_len);
                rv.key = first_key;
                rv.key_len = first_len;
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                rv.init();
                root->locate(first_key, first_len, 0, &rv);
                root->addData(1, addr, sizeof(char *), &rv);
                root->setLeaf(false);
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent = node_paths[prev_level];
                rv.key = (char *) new_block.getKey(0, &rv.key_len);
                char addr[5];
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                dfox_node parent_node(parent);
                parent_node.locate(rv.key, rv.key_len, prev_level, &rv);
                recursiveUpdate(rv.key, rv.key_len, parent,
                        ~(lastSearchPos[prev_level] + 1), addr, sizeof(char *),
                        lastSearchPos, node_paths, prev_level, &rv);
            }
            brk_idx++;
            if (idx > brk_idx) {
                node.setBuf(new_block.buf);
                idx -= brk_idx;
            }
            v->init();
            idx = ~node.locate(key, key_len, level, v);
        }
        node.addData(idx, value, value_len, v);
    } else {
        //if (node->isLeaf) {
        //    int16_t vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

byte *dfox_node::split(int16_t *pbrk_idx) {
    int16_t orig_filled_size = filledSize();
    byte *b = util::alignedAlloc(DFOX_NODE_SIZE);
    dfox_node new_block(b);
    new_block.init();
    if (!isLeaf())
        new_block.setLeaf(false);
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = DFOX_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int16_t new_idx;
    int16_t brk_idx = -1;
    int16_t brk_kv_pos = 0;
    int16_t tot_len = 0;
    // (1) move all data to new_block in order
    for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
        int16_t src_idx = getPtr(new_idx);
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
        if (tot_len > halfKVLen) {
            if (brk_idx == -1) {
                brk_idx = new_idx;
                brk_kv_pos = kv_last_pos;
            }
        }
    }
    // (2) move top half of new_block to bottom half of old_block
    kv_last_pos = getKVLastPos();
    int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
    memcpy(buf + DFOX_NODE_SIZE - old_blk_new_len, new_block.buf + kv_last_pos,
            old_blk_new_len);
    // (3) Insert pointers to the moved data in old block
    dfox_var v;
    FILLED_SIZE = 0;
    TRIE_LEN = 0;
    for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
        int16_t src_idx = new_block.getPtr(new_idx);
        src_idx += (DFOX_NODE_SIZE - brk_kv_pos);
        insPtr(new_idx, src_idx);
        if (new_idx == 0) // (5) Set Last data position for old block
            setKVLastPos(src_idx);
        char *key = (char *) (buf + src_idx + 1);
        v.init();
        v.key = key;
        v.key_len = buf[src_idx];
        if (new_idx)
            locate(v.key, v.key_len, 0, &v);
        insertCurrent(&v);
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
    new_block.TRIE_LEN = 0;
    new_block.setFilledSize(new_size); // (8) Filled Size for New Block
    for (int16_t i = 0; i < new_size; i++) {
        v.init();
        v.key = (char *) new_block.getKey(i, &v.key_len);
        if (i)
            new_block.locate(v.key, v.key_len, 0, &v);
        new_block.insertCurrent(&v);
    }
    *pbrk_idx = brk_idx;
    return new_block.buf;
}

dfox::dfox() {
    byte *b = util::alignedAlloc(DFOX_NODE_SIZE);
    root = new dfox_node(b);
    root->init();
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 0; //9999;
    blockCount = 1;
    maxThread = 9999;
}

dfox::~dfox() {
    delete root;
}

dfox_node::dfox_node(byte *m) {
    setBuf(m);
}

void dfox_node::init() {
    memset(buf, '\0', DFOX_NODE_SIZE);
    setLeaf(1);
    FILLED_SIZE = 0;
    TRIE_LEN = 0;
    threads = 0;
    setKVLastPos(DFOX_NODE_SIZE);
}

void dfox_node::setBuf(byte *m) {
    buf = m;
    trie = buf + IDX_HDR_SIZE;
}

void dfox_node::setKVLastPos(int16_t val) {
    util::setInt(LAST_DATA_PTR, val);
}

int16_t dfox_node::getKVLastPos() {
    return util::getInt(LAST_DATA_PTR);
}

void dfox_node::addData(int16_t pos, const char *value, int16_t value_len,
        dfox_var *v) {

    insertCurrent(v);
    int16_t kv_last_pos = getKVLastPos() - (v->key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = v->key_len;
    memcpy(buf + kv_last_pos + 1, v->key, v->key_len);
    buf[kv_last_pos + v->key_len + 1] = value_len;
    memcpy(buf + kv_last_pos + v->key_len + 2, value, value_len);

    insPtr(pos, kv_last_pos);

}

void dfox_node::insPtr(int16_t pos, int16_t kv_pos) {
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
    FILLED_SIZE = filledSz;
}

bool dfox_node::isLeaf() {
    return (IS_LEAF_BYTE & 0x01);
}

void dfox_node::setLeaf(char isLeaf) {
    if (isLeaf)
        IS_LEAF_BYTE |= 0x01;
    else
        IS_LEAF_BYTE &= 0xFE;
}

void dfox_node::setFilledSize(int16_t filledSize) {
    FILLED_SIZE = filledSize;
}

bool dfox_node::isFull(int16_t kv_len, dfox_var *v) {
    if (TRIE_LEN + v->need_count + FILLED_SIZE >= TRIE_PTR_AREA_SIZE)
        return true;
    if (FILLED_SIZE > MAX_PTRS)
        return true;
    if ((getKVLastPos() - kv_len - 2) < IDX_BLK_SIZE)
        return true;
    return false;
}

int16_t dfox_node::filledSize() {
    return FILLED_SIZE;
}

byte *dfox_node::getChild(int16_t pos) {
    byte *idx = getData(pos, &pos);
    unsigned long addr_num = util::fourBytesToPtr(idx);
    byte *ret = (byte *) addr_num;
    return ret;
}

int16_t dfox_node::getPtr(int16_t pos) {
    byte *kvIdx = buf + IDX_BLK_SIZE;
    kvIdx -= FILLED_SIZE;
    kvIdx += pos;
    int16_t idx = *kvIdx;
    if (DATA_PTR_HIGH_BITS[pos >> 3] & pos_mask[pos])
        idx |= 256;
    return idx;
}

byte *dfox_node::getKey(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + getPtr(pos);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *dfox_node::getData(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + getPtr(pos);
    *plen = kvIdx[0];
    kvIdx++;
    kvIdx += *plen;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

void dfox_node::insertCurrent(dfox_var *v) {
    byte child;
    byte origTC;
    byte leafPos;
    byte childPos;
    int16_t pos, min;

    if (TRIE_LEN == 0) {
        byte kc = v->key[0];
        byte offset = (kc & 0x07);
        append((kc & 0xF8) | 0x03);
        append(0x80 >> offset);
        v->insertState = 0;
        return;
    }

    switch (v->insertState) {
    case INSERT_MIDDLE1:
        v->tc &= 0xFE;
        setAt(v->origPos, v->tc);
        insAt(v->triePos, (v->msb5 | 0x03), v->mask);
        v->triePos += 2;
        break;
    case INSERT_MIDDLE2:
        insAt(v->triePos, (v->msb5 | 0x02), v->mask);
        v->triePos += 2;
        break;
    case INSERT_LEAF:
        origTC = getAt(v->origPos);
        leafPos = v->origPos + 1;
        if (origTC & 0x04)
            leafPos++;
        if (origTC & 0x02) {
            v->leaves |= v->mask;
            setAt(leafPos, v->leaves);
        } else {
            insAt(leafPos, v->mask);
            v->triePos++;
            setAt(v->origPos, (origTC | 0x02));
        }
        break;
    case INSERT_THREAD:
        threads++;
        childPos = v->origPos + 1;
        child = v->mask;
        origTC = getAt(v->origPos);
        if (origTC & 0x04) {
            child |= getAt(childPos);
            setAt(childPos, child);
        } else {
            insAt(childPos, child);
            v->triePos++;
            origTC |= 0x04;
            setAt(v->origPos, origTC);
        }
        pos = v->keyPos - 1;
        min = util::min(v->key_len, v->key_at_len);
        char c1, c2;
        c1 = v->key[pos];
        c2 = v->key_at[pos];
        pos++;
        if (pos < min) {
            leafPos = childPos;
            if (origTC & 0x02) {
                leafPos++;
                int16_t leaf = getAt(leafPos);
                leaf &= ~v->mask;
                if (leaf)
                    setAt(leafPos, leaf);
                else {
                    delAt(leafPos);
                    v->triePos--;
                    origTC &= 0xFD;
                    setAt(v->origPos, origTC);
                }
            }
            do {
                c1 = v->key[pos];
                c2 = v->key_at[pos];
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
                        insAt(v->triePos, msb5c2, c2leaf);
                    }
                }
                if (c1leaf != 0 && c1child != 0) {
                    insAt(v->triePos, msb5c1, c1child, c1leaf);
                    v->triePos += 3;
                } else if (c1leaf != 0) {
                    insAt(v->triePos, msb5c1, c1leaf);
                    v->triePos += 2;
                } else {
                    insAt(v->triePos, msb5c1, c1child);
                    v->triePos += 2;
                }
                if (c1 != c2)
                    break;
                pos++;
            } while (pos < min);
        }
        if (c1 == c2) {
            c2 = (pos == v->key_at_len ? v->key[pos] : v->key_at[pos]);
            int16_t msb5c2 = (c2 & 0xF8);
            int16_t offc2 = (c2 & 0x07);
            msb5c2 |= 0x03;
            insAt(v->triePos, msb5c2, (0x80 >> offc2));
            v->triePos += 2;
        }
        break;
    }
    v->insertState = 0;
}

int16_t dfox_node::locate(const char *key, int16_t key_len, int16_t level,
        dfox_var *v) {
    register byte keyPos = 0;
    register byte kc = key[keyPos++];
    register byte pos = 0;
    register byte i = 0;
    //register byte len = TRIE_LEN;
    while (1) {
        register byte tc = trie[i];
        if ((kc ^ tc) < 0x08) {
            register byte leaves;
            register byte children;
            register byte to_skip = 0;
            register byte offset = (kc & 0x07);
            register byte mask = (0x80 >> offset);
            if (v)
                v->origPos = i;
            i++;
            switch (tc & 0x06) {
            case 0x02:
                leaves = trie[i++];
                children = 0;
                break;
            case 0x04:
                children = trie[i++];
                leaves = 0;
                break;
            case 0x06:
                children = trie[i++];
                leaves = trie[i++];
                break;
            }
            if (children > mask)
                to_skip = GenTree::bit_count[children & left_mask[offset]];
            if (leaves > mask)
                pos += GenTree::bit_count[leaves & left_mask[offset]];
            while (to_skip) {
                register byte child_tc = trie[i++];
                if (child_tc & 0x04)
                    to_skip += GenTree::bit_count[trie[i++]];
                if (child_tc & 0x02)
                    pos += GenTree::bit_count[trie[i++]];
                if (child_tc & 0x01)
                    to_skip--;
            }
            if (children & mask) {
                if (leaves & mask) {
                    if (keyPos == key_len)
                        return pos;
                    else
                        pos++;
                } else {
                    if (keyPos == key_len) {
                        if (v) {
                            v->leaves = leaves;
                            v->mask = mask;
                            v->triePos = i;
                            v->insertState = INSERT_LEAF;
                            v->need_count = 2;
                        }
                        return ~pos;
                    }
                }
            } else {
                if (leaves & mask) {
                    //pos++;
                    int16_t key_at_len;
                    const char *key_at = (char *) getKey(pos, &key_at_len);
                    int16_t cmp = util::compare(key, key_len, key_at,
                            key_at_len, keyPos);
                    if (cmp == 0)
                        return pos;
                    if (cmp > 0)
                        pos++;
                    if (v) {
                        v->triePos = i;
                        v->mask = mask;
                        v->keyPos = keyPos;
                        v->key_at = key_at;
                        v->key_at_len = key_at_len;
                        v->insertState = INSERT_THREAD;
                        if (cmp < 0)
                            cmp = -cmp;
                        if (v->keyPos < cmp)
                            cmp -= keyPos;
                        v->need_count = cmp * 2;
                        v->need_count += 4;
                    }
                    return ~pos;
                } else {
                    if (v) {
                        v->triePos = i;
                        v->mask = mask;
                        v->leaves = leaves;
                        v->insertState = INSERT_LEAF;
                        v->need_count = 2;
                    }
                    return ~pos;
                }
            }
            kc = key[keyPos++];
        } else if (kc > tc) {
            register byte to_skip = 0;
            if (v)
                v->origPos = i;
            i++;
            if (tc & 0x04)
                to_skip += GenTree::bit_count[trie[i++]];
            if (tc & 0x02)
                pos += GenTree::bit_count[trie[i++]];
            while (to_skip) {
                byte child_tc = trie[i++];
                if (child_tc & 0x04)
                    to_skip += GenTree::bit_count[trie[i++]];
                if (child_tc & 0x02)
                    pos += GenTree::bit_count[trie[i++]];
                if (child_tc & 0x01)
                    to_skip--;
            }
            if (tc & 0x01) {
                if (v) {
                    v->triePos = i;
                    v->tc = tc;
                    v->mask = (0x80 >> (kc & 0x07));
                    v->msb5 = (kc & 0xF8);
                    v->insertState = INSERT_MIDDLE1;
                    v->need_count = 2;
                }
                return ~pos;
            }
        } else {
            if (v) {
                v->msb5 = (kc & 0xF8);
                v->mask = (0x80 >> (kc & 0x07));
                v->insertState = INSERT_MIDDLE2;
                v->need_count = 2;
                v->tc = tc;
                v->triePos = i;
            }
            return ~pos;
        }
    }
    return ~pos;
}

void dfox_node::delAt(byte pos) {
    TRIE_LEN--;
    byte *ptr = trie + pos;
    memmove(ptr, ptr + 1, TRIE_LEN - pos);
}

void dfox_node::delAt(byte pos, int16_t count) {
    TRIE_LEN -= count;
    byte *ptr = trie + pos;
    memmove(ptr, ptr + count, TRIE_LEN - pos);
}

void dfox_node::insAt(byte pos, byte b) {
    byte *ptr = trie + pos;
    memmove(ptr + 1, ptr, TRIE_LEN - pos);
    trie[pos] = b;
    TRIE_LEN++;
}

void dfox_node::insAt(byte pos, byte b1, byte b2) {
    byte *ptr = trie + pos;
    memmove(ptr + 2, ptr, TRIE_LEN - pos);
    trie[pos++] = b1;
    trie[pos] = b2;
    TRIE_LEN += 2;
}

void dfox_node::insAt(byte pos, byte b1, byte b2, byte b3) {
    byte *ptr = trie + pos;
    memmove(ptr + 3, ptr, TRIE_LEN - pos);
    trie[pos++] = b1;
    trie[pos++] = b2;
    trie[pos] = b3;
    TRIE_LEN += 3;
}

void dfox_node::setAt(byte pos, byte b) {
    trie[pos] = b;
}

void dfox_node::append(byte b) {
    trie[TRIE_LEN++] = b;
}

byte dfox_node::getAt(byte pos) {
    return trie[pos];
}

void dfox_var::init() {
    keyPos = 0;
    triePos = 0;
    need_count = 0;
    insertState = 0;
    lastSearchPos = -1;
}

byte dfox_node::left_mask[8] =
        { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
byte dfox_node::ryte_mask[8] =
        { 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00 };
byte dfox_node::ryte_incl_mask[8] = { 0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03,
        0x01 };
byte dfox_node::pos_mask[48] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40, 0x20, 0x10,
        0x08, 0x04, 0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40, 0x20, 0x10,
        0x08, 0x04, 0x02, 0x01 };
