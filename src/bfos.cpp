#include <iostream>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include "bfos.h"
#include "GenTree.h"

char *bfos::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    int16_t pos = -1;
    byte *node_data = root_data;
    bfos_node_handler node(node_data);
    node.key = key;
    node.key_len = key_len;
    recursiveSearchForGet(&node, &pos);
    if (pos < 0)
        return null;
    return (char *) node.getData(pos, pValueLen);
}

void bfos::recursiveSearchForGet(bfos_node_handler *node, int16_t *pIdx) {
    int16_t level = 0;
    int16_t pos = -1;
    byte *node_data = node->buf;
    node->initVars();
    while (!node->isLeaf()) {
        pos = node->locateForNode(level);
        if (pos < 0) {
            pos = ~pos;
            //if (pos)
            //    pos--;
        } else {
            /*            do {
             node_data = node->getChild(pos);
             node->setBuf(node_data);
             level++;
             pos = 0;
             } while (!node->isLeaf());
             *pIdx = pos;
             return;*/
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

void bfos::recursiveSearch(bfos_node_handler *node, int16_t lastSearchPos[],
        byte *node_paths[], int16_t *pIdx) {
    int16_t level = 0;
    byte *node_data = node->buf;
    node->initVars();
    while (!node->isLeaf()) {
        lastSearchPos[level] = node->locateForNode(level);
        node_paths[level] = node->buf;
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
            //if (lastSearchPos[level])
            //    lastSearchPos[level]--;
        } else {
            /*            do {
             node_data = node->getChild(lastSearchPos[level]);
             node->setBuf(node_data);
             level++;
             node_paths[level] = node->buf;
             lastSearchPos[level] = 0;
             } while (!node->isLeaf());
             *pIdx = lastSearchPos[level];
             return;*/
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

void bfos::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    int16_t lastSearchPos[10];
    byte *block_paths[10];
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
        int16_t pos = -1;
        recursiveSearch(&node, lastSearchPos, block_paths, &pos);
        recursiveUpdate(&node, pos, lastSearchPos, block_paths, numLevels - 1);
    }
}

void bfos::recursiveUpdate(bfos_node_handler *node, int16_t pos,
        int16_t lastSearchPos[], byte *node_paths[], int16_t level) {
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
            //maxKeyCount += node->TRIE_LEN;
            int16_t brk_idx;
            byte first_key[40];
            int16_t first_len;
            byte *b = node->split(&brk_idx, first_key, &first_len);
            bfos_node_handler new_block(b);
            new_block.isPut = true;
            int16_t cmp = util::compare((char *) first_key, first_len,
                    node->key, node->key_len);
            if (cmp <= 0) {
                new_block.initVars();
                new_block.key = node->key;
                new_block.key_len = node->key_len;
                new_block.value = node->value;
                new_block.value_len = node->value_len;
                idx = ~new_block.locate(level);
                new_block.addData(idx);
            } else {
                node->initVars();
                idx = ~node->locate(level);
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
                char addr[5];
                util::ptrToFourBytes((unsigned long) node->buf, addr);
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = addr;
                root.value_len = sizeof(char *);
                root.addData(0);
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                root.initVars();
                root.key = (char *) first_key;
                root.key_len = first_len;
                root.value = addr;
                //root.value_len = sizeof(char *);
                root.locate(0);
                root.addData(1);
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                bfos_node_handler parent(parent_data);
                char addr[5];
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                parent.initVars();
                parent.isPut = true;
                parent.key = (char *) first_key;
                parent.key_len = first_len;
                parent.value = addr;
                parent.value_len = sizeof(char *);
                lastSearchPos[prev_level] = parent.locate(prev_level);
                recursiveUpdate(&parent, lastSearchPos[prev_level],
                        lastSearchPos, node_paths, prev_level);
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
            s.orig_children = s.children = *s.t++;
            s.orig_leaves = s.leaves = *s.t++;
            s.t += GenTree::bit_count[s.children];
            s.offset_a[s.keyPos] = 7
                    - GenTree::last_bit_offset[s.children | s.leaves];
        }
        byte mask = x01 << s.offset_a[s.keyPos];
        if (s.leaves & mask) {
            s.leaves &= ~mask;
            return s.t
                    + (GenTree::bit_count[s.orig_leaves
                            & ryte_mask[s.offset_a[s.keyPos]]] << 1);
        }
        if (s.children & mask) {
            s.t = trie + s.tp[s.keyPos] + 3;
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
                s.orig_children = s.children = *s.t++;
                s.orig_leaves = s.leaves = *s.t++;
                s.t += GenTree::bit_count[s.children];
            } else {
                s.t += (GenTree::bit_count[s.orig_leaves] << 1);
                s.offset_a[s.keyPos] = 0x09;
                break;
            }
        }
    } while (1); // (s.t - trie) < TRIE_LEN);
    return 0;
}

byte *bfos_node_handler::split(int16_t *pbrk_idx, byte *first_key,
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
    char ins_key[40];
    int16_t ins_key_len;
    bfos_iterator_status s(trie);
    for (idx = 0; idx < orig_filled_size; idx++) {
        byte *ptr = nextPtr(s);
        int16_t src_idx = util::getInt(ptr);
        int16_t kv_len = buf[src_idx];
        ins_key_len = kv_len;
        kv_len++;
        memcpy(ins_key + s.keyPos, buf + src_idx, kv_len);
        ins_block->value_len = buf[src_idx + kv_len];
        kv_len++;
        ins_block->value = (const char *) buf + src_idx + kv_len;
        kv_len += ins_block->value_len;
        tot_len += kv_len;
        ins_key_len += s.keyPos + 1;
        for (int i = 0; i <= s.keyPos; i++)
            ins_key[i] = (trie[s.tp[i]] & xF8) | s.offset_a[i];
        ins_block->key = ins_key;
        ins_block->key_len = ins_key_len;
        if (idx && brk_idx >= 0)
            ins_block->locate(0);
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
            if (tot_len > halfKVLen) {
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
    int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
    setKVLastPos(kv_last_pos);
    util::setInt(trie + ptr, kv_last_pos);
    buf[kv_last_pos] = key_left;
    if (key_left)
        memcpy(buf + kv_last_pos + 1, key + keyPos, key_left);
    buf[kv_last_pos + key_left + 1] = value_len;
    memcpy(buf + kv_last_pos + key_left + 2, value, value_len);
    setFilledSize(filledSize() + 1);

}

bool bfos_node_handler::isLeaf() { // ok
    return IS_LEAF_BYTE;
}

void bfos_node_handler::setLeaf(char isLeaf) { // ok
    IS_LEAF_BYTE = isLeaf;
}

void bfos_node_handler::setFilledSize(int16_t filledSize) { // ok
    util::setInt(FILLED_SIZE, filledSize);
}

bool bfos_node_handler::isFull(int16_t kv_len) { // ok
    if ((getKVLastPos() - kv_len - 2) < (BFOS_HDR_SIZE + TRIE_LEN + need_count))
        return true;
    if (TRIE_LEN > 240)
        return true;
    return false;
}

int16_t bfos_node_handler::filledSize() { // ok
    return util::getInt(FILLED_SIZE);
}

byte *bfos_node_handler::getChild(int16_t pos) { // ok
    byte *idx = getData(pos, &pos);
    unsigned long addr_num = util::fourBytesToPtr(idx);
    byte *ret = (byte *) addr_num;
    return ret;
}

byte *bfos_node_handler::getKey(int16_t ptr, int16_t *plen) { // ok
    byte *kvIdx = buf + ptr;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *bfos_node_handler::getData(int16_t ptr, int16_t *plen) { // ok
    byte *kvIdx = buf + ptr;
    *plen = kvIdx[0];
    kvIdx++;
    kvIdx += *plen;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

void bfos_node_handler::updatePtrs(byte *upto, int diff) {
    byte *t = trie;
    while (t <= upto) {
        t++;
        if (t > upto)
            break;
        int count = GenTree::bit_count[*t++];
        byte leaves = *t++;
        while (count--) {
            if (t < upto && (t + *t) >= upto)
                *t += diff;
            t++;
        }
        t += (GenTree::bit_count[leaves] << 1);
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
        updatePtrs(triePos, 5);
        *origPos &= xFB;
        insAt(triePos, ((key_char & xF8) | x05), 0, mask, 0, 0);
        ret = triePos - trie + 3;
        break;
    case INSERT_BEFORE:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        updatePtrs(origPos, 5);
        if (keyPos > 1 && last_child_pos)
            trie[last_child_pos] -= 5;
        insAt(origPos, ((key_char & xF8) | x01), 0, mask, 0, 0);
        ret = origPos - trie + 3;
        break;
    case INSERT_LEAF:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        leafPos = origPos + 2;
        updatePtrs(triePos, 2);
        *leafPos |= mask;
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
        childPos = origPos + 1;
        *childPos |= mask;
        childPos += GenTree::bit_count[*childPos++ & ryte_mask[key_char]];
        childPos++;
        insAt(childPos, (byte) (TRIE_LEN - (childPos - trie) + 1));
        updatePtrs(childPos, 1);
        p = keyPos;
        min = util::min(key_len, keyPos + key_at_len);
        if (p < min) {
            leafPos = origPos + 2;
            (*leafPos) &= ~mask;
            leafPos += (GenTree::bit_count[*leafPos++ & ryte_mask[key_char]]
                    << 1);
            leafPos += GenTree::bit_count[*(origPos + 1)];
            ptr = util::getInt(leafPos);
            delAt(leafPos, 2);
            updatePtrs(leafPos, -2);
        } else {
            leafPos = origPos + 2;
            leafPos += (GenTree::bit_count[*leafPos++ & ryte_mask[key_char]]
                    << 1);
            leafPos += GenTree::bit_count[*(origPos + 1)];
            ptr = util::getInt(leafPos);
            pos = leafPos - trie;
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
                append(0);
                append(x01 << (c1 & x07));
                ret = isSwapped ? ret : TRIE_LEN;
                pos = isSwapped ? TRIE_LEN : pos;
                appendPtr(isSwapped ? ptr : 0);
                append((c2 & xF8) | x05);
                append(0);
                append(x01 << (c2 & x07));
                ret = isSwapped ? TRIE_LEN : ret;
                pos = isSwapped ? pos : TRIE_LEN;
                appendPtr(isSwapped ? 0 : ptr);
                break;
            case 1:
                append((c1 & xF8) | x05);
                append(0);
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
                append(0);
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
            append(0);
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
        append(0);
        append(mask);
        ret = TRIE_LEN;
        append(0);
        append(0);
        keyPos = 1;
        break;
    }
    return ret;
}

int16_t bfos_node_handler::locate(int16_t level) {
    keyPos = 1;
    byte key_char = *key;
    byte *t = trie;
    do {
        byte trie_char, r_leaves, r_children;
        origPos = t;
        trie_char = *t++;
        r_children = *t++;
        r_leaves = *t++;
        switch ((key_char ^ trie_char) > x07 ?
                (key_char > trie_char ? 0 : 2) : 1) {
        case 0:
            last_child_pos = 0;
            t += GenTree::bit_count[r_children];
            t += (GenTree::bit_count[r_leaves] << 1);
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
            key_char &= x07;
            r_mask = x01 << key_char;
            switch (r_leaves & r_mask ?
                    (r_children & r_mask ? (keyPos == key_len ? 3 : 4) : 2) :
                    (r_children & r_mask ? (keyPos == key_len ? 0 : 1) : 0)) {
            case 0:
                if (isPut) {
                    t += GenTree::bit_count[r_children];
                    t += (GenTree::bit_count[r_leaves & ryte_mask[key_char]]
                            << 1);
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
                t += (GenTree::bit_count[r_leaves & ryte_mask[key_char]] << 1);
                int16_t ptr;
                ptr = util::getInt(t);
                key_at = (char *) getKey(ptr, &key_at_len);
                cmp = util::compare(key + keyPos, key_len - keyPos, key_at,
                        key_at_len);
                if (cmp == 0)
                    return ptr;
                if (isPut) {
                    triePos = origPos + 3
                            + GenTree::bit_count[r_children
                                    & ryte_mask[key_char]];
                    insertState = INSERT_THREAD;
                    if (cmp < 0)
                        cmp = -cmp;
                    need_count = (cmp * 4) + 10;
                }
                return -1;
            case 3:
                t += GenTree::bit_count[r_children];
                t += (GenTree::bit_count[r_leaves & ryte_mask[key_char]] << 1);
                return util::getInt(t);
            case 4:
                break;
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

byte *bfos_node_handler::getFirstPtr() {
    bfos_iterator_status s(trie);
    return nextPtr(s);
}

int16_t bfos_node_handler::findLastPtr(byte *triePos, byte *trie_thread[],
        byte *prev_sibling[], int16_t key_pos) {
    byte *t = triePos;
    while ((*t & x04) == 0) {
        prev_sibling[key_pos] = t++;
        byte children = *t++;
        t += (GenTree::bit_count[*t++] << 1);
        t += GenTree::bit_count[children];
    }
    trie_thread[key_pos] = t;
    return prevPtr(t, trie_thread, prev_sibling, 8, key_pos, false);
}

int16_t bfos_node_handler::prevPtr(byte *triePos, byte *trie_thread[],
        byte *prev_sibling[], byte offset, int key_pos, bool is_prev_lvl) {
    bfos::count++;
    if (triePos) {
        triePos++;
        byte orig_children;
        byte children = orig_children = *triePos++;
        byte leaves = *triePos++;
        children &= ryte_mask[offset];
        leaves &= (is_prev_lvl ? ryte_incl_mask[offset] : ryte_mask[offset]);
        switch (children && leaves ?
                (GenTree::first_bit_offset[leaves]
                        < GenTree::first_bit_offset[children] ? 0 : 1) :
                (children ? 1 : (leaves ? 0 : 2))) {
        case 0:
            return util::getInt(
                    triePos + GenTree::bit_count[orig_children]
                            + ((GenTree::bit_count[leaves] - 1) << 1));
        case 1:
            triePos += GenTree::bit_count[children] - 1;
            triePos += *triePos;
            return findLastPtr(triePos, trie_thread, prev_sibling, key_pos + 1);
        }
    }
    if (prev_sibling[key_pos]) {
        triePos = prev_sibling[key_pos];
        prev_sibling[key_pos] = 0;
        return prevPtr(triePos, trie_thread, prev_sibling, 8, key_pos, false);
    }
    key_pos--;
    if (key_pos > 0) {
        int16_t ret = prevPtr(trie_thread[key_pos], trie_thread, prev_sibling,
                key[key_pos - 1] & x07, key_pos, true);
        if (ret)
            return ret;
    }
    return util::getInt(getFirstPtr());
}

int16_t bfos_node_handler::locateForNode(int16_t level) {
    keyPos = 1;
    byte key_char = *key;
    byte *t = trie;
    byte *last_t = t;
    byte last_child, last_leaf;
    last_child = last_leaf = 0;
    byte *prev_sibling[MAX_KEY_PREFIX_LEN] = { 0, 0 };
    byte *trie_thread[MAX_KEY_PREFIX_LEN] = { t };
    do {
        byte trie_char, r_leaves, r_children;
        origPos = t;
        trie_char = *t++;
        r_children = *t++;
        r_leaves = *t++;
        switch ((key_char ^ trie_char) > x07 ?
                (key_char > trie_char ? 0 : 2) : 1) {
        case 0:
            last_child_pos = 0;
            t += GenTree::bit_count[r_children];
            t += (GenTree::bit_count[r_leaves] << 1);
            if (trie_char & x04) {
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_AFTER;
                    need_count = 5;
                }
                if (GenTree::first_bit_offset[r_leaves] < GenTree::first_bit_offset[r_children])
                    return ~util::getInt(t - 2);
                return ~prevPtr(origPos, trie_thread, prev_sibling, 8, keyPos,
                        false);
            }
            last_t = origPos;
            last_child = r_children;
            last_leaf = r_leaves;
            prev_sibling[keyPos] = origPos;
            continue;
        case 1:
            byte r_mask;
            trie_thread[keyPos] = origPos;
            last_child_pos = 0;
            key_char &= x07;
            r_mask = ryte_mask[key_char];
            if ((r_children | r_leaves) & r_mask) {
                last_t = origPos;
                last_child = r_children & r_mask;
                last_leaf = r_leaves & r_mask;
            }
            r_mask = x01 << key_char;
            switch (r_leaves & r_mask ?
                    (r_children & r_mask ? (keyPos == key_len ? 3 : 4) : 2) :
                    (r_children & r_mask ? (keyPos == key_len ? 0 : 1) : 0)) {
            case 0:
                if (isPut) {
                    t += GenTree::bit_count[r_children];
                    t += (GenTree::bit_count[r_leaves & ryte_mask[key_char]]
                            << 1);
                    triePos = t;
                    insertState = INSERT_LEAF;
                    need_count = 2;
                }
                if (GenTree::first_bit_offset[last_leaf] < GenTree::first_bit_offset[last_child])
                    return ~util::getInt(last_t + 3 + GenTree::bit_count[*(last_t + 1)] + ((GenTree::bit_count[last_leaf]-1)<<1));
                return ~prevPtr(origPos, trie_thread, prev_sibling, key_char,
                        keyPos, false);
            case 1:
                break;
            case 2:
                int16_t cmp;
                t += GenTree::bit_count[r_children];
                t += (GenTree::bit_count[r_leaves & ryte_mask[key_char]] << 1);
                int16_t ptr;
                ptr = util::getInt(t);
                key_at = (char *) getKey(ptr, &key_at_len);
                cmp = util::compare(key + keyPos, key_len - keyPos, key_at,
                        key_at_len);
                if (cmp == 0)
                    return ptr;
                if (cmp < 0) {
                    ptr = 0;
                }
                if (isPut) {
                    triePos = origPos + 3
                            + GenTree::bit_count[r_children
                                    & ryte_mask[key_char]];
                    insertState = INSERT_THREAD;
                    if (cmp < 0)
                        cmp = -cmp;
                    need_count = (cmp * 4) + 10;
                }
                if (ptr)
                    return ptr;
                if (GenTree::first_bit_offset[last_leaf] < GenTree::first_bit_offset[last_child])
                    return ~util::getInt(last_t + 3 + GenTree::bit_count[*(last_t + 1)] + ((GenTree::bit_count[last_leaf]-1)<<1));
                return ~prevPtr(origPos, trie_thread, prev_sibling,
                            key_char, keyPos, false);
            case 3:
                t += GenTree::bit_count[r_children];
                t += (GenTree::bit_count[r_leaves & ryte_mask[key_char]] << 1);
                return util::getInt(t);
            case 4:
                break;
            }
            r_children &= ryte_mask[key_char];
            r_mask = ryte_incl_mask[key_char];
            if ((r_children | r_leaves) & r_mask) {
                last_t = origPos;
                last_child = r_children & r_mask;
                last_leaf = r_leaves & r_mask;
            }
            t += GenTree::bit_count[r_children];
            last_child_pos = t - trie;
            t += *t;
            key_char = key[keyPos++];
            trie_thread[keyPos] = t;
            prev_sibling[keyPos] = 0;
            continue;
        case 2:
            if (isPut) {
                insertState = INSERT_BEFORE;
                need_count = 5;
            }
            if (GenTree::first_bit_offset[last_leaf] < GenTree::first_bit_offset[last_child])
                return ~util::getInt(last_t + 3 + GenTree::bit_count[*(last_t + 1)] + ((GenTree::bit_count[last_leaf]-1)<<1));
            origPos = prev_sibling[keyPos];
            prev_sibling[keyPos] = 0;
            return ~prevPtr(origPos, trie_thread, prev_sibling, 8, keyPos,
                    false);
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
byte bfos_node_handler::ryte_mask[9] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F,
        0x3F, 0x7F, 0xFF };
byte bfos_node_handler::ryte_incl_mask[8] = { 0x01, 0x03, 0x07, 0x0F, 0x1F,
        0x3F, 0x7F, 0xFF };
byte bfos::split_buf[BFOS_NODE_SIZE];
int bfos::count;
