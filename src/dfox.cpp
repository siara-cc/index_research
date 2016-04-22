#include <iostream>
#include <math.h>
#include "dfox.h"
#include "GenTree.h"

dfox_var *dfox::newVar() {
    return new dfox_var();
}

dfox_node *dfox::newNode() {
    dfox_node *new_node = new dfox_node();
    new_node->set_locate_option(true);
    return new_node;
}

dfox_node *dfox_node::split(int *pbrk_idx) {
    int orig_filled_size = filledSize();
    dfox_node *new_block = new dfox_node();
    if (!isLeaf())
        new_block->setLeaf(false);
    int kv_last_pos = getKVLastPos();
    int halfKVLen = DFOX_NODE_SIZE - kv_last_pos + 1;
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
    memcpy(buf + DFOX_NODE_SIZE - old_blk_new_len, new_block->buf + kv_last_pos,
            old_blk_new_len);
    // (3) Insert pointers to the moved data in old block
    FILLED_SIZE = 0;
    TRIE_LEN = 0;
    for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
        int src_idx = new_block->getPtr(new_idx);
        src_idx += (DFOX_NODE_SIZE - brk_kv_pos);
        insPtr(new_idx, src_idx);
        if (new_idx == 0) // (5) Set Last data position for old block
            setKVLastPos(src_idx);
        char *key = (char *) (buf + src_idx + 1);
        v->init();
        v->key = key;
        v->key_len = buf[src_idx];
        locate(v);
        insertCurrent(v);
    }
    int new_size = orig_filled_size - brk_idx - 1;
    /*
     // (4) move new block pointers to front
     byte *new_kv_idx = new_block->buf + IDX_HDR_SIZE - orig_filled_size;
     memmove(new_kv_idx + new_idx, new_kv_idx, new_size); */
    // (4) and set corresponding bits
    byte high_bits[MAX_PTR_BITMAP_BYTES];
    memcpy(high_bits, new_block->DATA_PTR_HIGH_BITS, MAX_PTR_BITMAP_BYTES);
    memset(new_block->DATA_PTR_HIGH_BITS, 0, MAX_PTR_BITMAP_BYTES);
    for (int i = 0; i < new_size; i++) {
        int from_bit = i + new_idx;
        if (high_bits[from_bit >> 3] & pos_mask[from_bit])
            new_block->DATA_PTR_HIGH_BITS[i >> 3] |= pos_mask[i];
    }
    if (high_bits[MAX_PTR_BITMAP_BYTES-1] & 0x01)
        new_block->setLeaf(1);
    new_block->setKVLastPos(brk_kv_pos); // (5) New Block Last position
    //filled_size = brk_idx + 1;
    //setFilledSize(filled_size); // (7) Filled Size for Old block
    new_block->TRIE_LEN = 0;
    new_block->setFilledSize(new_size); // (8) Filled Size for New Block
    for (int i = 0; i < new_size; i++) {
        v->init();
        v->key = (char *) new_block->getKey(i, &v->key_len);
        new_block->locate(v);
        new_block->insertCurrent(v);
    }
    *pbrk_idx = brk_idx;
    return new_block;
}

dfox::dfox() {
    root = newNode();
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 0; //9999;
    blockCount = 1;
}

dfox_node::dfox_node() {
    buf = new byte[DFOX_NODE_SIZE];
    memset(buf, '\0', DFOX_NODE_SIZE);
    setLeaf(1);
    FILLED_SIZE = 0;
    TRIE_LEN = 0;
    setKVLastPos(DFOX_NODE_SIZE);
    trie = buf + IDX_HDR_SIZE;
}

void dfox_node::setKVLastPos(int val) {
    util::setInt(LAST_DATA_PTR, val);
}

int dfox_node::getKVLastPos() {
    return util::getInt(LAST_DATA_PTR);
}

void dfox_node::addData(int pos, const char *value, int value_len,
        bplus_tree_var *v) {
    addData(pos, value, value_len, (dfox_var *) v);
}

void dfox_node::addData(int pos, const char *value, int value_len,
        dfox_var *v) {

    insertCurrent(v);
    int kv_last_pos = getKVLastPos() - (v->key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = v->key_len;
    memcpy(buf + kv_last_pos + 1, v->key, v->key_len);
    buf[kv_last_pos + v->key_len + 1] = value_len;
    memcpy(buf + kv_last_pos + v->key_len + 2, value, value_len);

    insPtr(pos, kv_last_pos);

}

void dfox_node::insPtr(int pos, int kv_pos) {
    int filledSz = filledSize();
    filledSz++;
    byte *kvIdx = buf + IDX_BLK_SIZE;
    kvIdx -= filledSz;
    if (pos > 0) {
        memmove(kvIdx, kvIdx + 1, pos);
    }
    kvIdx[pos] = (byte) kv_pos & 0xFF;
    byte offset = pos & 0x07;
    byte carry = 0;
    if (kv_pos > 256)
        carry = 0x80;
    byte is_leaf = isLeaf();
    for (int i = (pos >> 3); i < MAX_PTR_BITMAP_BYTES; i++) {
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

void dfox_node::setFilledSize(int filledSize) {
    FILLED_SIZE = filledSize;
}

bool dfox_node::isFull(int kv_len, bplus_tree_var *v) {
    return isFull(kv_len, (dfox_var *) v);
}

bool dfox_node::isFull(int kv_len, dfox_var *v) {
    if (TRIE_LEN + 8 + v->need_count + FILLED_SIZE >= TRIE_PTR_AREA_SIZE)
        return true;
    if (FILLED_SIZE > MAX_PTRS)
        return true;
    if ((getKVLastPos() - kv_len - 2) < (IDX_BLK_SIZE + 2))
        return true;
    return false;
}

int dfox_node::filledSize() {
    return FILLED_SIZE;
}

dfox_node *dfox_node::getChild(int pos) {
    byte *idx = getData(pos, &pos);
    unsigned long addr_num = util::fourBytesToPtr(idx);
    dfox_node *ret = (dfox_node *) addr_num;
    return ret;
}

int dfox_node::getPtr(int pos) {
    byte *kvIdx = buf + IDX_BLK_SIZE;
    kvIdx -= FILLED_SIZE;
    kvIdx += pos;
    int idx = *kvIdx;
    if (DATA_PTR_HIGH_BITS[pos >> 3] & pos_mask[pos])
        idx += 256;
    return idx;
}

byte *dfox_node::getKey(int pos, int *plen) {
    byte *kvIdx = buf + getPtr(pos);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *dfox_node::getData(int pos, int *plen) {
    byte *kvIdx = buf + getPtr(pos);
    *plen = kvIdx[0];
    kvIdx++;
    kvIdx += *plen;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

void dfox_node::insertCurrent(dfox_var *v) {
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
        insAt(v->triePos, (v->msb5 | 0xC0), v->mask);
        v->triePos += 2;
        // trie.insert(triePos, (char) 0);
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
            v->triePos++;
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
                if (leaf)
                    setAt(leafPos, leaf);
                else {
                    delAt(leafPos);
                    v->triePos--;
                    origTC |= 0x20;
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
            int msb5c2 = c2 >> 3;
            int offc2 = c2 & 0x07;
            msb5c2 |= 0xC0;
            insAt(v->triePos, msb5c2, (0x80 >> offc2));
            v->triePos += 2;
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
            v->triePos++;
            origTC &= 0xDF; // ~0x20
            setAt(v->origPos, origTC);
        }
        break;
    case INSERT_MIDDLE2:
        insAt(v->triePos, (v->msb5 | 0x40), v->mask);
        v->triePos += 2;
        break;
    }
    v->insertState = 0;
    int idx_len = 0;
    long idx_list[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    v->init();
    //std::cout << "----------" << std::endl;

    //while (v->triePos < TRIE_LEN)
    //    recurseEntireTrie(0, v, idx_list, &idx_len);
    for (int i = 0; i < idx_len; i++) {
        long l = idx_list[i];
        int pos = ((l >> 16) & 0xFF);
        int sub_tree_count = ((l >> 8) & 0xFF);
        int sub_tree_size = (l & 0xFF);
        for (int j = i + 1; j < 4; j++) {
            long nxt_l = idx_list[j];
            int nxt_start = ((nxt_l >> 16) & 0xFF);
            int nxt_end = nxt_start + (nxt_l & 0xFF);
            if (nxt_start > pos && nxt_end < (pos + sub_tree_size))
                sub_tree_size += 2;
        }
        insAt(pos, (0x60 | sub_tree_count), sub_tree_size);
        for (int j = i + 1; j < 4; j++) {
            long nxt_l = idx_list[j];
            int nxt_pos = ((nxt_l >> 16) & 0xFF);
            if (nxt_pos > pos) {
                nxt_pos += 2;
                nxt_l = (nxt_l & 0xFF00FFFF);
                nxt_l |= (nxt_pos << 16);
                idx_list[j] = nxt_l;
            }
        }
//        std::cout << "level:" << (l >> 24) << ", pos:" << (pos + i * 2)
//                << ", lsp:" << sub_tree_count << ", size:" << sub_tree_size
//                << std::endl;
        if (i == 3)
            break;
    }
}

byte dfox_node::recurseEntireTrie(int level, dfox_var *v, long idx_list[],
        int *pidx_len) {
    if (v->triePos >= TRIE_LEN)
        return 0x80;
    byte cur_tc = getAt(v->triePos);
    if (0x60 == (cur_tc & 0x60)) {
        delAt(v->triePos, 2);
        cur_tc = getAt(v->triePos);
    }
    int origPos = v->triePos;
    int sub_tree_size = v->triePos;
    v->triePos++;
    int sub_tree_count = v->lastSearchPos;
    v->tc = cur_tc;
    byte children = 0;
    byte leaves = 0;
    if (0 == (cur_tc & 0x40))
        children = getAt(v->triePos++);
    if (0 == (cur_tc & 0x20))
        leaves = getAt(v->triePos++);
    for (int i = 0; i < 8; i++) {
        byte mask = (0x80 >> i);
        if (leaves & mask)
            v->lastSearchPos++;
        if (children & mask) {
            byte ctc;
            do {
                ctc = recurseEntireTrie(level + 1, v, idx_list, pidx_len);
            } while (ctc < 128);
        }
    }
    // test if current one is terminated, then sub_tree_size can be ignored
    // otherwise, add to list if more than threshold
    sub_tree_size = v->triePos - sub_tree_size;
    if (cur_tc < 128 && sub_tree_size > 4 && v->triePos < TRIE_LEN) {
        long to_insert = (level << 24);
        to_insert |= (origPos << 16);
        to_insert |= ((v->lastSearchPos - sub_tree_count) << 8);
        to_insert |= sub_tree_size;
        (*pidx_len)++;
        for (int i = 0; i < *pidx_len; i++) {
            if (idx_list[i] == 0 || to_insert < idx_list[i]) {
                for (int j = *pidx_len; j > i; j--)
                    idx_list[j] = idx_list[j - 1];
                idx_list[i] = to_insert;
                break;
            }
        }
    }

    return cur_tc;
}

byte dfox_node::recurseSkip(dfox_var *v, byte skip_count, byte skip_size) {
    if (v->triePos >= TRIE_LEN)
        return 0x80;
    byte tc = getAt(v->triePos);
    if (0x60 == (tc & 0x60)) {
        skip_count = (tc & 0x1F);
        v->triePos++;
        skip_size = getAt(v->triePos++);
//        std::cout << "SSkip Count:" << (int) skip_count << ", Skip Size:"
//                << (int) skip_size << std::endl;
        tc = getAt(v->triePos);
    }
    if (skip_count > 0) {
        //std::cout << "Skipping:" << (int) skip_size << std::endl;
        v->triePos += skip_size;
        v->lastSearchPos += skip_count;
//        while (tc < 128) {
//            tc = recurseSkip(v, 0, 0);
//        }
//        tc = getAt(v->triePos);
    } else {
        v->triePos++;
        byte children = 0;
        byte leaves = 0;
        if (0 == (tc & 0x40))
            children = getAt(v->triePos++);
        if (0 == (tc & 0x20))
            leaves = getAt(v->triePos++);
        v->lastSearchPos += GenTree::bit_count[leaves];
        for (int i = 0; i < GenTree::bit_count[children]; i++) {
            byte ctc;
            do {
                ctc = recurseSkip(v, 0, 0);
            } while (ctc < 128); //(ctc & 0x80) != 0x80);
        }
    }
    return tc;
}

bool dfox_node::recurseTrie(int level, dfox_var *v) {
    if (v->csPos >= v->key_len)
        return false;
    byte kc = v->key[v->csPos++];
    v->msb5 = kc >> 3;
    byte offset = kc & 0x07;
    v->mask = 0x80 >> offset;

    byte skip_count = 0;
    byte skip_size = 0;
    v->tc = getAt(v->triePos);
    if (0x60 == (v->tc & 0x60)) {
        skip_count = (v->tc & 0x1F);
        v->triePos++;
        skip_size = getAt(v->triePos++);
//        std::cout << "Skip Count:" << (int) skip_count << ", Skip Size:"
//                << (int) skip_size << std::endl;
        v->tc = getAt(v->triePos);
    }
    byte msb5tc = v->tc & 0x1F;
    v->origPos = v->triePos;
    while (msb5tc < v->msb5) {
        v->tc = recurseSkip(v, skip_count, skip_size);
        skip_count = 0;
        skip_size = 0;
        if (v->tc > 127 /* (v->tc & 0x80) == 0x80 */|| v->triePos == TRIE_LEN) {
            v->insertState = INSERT_MIDDLE1;
            v->need_count = 2;
            return true;
        }
        v->tc = getAt(v->triePos);
        if (0x60 == (v->tc & 0x60)) {
            skip_count = (v->tc & 0x1F);
            v->triePos++;
            skip_size = getAt(v->triePos++);
//            std::cout << "Skip Count:" << (int) skip_count << ", Skip Size:"
//                    << (int) skip_size << std::endl;
            v->tc = getAt(v->triePos);
        }
        msb5tc = v->tc & 0x1F;
        v->origPos = v->triePos;
    }
    if (msb5tc == v->msb5) {
        v->tc = getAt(v->triePos++);
        byte children = 0;
        v->leaves = 0;
        if (0 == (v->tc & 0x40)) {
            children = getAt(v->triePos++);
        }
        if (0 == (v->tc & 0x20)) {
            v->leaves = getAt(v->triePos++);
            v->lastSearchPos +=
                    GenTree::bit_count[v->leaves & left_mask[offset]];
        }
        byte lCount = GenTree::bit_count[children & left_mask[offset]];
        while (lCount-- > 0) {
            byte ctc;
            do {
                ctc = recurseSkip(v, 0, 0);
            } while (ctc < 128); //(ctc & 0x80) != 0x80);
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

int dfox_node::locate(bplus_tree_var *v) {
    return locate((dfox_var *) v);
}

int dfox_node::locate(dfox_var *v) {
    if (TRIE_LEN == 0)
        return -1;
    while (v->triePos < TRIE_LEN) {
        if (recurseTrie(0, v)) {
            if (v->insertState != 0) {
                v->lastSearchPos = ~(v->lastSearchPos + 1);
            }
            return v->lastSearchPos;
        }
    }
    if (v->insertState != 0) {
        v->lastSearchPos = ~(v->lastSearchPos + 1);
    }
    return v->lastSearchPos;
}

void dfox_node::delAt(byte pos) {
    TRIE_LEN--;
    byte *ptr = trie + pos;
    memmove(ptr, ptr + 1, TRIE_LEN - pos);
}

void dfox_node::delAt(byte pos, int count) {
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
    csPos = 0;
    triePos = 0;
    lastLevel = 0;
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
