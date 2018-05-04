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

byte *bfos_node_handler::getChildPtr(byte *ptr) {
    ptr += (*ptr + 1);
    return (byte *) util::bytesToPtr(ptr);
}

byte *bfos_node_handler::getLastPtr() {
    //keyPos = 0;
    do {
        int rslt = (last_child > last_leaf ? 2 : (last_leaf ^ last_child) > last_child ? 1 : 2);
        switch (rslt) {
        case 1:
            last_t += (*last_t & x02 ? BIT_COUNT(last_t[1]) + 1 : 0);
            last_t += BIT_COUNT2(last_leaf);
            return buf + util::getInt(last_t);
        case 2:
            last_t += BIT_COUNT(last_child);
            last_t += 2;
            last_t += *last_t;
            while (*last_t & x01) {
                last_t += (*last_t >> 1);
                last_t++;
            }
            while (!(*last_t & x04)) {
                last_t += (*last_t & x02 ? BIT_COUNT(last_t[1])
                     + BIT_COUNT2(last_t[2]) + 3 : BIT_COUNT2(last_t[1]) + 2);
            }
            last_child = (*last_t & x02 ? last_t[1] : 0);
            last_leaf = (*last_t & x02 ? last_t[2] : last_t[1]);
        }
    } while (1);
    return 0;
}

int16_t bfos_node_handler::traverseToLeaf(byte *node_paths[]) {
    //keyPos = 0;
    while (!isLeaf()) {
        if (node_paths)
            *node_paths++ = buf;
        locate();
        setBuf(getChildPtr(last_t));
    }
    return locate();
}

int16_t bfos_node_handler::locate() {
    byte *t = trie;
    byte trie_char = *t;
    origPos = t++;
    //if (keyPos) {
    //    if (trie_char & x01) {
    //        keyPos--;
    //        byte pfx_len = trie_char >> 1;
    //        if (keyPos < pfx_len) {
    //            trie_char = ((pfx_len - keyPos) << 1) + 1;
    //            t += keyPos;
    //        } else {
    //            keyPos = pfx_len;
    //            t += pfx_len;
    //            trie_char = *t;
    //            origPos = t++;
    //        }
    //    } else
    //        keyPos = 0;
    //}
    byte key_char = *key; //[keyPos++];
    keyPos = 1;
    last_t = 0;
    do {
#if BS_MIDDLE_PREFIX == 1
        switch (trie_char & x01 ? 3 : (key_char ^ trie_char) > x07
                ? (key_char > trie_char ? 0 : 2) : 1) {
#else
        switch ((key_char ^ trie_char) > x07 ?
                (key_char > trie_char ? 0 : 2) : 1) {
#endif
        case 0:
            last_t = origPos;
            last_child = (trie_char & x02 ? *t++ : 0);
            last_leaf = *t++;
            t += BIT_COUNT(last_child);
            t += BIT_COUNT2(last_leaf);
            if (trie_char & x04) {
                if (!isLeaf())
                    last_t = getLastPtr();
                if (isPut) {
                    insertState = INSERT_AFTER;
                    need_count = 4;
                }
                return -1;
            }
            break;
        case 1:
            byte r_mask, r_leaves, r_children;
            r_children = (trie_char & x02 ? *t++ : 0);
            r_leaves = *t++;
            key_char &= x07;
            r_mask = x01 << key_char;
            //trie_char = (r_leaves & r_mask ? x02 : x00) |
            //        ((r_children & r_mask) && keyPos != key_len ? x01 : x00);
            trie_char = r_leaves & r_mask ?
                    (r_children & r_mask ? (keyPos == key_len ? x02 : x03) : x02) :
                    (r_children & r_mask ? (keyPos == key_len ? x00 : x01) : x00);
            r_mask--;
            if (!isLeaf() && ((r_children | r_leaves) & r_mask)) {
                last_t = origPos;
                last_child = r_children & r_mask;
                last_leaf = r_leaves & r_mask;
            }
            switch (trie_char) {
            case 0:
                if (isPut) {
                   insertState = INSERT_LEAF;
                   need_count = 2;
                }
                if (!isLeaf())
                    last_t = getLastPtr();
                return -1;
            case 1:
                break;
            case 2:
                int16_t cmp;
                t += BIT_COUNT(r_children);
                t += BIT_COUNT2(r_leaves & r_mask);
                key_at = buf + util::getInt(t);
                key_at_len = *key_at++;
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at, key_at_len);
                if (cmp == 0) {
                    last_t = --key_at;
                    return 1;
                }
                if (cmp < 0) {
                    cmp = -cmp;
                    if (!isLeaf())
                        last_t = getLastPtr();
                } else
                    last_t = key_at - 1;
                if (isPut) {
                    insertState = INSERT_THREAD;
#if BS_MIDDLE_PREFIX == 1
                    need_count = cmp + 10;
#else
                    need_count = (cmp * 4) + 10;
#endif
                }
                return -1;
            case 3:
                if (!isLeaf()) {
                    last_t = origPos;
                    last_child = r_children & r_mask;
                    last_leaf = r_leaves & r_mask;
                    last_leaf |= (x01 << key_char);
                }
                break;
            }
            t += BIT_COUNT(r_children & r_mask);
            t += *t;
            key_char = key[keyPos++];
            break;
        case 2:
            if (!isLeaf())
                last_t = getLastPtr();
            if (isPut) {
                insertState = INSERT_BEFORE;
                need_count = 4;
            }
            return -1;
#if BS_MIDDLE_PREFIX == 1
        case 3:
            byte pfx_len;
            pfx_len = (trie_char >> 1);
            while (pfx_len && key_char == *t && keyPos < key_len) {
                key_char = key[keyPos++];
                t++;
                pfx_len--;
            }
            if (!pfx_len)
                break;
            triePos = t;
            if (!isLeaf()) {
                setPrefixLast(key_char, t, pfx_len);
                last_t = getLastPtr();
            }
            if (isPut) {
                insertState = INSERT_CONVERT;
                need_count = 8;
            }
            return -1;
#endif
        }
        trie_char = *t;
        origPos = t++;
    } while (1);
    return -1;
}

void bfos_node_handler::setPrefixLast(byte key_char, byte *t, byte pfx_rem_len) {
    if (key_char > *t) {
        t += pfx_rem_len;
        while (!(*t & x04)) {
            t += (*t & x02 ? BIT_COUNT(t[1])
                 + BIT_COUNT2(t[2]) + 3 : BIT_COUNT2(t[1]) + 2);
        }
        last_t = t;
        last_child = (*t & x02 ? t[1] : 0);
        last_leaf = (*t & x02 ? t[2] : t[1]);
    } else {
        if (!last_t) {
            last_t = t + pfx_rem_len;
            last_child = (*t & x02 ? t[1] : 0);
            last_child &= (last_child ^ (last_child - 1));
            last_leaf = (*t & x02 ? t[2] : t[1]);
            last_leaf &= (last_leaf ^ (last_leaf - 1));
        }
    }
}

char *bfos_node_handler::getValueAt(int16_t *vlen) {
    key_at += *key_at;
    key_at++;
    *vlen = (int16_t) *key_at++;
    return (char *) key_at;
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
    insertState = INSERT_EMPTY;
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
                new_block.keyPos = 0;
                //byte is_leaf = new_block.isLeaf();
                //new_block.setLeaf(true);
                idx = ~new_block.locate();
                //new_block.addData();
                //new_block.setLeaf(is_leaf);
            } else {
                node->initVars();
                //byte is_leaf = node->isLeaf();
                //node->setLeaf(true);
                idx = ~node->locate();
                //node->addData();
                //node->setLeaf(is_leaf);
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
                root.keyPos = 1;
                root.insertState = INSERT_EMPTY;
                root.addData();
                root.initVars();
                root.key = (const char *) first_key;
                root.key_len = first_len;
                root.value = (char *) addr;
                root.value_len = util::ptrToBytes((unsigned long) new_block.buf, addr);
                //root.keyPos = 0;
                //root.setLeaf(true);
                root.locate();
                //root.setLeaf(false);
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
                //parent.keyPos = 0;
                //parent.setLeaf(true);
                parent.locate();
                //parent.setLeaf(false);
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

void bfos_node_handler::markTrieByte(int16_t brk_idx, byte *new_t, byte *t) {
    if (brk_idx == 0 || brk_idx < 0) {
        if (new_t[t - trie] != 0)
            new_t[t - trie] = 1;
    } else {
        if (new_t[t - trie] < 2)
            new_t[t - trie] = 0;
        else
            new_t[t - trie] = 2;
    }
}

void bfos_node_handler::markTrieByteUp(int16_t brk_idx, byte *new_t, byte *t) {
    if (brk_idx > 0 && new_t[t - trie] < 2)
        new_t[t - trie] = 0;
}

void bfos_node_handler::markTrieByteLeaf(int16_t brk_idx, byte *new_t, byte *t) {
    new_t[t - trie] = (brk_idx == 0 ? 1 : 2);
}

byte *bfos_node_handler::nextPtr(byte *first_key, byte *tp, byte **t_ptr, byte& ctr,
        byte& tc, byte& child, byte& leaf, int16_t brk_idx, byte *new_t) {
    byte only_leaf = 1;
    if (ctr > 15) {
        ctr -= 16;
        only_leaf = 0;
    }
    do {
        while (ctr == x08) {
            if (tc & x04) {
                if (keyPos == 0)
                    return 0;
                do {
                    keyPos--;
                    *t_ptr = trie + tp[keyPos];
                    tc = *(*t_ptr);
                    markTrieByteUp(brk_idx, new_t, (*t_ptr)++);
                } while (tc & x01);
                ctr = first_key[keyPos] & x07;
                if (tc & x02) {
                    child = *(*t_ptr);
                    markTrieByteUp(brk_idx, new_t, (*t_ptr)++);
                    markTrieByteUp(brk_idx, new_t, (*t_ptr) + 1
                            + BIT_COUNT(child & ((x01 << ctr) - 1)));
                } else
                    child = 0;
                leaf = *(*t_ptr);
                markTrieByteUp(brk_idx, new_t, (*t_ptr)++);
                ctr++;
                *t_ptr += BIT_COUNT(child);
            } else {
                *t_ptr += BIT_COUNT2(leaf);
                ctr = 0x09;
                break;
            }
        }
        if (ctr > x07) {
            tp[keyPos] = *t_ptr - trie;
            tc = *(*t_ptr);
            markTrieByte(brk_idx, new_t, (*t_ptr)++);
            if (tc & x01) {
                byte len = tc >> 1;
                memset(tp + keyPos, *t_ptr - trie - 1, len);
                memcpy(first_key + keyPos, *t_ptr, len);
                keyPos += len;
                while (len--)
                    markTrieByte(brk_idx, new_t, (*t_ptr)++);
                tp[keyPos] = *t_ptr - trie;
                tc = *(*t_ptr)++;
            }
            if (tc & x02) {
                child = *(*t_ptr);
                markTrieByte(brk_idx, new_t, (*t_ptr)++);
            } else
                child = 0;
            leaf = *(*t_ptr);
            markTrieByte(brk_idx, new_t, (*t_ptr)++);
            *t_ptr += BIT_COUNT(child);
            ctr = 0; //util::first_bit_offset[child | leaf];
        }
        first_key[keyPos] = (tc & xF8) | ctr;
        byte mask = x01 << ctr;
        if ((leaf & mask) && only_leaf) {
            byte *ret = *t_ptr + BIT_COUNT2(leaf & (mask - 1));
            if (brk_idx < 0 && ((child | leaf) & (mask - 1))) {
                byte *t = trie + tp[keyPos];
                markTrieByteUp(brk_idx, new_t, t);
                if (*t & x02)
                    markTrieByteUp(brk_idx, new_t, ++t);
                markTrieByteUp(brk_idx, new_t, ++t);
            }
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
            markTrieByte(brk_idx, new_t, *t_ptr);
            (*t_ptr) += **t_ptr;
            keyPos++;
            ctr = 0x09;
            tc = 0;
        }
        ctr++;
    } while (1); //(*t_ptr - trie) < BPT_TRIE_LEN);
    return 0;
}

byte *bfos_node_handler::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t orig_filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
    bfos_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    new_block.BX_MAX_KEY_LEN = BX_MAX_KEY_LEN;
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = BFOS_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int16_t brk_idx;
    int16_t tot_len;
    int16_t brk_kv_pos;
    brk_idx = brk_kv_pos = tot_len = 0;
    // (1) move all data to new_block in order
    int16_t idx;
    byte ctr = 9;
    byte last_key[BX_MAX_KEY_LEN + 1];
    byte last_key_len, first_key_len;
    byte tp[BX_MAX_KEY_LEN + 1];
    byte *t = trie;
    byte **t_ptr = &t;
    byte tc, child, leaf;
    tc = child = leaf = 0;
    //if (!isLeaf())
    //   cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
    keyPos = 0;
    memset(new_block.trie, 3, BPT_TRIE_LEN);
    new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
    for (idx = 0; idx < orig_filled_size; idx++) {
        byte *leaf_ptr = nextPtr(first_key, tp, t_ptr, ctr,
                tc, child, leaf, brk_idx, new_block.trie);
        int16_t src_idx = util::getInt(leaf_ptr);
        int16_t kv_len = buf[src_idx];
        kv_len++;
        kv_len += buf[src_idx + kv_len];
        kv_len++;
        tot_len += kv_len;
        memcpy(new_block.buf + kv_last_pos, buf + src_idx, kv_len);
        util::setInt(leaf_ptr, kv_last_pos);
        util::setInt(new_block.trie + (leaf_ptr - trie), kv_last_pos);
        memcpy(first_key + keyPos + 1, buf + src_idx + 1, buf[src_idx]);
        first_key[keyPos+1+buf[src_idx]] = 0;
        cout << first_key << endl;
        markTrieByteLeaf(brk_idx, new_block.trie, leaf_ptr);
        markTrieByteLeaf(brk_idx, new_block.trie, leaf_ptr + 1);
        if (brk_idx < 0) {
            brk_idx = -brk_idx;
            keyPos++;
            first_key_len = keyPos;
            if (isLeaf()) {
                //*first_len_ptr = s.keyPos;
                *first_len_ptr = util::compare((const char *) first_key, keyPos,
                        (const char *) last_key, last_key_len);
            } else {
                memcpy(first_key + keyPos + 1, buf + src_idx + 1, buf[src_idx]);
                *first_len_ptr = keyPos + 1 + buf[src_idx];
            }
            first_key[*first_len_ptr] = 0;
            cout << first_key << endl;
            keyPos--;
        }
        kv_last_pos += kv_len;
        if (brk_idx == 0) {
            //brk_key_len = nextKey(s);
            //if (tot_len > halfKVLen) {
            if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                //memcpy(first_key + keyPos + 1, buf + src_idx + 1, buf[src_idx]);
                //first_key[keyPos+1+buf[src_idx]] = 0;
                //cout << first_key << ":";
                brk_idx = idx + 1;
                brk_idx = -brk_idx;
                brk_kv_pos = kv_last_pos;
                last_key_len = keyPos + 1;
                memcpy(last_key, first_key, last_key_len);
            }
        }
    }
    nextPtr(first_key, tp, t_ptr, ctr, tc, child, leaf, brk_idx, new_block.trie);
    deleteTrieParts(new_block, last_key, last_key_len - 1, first_key, first_key_len - 1);
    kv_last_pos = getKVLastPos() + BFOS_NODE_SIZE - kv_last_pos;
    new_block.setKVLastPos(kv_last_pos);
    memmove(new_block.buf + kv_last_pos, new_block.buf + getKVLastPos(), BFOS_NODE_SIZE - kv_last_pos);
    int16_t diff = (kv_last_pos - getKVLastPos());
    brk_kv_pos += diff;
    new_block.setPtrDiff(diff);
    diff = BFOS_NODE_SIZE - brk_kv_pos;
    setPtrDiff(diff);

    {
        int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
        memcpy(buf + BFOS_NODE_SIZE - old_blk_new_len,
                new_block.buf + kv_last_pos, old_blk_new_len); // Copy back first half to old block
        setKVLastPos(BFOS_NODE_SIZE - old_blk_new_len);
        setFilledSize(brk_idx);
    }

    {
        int16_t new_size = orig_filled_size - brk_idx;
        new_block.setKVLastPos(brk_kv_pos);
        new_block.setFilledSize(new_size);
    }

    if (!isLeaf())
        new_block.setLeaf(false);

    consolidateInitialPrefix(buf);
    new_block.consolidateInitialPrefix(new_block.buf);

    //keyPos = 0;

    return new_block.buf;

}

void bfos_node_handler::updatePtrsAfterDelete(byte *upto, int diff) {
    byte *t = trie;
    byte tc = *t++;
    while (t <= upto) {
        if (tc & x01) {
            t += (tc >> 1);
            tc = *t++;
            continue;
        }
        int count = (tc & x02 ? BIT_COUNT(*t++) : 0);
        byte leaves = *t++;
        while (count--) {
            if (t < upto && (t + *t) >= upto) {
                if ((t + *t) < (upto - diff))
                    *t += (t + *t - upto);
                else
                    *t += diff;
            }
            t++;
        }
        t += BIT_COUNT2(leaves);
        tc = *t++;
    }
}

int16_t bfos_node_handler::deleteTrieSegment(byte *from, byte trie_idx) {
    int16_t diff = trie + trie_idx - from;
    memmove(from, trie + trie_idx, BPT_TRIE_LEN - trie_idx);
    diff = trie + trie_idx - from;
    BPT_TRIE_LEN -= diff;
    updatePtrsAfterDelete(from, -diff);
    return diff;
}

void bfos_node_handler::deleteTrieParts(bfos_node_handler& new_block,
        byte *last_key, int16_t last_key_len, byte *first_key, int16_t first_key_len) {
    byte old_trie_idx, new_trie_idx, idx, eq_skip_count;
    byte *old_delete_start, *new_delete_start;
    old_trie_idx = new_trie_idx = idx = eq_skip_count = 0;
    old_delete_start = new_delete_start = null;
    do {
        switch (new_block.trie[new_trie_idx]) {
        case 0:
            new_block.trie[new_trie_idx] = trie[old_trie_idx];
            if (eq_skip_count) {
                eq_skip_count--;
                old_trie_idx++;
                new_trie_idx++;
            } else {
                byte tc, offset;
                tc = trie[old_trie_idx++];
                offset = last_key[idx] & x07;
                trie[old_trie_idx] |= x04;
                if (tc & x02) {
                    eq_skip_count = 1;
                    new_block.trie[new_trie_idx + 1] = trie[old_trie_idx];
                    new_block.trie[new_trie_idx + 2] = trie[old_trie_idx + 1];
                    trie[old_trie_idx++] &= ~((idx == last_key_len ? xFF : xFE) << offset);
                } else
                    new_block.trie[new_trie_idx + 1] = trie[old_trie_idx];
                trie[old_trie_idx++] &= ~(xFE << offset);
                tc = new_block.trie[new_trie_idx++];
                offset = first_key[idx] & x07;
                if (tc & x02)
                    new_block.trie[new_trie_idx++] &= (xFF << offset);
                new_block.trie[new_trie_idx] &= ((idx == first_key_len ? xFF : xFE) << offset);
                idx++;
            }
            continue;
        case 1:
            new_block.trie[new_trie_idx] = trie[old_trie_idx];
            if (new_delete_start == null)
                new_delete_start = new_block.trie + new_trie_idx;
            break;
        case 2:
            new_block.trie[new_trie_idx] = trie[old_trie_idx];
            if (old_delete_start == null)
                old_delete_start = trie + old_trie_idx;
        }
        old_trie_idx++;
        new_trie_idx++;
        if (old_delete_start != null && (old_trie_idx == BPT_TRIE_LEN
                || new_block.trie[new_trie_idx] == 0 || new_block.trie[new_trie_idx] == 1)) {
            if (old_trie_idx <= BPT_TRIE_LEN)
                old_trie_idx -= deleteTrieSegment(old_delete_start, old_trie_idx);
            old_delete_start = null;
        }
        if (new_delete_start != null && (new_trie_idx == new_block.BPT_TRIE_LEN
                || new_block.trie[new_trie_idx] == 0 || new_block.trie[new_trie_idx] == 2)) {
            if (new_trie_idx <= new_block.BPT_TRIE_LEN)
                new_trie_idx -= new_block.deleteTrieSegment(new_delete_start, new_trie_idx);
            new_delete_start = null;
        }
    } while (old_trie_idx < BPT_TRIE_LEN || new_trie_idx < new_block.BPT_TRIE_LEN);
}

void bfos_node_handler::setPtrDiff(int16_t diff) {
    byte *t = trie;
    byte *t_end = trie + BPT_TRIE_LEN;
    while (t < t_end) {
        byte tc = *t++;
        if (tc & x01) {
            t += (tc >> 1);
            continue;
        }
        byte leaves = 0;
        if (tc & x02) {
            leaves = t[1];
            t += BIT_COUNT(*t);
            t += 2;
        } else
            leaves = *t++;
        for (int i = 0; i < BIT_COUNT(leaves); i++) {
            util::setInt(t, util::getInt(t) + diff);
            t += 2;
        }
    }
}

void bfos_node_handler::consolidateInitialPrefix(byte *t) {
    t += BFOS_HDR_SIZE;
    byte *t1 = t;
    if (*t & x01) {
        t1 += (*t >> 1);
        t1++;
    }
    byte *t2 = t1 + (*t & x01 ? 0 : 1);
    byte count = 0;
    byte trie_len_diff = 0;
    while ((*t1 & x01) || ((*t1 & x02) && (*t1 & x04) && BIT_COUNT(t1[1]) == 1
            && BIT_COUNT(t1[2]) == 0 && t1[3] == 1)) {
        if (*t1 & x01) {
            byte len = *t1++ >> 1;
            memcpy(t2, t1, len);
            t2 += len;
            t1 += len;
            count += len;
            trie_len_diff++;
        } else {
            *t2++ = (*t1 & xF8) + BIT_COUNT(t1[1] - 1);
            t1 += 4;
            count++;
            trie_len_diff += 3;
        }
    }
    if (t1 > t2) {
        memmove(t2, t1, BPT_TRIE_LEN - (t1 - t));
        if (*t & x01) {
            *t = (((*t >> 1) + count) << 1) + 1;
        } else {
            *t = (count << 1) + 1;
            trie_len_diff--;
        }
        BPT_TRIE_LEN -= trie_len_diff;
        //cout << (int) (*t >> 1) << endl;
    }
}

void bfos_node_handler::updatePtrs(byte *upto, int diff) {
    byte *t = trie;
    byte tc = *t++;
    while (t <= upto) {
        if (tc & x01) {
            t += (tc >> 1);
            tc = *t++;
            continue;
        }
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
    int16_t diff;
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
        triePos = leafPos + BIT_COUNT(*origPos & x02 ? origPos[1] : 0) + BIT_COUNT2(*leafPos & (mask - 1)) + 1;
        *leafPos |= mask;
        updatePtrs(triePos, 2);
        insAt(triePos, x00, x00);
        ret = triePos - trie;
        break;
#if BS_MIDDLE_PREFIX == 1
    case INSERT_CONVERT:
        byte b, c;
        char cmp_rel;
        key_char = key[keyPos - 1];
        diff = triePos - origPos;
        // 3 possible relationships between key_char and *triePos, 4 possible positions of triePos
        c = *triePos;
        cmp_rel = ((c ^ key_char) > x07 ? (c < key_char ? 0 : 1) : 2);
        if (diff == 1)
            triePos = origPos;
        b = (cmp_rel == 2 ? x04 : x00) | (cmp_rel == 1 ? x00 : x02);
        need_count = (*origPos >> 1) - diff;
        diff--;
        *triePos++ = ((cmp_rel == 0 ? c : key_char) & xF8) | b;
        b = (cmp_rel == 2 ? 4 : 6);
        if (diff) {
            b++;
            *origPos = (diff << 1) | x01;
        }
        if (need_count)
            b++;
        insBytes(triePos, b);
        updatePtrs(triePos, b);
        *triePos++ = 1 << ((cmp_rel == 1 ? key_char : c) & x07);
        switch (cmp_rel) {
        case 0:
            *triePos++ = 0;
            *triePos++ = 5;
            *triePos++ = (key_char & xF8) | 0x04;
            *triePos++ = 1 << (key_char & x07);
            ret = triePos - trie;
            triePos += 2;
            break;
        case 1:
            ret = triePos - trie;
            triePos += 2;
            *triePos++ = (c & xF8) | x06;
            *triePos++ = 1 << (c & x07);
            *triePos++ = 0;
            *triePos++ = 1;
            break;
        case 2:
            *triePos++ = 1 << (key_char & x07);
            *triePos++ = 3;
            ret = triePos - trie;
            triePos += 2;
            break;
        }
        if (need_count)
            *triePos = (need_count << 1) | x01;
        break;
#endif
    case INSERT_THREAD:
        int16_t p, min;
        byte c1, c2;
        byte *childPos;
        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        c1 = c2 = key_char;
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
#if BS_MIDDLE_PREFIX == 1
        need_count -= 11;
        diff = p + need_count;
        if (diff == min) {
            if (need_count) {
                need_count--;
                diff--;
            }
        }
        diff = need_count + (need_count ? 1 : 0)
                + (diff == min ? 4 : (key[diff] ^ key_at[diff - keyPos]) > x07 ? 6 :
                        (key[diff] == key_at[diff - keyPos] ? 8 : 4));
        if (need_count) {
            append((need_count << 1) | x01);
            append(key + keyPos, need_count);
            p += need_count;
            //dfox::count1 += need_count;
        }
#endif
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
#if DX_MIDDLE_PREFIX == 1
            switch ((c1 ^ c2) > x07 ? 0 : (c1 == c2 ? 3 : 1)) {
#else
            switch ((c1 ^ c2) > x07 ?
                    0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
            case 2:
                append((c1 & xF8) | x06);
                append(x01 << (c1 & x07));
                append(0);
                append(1);
                break;
#endif
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

void bfos_node_handler::initVars() {
}

byte bfos::split_buf[BFOS_NODE_SIZE];
int bfos::count1, bfos::count2;
