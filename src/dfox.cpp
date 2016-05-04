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
        pos = node.locateForGet(key, key_len, level);
        if (pos < 0) {
            pos = ~pos;
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
    pos = node.locateForGet(key, key_len, level);
    *pIdx = pos;
    return node_data;
}

byte *dfox::recursiveSearch(const char *key, int16_t key_len, byte *node_data,
        int16_t lastSearchPos[], byte *node_paths[], int16_t *pIdx,
        dfox_var *v) {
    int16_t level = 0;
    dfox_node node(node_data);
    while (!node.isLeaf()) {
        lastSearchPos[level] = node.locate(v, level);
        node_paths[level] = node_data;
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
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
    lastSearchPos[level] = node.locate(v, level);
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
                root->locate(&rv, 0);
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
                parent_node.locate(&rv, prev_level);
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
            node.locate(v, level);
            idx = ~v->lastSearchPos;
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
        locate(&v, 0);
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
        new_block.locate(&v, 0);
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
    kvIdx[pos] = (byte) kv_pos & 0xFF;
    byte offset = pos & 0x07;
    byte carry = 0;
    if (kv_pos > 256)
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
    if ((getKVLastPos() - kv_len - 2) < (IDX_BLK_SIZE + 2))
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
        // trie.insert(triePos, (char) 0);
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
    case INSERT_MIDDLE2:
        insAt(v->triePos, (v->msb5 | 0x02), v->mask);
        v->triePos += 2;
        break;
    }
    v->insertState = 0;
    int16_t idx_len = 0;
    long idx_list[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    v->init();
    //std::cout << "----------" << std::endl;

    //while (v->triePos < TRIE_LEN)
    //    recurseEntireTrie(0, v, idx_list, &idx_len);
    for (int16_t i = 0; i < idx_len; i++) {
        long l = idx_list[i];
        int16_t pos = ((l >> 16) & 0xFF);
        int16_t sub_tree_count = ((l >> 8) & 0xFF);
        int16_t sub_tree_size = (l & 0xFF);
        for (int16_t j = i + 1; j < 4; j++) {
            long nxt_l = idx_list[j];
            int16_t nxt_start = ((nxt_l >> 16) & 0xFF);
            int16_t nxt_end = nxt_start + (nxt_l & 0xFF);
            if (nxt_start > pos && nxt_end < (pos + sub_tree_size))
                sub_tree_size += 2;
        }
        insAt(pos, (0x60 | sub_tree_count), sub_tree_size);
        for (int16_t j = i + 1; j < 4; j++) {
            long nxt_l = idx_list[j];
            int16_t nxt_pos = ((nxt_l >> 16) & 0xFF);
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

byte dfox_node::recurseEntireTrie(int16_t level, dfox_var *v, long idx_list[],
        int16_t *pidx_len) {
    if (v->triePos >= TRIE_LEN)
        return 0x80;
    byte cur_tc = getAt(v->triePos);
    if (0x60 == (cur_tc & 0x60)) {
        delAt(v->triePos, 2);
        cur_tc = getAt(v->triePos);
    }
    int16_t origPos = v->triePos;
    int16_t sub_tree_size = v->triePos;
    v->triePos++;
    int16_t sub_tree_count = v->lastSearchPos;
    v->tc = cur_tc;
    byte children = 0;
    byte leaves = 0;
    if (0 == (cur_tc & 0x40))
        children = getAt(v->triePos++);
    if (0 == (cur_tc & 0x20))
        leaves = getAt(v->triePos++);
    for (int16_t i = 0; i < 8; i++) {
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
        for (int16_t i = 0; i < *pidx_len; i++) {
            if (idx_list[i] == 0 || to_insert < idx_list[i]) {
                for (int16_t j = *pidx_len; j > i; j--)
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
        return 0x01;
    byte tc = getAt(v->triePos);
    if (0 == (tc & 0x06)) {
        skip_count = (tc >> 3);
        v->triePos++;
        skip_size = getAt(v->triePos++);
//        std::cout << "SSkip Count:" << (int16_t) skip_count << ", Skip Size:"
//                << (int16_t) skip_size << std::endl;
        tc = getAt(v->triePos);
    }
    if (skip_count > 0) {
        //std::cout << "Skipping:" << (int16_t) skip_size << std::endl;
        v->triePos += skip_size;
        v->lastSearchPos += skip_count;
    } else {
        v->triePos++;
        byte children = 0;
        byte leaves = 0;
        if ((tc & 0x04))
            children = getAt(v->triePos++);
        if ((tc & 0x02)) {
            leaves = getAt(v->triePos++);
            v->lastSearchPos += GenTree::bit_count[leaves];
        }
        if (children) {
            byte bc = GenTree::bit_count[children];
            while (bc-- > 0) {
                byte ctc;
                do {
                    ctc = recurseSkip(v, 0, 0);
                } while ((ctc & 0x01) == 0);
            }
        }
    }
    return tc;
}

bool dfox_node::recurseTrie(int16_t level, dfox_var *v) {
    if (v->keyPos >= v->key_len)
        return false;
    byte skip_count = 0;
    byte skip_size = 0;
    v->tc = getAt(v->triePos);
    if (0 == (v->tc & 0x06)) {
        skip_count = (v->tc >> 3);
        v->triePos++;
        skip_size = getAt(v->triePos++);
//        std::cout << "Skip Count:" << (int16_t) skip_count << ", Skip Size:"
//                << (int16_t) skip_size << std::endl;
        v->tc = getAt(v->triePos);
    }
    byte kc = v->key[v->keyPos++];
    while (v->tc < kc) {
        if ((kc ^ v->tc) < 0x08)
            break;
        v->origPos = v->triePos;
        v->tc = recurseSkip(v, skip_count, skip_size);
        if ((v->tc & 0x01) || v->triePos == TRIE_LEN) {
            v->msb5 = (kc & 0xF8);
            v->mask = (0x80 >> (kc & 0x07));
            v->insertState = INSERT_MIDDLE1;
            v->need_count = 2;
            return true;
        }
        skip_count = 0;
        skip_size = 0;
        v->tc = getAt(v->triePos);
        if (0 == (v->tc & 0x06)) {
            skip_count = (v->tc >> 3);
            v->triePos++;
            skip_size = getAt(v->triePos++);
//            std::cout << "Skip Count:" << (int16_t) skip_count << ", Skip Size:"
//                    << (int16_t) skip_size << std::endl;
            v->tc = getAt(v->triePos);
        }
    }
    v->origPos = v->triePos;
    if ((kc ^ v->tc) < 0x08) {
        v->tc = getAt(v->triePos++);
        byte children = 0;
        v->leaves = 0;
        byte offset = (kc & 0x07);
        if ((v->tc & 0x04))
            children = getAt(v->triePos++);
        if ((v->tc & 0x02)) {
            v->leaves = getAt(v->triePos++);
            v->lastSearchPos +=
                    GenTree::bit_count[v->leaves & left_mask[offset]];
        }
        if (children) {
            byte bc = GenTree::bit_count[children & left_mask[offset]];
            while (bc-- > 0) {
                byte ctc;
                do {
                    ctc = recurseSkip(v, 0, 0);
                } while ((ctc & 0x01) == 0);
            }
        }
        v->mask = (0x80 >> offset);
        if (children & v->mask) {
            if (v->leaves & v->mask) {
                v->lastSearchPos++;
                if (v->keyPos == v->key_len) {
                    return true;
                }
            } else {
                if (v->keyPos == v->key_len) {
                    v->insertState = INSERT_LEAF;
                    v->need_count = 2;
                    return true;
                }
            }
            if (recurseTrie(level + 1, v)) {
                return true;
            }
        } else {
            if (v->leaves & v->mask) {
                v->lastSearchPos++;
                v->key_at = (char *) getKey(v->lastSearchPos, &v->key_at_len);
                int16_t cmp = util::compare(v->key, v->key_len, v->key_at,
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
                return true;
            } else {
                v->insertState = INSERT_LEAF;
                v->need_count = 2;
                return true;
            }
        }
    } else {
        byte offset = (kc & 0x07);
        v->mask = (0x80 >> offset);
        v->msb5 = (kc & 0xF8);
        v->insertState = INSERT_MIDDLE2;
        v->need_count = 2;
        return true;
    }
    return false;
}

#define PTC_KEY_FOUND 0
#define PTC_CONTINUE 1
#define PTC_NOT_FOUND 2

#define AFTER_SKIP_INITIAL 0
#define AFTER_SKIP_PROCESS_TC 1
#define AFTER_SKIP_RETURN 2

int16_t dfox_node::locate(dfox_var *v, int16_t level) {
    if (TRIE_LEN == 0)
        return -1;
    register byte to_skip = 0;
    register byte after_skip = AFTER_SKIP_INITIAL;
    register byte kc = v->key[v->keyPos++];
    register byte offset = (kc & 0x07);
    v->mask = (0x80 >> (kc & 0x07));
    register int16_t i = 0;
    register int16_t len = TRIE_LEN;
    while (i < len) {
        register int16_t origPos = i;
        register byte tc = getAt(i++);
        register byte leaves = 0;
        register byte children = 0;
        if (tc & 0x04)
            children = getAt(i++);
        if (tc & 0x02)
            leaves = getAt(i++);
        if (to_skip) {
            if (tc & 0x01)
                to_skip--;
            if (leaves)
                v->lastSearchPos += GenTree::bit_count[leaves];
            if (children)
                to_skip += GenTree::bit_count[children];
            if (!to_skip) {
                if (after_skip == AFTER_SKIP_PROCESS_TC) {
                    v->triePos = i;
                    byte ret = processTC(v);
                    if (ret == PTC_KEY_FOUND)
                        return v->lastSearchPos;
                    else if (ret == PTC_NOT_FOUND) {
                        v->lastSearchPos = ~(v->lastSearchPos + 1);
                        return v->lastSearchPos;
                    }
                    kc = v->key[v->keyPos++];
                    offset = (kc & 0x07);
                    v->mask = (0x80 >> offset);
                } else if (after_skip == AFTER_SKIP_RETURN) {
                    v->triePos = i;
                    v->lastSearchPos = ~(v->lastSearchPos + 1);
                    return v->lastSearchPos;
                }
                after_skip = AFTER_SKIP_INITIAL;
            }
        } else {
            v->origPos = origPos;
            if ((kc ^ tc) < 0x08) {
                if (leaves) {
                    v->lastSearchPos += GenTree::bit_count[leaves
                            & left_mask[offset]];
                }
                if (children) {
                    to_skip = GenTree::bit_count[children & left_mask[offset]];
                }
                v->tc = tc;
                v->children = children;
                v->leaves = leaves;
                v->triePos = i;
                if (to_skip)
                    after_skip = AFTER_SKIP_PROCESS_TC;
                else {
                    byte ret = processTC(v);
                    if (ret == PTC_KEY_FOUND)
                        return v->lastSearchPos;
                    else if (ret == PTC_NOT_FOUND) {
                        v->lastSearchPos = ~(v->lastSearchPos + 1);
                        return v->lastSearchPos;
                    }
                    kc = v->key[v->keyPos++];
                    offset = (kc & 0x07);
                    v->mask = (0x80 >> offset);
                }
            } else if (kc > tc) {
                if (leaves)
                    v->lastSearchPos += GenTree::bit_count[leaves];
                if (children)
                    to_skip += GenTree::bit_count[children];
                if (tc & 0x01) {
                    v->triePos = i;
                    v->tc = tc;
                    v->msb5 = (kc & 0xF8);
                    v->insertState = INSERT_MIDDLE1;
                    v->need_count = 2;
                    if (to_skip)
                        after_skip = AFTER_SKIP_RETURN;
                    else {
                        v->lastSearchPos = ~(v->lastSearchPos + 1);
                        return v->lastSearchPos;
                    }
                }
            } else {
                v->msb5 = (kc & 0xF8);
                v->insertState = INSERT_MIDDLE2;
                v->need_count = 2;
                i--;
                if (children)
                    i--;
                if (leaves)
                    i--;
                v->triePos = i;
                v->lastSearchPos = ~(v->lastSearchPos + 1);
                return v->lastSearchPos;
            }
        }
    }
    v->lastSearchPos = ~(v->lastSearchPos + 1);
    return v->lastSearchPos;
}
//if (recurseTrie(0, v)) {
//    if (v->insertState != 0) {
//        v->lastSearchPos = ~(v->lastSearchPos + 1);
//    }
//    return v->lastSearchPos;
//}

byte dfox_node::processTC(dfox_var *v) {
    if (v->children & v->mask) {
        if (v->leaves & v->mask) {
            v->lastSearchPos++;
            if (v->keyPos == v->key_len) {
                return PTC_KEY_FOUND;
            }
        } else {
            if (v->keyPos == v->key_len) {
                v->insertState = INSERT_LEAF;
                v->need_count = 2;
                return PTC_NOT_FOUND;
            }
        }
        return PTC_CONTINUE;
    } else {
        if (v->leaves & v->mask) {
            v->lastSearchPos++;
            v->key_at = (char *) getKey(v->lastSearchPos, &v->key_at_len);
            int16_t cmp = util::compare(v->key, v->key_len, v->key_at,
                    v->key_at_len, v->keyPos);
            if (cmp == 0)
                return PTC_KEY_FOUND;
            if (cmp < 0)
                v->lastSearchPos--;
            v->insertState = INSERT_THREAD;
            if (cmp < 0)
                cmp = -cmp;
            if (v->keyPos < cmp)
                cmp -= v->keyPos;
            //cmp--;
            v->need_count = cmp * 2;
            v->need_count += 4;
            return PTC_NOT_FOUND;
        } else {
            v->insertState = INSERT_LEAF;
            v->need_count = 2;
            return PTC_NOT_FOUND;
        }
    }
    return PTC_NOT_FOUND;

}

int16_t dfox_node::locateForGet(const char *key, int16_t key_len,
        int16_t level) {
    register int16_t keyPos = 0;
    register byte kc = key[keyPos++];
    register byte offset = (kc & 0x07);
    register byte mask = (0x80 >> offset);
    register byte to_skip = 0;
    byte after_skip = AFTER_SKIP_INITIAL;
    register int16_t pos = -1;
    register byte i = 0;
    register byte len = TRIE_LEN;
    byte orig_child = 0;
    byte orig_leaf = 0;
    while (i < len) {
        register byte tc = trie[i++];
        register byte leaves = 0;
        register byte children = 0;
        if (tc & 0x04)
            children = trie[i++];
        if (tc & 0x02)
            leaves = trie[i++];
        if (to_skip) {
            if (tc & 0x01)
                to_skip--;
            if (leaves) {
                pos += GenTree::bit_count[leaves >> 4];
                pos += GenTree::bit_count[leaves & 0x0F];
            }
            if (children) {
                to_skip += GenTree::bit_count[children >> 4];
                to_skip += GenTree::bit_count[children & 0x0F];
            }
            if (!to_skip) {
                if (after_skip == AFTER_SKIP_PROCESS_TC) {
                    if (orig_child & mask) {
                        if (orig_leaf & mask) {
                            pos++;
                            if (keyPos == key_len)
                                return pos;
                        } else {
                            if (keyPos == key_len)
                                return ~(pos + 1);
                        }
                    } else {
                        if (orig_leaf & mask) {
                            pos++;
                            int16_t key_at_len;
                            const char *key_at = (char *) getKey(pos,
                                    &key_at_len);
                            int16_t cmp = util::compare(key, key_len, key_at,
                                    key_at_len, keyPos);
                            if (cmp == 0)
                                return pos;
                            if (cmp < 0)
                                pos--;
                            return ~(pos + 1);
                        } else
                            return ~(pos + 1);
                    }
                    kc = key[keyPos++];
                    offset = (kc & 0x07);
                    mask = (0x80 >> offset);
                } else if (after_skip == AFTER_SKIP_RETURN)
                    return ~(pos + 1);
                after_skip = AFTER_SKIP_INITIAL;
            }
        } else {
            if ((kc ^ tc) < 0x08) {
                if (leaves) {
                    leaves &= left_mask[offset];
                    pos += GenTree::bit_count[leaves >> 4];
                    pos += GenTree::bit_count[leaves & 0x0F];
                }
                if (children) {
                    children &= left_mask[offset];
                    to_skip = GenTree::bit_count[children >> 4];
                    to_skip += GenTree::bit_count[children & 0x0F];
                }
                if (to_skip) {
                    orig_child = children;
                    orig_leaf = leaves;
                    after_skip = AFTER_SKIP_PROCESS_TC;
                } else {
                    if (children & mask) {
                        if (leaves & mask) {
                            pos++;
                            if (keyPos == key_len)
                                return pos;
                        } else {
                            if (keyPos == key_len)
                                return ~(pos + 1);
                        }
                    } else {
                        if (leaves & mask) {
                            pos++;
                            int16_t key_at_len;
                            const char *key_at = (char *) getKey(pos,
                                    &key_at_len);
                            int16_t cmp = util::compare(key, key_len, key_at,
                                    key_at_len, keyPos);
                            if (cmp == 0)
                                return pos;
                            if (cmp < 0)
                                pos--;
                            return ~(pos + 1);
                        } else
                            return ~(pos + 1);
                    }
                    kc = key[keyPos++];
                    offset = (kc & 0x07);
                    mask = (0x80 >> offset);
                }
            } else if (kc > tc) {
                if (leaves) {
                    pos += GenTree::bit_count[leaves >> 4];
                    pos += GenTree::bit_count[leaves & 0x0F];
                }
                if (children) {
                    to_skip += GenTree::bit_count[children >> 4];
                    to_skip += GenTree::bit_count[children & 0x0F];
                }
                if (tc & 0x01) {
                    if (to_skip)
                        after_skip = AFTER_SKIP_RETURN;
                    else
                        return ~(pos + 1);
                }
            } else
                return ~(pos + 1);
        }
    }
    return ~(pos + 1);
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
