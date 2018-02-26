#include <math.h>
#include <stdint.h>
#include "bfos.h"
#include "GenTree.h"

#define NEXT_LEVEL setBuf(getChildPtr(key_at)); \
    if (isPut) node_paths[level++] = buf; \
    if (isLeaf()) \
        return; \
    keyPos = 1; \
    key_char = *key; \
    t = trie;

char *bfos::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    bfos_node_handler node(root_data);
    byte *node_paths[7];
    node.key = key;
    node.key_len = key_len;
    if (!node.isLeaf())
        node.traverseToLeaf(node_paths);
    if (node.locate() == -1)
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
        if (!node.isLeaf())
            node.traverseToLeaf(node_paths);
        node.locate();
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

int16_t bfos_node_handler::nextPtr(bfos_iterator_status& s) {
    do {
        if (s.offset_a[s.keyPos] > x07) {
            s.tp[s.keyPos] = s.t - trie;
            s.tc = *s.t++;
            s.orig_children = s.children = (s.tc & x02 ? *s.t++ : 0);
            s.orig_leaves = s.leaves = *s.t++;
            s.t += BIT_COUNT(s.children);
            s.offset_a[s.keyPos] = util::first_bit_offset[s.children
                    | s.leaves];
        }
        byte mask = x01 << s.offset_a[s.keyPos];
        if (s.leaves & mask) {
            s.leaves &= ~mask;
            return util::getInt(s.t
                    + util::bit_count2x[s.orig_leaves
                            & ryte_mask[s.offset_a[s.keyPos]]]);
        }
        if (s.children & mask) {
            s.t = trie + s.tp[s.keyPos];
            s.t += ((*s.t & x02) == x02 ? 3 : 2);
            s.t += util::bit_count[s.orig_children
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
                s.orig_children = s.children = (s.tc & x02 ? *s.t++ : 0);
                s.orig_leaves = s.leaves = *s.t++;
                s.t += BIT_COUNT(s.children);
            } else {
                s.t += BIT_COUNT2(s.orig_leaves);
                s.offset_a[s.keyPos] = 0x09;
                break;
            }
        }
    } while (1); // (s.t - trie) < BPT_TRIE_LEN);
    return 0;
}

byte *bfos_node_handler::split(byte *first_key, int16_t *first_len_ptr) {
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
        ins_key_len += s.keyPos + 1;
        for (int i = 0; i <= s.keyPos; i++)
            ins_key[i] = (trie[s.tp[i]] & xF8) | s.offset_a[i];
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
            if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                brk_idx = idx + 1;
                brk_idx = -brk_idx;
                ins_block = &new_block;
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
    total_size = maxKeyCountLeaf = maxKeyCountNode = 0;
    numLevels = blockCountLeaf = blockCountNode = 1;
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
    if (BPT_TRIE_LEN + need_count > 240)
        return true;
    return false;
}

byte *bfos_node_handler::getChildPtr(byte *ptr) {
    ptr += (*ptr + 1);
    return (byte *) util::bytesToPtr(ptr);
}

void bfos_node_handler::delAt(byte *ptr, int16_t count) {
    BPT_TRIE_LEN -= count;
    memmove(ptr, ptr + count, trie + BPT_TRIE_LEN - ptr);
}

byte bfos_node_handler::insAt(byte *ptr, byte b) {
    memmove(ptr + 1, ptr, trie + BPT_TRIE_LEN - ptr);
    *ptr = b;
    BPT_TRIE_LEN++;
    return 1;
}

byte bfos_node_handler::insAt(byte *ptr, byte b1, byte b2) {
    memmove(ptr + 2, ptr, trie + BPT_TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr = b2;
    BPT_TRIE_LEN += 2;
    return 2;
}

byte bfos_node_handler::insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4) {
    memmove(ptr + 4, ptr, trie + BPT_TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr++ = b3;
    *ptr = b4;
    BPT_TRIE_LEN += 4;
    return 4;
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
        updatePtrs(triePos, 4);
        insAt(triePos, ((key_char & xF8) | x04), mask, 0, 0);
        ret = triePos - trie + 2;
        break;
    case INSERT_BEFORE:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        updatePtrs(origPos, 4);
        if (keyPos > 1 && last_child_pos)
            trie[last_child_pos] -= 4;
        insAt(origPos, (key_char & xF8), mask, 0, 0);
        ret = origPos - trie + 2;
        break;
    case INSERT_LEAF:
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        leafPos = origPos + ((*origPos & x02) ? 2 : 1);
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
                + BIT_COUNT(*childPos & ryte_mask[key_char]);
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
        triePos += BIT_COUNT2(*leafPos & ryte_mask[key_char]);
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
    return ret;
}

int16_t bfos_node_handler::getLastPtrOfChild(byte *triePos) {
    do {
        byte tc = *triePos++;
        byte children = (tc & x02 ? *triePos++ : 0);
        byte leaves = *triePos++;
        triePos += BIT_COUNT(children);
        if (tc & x04) {
            if (util::first_bit_mask[leaves] <= children) {
                triePos--;
                triePos += *triePos;
            } else {
                triePos += BIT_COUNT2(leaves);
                return util::getInt(triePos - 2);
            }
        } else
            triePos += BIT_COUNT2(leaves);
    } while (1);
    return -1;
}

byte *bfos_node_handler::getLastPtr(byte *last_t, byte last_off) {
    byte last_child, last_leaf, cnt;
    byte trie_char = *last_t++;
    last_child = (trie_char & x02 ? *last_t++ : 0);
    last_leaf = *last_t++;
    cnt = BIT_COUNT(last_child);
    last_child &= ryte_mask[last_off];
    last_leaf &= ryte_leaf_mask[last_off];
    if (last_child < util::first_bit_mask[last_leaf]) {
        last_t += cnt;
        last_t += BIT_COUNT2(last_leaf);
        return buf + util::getInt(last_t - 2);
    }
    last_t += BIT_COUNT(last_child);
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
            r_children = (trie_char & x02 ? *t++ : 0);
            r_leaves = *t++;
            t += BIT_COUNT(r_children);
            t += BIT_COUNT2(r_leaves);
            if (trie_char & x04) {
                if (r_children < util::first_bit_mask[r_leaves])
                    key_at = buf + util::getInt(t - 2);
                else {
                    t -= BIT_COUNT2(r_leaves);
                    t--;
                    key_at = buf + getLastPtrOfChild(t + *t);
                }
                NEXT_LEVEL;
            }
            continue;
        case 1:
            byte r_mask;
            r_children = (trie_char & x02 ? *t++ : 0);
            r_leaves = *t++;
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
                key_at = getLastPtr(last_t, last_off);
                NEXT_LEVEL
                ;
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
                t += BIT_COUNT(r_children);
                t += BIT_COUNT2(r_leaves & r_mask);
                int16_t ptr;
                ptr = util::getInt(t);
                key_at = buf + ptr;
                key_at_len = *key_at;
                key_at++;
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at, key_at_len);
                if (cmp == 0) {
                    key_at = buf + ptr;
                    NEXT_LEVEL
                    continue;
                }
                if (cmp < 0) {
                    ptr = 0;
                    if ((r_children | r_leaves) & r_mask) {
                        last_t = origPos;
                        last_off = key_char;
                    }
                }
                key_at = ptr ? buf + ptr : getLastPtr(last_t, last_off);
                NEXT_LEVEL
                continue;
            case 3:
                t += BIT_COUNT(r_children);
                t += BIT_COUNT2(r_leaves & ryte_mask[key_char]);
                key_at = buf + util::getInt(t);
                NEXT_LEVEL
                continue;
            case 4:
                last_t = origPos;
                last_off = key_char + 9;
                break;
            }
            t += BIT_COUNT(r_children & ryte_mask[key_char]);
            t += *t;
            key_char = key[keyPos++];
            continue;
        case 2:
            key_at = getLastPtr(last_t, last_off);
            NEXT_LEVEL
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
            r_children = (trie_char & x02 ? *t++ : 0);
            r_leaves = *t++;
            t += BIT_COUNT(r_children);
            t += BIT_COUNT2(r_leaves);
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
            r_children = (trie_char & x02 ? *t++ : 0);
            r_leaves = *t++;
            key_char &= x07;
            r_mask = x01 << key_char;
            switch (r_leaves & r_mask ?
                    (r_children & r_mask ? (keyPos == key_len ? 3 : 1) : 2) :
                    (r_children & r_mask ? (keyPos == key_len ? 0 : 1) : 0)) {
            case 0:
                if (isPut) {
                    t += BIT_COUNT(r_children);
                    t += BIT_COUNT2(r_leaves & ryte_mask[key_char]);
                    triePos = t;
                    insertState = INSERT_LEAF;
                    need_count = 2;
                }
                return -1;
            case 1:
                break;
            case 2:
                int16_t cmp;
                t += BIT_COUNT(r_children);
                t += BIT_COUNT2(r_leaves & ryte_mask[key_char]);
                int16_t ptr;
                ptr = util::getInt(t);
                key_at = buf + ptr;
                key_at_len = *key_at;
                key_at++;
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at, key_at_len);
                if (cmp == 0) {
                    key_at = buf + ptr;
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
                t += BIT_COUNT(r_children);
                t += BIT_COUNT2(r_leaves & ryte_mask[key_char]);
                key_at = buf + util::getInt(t);
                return 1;
            }
            t += BIT_COUNT(r_children & ryte_mask[key_char]);
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

char *bfos_node_handler::getValueAt(int16_t *vlen) {
    key_at += *key_at;
    key_at++;
    *vlen = (int16_t) *key_at++;
    return (char *) key_at;
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
