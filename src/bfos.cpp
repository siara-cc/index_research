#include <math.h>
#include <stdint.h>
#include "bfos.h"
#include "GenTree.h"

char *bfos::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    bfos_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    if (node.traverseToLeaf() == -1)
        return null;
    return node.getValueAt(pValueLen);
}

void bfos::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    byte *node_paths[7];
    bfos_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    node.value = value;
    node.value_len = value_len;
    node.isPut = true;
    if (node.filledSize() == 0) {
        node.addData();
        total_size++;
    } else {
        node.traverseToLeaf(node_paths);
        recursiveUpdate(&node, -1, node_paths, numLevels - 1);
    }
}

void bfos::recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
        byte *node_paths[], int16_t level) {
    int16_t idx = pos; // lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (node->isFull(node->key_len + node->value_len)) {
            //std::cout << "Full\n" << std::endl;
            //if (maxKeyCount < block->filledSize())
            //    maxKeyCount = block->filledSize();
            //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->BPT_TRIE_LEN);
            //cout << (int) node->BPT_TRIE_LEN << endl;
            if (node->isLeaf()) {
                maxKeyCountLeaf += node->filledSize();
                blockCountLeaf++;
            } else {
                maxKeyCountNode += node->filledSize();
                blockCountNode++;
            }
            //    maxKeyCount += node->BPT_TRIE_LEN;
            //maxKeyCount += node->PREFIX_LEN;
            byte first_key[64];
            int16_t first_len;
            byte *b = node->split(first_key, &first_len);
            bfos_node_handler new_block(b);
            new_block.isPut = true;
            int16_t cmp = util::compare((char *) first_key, first_len, node->key,
                    node->key_len);
            if (cmp <= 0) {
                new_block.initVars();
                new_block.key = node->key;
                new_block.key_len = node->key_len;
                new_block.value = node->value;
                new_block.value_len = node->value_len;
                idx = ~new_block.locate();
                new_block.addData();
            } else {
                node->initVars();
                idx = ~node->locate();
                node->addData();
            }
            if (root_data == node->buf) {
                blockCountNode++;
                root_data = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
                bfos_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                root.setLeaf(false);
                byte addr[8];
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = (char *) addr;
                root.value_len = util::ptrToBytes((unsigned long) node->buf, addr);
                root.addData();
                root.initVars();
                root.key = (const char *) first_key;
                root.key_len = first_len;
                root.value = (char *) addr;
                root.value_len = util::ptrToBytes((unsigned long) new_block.buf, addr);
                root.locate();
                root.addData();
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                bfos_node_handler parent(parent_data);
                byte addr[8];
                parent.initVars();
                parent.isPut = true;
                parent.key = (const char *) first_key;
                parent.key_len = first_len;
                parent.value = (char *) addr;
                parent.value_len = util::ptrToBytes((unsigned long) new_block.buf, addr);
                parent.locate();
                recursiveUpdate(&parent, -1, node_paths, prev_level);
            }
        } else
            node->addData();
    } else {
        //if (node->isLeaf) {
        //    int16_t vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

byte *bfos_node_handler::nextPtr(byte *first_key, byte *tp, byte **t_ptr, byte& ctr, byte& tc, byte& child, byte& leaf) {
    byte only_leaf = 1;
    if (ctr > 15) {
        ctr -= 16;
        only_leaf = 0;
    }
    do {
        while (ctr == x08) {
            if (tc & x04) {
                keyPos--;
                *t_ptr = trie + tp[keyPos];
                ctr = first_key[keyPos] & x07;
                ctr++;
                tc = *(*t_ptr)++;
                child = (tc & x02 ? *(*t_ptr)++ : 0);
                leaf = *(*t_ptr)++;
                *t_ptr += BIT_COUNT(child);
            } else {
                *t_ptr += BIT_COUNT2(leaf);
                ctr = 0x09;
                break;
            }
        }
        if (ctr > x07) {
            tp[keyPos] = *t_ptr - trie;
            tc = *(*t_ptr)++;
            child = (tc & x02 ? *(*t_ptr)++ : 0);
            leaf = *(*t_ptr)++;
            *t_ptr += BIT_COUNT(child);
            ctr = 0; //util::first_bit_offset[child | leaf];
        }
        first_key[keyPos] = (tc & xF8) | ctr;
        byte mask = x01 << ctr;
        if ((leaf & mask) && only_leaf) {
            byte *ret = *t_ptr + BIT_COUNT2(leaf & (mask - 1));
            if (child & mask)
                ctr += 16;
            else
                ctr++;
            return ret;
        }
        only_leaf = 1;
        if (child & mask) {
            *t_ptr = trie + tp[keyPos] + 3;
            (*t_ptr) += BIT_COUNT(child & (mask - 1));
            (*t_ptr) += **t_ptr;
            keyPos++;
            ctr = 0x09;
        }
        ctr++;
    } while (1); // (s.t - trie) < BPT_TRIE_LEN);
    return 0;
}

byte *bfos_node_handler::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t orig_filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
    bfos_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    new_block.BX_MAX_KEY_LEN = BX_MAX_KEY_LEN;
    if (!isLeaf())
        new_block.setLeaf(false);
    bfos_node_handler old_block(bfos::split_buf);
    old_block.initBuf();
    old_block.isPut = true;
    old_block.BX_MAX_KEY_LEN = BX_MAX_KEY_LEN;
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
    byte ins_key[BX_MAX_KEY_LEN + 64];
    int16_t ins_key_len;
    byte ctr = 9;
    byte tp[BX_MAX_KEY_LEN + 1];
    byte *t = trie;
    byte **t_ptr = &t;
    byte tc, child, leaf;
    tc = child = leaf = 0;
    //if (!isLeaf())
    //   cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
    keyPos = 0;
    for (idx = 0; idx < orig_filled_size; idx++) {
        int16_t src_idx = util::getInt(nextPtr(ins_key, tp, t_ptr, ctr, tc, child, leaf));
        int16_t kv_len = buf[src_idx];
        ins_key_len = kv_len;
        kv_len++;
        memcpy(ins_key + keyPos + 1, buf + src_idx + 1, ins_key_len);
        ins_block->value_len = buf[src_idx + kv_len];
        kv_len++;
        ins_block->value = (const char *) buf + src_idx + kv_len;
        kv_len += ins_block->value_len;
        tot_len += kv_len;
        ins_key_len += keyPos + 1;
        ins_key[ins_key_len] = 0;
        ins_block->key = (char *) ins_key;
        ins_block->key_len = ins_key_len;
        if (idx && brk_idx >= 0)
            ins_block->locate();
        ins_block->addData();
        if (brk_idx < 0) {
            brk_idx = -brk_idx;
            keyPos++;
            if (isLeaf()) {
                //*first_len_ptr = s.keyPos;
                *first_len_ptr = util::compare((const char *) ins_key, keyPos,
                        (const char *) first_key, *first_len_ptr);
                memcpy(first_key, ins_key, keyPos);
            } else {
                *first_len_ptr = ins_key_len;
                memcpy(first_key, ins_key, ins_key_len);
            }
            keyPos--;
        }
        kv_last_pos += kv_len;
        if (brk_idx == 0) {
            if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                brk_idx = idx + 1;
                brk_idx = -brk_idx;
                ins_block = &new_block;
                *first_len_ptr = keyPos + 1;
                memcpy(first_key, ins_key, *first_len_ptr);
            }
        }
    }
    memcpy(buf, old_block.buf, BFOS_NODE_SIZE);

    return new_block.buf;
}

bfos::bfos() {
    root_data = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
    bfos_node_handler root(root_data);
    root.initBuf();
    total_size = maxKeyCountLeaf = maxKeyCountNode = blockCountNode = 0;
    numLevels = blockCountLeaf = 1;
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
    BPT_TRIE_LEN = 0;
    BX_MAX_KEY_LEN = 1;
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

void bfos_node_handler::addData() {

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

bool bfos_node_handler::isFull(int16_t kv_len) {
    if ((getKVLastPos() - kv_len - 2) < (BFOS_HDR_SIZE + BPT_TRIE_LEN + need_count))
        return true;
    if (BPT_TRIE_LEN + need_count > 248)
        return true;
    return false;
}

byte *bfos_node_handler::getChildPtr(byte *ptr) {
    ptr += (*ptr + 1);
    return (byte *) util::bytesToPtr(ptr);
}

void bfos_node_handler::updatePtrs(byte *upto, int diff) {
    byte *t = trie;
    byte tc = *t++;
    while (t <= upto) {
        int count = (tc & x02 ? BIT_COUNT(*t++) : 0);
        byte leaves = *t++;
        while (count--) {
            if (t < upto && (t + *t) >= upto)
                *t += diff;
            // todo: avoid inside loops
            if (insertState == INSERT_BEFORE && keyPos > 1 && (t + *t) == (origPos + 4))
                *t -= 4;
            t++;
        }
        t += BIT_COUNT2(leaves);
        tc = *t++;
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
        triePos = origPos + (*origPos & x02 ? BIT_COUNT(origPos[1])
                + BIT_COUNT2(origPos[2]) + 3 : BIT_COUNT2(origPos[1]) + 2);
        updatePtrs(triePos, 4);
        insAt(triePos, ((key_char & xF8) | x04), mask, 0, 0);
        ret = triePos - trie + 2;
        break;
    case INSERT_BEFORE:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        updatePtrs(origPos, 4);
        insAt(origPos, (key_char & xF8), mask, 0, 0);
        ret = origPos - trie + 2;
        break;
    case INSERT_LEAF:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        leafPos = origPos + ((*origPos & x02) ? 2 : 1);
        triePos = leafPos + BIT_COUNT(*origPos & x02 ? origPos[1]: 0) + BIT_COUNT2(*leafPos & (mask - 1)) + 1;
        *leafPos |= mask;
        updatePtrs(triePos, 2);
        insAt(triePos, x00, x00);
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
        if (*origPos & x02) {
            triePos = childPos + 2
                + BIT_COUNT(*childPos & (mask - 1));
            insAt(triePos, (byte) (BPT_TRIE_LEN - (triePos - trie) + 1));
            updatePtrs(triePos, 1);
            *childPos |= mask;
        } else {
            *origPos |= x02;
            insAt(childPos, mask);
            triePos = childPos + 2;
            insAt(triePos, (byte) (BPT_TRIE_LEN - (triePos - trie) + 1));
            updatePtrs(childPos, 2);
        }
        p = keyPos;
        min = util::min16(key_len, keyPos + key_at_len);
        leafPos = origPos + ((*origPos & x02) ? 2 : 1);
        triePos = leafPos + BIT_COUNT(*childPos) + 1;
        triePos += BIT_COUNT2(*leafPos & (mask - 1));
        ptr = util::getInt(triePos);
        if (p < min) {
            (*leafPos) &= ~mask;
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
                append(c1 & xF8);
                append(x01 << (c1 & x07));
                ret = isSwapped ? ret : BPT_TRIE_LEN;
                pos = isSwapped ? BPT_TRIE_LEN : pos;
                appendPtr(isSwapped ? ptr : 0);
                append((c2 & xF8) | x04);
                append(x01 << (c2 & x07));
                ret = isSwapped ? BPT_TRIE_LEN : ret;
                pos = isSwapped ? pos : BPT_TRIE_LEN;
                appendPtr(isSwapped ? 0 : ptr);
                break;
            case 1:
                append((c1 & xF8) | x04);
                append((x01 << (c1 & x07)) | (x01 << (c2 & x07)));
                ret = isSwapped ? ret : BPT_TRIE_LEN;
                pos = isSwapped ? BPT_TRIE_LEN : pos;
                appendPtr(isSwapped ? ptr : 0);
                ret = isSwapped ? BPT_TRIE_LEN : ret;
                pos = isSwapped ? pos : BPT_TRIE_LEN;
                appendPtr(isSwapped ? 0 : ptr);
                break;
            case 2:
                append((c1 & xF8) | x06);
                append(x01 << (c1 & x07));
                append(0);
                append(1);
                break;
            case 3:
                append((c1 & xF8) | x06);
                append(x01 << (c1 & x07));
                append(x01 << (c1 & x07));
                append(3);
                ret = (p + 1 == key_len) ? BPT_TRIE_LEN : ret;
                pos = (p + 1 == key_len) ? pos : BPT_TRIE_LEN;
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
            append((c2 & xF8) | x04);
            append(x01 << (c2 & x07));
            ret = (p == key_len) ? ret : BPT_TRIE_LEN;
            pos = (p == key_len) ? BPT_TRIE_LEN : pos;
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
        append((key_char & xF8) | x04);
        append(mask);
        ret = BPT_TRIE_LEN;
        append(0);
        append(0);
        keyPos = 1;
        break;
    }

    if (BX_MAX_KEY_LEN < (isLeaf() ? keyPos : key_len))
        BX_MAX_KEY_LEN = (isLeaf() ? keyPos : key_len);

    return ret;
}

byte *bfos_node_handler::getLastPtr() {
    do {
        int rslt = (last_child < last_leaf && (last_leaf ^ last_child) > last_child ? 1 : 2);
        switch (rslt) {
        case 1:
            last_t += (*last_t & x02 ? BIT_COUNT(last_t[1]) + 1 : 0);
            last_t += BIT_COUNT2(last_leaf);
            return buf + util::getInt(last_t);
        case 2:
            last_t += BIT_COUNT(last_child);
            last_t += 2;
            last_t += *last_t;
            while (!(*last_t & x04)) {
                last_t += (*last_t & x02 ? BIT_COUNT(last_t[1])
                     + BIT_COUNT2(last_t[2]) + 3 : BIT_COUNT2(last_t[1]) + 2);
            }
            last_child = (*last_t & x02 ? last_t[1] : 0);
            last_leaf = (*last_t & x02 ? last_t[2] : last_t[1]);
        }
    } while (1);
    return null;
}

int16_t bfos_node_handler::traverseToLeaf(byte *node_paths[]) {
    byte level = 1;
    if (node_paths)
        *node_paths = buf;
    while (!isLeaf()) {
        locate();
        if (insertState != INSERT_THREAD)
            last_t = getLastPtr();
        setBuf(getChildPtr(last_t));
        if (node_paths)
            node_paths[level++] = buf;
    }
    return locate();
}

#ifndef _MSC_VER
__attribute__((aligned(32)))
__attribute__((hot))
#endif
int16_t bfos_node_handler::locate() {
    keyPos = 1;
    byte key_char = *key;
    byte *t = trie;
    need_count = 5;
    do {
        byte trie_char;
        origPos = t;
        trie_char = *t++;
        switch ((key_char ^ trie_char) > x07 ?
                (key_char > trie_char ? 0 : 2) : 1) {
        case 0:
            last_t = origPos;
            last_child = (trie_char & x02 ? *t++ : 0);
            last_leaf = *t++;
            t += BIT_COUNT(last_child) + BIT_COUNT2(last_leaf);
            if (trie_char & x04) {
                insertState = INSERT_AFTER;
                return -1;
            }
            continue;
        case 1:
            byte r_mask, r_leaves, r_children;
            r_children = (trie_char & x02 ? *t++ : 0);
            r_leaves = *t++;
            r_mask = x01 << (key_char & x07);
            //trie_char = (r_leaves & r_mask ? x02 : x00) |
            //        ((r_children & r_mask) && keyPos != key_len ? x01 : x00);
            //trie_char = r_leaves & r_mask ?
            //        (r_children & r_mask && keyPos != key_len ? x03 : x02) :
            //        (r_children & r_mask && keyPos != key_len ? x01 : x00);
            trie_char = r_leaves & r_mask ?
                    (r_children & r_mask ? (keyPos == key_len ? 2 : 3) : 2) :
                    (r_children & r_mask ? (keyPos == key_len ? 0 : 1) : 0);
            r_mask--;
            if (!isLeaf() && (r_children | r_leaves) & r_mask) {
                last_t = origPos;
                last_child = r_children & r_mask;
                last_leaf = r_leaves & r_mask;
            }
            switch (trie_char) {
            case 0:
                insertState = INSERT_LEAF;
                need_count = 2;
                return -1;
            case 1:
                break;
            case 2:
                int16_t cmp;
                t += BIT_COUNT(r_children) + BIT_COUNT2(r_leaves & r_mask);
                int16_t ptr;
                ptr = util::getInt(t);
                key_at = buf + ptr;
                key_at_len = *key_at++;
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at, key_at_len);
                insertState = INSERT_THREAD;
                if (cmp == 0) {
                    last_t = --key_at;
                    return ptr;
                }
                if (cmp < 0) {
                    cmp = -cmp;
                    if (!isLeaf())
                        last_t = getLastPtr();
                } else
                    last_t = key_at - 1;
                need_count = (cmp * 4) + 10;
                return -1;
            case 3:
                if (!isLeaf()) {
                    last_t = origPos;
                    last_child = r_children & r_mask;
                    last_leaf = r_leaves & ((r_mask << 1) + 1);
                }
                break;
            }
            t += BIT_COUNT(r_children & r_mask);
            t += *t;
            key_char = key[keyPos++];
            continue;
        case 2:
            insertState = INSERT_BEFORE;
            return -1;
        }
    } while (1);
    return -1;
}

char *bfos_node_handler::getValueAt(int16_t *vlen) {
    key_at += *key_at;
    key_at++;
    *vlen = (int16_t) *key_at++;
    return (char *) key_at;
}

void bfos_node_handler::initVars() {
}

byte bfos::split_buf[BFOS_NODE_SIZE];
int bfos::count1, bfos::count2;
