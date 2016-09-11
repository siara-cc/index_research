#include <iostream>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include "bft.h"

char *bft::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    bft_node_handler node(root_data);
    byte *node_paths[10];
    node.key = key;
    node.key_len = key_len;
    if (!node.isLeaf())
        node.traverseToLeaf(node_paths);
    if (node.locateKeyInLeaf() == -1)
        return null;
    return (char *) node.getData(node.keyFoundAt - node.buf, pValueLen);
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
        node.addData(0);
        total_size++;
    } else {
        if (!node.isLeaf())
            node.traverseToLeaf(node_paths);
        node.locateKeyInLeaf();
        recursiveUpdate(&node, -1, node_paths, numLevels - 1);
    }
}

void bft::recursiveUpdate(bft_node_handler *node, int16_t pos,
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
            char first_key[40];
            int16_t first_len;
            byte *b = node->split(&brk_idx, first_key, &first_len);
            bft_node_handler new_block(b);
            new_block.isPut = true;
            int16_t cmp = util::compare(first_key, first_len, node->key,
                    node->key_len);
            if (cmp <= 0) {
                new_block.initVars();
                new_block.key = node->key;
                new_block.key_len = node->key_len;
                new_block.value = node->value;
                new_block.value_len = node->value_len;
                idx = ~new_block.locateKeyInLeaf();
                new_block.addData(idx);
            } else {
                node->initVars();
                idx = ~node->locateKeyInLeaf();
                node->addData(idx);
            }
            if (node->isLeaf())
                blockCount++;
            if (root_data == node->buf) {
                //blockCount++;
                root_data = (byte *) util::alignedAlloc(BFT_NODE_SIZE);
                bft_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                root.setLeaf(0);
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
                root.key = first_key;
                root.key_len = first_len;
                root.value = addr;
                //root.value_len = sizeof(char *);
                root.locateKeyInLeaf();
                root.addData(1);
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                bft_node_handler parent(parent_data);
                char addr[5];
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                parent.initVars();
                parent.isPut = true;
                parent.key = first_key;
                parent.key_len = first_len;
                parent.value = addr;
                parent.value_len = sizeof(char *);
                parent.locateKeyInLeaf();
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

byte *bft_node_handler::nextPtr(bft_iterator_status& s) {
    if (s.is_child_pending) {
        s.keyPos++;
        s.t += s.is_child_pending;
    } else if (s.is_next) {
        while (*s.t & x80) {
            s.keyPos--;
            s.t = trie + s.tp[s.keyPos];
        }
        s.t += (*s.t & x01 ? 4 : 2);
    } else
        s.is_next = 1;
    do {
        s.tp[s.keyPos] = s.t - trie;
        s.is_child_pending = *s.t & 0x7E;
        if (*s.t & x01)
            return s.t + 1;
        else {
            s.keyPos++;
            s.t += s.is_child_pending;
        }
    } while (1); // (s.t - trie) < TRIE_LEN);
    return 0;
}

byte *bft_node_handler::split(int16_t *pbrk_idx, char *first_key,
        int16_t *first_len_ptr) {
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
    byte ins_key[40];
    int16_t ins_key_len;
    bft_iterator_status s(trie);
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
            ins_key[i] = trie[s.tp[i] - 1];
        ins_block->key = (char *) ins_key;
        ins_block->key_len = ins_key_len;
        if (idx && brk_idx >= 0)
            ins_block->locateKeyInLeaf();
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
    memcpy(buf, old_block.buf, BFT_NODE_SIZE);

    *pbrk_idx = brk_idx;
    return new_block.buf;
}

bft::bft() {
    root_data = (byte *) util::alignedAlloc(BFT_NODE_SIZE);
    bft_node_handler root(root_data);
    root.initBuf();
    total_size = maxKeyCount = 0;
    numLevels = blockCount = 1;
    maxThread = 9999;
}

bft::~bft() {
    delete root_data;
}

bft_node_handler::bft_node_handler(byte * m) {
    setBuf(m);
    isPut = false;
}

void bft_node_handler::initBuf() {
    //memset(buf, '\0', BFT_NODE_SIZE);
    setLeaf(1);
    setFilledSize(0);
    TRIE_LEN = 0;
    keyPos = 1;
    insertState = INSERT_EMPTY;
    setKVLastPos(BFT_NODE_SIZE);
    trie = buf + BFT_HDR_SIZE;
}

void bft_node_handler::setBuf(byte *m) {
    buf = m;
    trie = buf + BFT_HDR_SIZE;
    insertState = INSERT_EMPTY;
}

void bft_node_handler::setKVLastPos(int16_t val) {
    util::setInt(LAST_DATA_PTR, val);
}

int16_t bft_node_handler::getKVLastPos() {
    return util::getInt(LAST_DATA_PTR);
}

void bft_node_handler::addData(int16_t pos) {

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

bool bft_node_handler::isLeaf() {
    return IS_LEAF_BYTE;
}

void bft_node_handler::setLeaf(char isLeaf) {
    IS_LEAF_BYTE = isLeaf;
}

void bft_node_handler::setFilledSize(int16_t filledSize) {
    util::setInt(FILLED_SIZE, filledSize);
}

bool bft_node_handler::isFull(int16_t kv_len) {
    if ((getKVLastPos() - kv_len - 2) < (BFT_HDR_SIZE + TRIE_LEN + need_count))
        return true;
    if (TRIE_LEN + need_count > 128)
        return true;
    return false;
}

int16_t bft_node_handler::filledSize() {
    return util::getInt(FILLED_SIZE);
}

byte *bft_node_handler::getChild(int16_t pos) {
    byte *idx = getData(pos, &pos);
    unsigned long addr_num = util::fourBytesToPtr(idx);
    byte *ret = (byte *) addr_num;
    return ret;
}

byte *bft_node_handler::getKey(int16_t ptr, int16_t *plen) {
    byte *kvIdx = buf + ptr;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *bft_node_handler::getData(int16_t ptr, int16_t *plen) {
    byte *kvIdx = buf + ptr;
    *plen = kvIdx[0];
    kvIdx++;
    kvIdx += *plen;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

void bft_node_handler::updatePtrs(byte *upto, int diff) {
    byte *t = trie + 1;
    while (t <= upto) {
        byte child = (*t & 0x7E);
        if (child && (t + child) >= upto)
            *t = (*t & 0x81) | (child + diff);
        if (*t & x01)
            t += 2;
        t += 2;
    }
}

int16_t bft_node_handler::insertCurrent() {
    byte key_char;
    byte *childPos;
    int16_t ret, ptr, pos;
    ret = pos = 0;

    switch (insertState) {
    case INSERT_AFTER:
        key_char = key[keyPos - 1];
        origPos++;
        *origPos &= 0x7F;
        updatePtrs(triePos, 4);
        insAt(triePos, key_char, 0x81, 0, 0);
        ret = triePos - trie + 2;
        break;
    case INSERT_BEFORE:
        key_char = key[keyPos - 1];
        updatePtrs(origPos, 4);
        if (keyPos > 1 && last_child_pos)
            trie[last_child_pos] -= 4;
        insAt(origPos, key_char, x01, 0, 0);
        ret = origPos - trie + 2;
        break;
    case INSERT_LEAF:
        key_char = key[keyPos - 1];
        childPos = origPos + 1;
        *childPos |= x01;
        childPos++;
        updatePtrs(childPos, 2);
        insAt(childPos, 0, 0);
        ret = childPos - trie;
        break;
    case INSERT_THREAD:
        int16_t p, min;
        byte c1, c2;
        key_char = key[keyPos - 1];
        c1 = c2 = key_char;
        childPos = origPos + 1;
        triePos = childPos + 1;
        *childPos |= (TRIE_LEN - (triePos - trie) + 2);
        p = keyPos;
        min = util::min(key_len, keyPos + key_at_len);
        ptr = util::getInt(triePos);
        if (p < min) {
            (*childPos) &= 0xFE;
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
            switch (c1 == c2 ? (p + 1 == min ? 2 : 1) : 0) {
            case 0:
                append(c1);
                append(x01);
                ret = isSwapped ? ret : TRIE_LEN;
                pos = isSwapped ? TRIE_LEN : pos;
                appendPtr(isSwapped ? ptr : 0);
                append(c2);
                append(0x81);
                ret = isSwapped ? TRIE_LEN : ret;
                pos = isSwapped ? pos : TRIE_LEN;
                appendPtr(isSwapped ? 0 : ptr);
                break;
            case 1:
                append(c1);
                append(0x82);
                break;
            case 2:
                append(c1);
                append(0x85);
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
            append(c2);
            append(0x81);
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
        append(key_char);
        append(0x81);
        ret = TRIE_LEN;
        append(0);
        append(0);
        keyPos = 1;
        break;
    }
    return ret;
}

byte *bft_node_handler::getFirstPtr() {
    bft_iterator_status s(trie);
    return nextPtr(s); //, 0, 0);
}

int16_t bft_node_handler::getLastPtrOfChild(byte *triePos) {
    do {
        byte children = (0x7E & *triePos);
        if (*triePos & x80) {
            if (children)
                triePos += children;
            else
                return util::getInt(triePos + 1);
        } else {
            if (*triePos & x01)
                triePos += 4;
            else
                triePos += 2;
        }
    } while (1);
    return -1;
}

byte *bft_node_handler::getLastPtr(byte *last_t) {
    byte last_child;
    last_t++;
    last_child = (0x7E & *last_t++);
    if (!last_child || last_child_pos)
        return buf + util::getInt(last_t);
    return buf + getLastPtrOfChild(last_t + last_child - 1);
}

void bft_node_handler::traverseToLeaf(byte *node_paths[]) {
    keyPos = 1;
    byte level;
    byte key_char = *key;
    byte *t = trie;
    byte *last_t = t;
    level = keyPos = 1;
    *node_paths = buf;
    last_child_pos = 0;
    do {
        byte trie_char;
        origPos = t;
        trie_char = *t++;
        if (key_char > trie_char) {
            last_t = origPos;
            last_child_pos = 0;
            if (*t & 0x80) {
                byte r_children = (*t & 0x7E);
                if (r_children)
                    keyFoundAt = buf + getLastPtrOfChild(t + r_children);
                else
                    keyFoundAt = buf + util::getInt(t + 1);
                keyFoundAt += (*keyFoundAt + 2);
                setBuf((byte *) util::fourBytesToPtr(keyFoundAt));
                node_paths[level++] = buf;
                if (isLeaf())
                    return;
                keyPos = 1;
                key_char = *key;
                t = trie;
            } else {
                if (*t++ & x01)
                    t += 2;
            }
        } else if (key_char == trie_char) {
            byte r_children = *t++;
            switch (r_children & x01 ?
                    (r_children & 0x7E ? (keyPos == key_len ? 3 : 1) : 2) :
                    (r_children & 0x7E ? (keyPos == key_len ? 0 : 4) : 0)) {
            case 2:
                int16_t cmp;
                int16_t ptr;
                ptr = util::getInt(t);
                key_at = (char *) buf + ptr;
                key_at_len = *key_at;
                key_at++;
                cmp = util::compare(key + keyPos, key_len - keyPos, key_at,
                        key_at_len);
                if (cmp == 0) {
                    keyFoundAt = (byte *) key_at + key_at_len + 1;
                    setBuf((byte *) util::fourBytesToPtr(keyFoundAt));
                    node_paths[level++] = buf;
                    if (isLeaf())
                        return;
                    keyPos = 1;
                    key_char = *key;
                    t = trie;
                    continue;
                }
                if (cmp < 0)
                    ptr = 0;
                keyFoundAt = ptr ? buf + ptr : getLastPtr(last_t);
                keyFoundAt += (*keyFoundAt + 2);
                setBuf((byte *) util::fourBytesToPtr(keyFoundAt));
                node_paths[level++] = buf;
                if (isLeaf())
                    return;
                keyPos = 1;
                key_char = *key;
                t = trie;
                continue;
            case 1:
                last_t = origPos;
                last_child_pos = 1;
                break;
            case 0:
                keyFoundAt = getLastPtr(last_t);
                keyFoundAt += (*keyFoundAt + 2);
                setBuf((byte *) util::fourBytesToPtr(keyFoundAt));
                if (node_paths)
                    node_paths[level++] = buf;
                if (isLeaf())
                    return;
                keyPos = 1;
                key_char = *key;
                t = trie;
                continue;
            case 3:
                keyFoundAt = buf + util::getInt(t);
                keyFoundAt += (*keyFoundAt + 2);
                setBuf((byte *) util::fourBytesToPtr(keyFoundAt));
                node_paths[level++] = buf;
                if (isLeaf())
                    return;
                keyPos = 1;
                key_char = *key;
                t = trie;
                continue;
            }
            t += (r_children & 0x7E);
            t -= 2;
            key_char = key[keyPos++];
            continue;
        } else {
            keyFoundAt = getLastPtr(last_t);
            keyFoundAt += (*keyFoundAt + 2);
            setBuf((byte *) util::fourBytesToPtr(keyFoundAt));
            node_paths[level++] = buf;
            if (isLeaf())
                return;
            keyPos = 1;
            key_char = *key;
            t = trie;
            continue;
        }
    } while (1);
}

int16_t bft_node_handler::locateKeyInLeaf() {
    keyPos = 1;
    byte key_char = *key;
    byte *t = trie;
    do {
        byte trie_char, r_children;
        origPos = t;
        trie_char = *t++;
        if (key_char > trie_char) {
            last_child_pos = 0;
            r_children = *t++;
            if (r_children & x01)
                t += 2;
            if (r_children & 0x80) {
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_AFTER;
                    need_count = 4;
                }
                return -1;
            }
        } else if (key_char == trie_char) {
            last_child_pos = 0;
            r_children = *t++;
            switch (r_children & x01 ?
                    (r_children & 0x7E ? (keyPos == key_len ? 3 : 1) : 2) :
                    (r_children & 0x7E ? (keyPos == key_len ? 0 : 1) : 0)) {
            case 2:
                int16_t cmp;
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
                    need_count = (cmp * 2) + 8;
                }
                return -1;
            case 1:
                break;
            case 0:
                if (isPut) {
                    triePos = t;
                    insertState = INSERT_LEAF;
                    need_count = 2;
                }
                return -1;
            case 3:
                keyFoundAt = buf + util::getInt(t);
                return 1;
            }
            t--;
            last_child_pos = t - trie;
            t += (r_children & 0x7E);
            t--;
            key_char = key[keyPos++];
        } else {
            if (isPut) {
                insertState = INSERT_BEFORE;
                need_count = 4;
            }
            return -1;
        }
    } while (1);
    return -1;
}

void bft_node_handler::delAt(byte *ptr) {
    TRIE_LEN--;
    memmove(ptr, ptr + 1, trie + TRIE_LEN - ptr);
}

void bft_node_handler::delAt(byte *ptr, int16_t count) {
    TRIE_LEN -= count;
    memmove(ptr, ptr + count, trie + TRIE_LEN - ptr);
}

void bft_node_handler::insAt(byte *ptr, byte b) {
    memmove(ptr + 1, ptr, trie + TRIE_LEN - ptr);
    *ptr = b;
    TRIE_LEN++;
}

byte bft_node_handler::insAt(byte *ptr, byte b1, byte b2) {
    memmove(ptr + 2, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr = b2;
    TRIE_LEN += 2;
    return 2;
}

byte bft_node_handler::insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4) {
    memmove(ptr + 4, ptr, trie + TRIE_LEN - ptr);
    *ptr++ = b1;
    *ptr++ = b2;
    *ptr++ = b3;
    *ptr = b4;
    TRIE_LEN += 4;
    return 4;
}

byte bft_node_handler::insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4,
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

void bft_node_handler::setAt(byte pos, byte b) {
    trie[pos] = b;
}

void bft_node_handler::append(byte b) {
    trie[TRIE_LEN++] = b;
}

void bft_node_handler::appendPtr(int16_t p) {
    util::setInt(trie + TRIE_LEN, p);
    TRIE_LEN += 2;
}

byte bft_node_handler::getAt(byte pos) {
    return trie[pos];
}

void bft_node_handler::initVars() {
}

byte bft::split_buf[BFT_NODE_SIZE];
int bft::count1, bft::count2;
