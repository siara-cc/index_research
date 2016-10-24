#include <iostream>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include "bfos.h"
#include "GenTree.h"

#define NEXT_LEVEL(); keyFoundAt += (*keyFoundAt + 1); \
                setBuf((byte *) util::fourBytesToPtr(keyFoundAt)); \
                if (isPut) node_paths[level++] = buf; \
                if (isLeaf()) \
                    return; \
                keyPos = 1; \
                key_char = *key; \
                t = trie;

char *bfos::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    bfos_node_handler node(root_data);
    byte *node_paths[8];
    node.key = key;
    node.key_len = key_len;
    if (!node.isLeaf())
        node.traverseToLeaf(node_paths);
    if (node.locate() == -1)
        return null;
    return (char *) node.getData(node.keyFoundAt - node.buf, pValueLen);
}

void bfos::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    byte *node_paths[8];
    bfos_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    node.value = value;
    node.value_len = value_len;
    node.isPut = true;
    if (node.filledSize() == 0) {
        node.addData(0);
        total_size++;
    } else {
        if (!node.isLeaf())
            node.traverseToLeaf(node_paths);
        node.locate();
        recursiveUpdate(&node, -1, node_paths, numLevels - 1);
    }
}

void bfos::recursiveUpdate(bfos_node_handler *node, int16_t pos,
        byte *node_paths[], int16_t level) {
    int16_t idx = pos; // lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (node->isFull(node->key_len + node->value_len)) {
            //std::cout << "Full\n" << std::endl;
            //if (maxKeyCount < block->filledSize())
            //    maxKeyCount = block->filledSize();
            //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
            //cout << (int) node->TRIE_LEN << endl;
            if (node->isLeaf())
                maxKeyCount += node->filledSize();
            //    maxKeyCount += node->TRIE_LEN;
            //maxKeyCount += node->PREFIX_LEN;
            int16_t brk_idx;
            char first_key[64];
            int16_t first_len;
            byte *b = node->split(&brk_idx, first_key, &first_len);
            bfos_node_handler new_block(b);
            new_block.isPut = true;
            int16_t cmp = util::compare(first_key, first_len, node->key,
                    node->key_len);
            if (cmp <= 0) {
                new_block.initVars();
                new_block.key = node->key;
                new_block.key_len = node->key_len;
                new_block.value = node->value;
                new_block.value_len = node->value_len;
                idx = ~new_block.locate();
                new_block.addData(idx);
            } else {
                node->initVars();
                idx = ~node->locate();
                node->addData(idx);
            }
            if (node->isLeaf())
                blockCount++;
            if (root_data == node->buf) {
                //blockCount++;
                root_data = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
                bfos_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                root.setLeaf(false);
                byte addr[5];
                util::ptrToFourBytes((unsigned long) node->buf, addr);
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = (char *) addr;
                root.value_len = sizeof(char *);
                root.addData(0);
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                root.initVars();
                root.key = first_key;
                root.key_len = first_len;
                root.value = (char *) addr;
                //root.value_len = sizeof(char *);
                root.locate();
                root.addData(1);
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                bfos_node_handler parent(parent_data);
                byte addr[5];
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                parent.initVars();
                parent.isPut = true;
                parent.key = first_key;
                parent.key_len = first_len;
                parent.value = (char *) addr;
                parent.value_len = sizeof(char *);
                parent.locate();
                recursiveUpdate(&parent, -1, node_paths, prev_level);
            }
        } else
            node->addData(idx);
    } else {
        //if (node->isLeaf) {
        //    int16_t vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

byte *bfos_node_handler::nextPtr(bfos_iterator_status& s) {
    do {
        if (s.offset_a[s.keyPos] > x07) {
            s.tp[s.keyPos] = s.t - trie;
            s.tc = *s.t++;
#if BFOS_UNIT_SZ_3 == 1
            s.orig_children = s.children = *s.t++;
            s.orig_leaves = s.leaves = *s.t++;
#else
            s.orig_children = s.children = (s.tc & x02 ? *s.t++ : 0);
            s.orig_leaves = s.leaves = (s.tc & x01 ? *s.t++ : 0);
#endif
            s.t += GenTree::bit_count[s.children];
            s.offset_a[s.keyPos] = GenTree::last_bit_offset[s.children
                    | s.leaves];
        }
        byte mask = x01 << s.offset_a[s.keyPos];
        if (s.leaves & mask) {
            s.leaves &= ~mask;
            return s.t
                    + GenTree::bit_count2x[s.orig_leaves
                            & ryte_mask[s.offset_a[s.keyPos]]];
        }
        if (s.children & mask) {
            s.t = trie + s.tp[s.keyPos];
#if BFOS_UNIT_SZ_3 == 1
            s.t += 3;
#else
            s.t += ((*s.t & x03) == x03 ? 3 : 2);
#endif
            s.t += GenTree::bit_count[s.orig_children
                    & ryte_mask[s.offset_a[s.keyPos]]];
            s.t += *s.t;
            s.offset_a[++s.keyPos] = 0x09;
        }
        s.offset_a[s.keyPos]++;
        while (s.offset_a[s.keyPos] == x08) {
            if (s.tc & x04) {
                s.keyPos--;
                s.t = trie + s.tp[s.keyPos];
                s.offset_a[s.keyPos]++;
                s.tc = *s.t++;
#if BFOS_UNIT_SZ_3 == 1
                s.orig_children = s.children = *s.t++;
                s.orig_leaves = s.leaves = *s.t++;
#else
                s.orig_children = s.children = (s.tc & x02 ? *s.t++ : 0);
                s.orig_leaves = s.leaves = (s.tc & x01 ? *s.t++ : 0);
#endif
                s.t += GenTree::bit_count[s.children];
            } else {
                s.t += GenTree::bit_count2x[s.orig_leaves];
                s.offset_a[s.keyPos] = 0x09;
                break;
            }
        }
    } while (1); // (s.t - trie) < TRIE_LEN);
    return 0;
}

byte *bfos_node_handler::split(int16_t *pbrk_idx, char *first_key,
        int16_t *first_len_ptr) {
    int16_t orig_filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
    bfos_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    if (!isLeaf())
        new_block.setLeaf(false);
    bfos_node_handler old_block(bfos::split_buf);
    old_block.initBuf();
    old_block.isPut = true;
    if (!isLeaf())
        old_block.setLeaf(false);
    bfos_node_handler *ins_block = &old_block;
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = BFOS_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int16_t brk_idx = 0;
    int16_t tot_len = 0;
    // (1) move all data to new_block in order
    int16_t idx;
    byte ins_key[64];
    int16_t ins_key_len;
    bfos_iterator_status s(trie);
    for (idx = 0; idx < orig_filled_size; idx++) {
        byte *ptr = nextPtr(s);
        int16_t src_idx = util::getInt(ptr);
        int16_t kv_len = buf[src_idx];
        ins_key_len = kv_len;
        kv_len++;
        memcpy(ins_key + s.keyPos, buf + src_idx, kv_len);
        if (isLeaf()) {
            ins_block->value_len = buf[src_idx + kv_len];
            kv_len++;
        } else
            ins_block->value_len = sizeof(char *);
        ins_block->value = (const char *) buf + src_idx + kv_len;
        kv_len += ins_block->value_len;
        tot_len += kv_len;
        ins_key_len += s.keyPos + 1;
        for (int i = 0; i <= s.keyPos; i++)
            ins_key[i] = (trie[s.tp[i]] & xF8) | s.offset_a[i];
        ins_block->key = (char *) ins_key;
        ins_block->key_len = ins_key_len;
        if (idx && brk_idx >= 0)
            ins_block->locate();
        ins_block->addData(kv_last_pos);
        if (brk_idx < 0) {
            brk_idx = -brk_idx;
            s.keyPos++;
            if (isLeaf()) {
                *first_len_ptr = s.keyPos;
                memcpy(first_key, ins_key, s.keyPos);
            } else {
                *first_len_ptr = ins_key_len;
                memcpy(first_key, ins_key, ins_key_len);
            }
            s.keyPos--;
        }
        kv_last_pos += kv_len;
        if (brk_idx == 0) {
            if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                brk_idx = idx + 1;
                brk_idx = -brk_idx;
                ins_block = &new_block;
            }
        }
    }
    memcpy(buf, old_block.buf, BFOS_NODE_SIZE);

    *pbrk_idx = brk_idx;
    return new_block.buf;
}

bfos::bfos() {
    root_data = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
    bfos_node_handler root(root_data);
    root.initBuf();
    total_size = maxKeyCount = 0;
    numLevels = blockCount = 1;
    maxThread = 9999;
}

bfos::~bfos() {
    delete root_data;
}

bfos_node_handler::bfos_node_handler(byte * m) {
    setBuf(m);
    isPut = false;
}

void bfos_node_handler::initBuf() {
    //memset(buf, '\0', BFOS_NODE_SIZE);
    setLeaf(1);
    setFilledSize(0);
    TRIE_LEN = 0;
    keyPos = 1;
    insertState = INSERT_EMPTY;
    setKVLastPos(BFOS_NODE_SIZE);
    trie = buf + BFOS_HDR_SIZE;
}

void bfos_node_handler::setBuf(byte *m) {
    buf = m;
    trie = buf + BFOS_HDR_SIZE;
    insertState = INSERT_EMPTY;
}

void bfos_node_handler::setKVLastPos(int16_t val) {
    util::setInt(LAST_DATA_PTR, val);
}

int16_t bfos_node_handler::getKVLastPos() {
    return util::getInt(LAST_DATA_PTR);
}

void bfos_node_handler::addData(int16_t pos) {

    int16_t ptr = insertCurrent();

    int16_t key_left = key_len - keyPos;
    int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 1);
    if (isLeaf())
        kv_last_pos--;
    setKVLastPos(kv_last_pos);
    util::setInt(trie + ptr, kv_last_pos);
    buf[kv_last_pos] = key_left;
    if (key_left)
        memcpy(buf + kv_last_pos + 1, key + keyPos, key_left);
    if (isLeaf()) {
        buf[kv_last_pos + key_left + 1] = value_len;
        memcpy(buf + kv_last_pos + key_left + 2, value, value_len);
    } else
        memcpy(buf + kv_last_pos + key_left + 1, value, value_len);
    setFilledSize(filledSize() + 1);

}

bool bfos_node_handler::isLeaf() {
    return IS_LEAF_BYTE;
}

void bfos_node_handler::setLeaf(char isLeaf) {
    IS_LEAF_BYTE = isLeaf;
}

void bfos_node_handler::setFilledSize(int16_t filledSize) {
    util::setInt(FILLED_SIZE, filledSize);
}

bool bfos_node_handler::isFull(int16_t kv_len) {
    if ((getKVLastPos() - kv_len - 2) < (BFOS_HDR_SIZE + TRIE_LEN + need_count))
        return true;
    if (TRIE_LEN + need_count > 240)
        return true;
    return false;
}

int16_t bfos_node_handler::filledSize() {
    return util::getInt(FILLED_SIZE);
}

byte *bfos_node_handler::getChild(int16_t pos) {
    byte *idx = getData(pos, &pos);
    unsigned long addr_num = util::fourBytesToPtr(idx);
    byte *ret = (byte *) addr_num;
    return ret;
}

byte *bfos_node_handler::getKey(int16_t ptr, int16_t *plen) {
    byte *kvIdx = buf + ptr;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *bfos_node_handler::getData(int16_t ptr, int16_t *plen) {
    byte *kvIdx = buf + ptr;
    *plen = kvIdx[0];
    kvIdx++;
    kvIdx += *plen;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

void bfos_node_handler::updatePtrs(byte *upto, int diff) {
#if BFOS_UNIT_SZ_3 == 0
    byte tc = *trie;
#endif
    byte *t = trie + 1;
    while (t <= upto) {
#if BFOS_UNIT_SZ_3 == 1
        int count = GenTree::bit_count[*t++];
        byte leaves = *t++;
#else
        int count = (tc & x02 ? GenTree::bit_count[*t++] : 0);
        byte leaves = (tc & x01 ? *t++ : 0);
#endif
        while (count--) {
            if (t < upto && (t + *t) >= upto)
                *t += diff;
            t++;
        }
        t += GenTree::bit_count2x[leaves];
#if BFOS_UNIT_SZ_3 == 0
        tc = *t;
#endif
        t++;
    }
}

int16_t bfos_node_handler::insertCurrent() {
    byte key_char;
    byte mask;
    byte *leafPos;
    int16_t ret, ptr, pos;
    ret = pos = 0;

    switch (insertState) {
    case INSERT_AFTER:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        *origPos &= xFB;
#if BFOS_UNIT_SZ_3 == 1
        updatePtrs(triePos, 5);
        insAt(triePos, ((key_char & xF8) | x05), 0, mask, 0, 0);
        ret = triePos - trie + 3;
#else
        updatePtrs(triePos, 4);
        insAt(triePos, ((key_char & xF8) | x05), mask, 0, 0);
        ret = triePos - trie + 2;
#endif
        break;
    case INSERT_BEFORE:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
#if BFOS_UNIT_SZ_3 == 1
        updatePtrs(origPos, 5);
        if (keyPos > 1 && last_child_pos)
        trie[last_child_pos] -= 5;
        insAt(origPos, ((key_char & xF8) | x01), 0, mask, 0, 0);
        ret = origPos - trie + 3;
#else
        updatePtrs(origPos, 4);
        if (keyPos > 1 && last_child_pos)
            trie[last_child_pos] -= 4;
        insAt(origPos, ((key_char & xF8) | x01), mask, 0, 0);
        ret = origPos - trie + 2;
#endif
        break;
    case INSERT_LEAF:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
#if BFOS_UNIT_SZ_3 == 1
        leafPos = origPos + 2;
        *leafPos |= mask;
#else
        leafPos = origPos + ((*origPos & x02) ? 2 : 1);
        if (*origPos & x01) {
            *leafPos |= mask;
        } else {
            updatePtrs(leafPos, 1);
            insAt(leafPos, mask);
            *origPos |= x01;
            triePos++;
        }
#endif
        updatePtrs(triePos, 2);
        insAt(triePos, 0, 0);
        ret = triePos - trie;
        break;
    case INSERT_THREAD:
        int16_t p, min;
        byte c1, c2;
        byte *childPos;
        key_char = key[keyPos - 1];
        c1 = c2 = key_char;
        key_char &= x07;
        mask = x01 << key_char;
#if BFOS_UNIT_SZ_3 == 1
        childPos = origPos + 1;
        *childPos |= mask;
        triePos = childPos + GenTree::bit_count[*childPos & ryte_mask[key_char]]
        + 2;
        insAt(triePos, (byte) (TRIE_LEN - (triePos - trie) + 1));
        updatePtrs(triePos, 1);
#else
        childPos = origPos + 1;
        if (*origPos & x02) {
            triePos = childPos + ((*origPos & x01) ? 2 : 1)
                    + GenTree::bit_count[*childPos & ryte_mask[key_char]];
            insAt(triePos, (byte) (TRIE_LEN - (triePos - trie) + 1));
            updatePtrs(triePos, 1);
            *childPos |= mask;
        } else {
            *origPos |= x02;
            insAt(childPos, mask);
            triePos = childPos + (*origPos & x01 ? 2 : 1);
            insAt(triePos, (byte) (TRIE_LEN - (triePos - trie) + 1));
            updatePtrs(childPos, 2);
        }
#endif
        p = keyPos;
        min = util::min(key_len, keyPos + key_at_len);
#if BFOS_UNIT_SZ_3 == 1
        leafPos = origPos + 2;
#else
        leafPos = origPos + ((*origPos & x02) ? 2 : 1);
#endif
        triePos = leafPos + GenTree::bit_count[*childPos] + 1;
        triePos += GenTree::bit_count2x[*leafPos & ryte_mask[key_char]];
        ptr = util::getInt(triePos);
        if (p < min) {
            (*leafPos) &= ~mask;
#if BFOS_UNIT_SZ_3 == 0
            if (!*leafPos) {
                *origPos &= ~x01;
                delAt(leafPos, 1);
                updatePtrs(leafPos, -1);
                triePos--;
            }
#endif
            delAt(triePos, 2);
            updatePtrs(triePos, -2);
        } else {
            pos = triePos - trie;
            ret = pos;
        }
        while (p < min) {
            bool isSwapped = false;
            c1 = key[p];
            c2 = key_at[p - keyPos];
            if (c1 > c2) {
                byte swap = c1;
                c1 = c2;
                c2 = swap;
                isSwapped = true;
            }
            switch ((c1 ^ c2) > x07 ?
                    0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
            case 0:
                append((c1 & xF8) | x01);
#if BFOS_UNIT_SZ_3 == 1
                append(0);
#endif
                append(x01 << (c1 & x07));
                ret = isSwapped ? ret : TRIE_LEN;
                pos = isSwapped ? TRIE_LEN : pos;
                appendPtr(isSwapped ? ptr : 0);
                append((c2 & xF8) | x05);
#if BFOS_UNIT_SZ_3 == 1
                append(0);
#endif
                append(x01 << (c2 & x07));
                ret = isSwapped ? TRIE_LEN : ret;
                pos = isSwapped ? pos : TRIE_LEN;
                appendPtr(isSwapped ? 0 : ptr);
                break;
            case 1:
                append((c1 & xF8) | x05);
#if BFOS_UNIT_SZ_3 == 1
                append(0);
#endif
                append((x01 << (c1 & x07)) | (x01 << (c2 & x07)));
                ret = isSwapped ? ret : TRIE_LEN;
                pos = isSwapped ? TRIE_LEN : pos;
                appendPtr(isSwapped ? ptr : 0);
                ret = isSwapped ? TRIE_LEN : ret;
                pos = isSwapped ? pos : TRIE_LEN;
                appendPtr(isSwapped ? 0 : ptr);
                break;
            case 2:
                append((c1 & xF8) | x06);
                append(x01 << (c1 & x07));
#if BFOS_UNIT_SZ_3 == 1
                append(0);
#endif
                append(1);
                break;
            case 3:
                append((c1 & xF8) | x07);
                append(x01 << (c1 & x07));
                append(x01 << (c1 & x07));
                append(3);
                ret = (p + 1 == key_len) ? TRIE_LEN : ret;
                pos = (p + 1 == key_len) ? pos : TRIE_LEN;
                appendPtr((p + 1 == key_len) ? 0 : ptr);
                break;
            }
            if (c1 != c2)
                break;
            p++;
        }
        int16_t diff;
        diff = p - keyPos;
        keyPos = p + 1;
        if (c1 == c2) {
            c2 = (p == key_len ? key_at[diff] : key[p]);
            append((c2 & xF8) | x05);
#if BFOS_UNIT_SZ_3 == 1
            append(0);
#endif
            append(x01 << (c2 & x07));
            ret = (p == key_len) ? ret : TRIE_LEN;
            pos = (p == key_len) ? TRIE_LEN : pos;
            appendPtr((p == key_len) ? ptr : 0);
            if (p == key_len)
                keyPos--;
        }
        if (diff < key_at_len)
            diff++;
        if (diff) {
            p = ptr;
            key_at_len -= diff;
            p += diff;
            if (key_at_len >= 0) {
                buf[p] = key_at_len;
                util::setInt(trie + pos, p);
            }
        }
        break;
    case INSERT_EMPTY:
        key_char = *key;
        mask = x01 << (key_char & x07);
        append((key_char & xF8) | x05);
#if BFOS_UNIT_SZ_3 == 1
        append(0);
#endif
        append(mask);
        ret = TRIE_LEN;
        append(0);
        append(0);
        keyPos = 1;
        break;
    }
    return ret;
}

byte *bfos_node_handler::getFirstPtr() {
    bfos_iterator_status s(trie);
    return nextPtr(s); //, 0, 0);
}

int16_t bfos_node_handler::getLastPtrOfChild(byte *triePos) {
    do {
        byte tc = *triePos++;
#if BFOS_UNIT_SZ_3 == 1
        byte children = *triePos++;
        byte leaves = *triePos++;
#else
        byte children = (tc & x02 ? *triePos++ : 0);
        byte leaves = (tc & x01 ? *triePos++ : 0);
#endif
        triePos += GenTree::bit_count[children];
        if (tc & x04) {
            if (GenTree::first_bit_mask[leaves] <= children) {
                triePos--;
                triePos += *triePos;
            } else {
                triePos += GenTree::bit_count2x[leaves];
                return util::getInt(triePos - 2);
            }
        } else
            triePos += GenTree::bit_count2x[leaves];
    } while (1);
    return -1;
}

byte *bfos_node_handler::getLastPtr(byte *last_t, byte last_off) {
    byte last_child, last_leaf, cnt;
#if BFOS_UNIT_SZ_3 == 1
    last_t++;
    last_child = *last_t++;
    last_leaf = *last_t++;
#else
    byte trie_char = *last_t++;
    last_child = (trie_char & x02 ? *last_t++ : 0);
    last_leaf = (trie_char & x01 ? *last_t++ : 0);
#endif
    cnt = GenTree::bit_count[last_child];
    last_child &= ryte_mask[last_off];
    last_leaf &= ryte_leaf_mask[last_off];
    if (last_child < GenTree::first_bit_mask[last_leaf]) {
        last_t += cnt;
        last_t += GenTree::bit_count2x[last_leaf];
        return buf + util::getInt(last_t - 2);
    }
    last_t += GenTree::bit_count[last_child];
    last_t--;
    return buf + getLastPtrOfChild(last_t + *last_t);
}

void bfos_node_handler::traverseToLeaf(byte *node_paths[]) {
    keyPos = 1;
    byte level;
    byte key_char = *key;
    byte *t = trie;
    byte *last_t = t;
    byte last_off;
    level = keyPos = last_off = 1;
    *node_paths = buf;
    do {
        byte trie_char, r_leaves, r_children;
        origPos = t;
        trie_char = *t++;
        switch ((key_char ^ trie_char) > x07 ?
                (key_char > trie_char ? 0 : 2) : 1) {
        case 0:
            last_t = origPos;
            last_off = 8;
#if BFOS_UNIT_SZ_3 == 1
            r_children = *t++;
            r_leaves = *t++;
#else
            r_children = (trie_char & x02 ? *t++ : 0);
            r_leaves = (trie_char & x01 ? *t++ : 0);
#endif
            t += GenTree::bit_count[r_children];
            t += GenTree::bit_count2x[r_leaves];
            if (trie_char & x04) {
                if (r_children < GenTree::first_bit_mask[r_leaves])
                    keyFoundAt = buf + util::getInt(t - 2);
                else {
                    t -= GenTree::bit_count2x[r_leaves];
                    t--;
                    keyFoundAt = buf + getLastPtrOfChild(t + *t);
                }
                NEXT_LEVEL();
            }
            continue;
        case 1:
            byte r_mask;
#if BFOS_UNIT_SZ_3 == 1
            r_children = *t++;
            r_leaves = *t++;
#else
            r_children = (trie_char & x02 ? *t++ : 0);
            r_leaves = (trie_char & x01 ? *t++ : 0);
#endif
            key_char &= x07;
            r_mask = x01 << key_char;
            switch (r_leaves & r_mask ?
                    (r_children & r_mask ? (keyPos == key_len ? 3 : 4) : 2) :
                    (r_children & r_mask ? (keyPos == key_len ? 0 : 1) : 0)) {
            case 0:
                if ((r_children | r_leaves) & ryte_mask[key_char]) {
                    last_t = origPos;
                    last_off = key_char;
                }
                keyFoundAt = getLastPtr(last_t, last_off);
                NEXT_LEVEL();
                continue;
            case 1:
                if ((r_children | r_leaves) & ryte_mask[key_char]) {
                    last_t = origPos;
                    last_off = key_char;
                }
                break;
            case 2:
                int16_t cmp;
                r_mask = ryte_mask[key_char];
                t += GenTree::bit_count[r_children];
                t += GenTree::bit_count2x[r_leaves & r_mask];
                int16_t ptr;
                ptr = util::getInt(t);
                key_at = (char *) buf + ptr;
                key_at_len = *key_at;
                key_at++;
                cmp = util::compare(key + keyPos, key_len - keyPos, key_at,
                        key_at_len);
                if (cmp == 0) {
                    keyFoundAt = buf + ptr;
                    NEXT_LEVEL();
                    continue;
                }
                if (cmp < 0) {
                    ptr = 0;
                    if ((r_children | r_leaves) & r_mask) {
                        last_t = origPos;
                        last_off = key_char;
                    }
                }
                keyFoundAt = ptr ? buf + ptr : getLastPtr(last_t, last_off);
                NEXT_LEVEL();
                continue;
            case 3:
                t += GenTree::bit_count[r_children];
                t += GenTree::bit_count2x[r_leaves & ryte_mask[key_char]];
                keyFoundAt = buf + util::getInt(t);
                NEXT_LEVEL();
                continue;
            case 4:
                last_t = origPos;
                last_off = key_char + 9;
                break;
            }
            t += GenTree::bit_count[r_children & ryte_mask[key_char]];
            t += *t;
            key_char = key[keyPos++];
            continue;
        case 2:
            keyFoundAt = getLastPtr(last_t, last_off);
            NEXT_LEVEL();
            break;
        }
    } while (1);
}

int16_t bfos_node_handler::locate() {
    keyPos = 1;
    byte key_char = *key;
    byte *t = trie;
    do {
        byte trie_char, r_leaves, r_children;
        origPos = t;
        trie_char = *t++;
        switch ((key_char ^ trie_char) > x07 ?
                (key_char > trie_char ? 0 : 2) : 1) {
        case 0:
            last_child_pos = 0;
#if BFOS_UNIT_SZ_3 == 1
            r_children = *t++;
            r_leaves = *t++;
#else
            r_children = (trie_char & x02 ? *t++ : 0);
            r_leaves = (trie_char & x01 ? *t++ : 0);
#endif
            t += GenTree::bit_count[r_children];
            t += GenTree::bit_count2x[r_leaves];
            if (trie_char & x04) {
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_AFTER;
                    need_count = 5;
                }
                return -1;
            }
            continue;
        case 1:
            byte r_mask;
            last_child_pos = 0;
#if BFOS_UNIT_SZ_3 == 1
            r_children = *t++;
            r_leaves = *t++;
#else
            r_children = (trie_char & x02 ? *t++ : 0);
            r_leaves = (trie_char & x01 ? *t++ : 0);
#endif
            key_char &= x07;
            r_mask = x01 << key_char;
            switch (r_leaves & r_mask ?
                    (r_children & r_mask ? (keyPos == key_len ? 3 : 1) : 2) :
                    (r_children & r_mask ? (keyPos == key_len ? 0 : 1) : 0)) {
            case 0:
                if (isPut) {
                    t += GenTree::bit_count[r_children];
                    t += GenTree::bit_count2x[r_leaves & ryte_mask[key_char]];
                    triePos = t;
                    insertState = INSERT_LEAF;
                    need_count = 2;
                }
                return -1;
            case 1:
                break;
            case 2:
                int16_t cmp;
                t += GenTree::bit_count[r_children];
                t += GenTree::bit_count2x[r_leaves & ryte_mask[key_char]];
                int16_t ptr;
                ptr = util::getInt(t);
                key_at = (char *) buf + ptr;
                key_at_len = *key_at;
                key_at++;
                cmp = util::compare(key + keyPos, key_len - keyPos, key_at,
                        key_at_len);
                if (cmp == 0) {
                    keyFoundAt = buf + ptr;
                    return ptr;
                }
                if (isPut) {
                    insertState = INSERT_THREAD;
                    if (cmp < 0)
                        cmp = -cmp;
                    need_count = (cmp * 4) + 10;
                }
                return -1;
            case 3:
                t += GenTree::bit_count[r_children];
                t += GenTree::bit_count2x[r_leaves & ryte_mask[key_char]];
                keyFoundAt = buf + util::getInt(t);
                return 1;
            }
            t += GenTree::bit_count[r_children & ryte_mask[key_char]];
            last_child_pos = t - trie;
            t += *t;
            key_char = key[keyPos++];
            continue;
        case 2:
            if (isPut) {
                insertState = INSERT_BEFORE;
                need_count = 5;
            }
            return -1;
        }
    } while (1);
    return -1;
}

void bfos_node_handler::delAt(byte *ptr) {
    TRIE_LEN--;
    memmove(ptr, ptr + 1, trie + TRIE_LEN - ptr);
}

void bfos_node_handler::delAt(byte *ptr, int16_t count) {
    TRIE_LEN -= count;
    memmove(ptr, ptr + count, trie + TRIE_LEN - ptr);
}

void bfos_node_handler::insAt(byte *ptr, byte b) {
    memmove(ptr + 1, ptr, trie + TRIE_LEN - ptr);
    *ptr = b;
    TRIE_LEN++;
}

byte bfos_node_handler::insAt(byte *ptr, byte b1, byte b2) {
    memmove(ptr + 2, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr = b2;
    TRIE_LEN += 2;
    return 2;
}

byte bfos_node_handler::insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4) {
    memmove(ptr + 4, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr++ = b3;
    *ptr = b4;
    TRIE_LEN += 4;
    return 4;
}

byte bfos_node_handler::insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4,
        byte b5) {
    memmove(ptr + 5, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr++ = b3;
    *ptr++ = b4;
    *ptr = b5;
    TRIE_LEN += 5;
    return 5;
}

void bfos_node_handler::setAt(byte pos, byte b) {
    trie[pos] = b;
}

void bfos_node_handler::append(byte b) {
    trie[TRIE_LEN++] = b;
}

void bfos_node_handler::appendPtr(int16_t p) {
    util::setInt(trie + TRIE_LEN, p);
    TRIE_LEN += 2;
}

byte bfos_node_handler::getAt(byte pos) {
    return trie[pos];
}

void bfos_node_handler::initVars() {
}

byte bfos_node_handler::left_mask[9] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8,
        0xFC, 0xFE, 0xFF };
byte bfos_node_handler::left_incl_mask[8] = { 0x80, 0xC0, 0xE0, 0xF0, 0xF8,
        0xFC, 0xFE, 0xFF };
byte bfos_node_handler::ryte_mask[18] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F,
        0x3F, 0x7F, 0xFF, 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF };
byte bfos_node_handler::ryte_incl_mask[8] = { 0x01, 0x03, 0x07, 0x0F, 0x1F,
        0x3F, 0x7F, 0xFF };
byte bfos_node_handler::ryte_leaf_mask[18] = { 0x00, 0x01, 0x03, 0x07, 0x0F,
        0x1F, 0x3F, 0x7F, 0xFF, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF,
        0xFF };
byte bfos::split_buf[BFOS_NODE_SIZE];
int bfos::count1, bfos::count2;
