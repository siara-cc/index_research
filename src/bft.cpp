#include <math.h>
#include "bft.h"

#define NEXT_LEVEL setBuf(getChildPtr(key_at)); \
    if (isPut) node_paths[level++] = buf; \
    if (isLeaf()) \
        return locate(); \
    keyPos = BFT_PREFIX_LEN; \
    key_char = key[keyPos++]; \
    t = trie;

char *bft::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    bft_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    if ((node.isLeaf() ? node.locate() : node.traverseToLeaf()) < 0)
        return null;
    return node.getValueAt(pValueLen);
}

void bft::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    byte *node_paths[10];
    bft_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    node.value = value;
    node.value_len = value_len;
    node.isPut = true;
    if (node.filledSize() == 0) {
        node.addData();
        total_size++;
    } else {
        if (node.isLeaf())
            node.locate();
        else
            node.traverseToLeaf(node_paths);
        recursiveUpdate(&node, -1, node_paths, numLevels - 1);
    }
}

void bft::recursiveUpdate(bplus_tree_node_handler *node, int16_t pos,
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
            if (node->isLeaf()) {
                maxKeyCountLeaf += node->filledSize();
                blockCountLeaf++;
            } else {
                maxKeyCountNode += node->filledSize();
                blockCountNode++;
            }
                //maxKeyCount += node->BPT_TRIE_LEN;
            //maxKeyCount += node->PREFIX_LEN;
            byte first_key[64];
            int16_t first_len;
            byte *b = node->split(first_key, &first_len);
            bft_node_handler new_block(b);
            new_block.isPut = true;
            int16_t cmp = util::compare((char *) first_key, first_len,
                    node->key, node->key_len);
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
                root_data = (byte *) util::alignedAlloc(BFT_NODE_SIZE);
                bft_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                root.setLeaf(0);
                byte addr[9];
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = (char *) addr;
                root.value_len = util::ptrToBytes((unsigned long) node->buf, addr);
                root.addData();
                root.initVars();
                root.key = (char *) first_key;
                root.key_len = first_len;
                root.value = (char *) addr;
                root.value_len = util::ptrToBytes((unsigned long) new_block.buf, addr);
                root.locate();
                root.addData();
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                bft_node_handler parent(parent_data);
                byte addr[9];
                parent.initVars();
                parent.isPut = true;
                parent.key = (char *) first_key;
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

int16_t bft_node_handler::nextPtr(bft_iterator_status& s) {
    if (s.is_child_pending) {
        s.keyPos++;
        s.t += (s.is_child_pending * BFT_UNIT_SIZE);
    } else if (s.is_next) {
        while (*s.t & (BFT_UNIT_SIZE == 3 ? x40 : x80)) {
            s.keyPos--;
            s.t = trie + s.tp[s.keyPos];
        }
        s.t += BFT_UNIT_SIZE;
    } else
        s.is_next = 1;
    do {
        s.tp[s.keyPos] = s.t - trie;
#if BFT_UNIT_SIZE == 3
        s.is_child_pending = *s.t & x3F;
        int16_t leaf_ptr = get9bitPtr(s.t);
#else
        s.is_child_pending = *s.t & x7F;
        int16_t leaf_ptr = util::getInt(s.t + 1);
#endif
        if (leaf_ptr)
            return leaf_ptr;
        else {
            s.keyPos++;
            s.t += (s.is_child_pending * BFT_UNIT_SIZE);
        }
    } while (1); // (s.t - trie) < BFT_TRIE_LEN);
    return 0;
}

byte *bft_node_handler::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t orig_filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(BFT_NODE_SIZE);
    bft_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    if (!isLeaf())
        new_block.setLeaf(0);
    bft_node_handler old_block(bft::split_buf);
    old_block.initBuf();
    old_block.isPut = true;
    if (!isLeaf())
        old_block.setLeaf(0);
    bft_node_handler *ins_block = &old_block;
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = BFT_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int16_t brk_idx = 0;
    int16_t tot_len = 0;
    // (1) move all data to new_block in order
    int16_t idx;
    byte ins_key[64], old_first_key[64];
    int16_t ins_key_len, old_first_len;
    bft_iterator_status s(trie, BFT_PREFIX_LEN);
    for (idx = 0; idx < orig_filled_size; idx++) {
        int16_t src_idx = nextPtr(s);
        int16_t kv_len = buf[src_idx];
        ins_key_len = kv_len;
        kv_len++;
        memcpy(ins_key + s.keyPos, buf + src_idx, kv_len);
        ins_block->value_len = buf[src_idx + kv_len];
        kv_len++;
        ins_block->value = (const char *) buf + src_idx + kv_len;
        kv_len += ins_block->value_len;
        tot_len += kv_len;
        for (int i = BFT_PREFIX_LEN; i <= s.keyPos; i++)
            ins_key[i] = trie[s.tp[i] - 1];
        for (int i = 0; i < BFT_PREFIX_LEN; i++)
            ins_key[i] = key[i];
        ins_key_len += s.keyPos;
        ins_key_len++;
        if (idx == 0) {
            memcpy(old_first_key, ins_key, ins_key_len);
            old_first_len = ins_key_len;
        }
        ins_block->key = (char *) ins_key;
        ins_block->key_len = ins_key_len;
        if (idx && brk_idx >= 0)
            ins_block->locate();
        ins_block->addData();
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
            //if (tot_len > halfKVLen) {
            if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                brk_idx = idx + 1;
                brk_idx = -brk_idx;
                ins_block = &new_block;
            }
        }
    }
    memcpy(buf, old_block.buf, BFT_NODE_SIZE);

    /*
    if (first_key[0] - old_first_key[0] < 2) {
        int16_t len_to_chk = util::min(old_first_len, *first_len_ptr);
        int16_t prefix = 0;
        while (prefix < len_to_chk) {
            if (old_first_key[prefix] != first_key[prefix]) {
                if (first_key[prefix] - old_first_key[prefix] > 1)
                    prefix--;
                break;
            }
            prefix++;
        }
        if (BFT_PREFIX_LEN) {
            new_block.deletePrefix(BFT_PREFIX_LEN);
            new_block.BFT_PREFIX_LEN = BFT_PREFIX_LEN;
        }
        prefix -= deletePrefix(prefix);
        BFT_PREFIX_LEN = prefix;
    }*/

    return new_block.buf;
}

int16_t bft_node_handler::deletePrefix(int16_t prefix_len) {
    byte *t = trie;
    while (prefix_len--) {
        byte *delete_start = t++;
#if BFT_UNIT_SIZE == 3
        if (get9bitPtr(t)) {
#else
        if (util::getInt(t)) {
#endif
            prefix_len++;
            return prefix_len;
        }
#if BFT_UNIT_SIZE == 3
        t += (*t & x3F);
        BFT_TRIE_LEN -= 3;
        memmove(delete_start, delete_start + 3, BFT_TRIE_LEN);
        t -= 3;
#else
        t += (*t & x7F);
        BFT_TRIE_LEN -= 4;
        memmove(delete_start, delete_start + 4, BFT_TRIE_LEN);
        t -= 4;
#endif
    }
    return prefix_len + 1;
}

bft::bft() {
    root_data = (byte *) util::alignedAlloc(BFT_NODE_SIZE);
    bft_node_handler root(root_data);
    root.initBuf();
    total_size = maxKeyCountLeaf = maxKeyCountNode = 0;
    numLevels = blockCountLeaf = blockCountNode = 1;
    maxThread = 9999;
}

bft::~bft() {
    delete root_data;
}

bft_node_handler::bft_node_handler(byte * m) {
    setBuf(m);
    insertState = INSERT_EMPTY;
    isPut = false;
}

void bft_node_handler::initBuf() {
    //memset(buf, '\0', BFT_NODE_SIZE);
    setLeaf(1);
    setFilledSize(0);
    BFT_TRIE_LEN = 0;
    BFT_PREFIX_LEN = 0;
    keyPos = 1;
    insertState = INSERT_EMPTY;
    setKVLastPos(BFT_NODE_SIZE);
    trie = buf + BFT_HDR_SIZE;
}

void bft_node_handler::setBuf(byte *m) {
    buf = m;
    trie = m + BFT_HDR_SIZE;
}

void bft_node_handler::addData() {

    int16_t ptr = insertCurrent();

    int16_t key_left = key_len - keyPos;
    int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
    setKVLastPos(kv_last_pos);
#if BFT_UNIT_SIZE == 3
    trie[ptr--] = kv_last_pos;
    if (kv_last_pos & x100)
        trie[ptr] |= x80;
    else
        trie[ptr] &= x7F;
#else
    util::setInt(trie + ptr, kv_last_pos);
#endif
    buf[kv_last_pos] = key_left;
    if (key_left)
        memcpy(buf + kv_last_pos + 1, key + keyPos, key_left);
    buf[kv_last_pos + key_left + 1] = value_len;
    memcpy(buf + kv_last_pos + key_left + 2, value, value_len);
    setFilledSize(filledSize() + 1);

}

bool bft_node_handler::isFull(int16_t kv_len) {
#if BFT_UNIT_SIZE == 3
    if ((BFT_TRIE_LEN + need_count) > 189) {
        //if ((origPos - trie) <= (72 + need_count)) {
            return true;
        //}
    }
#endif
    if ((getKVLastPos() - kv_len - 2)
            < (BFT_HDR_SIZE + BFT_TRIE_LEN + need_count)) {
        return true;
    }
    if (BFT_TRIE_LEN + need_count > 240)
        return true;
    return false;
}

byte *bft_node_handler::getChildPtr(byte *ptr) {
    ptr += (*ptr + 1);
    return (byte *) util::bytesToPtr(ptr);
}

void bft_node_handler::updatePtrs(byte *upto, int diff) {
    byte *t = trie + 1;
    while (t <= upto) {
#if BFT_UNIT_SIZE == 3
        byte child = (*t & x3F);
        if (child && (t + child * 3) >= upto)
            *t += diff;
        t += 3;
#else
        byte child = (*t & x7F);
        if (child && (t + child * 4) >= upto)
            *t += diff;
        t += 4;
#endif
    }
}

int16_t bft_node_handler::insertCurrent() {
    byte key_char;
    int16_t ret, ptr, pos;
    ret = pos = 0;

    switch (insertState) {
    case INSERT_AFTER:
        key_char = key[keyPos - 1];
        origPos++;
        *origPos &= (BFT_UNIT_SIZE == 3 ? xBF : x7F);
        updatePtrs(triePos, 1);
#if BFT_UNIT_SIZE == 3
        insAt(triePos, key_char, x40, 0);
#else
        insAt(triePos, key_char, x80, 0, 0);
#endif
        ret = triePos - trie + 2;
        break;
    case INSERT_BEFORE:
        key_char = key[keyPos - 1];
        updatePtrs(origPos, 1);
        if (keyPos > 1 && last_child_pos)
            trie[last_child_pos]--;
#if BFT_UNIT_SIZE == 3
        insAt(origPos, key_char, x00, 0);
#else
        insAt(origPos, key_char, x00, 0, 0);
#endif
        ret = origPos - trie + 2;
        break;
    case INSERT_LEAF:
        key_char = key[keyPos - 1];
        ret = origPos - trie + 2;
        break;
    case INSERT_THREAD:
        int16_t p, min;
        byte c1, c2;
        key_char = key[keyPos - 1];
        c1 = c2 = key_char;
        p = keyPos;
        key_at++;
        min = util::min16(key_len, keyPos + key_at_len);
        origPos++;
#if BFT_UNIT_SIZE == 3
        *origPos |= ((BFT_TRIE_LEN - (origPos - trie - 1)) / 3);
        ptr = *(origPos + 1);
        if (*origPos & x80)
            ptr |= x100;
#else
        *origPos |= ((BFT_TRIE_LEN - (origPos - trie - 1)) / 4);
        ptr = util::getInt(origPos + 1);
#endif
        if (p < min) {
#if BFT_UNIT_SIZE == 3
            *origPos &= x7F;
            origPos++;
            *origPos = 0;
#else
            util::setInt(origPos + 1, 0);
#endif
        } else {
            origPos++;
            pos = origPos - trie;
            ret = pos;
        }
        while (p < min) {
            byte swap = 0;
            c1 = key[p];
            c2 = key_at[p - keyPos];
            if (c1 > c2) {
                swap = c1;
                c1 = c2;
                c2 = swap;
            }
            switch (c1 == c2 ? (p + 1 == min ? 2 : 1) : 0) {
            case 0:
                append(c1);
                append(x00);
                if (swap) {
                    pos = BFT_TRIE_LEN;
                    appendPtr(ptr);
                } else {
                    ret = BFT_TRIE_LEN;
                    appendPtr(0);
                }
                append(c2);
                append(BFT_UNIT_SIZE == 3 ? x40 : x80);
                if (swap) {
                    ret = BFT_TRIE_LEN;
                    appendPtr(0);
                } else {
                    pos = BFT_TRIE_LEN;
                    appendPtr(ptr);
                }
                break;
            case 1:
                append(c1);
                append(BFT_UNIT_SIZE == 3 ? x41 : x81);
                appendPtr(0);
                break;
            case 2:
                append(c1);
                append(BFT_UNIT_SIZE == 3 ? x41 : x81);
                if (p + 1 == key_len) {
                    ret = BFT_TRIE_LEN;
                    appendPtr(0);
                } else {
                    pos = BFT_TRIE_LEN;
                    appendPtr(ptr);
                }
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
            append(c2);
            append(BFT_UNIT_SIZE == 3 ? x40 : x80);
            if (p == key_len) {
                pos = BFT_TRIE_LEN;
                appendPtr(ptr);
                keyPos--;
            } else {
                ret = BFT_TRIE_LEN;
                appendPtr(0);
            }
        }
        if (diff < key_at_len)
            diff++;
        if (diff) {
            p = ptr;
            key_at_len -= diff;
            p += diff;
            if (key_at_len >= 0) {
                buf[p] = key_at_len;
#if BFT_UNIT_SIZE == 3
                set9bitPtr(trie + pos, p);
#else
                util::setInt(trie + pos, p);
#endif
            }
        }
        break;
    case INSERT_EMPTY:
        key_char = *key;
        append(key_char);
        append(BFT_UNIT_SIZE == 3 ? x40 : x80);
        ret = BFT_TRIE_LEN;
        appendPtr(0);
        keyPos = 1;
        break;
    }
    return ret;
}

int16_t bft_node_handler::getFirstPtr() {
    bft_iterator_status s(trie, BFT_PREFIX_LEN);
    return nextPtr(s);
}

int16_t bft_node_handler::getLastPtrOfChild(byte *triePos) {
    do {
#if BFT_UNIT_SIZE == 3
        if (*triePos & x40) {
            byte children = (*triePos & x3F);
            if (children)
                triePos += (children * 3);
            else
                return get9bitPtr(triePos);
        } else
            triePos += 3;
#else
        if (*triePos & x80) {
            byte children = (*triePos & x7F);
            if (children)
                triePos += (children * 4);
            else
                return util::getInt(triePos + 1);
        } else
            triePos += 4;
#endif
    } while (1);
    return -1;
}

byte *bft_node_handler::getLastPtr(byte *last_t) {
#if BFT_UNIT_SIZE == 3
    byte last_child = (*last_t & x3F);
    if (!last_child || last_child_pos)
        return buf + get9bitPtr(last_t);
    return buf + getLastPtrOfChild(last_t + (last_child * 3));
#else
    byte last_child = (*last_t & x7F);
    if (!last_child || last_child_pos)
        return buf + util::getInt(last_t + 1);
    return buf + getLastPtrOfChild(last_t + (last_child * 4));
#endif
}

int16_t bft_node_handler::traverseToLeaf(byte *node_paths[]) {
    byte level;
    byte key_char = *key;
    byte *t, *last_t;
    t = last_t = trie;
    level = keyPos = 1;
    if (isPut)
        *node_paths = buf;
    do {
        while (key_char > *t) {
            last_t = ++t;
#if BFT_UNIT_SIZE == 3
            if (*t & x40) {
                byte r_children = *t & x3F;
                if (r_children)
                    key_at = buf + getLastPtrOfChild(t + r_children * 3);
                else
                    key_at = buf + get9bitPtr(t);
                NEXT_LEVEL
                continue;
            } else
                t += 2;
#else
            if (*t & x80) {
                byte r_children = *t & x7F;
                if (r_children)
                    key_at = buf + getLastPtrOfChild(t + r_children * 4);
                else
                    key_at = buf + util::getInt(t + 1);
                NEXT_LEVEL
            } else
                t += 3;
#endif
            last_child_pos = 0;
        }
        if (key_char == *t) {
            byte r_children;
            int16_t ptr;
            t++;
#if BFT_UNIT_SIZE == 3
            r_children = *t & x3F;
            ptr = get9bitPtr(t);
#else
            r_children = *t & x7F;
            ptr = util::getInt(t + 1);
#endif
            switch (ptr ?
                    (r_children ? (keyPos == key_len ? 3 : 1) : 2) :
                    (r_children ? (keyPos == key_len ? 0 : 4) : 0)) {
            case 2:
                key_at = buf + ptr;
                key_at_len = *key_at;
                if (util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at + 1, key_at_len) < 0) {
                    key_at = getLastPtr(last_t);
                }
                NEXT_LEVEL
                continue;
            case 0:
                key_at = getLastPtr(last_t);
                NEXT_LEVEL
                continue;
            case 1:
                last_t = t;
                last_child_pos = 1;
                break;
            case 3:
                key_at = buf + ptr;
                NEXT_LEVEL
                continue;
            }
            t += (r_children * BFT_UNIT_SIZE);
            t--;
            key_char = key[keyPos++];
            continue;
        } else {
            key_at = getLastPtr(last_t);
            NEXT_LEVEL
            continue;
        }
    } while (1);
}

int16_t bft_node_handler::locate() {
    byte *t = trie;
    keyPos = BFT_PREFIX_LEN;
    byte key_char = key[keyPos++];
    do {
        origPos = t;
        while (key_char > *t) {
            t++;
#if BFT_UNIT_SIZE == 3
            if (*t & x40) {
                t += 2;
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_AFTER;
                    need_count = 3;
                }
                return -1;
            }
            t += 2;
#else
            if (*t & x80) {
                t += 3;
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_AFTER;
                    need_count = 4;
                }
                return -1;
            }
            t += 3;
#endif
            last_child_pos = 0;
            origPos = t;
        }
        if (key_char == *t++) {
            byte r_children;
            int16_t ptr;
            last_child_pos = 0;
#if BFT_UNIT_SIZE == 3
            r_children = *t & x3F;
            ptr = get9bitPtr(t);
#else
            r_children = *t & x7F;
            ptr = util::getInt(t + 1);
#endif
            switch (ptr ?
                    (r_children ? (keyPos == key_len ? 3 : 1) : 2) :
                    (r_children ? (keyPos == key_len ? 0 : 1) : 0)) {
            case 2:
                int16_t cmp;
                key_at = buf + ptr;
                key_at_len = *key_at;
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at + 1, key_at_len);
                if (cmp == 0)
                    return ptr;
                if (isPut) {
                    insertState = INSERT_THREAD;
                    if (cmp < 0)
                        cmp = -cmp;
                    need_count = (cmp * BFT_UNIT_SIZE) + BFT_UNIT_SIZE * 2;
                }
                return -1;
            case 0:
                if (isPut) {
                    insertState = INSERT_LEAF;
                    need_count = 0;
                }
                return -1;
            case 1:
                break;
            case 3:
                key_at = buf + ptr;
                return 1;
            }
            last_child_pos = t - trie;
            t += (r_children * BFT_UNIT_SIZE);
            t--;
            key_char = key[keyPos++];
            continue;
        } else {
            if (isPut) {
                insertState = INSERT_BEFORE;
                need_count = BFT_UNIT_SIZE;
            }
            return -1;
        }
    } while (1);
    return -1;
}

char *bft_node_handler::getValueAt(int16_t *vlen) {
    key_at += *key_at;
    key_at++;
    *vlen = (int16_t) *key_at++;
    return (char *) key_at;
}

void bft_node_handler::appendPtr(int16_t p) {
#if BFT_UNIT_SIZE == 3
    trie[BFT_TRIE_LEN] = p;
    if (p & x100)
        trie[BFT_TRIE_LEN - 1] |= x80;
    else
        trie[BFT_TRIE_LEN - 1] &= x7F;
    BFT_TRIE_LEN++;
#else
    util::setInt(trie + BFT_TRIE_LEN, p);
    BFT_TRIE_LEN += 2;
#endif
}

int16_t bft_node_handler::get9bitPtr(byte *t) {
    int16_t ptr = (*t++ & x80 ? 256 : 0);
    ptr |= *t;
    return ptr;
}

void bft_node_handler::set9bitPtr(byte *t, int16_t p) {
    *t-- = p;
    if (p & x100)
        *t |= x80;
    else
        *t &= x7F;
}

void bft_node_handler::initVars() {
}

byte bft::split_buf[BFT_NODE_SIZE];
int bft::count1, bft::count2;
