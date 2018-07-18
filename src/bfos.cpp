#include <math.h>
#include <stdint.h>
#include "bfos.h"

char *bfos::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    bfos_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    if (node.traverseToLeaf() == -1)
        return null;
    return node.getValueAt(pValueLen);
}

byte *bfos_node_handler::getLastPtr() {
    //keyPos = 0;
    do {
        int rslt = (last_child > last_leaf ? 2 : (last_leaf ^ last_child) > last_child ? 1 : 2);
        switch (rslt) {
        case 1:
            return buf + util::getInt(last_t + (*last_t & x02 ? BIT_COUNT(last_t[1]) + 1 : 0)
                    + BIT_COUNT2(last_leaf));
        case 2:
            last_t += BIT_COUNT(last_child) + 2;
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
        setBuf(getChildPtr(last_t - buf < getKVLastPos() ? getLastPtr() : last_t));
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
            t += BIT_COUNT(last_child) + BIT_COUNT2(last_leaf);
            if (trie_char & x04) {
                //if (!isLeaf())
                //    last_t = getLastPtr();
                    insertState = INSERT_AFTER;
                return -1;
            }
            break;
        case 1:
            byte r_mask, r_leaves, r_children;
            r_children = (trie_char & x02 ? *t++ : 0);
            r_leaves = *t++;
            //key_char &= x07;
            r_mask = x01 << (key_char & x07);
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
                   insertState = INSERT_LEAF;
                //if (!isLeaf())
                //    last_t = getLastPtr();
                return -1;
            case 1:
                break;
            case 2:
                int16_t cmp;
                t += BIT_COUNT(r_children) + BIT_COUNT2(r_leaves & r_mask);
                key_at = buf + util::getInt(t);
                key_at_len = *key_at++;
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at, key_at_len);
                if (cmp == 0) {
                    last_t = key_at - 1;
                    return 1;
                }
                if (cmp < 0) {
                    cmp = -cmp;
                    //if (!isLeaf())
                    //    last_t = getLastPtr();
                } else
                    last_t = key_at - 1;
                    insertState = INSERT_THREAD;
#if BS_MIDDLE_PREFIX == 1
                    need_count = cmp + 7;
#else
                    need_count = (cmp * 4) + 10;
#endif
                return -1;
            case 3:
                if (!isLeaf()) {
                    last_t = origPos;
                    last_child = r_children & r_mask;
                    last_leaf = (r_leaves & r_mask) | (r_mask + 1);
                }
                break;
            }
            t += BIT_COUNT(r_children & r_mask);
            t += *t;
            key_char = key[keyPos++];
            break;
        case 2:
            //if (!isLeaf())
            //    last_t = getLastPtr();
                insertState = INSERT_BEFORE;
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
                //last_t = getLastPtr();
            }
                insertState = INSERT_CONVERT;
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
    }
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
    insertState = INSERT_EMPTY;
}

void bfos_node_handler::initBuf() {
    //memset(buf, '\0', BFOS_NODE_SIZE);
    setLeaf(1);
    setFilledSize(0);
    BPT_TRIE_LEN = 0;
    BX_MAX_PFX_LEN = 1;
    BPT_MAX_KEY_LEN = 1;
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
    decodeNeedCount();
    if (getKVLastPos() < (BFOS_HDR_SIZE + BPT_TRIE_LEN + need_count + kv_len + 3))
        return true;
    if (BPT_TRIE_LEN > 252 - need_count)
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
                maxTrieLenLeaf += node->BPT_TRIE_LEN;
                blockCountLeaf++;
            } else {
                maxKeyCountNode += node->filledSize();
                maxTrieLenNode += node->BPT_TRIE_LEN;
                blockCountNode++;
            }
            //    maxKeyCount += node->BPT_TRIE_LEN;
            //maxKeyCount += node->PREFIX_LEN;
            byte first_key[node->BPT_MAX_KEY_LEN];
            int16_t first_len;
            byte *b = node->split(first_key, &first_len);
            bfos_node_handler new_block(b);
            int16_t cmp = util::compare((char *) first_key, first_len, node->key,
                    node->key_len);
            if (cmp <= 0) {
                new_block.initVars();
                new_block.key = node->key;
                new_block.key_len = node->key_len;
                new_block.value = node->value;
                new_block.value_len = node->value_len;
                //byte is_leaf = new_block.isLeaf();
                //new_block.setLeaf(true);
                idx = ~new_block.locate();
                new_block.addData();
                //new_block.setLeaf(is_leaf);
            } else {
                node->initVars();
                //byte is_leaf = node->isLeaf();
                //node->setLeaf(true);
                idx = ~node->locate();
                node->addData();
                //node->setLeaf(is_leaf);
            }
            if (root_data == node->buf) {
                blockCountNode++;
                root_data = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
                bfos_node_handler root(root_data);
                root.initBuf();
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

byte *bfos_node_handler::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t orig_filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(BFOS_NODE_SIZE);
    bfos_node_handler new_block(b);
    new_block.initBuf();
    new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
    new_block.BX_MAX_PFX_LEN = BX_MAX_PFX_LEN;
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVPos = kv_last_pos + (BFOS_NODE_SIZE - kv_last_pos) / 2;

    int16_t brk_idx, brk_kv_pos;
    brk_idx = brk_kv_pos = 0;
    // (1) move all data to new_block in order
    int16_t idx = 0;
    byte alloc_size = BX_MAX_PFX_LEN + 1;
    byte curr_key[alloc_size];
    byte tp[alloc_size];
    byte tp_cpy[alloc_size];
    int16_t tp_cpy_len = 0;
    byte *t = new_block.trie;
    //if (!isLeaf())
    //   cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
    new_block.keyPos = 0;
    memcpy(new_block.trie, trie, BPT_TRIE_LEN);
    new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
    byte tc, leaf, child, leaf_child;
    tc = leaf = child = leaf_child = 0;
    byte ctr = 9;
    do {
        while (ctr == x08) {
            if (tc & x04) {
                new_block.keyPos--;
                t = new_block.trie + tp[new_block.keyPos];
                if (*t & x01) {
                    new_block.keyPos -= (*t >> 1);
                    t = new_block.trie + tp[new_block.keyPos];
                }
                tc = *t++;
                child = (tc & x02) ? *t++ : 0;
                leaf = *t++;
                t += BIT_COUNT(child);
                ctr = curr_key[new_block.keyPos] & x07;
                leaf_child = (leaf | child) & (xFE << ctr);
                ctr = leaf_child ? FIRST_BIT_OFFSET_FROM_RIGHT(leaf_child) : 8;
            } else {
                t += BIT_COUNT2(leaf);
                ctr = 0x09;
                break;
            }
        }
        if (ctr > x07) {
            tp[new_block.keyPos] = t - new_block.trie;
            tc = *t++;
            if (tc & x01) {
                byte len = tc >> 1;
                memset(tp + new_block.keyPos, t - new_block.trie - 1, len);
                memcpy(curr_key + new_block.keyPos, t, len);
                t += len;
                new_block.keyPos += len;
                tp[new_block.keyPos] = t - new_block.trie;
                tc = *t++;
            }
            child = (tc & x02) ? *t++ : 0;
            leaf = *t++;
            t += BIT_COUNT(child);
            leaf_child = leaf | child;
            ctr = FIRST_BIT_OFFSET_FROM_RIGHT(leaf_child);
        }
        curr_key[new_block.keyPos] = (tc & xF8) | ctr;
        byte mask = x01 << ctr;
        if (leaf & mask) {
            byte *leaf_ptr = t + BIT_COUNT2(leaf & (mask - 1));
            int16_t src_idx = util::getInt(leaf_ptr);
            int16_t kv_len = buf[src_idx];
            kv_len++;
            kv_len += buf[src_idx + kv_len];
            kv_len++;
            memcpy(new_block.buf + kv_last_pos, buf + src_idx, kv_len);
            util::setInt(leaf_ptr, kv_last_pos);
            //memcpy(curr_key + new_block.keyPos + 1, buf + src_idx + 1, buf[src_idx]);
            //curr_key[new_block.keyPos+1+buf[src_idx]] = 0;
            //cout << curr_key << endl;
            if (brk_idx < 0) {
                brk_idx = -brk_idx;
                new_block.keyPos++;
                tp_cpy_len = new_block.keyPos;
                if (isLeaf()) {
                    //*first_len_ptr = s.keyPos;
                    *first_len_ptr = util::compare((const char *) curr_key, new_block.keyPos,
                            (const char *) first_key, *first_len_ptr);
                    memcpy(first_key, curr_key, tp_cpy_len);
                } else {
                    memcpy(first_key, curr_key, new_block.keyPos);
                    memcpy(first_key + new_block.keyPos, buf + src_idx + 1, buf[src_idx]);
                    *first_len_ptr = new_block.keyPos + buf[src_idx];
                }
                memcpy(tp_cpy, tp, tp_cpy_len);
                //curr_key[new_block.keyPos] = 0;
                //cout << "Middle:" << curr_key << endl;
                new_block.keyPos--;
            }
            kv_last_pos += kv_len;
            if (brk_idx == 0) {
                //brk_key_len = nextKey(s);
                //if (kv_last_pos > halfKVLen) {
                if (kv_last_pos > halfKVPos || idx == (orig_filled_size / 2)) {
                    //memcpy(first_key + keyPos + 1, buf + src_idx + 1, buf[src_idx]);
                    //first_key[keyPos+1+buf[src_idx]] = 0;
                    //cout << first_key << ":";
                    brk_idx = idx + 1;
                    brk_idx = -brk_idx;
                    brk_kv_pos = kv_last_pos;
                    *first_len_ptr = new_block.keyPos + 1;
                    memcpy(first_key, curr_key, *first_len_ptr);
                    BPT_TRIE_LEN = new_block.copyTrieHalf(tp, first_key, *first_len_ptr, trie, 1);
                }
            }
            idx++;
            if (idx == orig_filled_size)
                break;
        }
        if (child & mask) {
            t -= BIT_COUNT(child & (xFF << ctr));
            byte child_offset = *t;
            *t = t - new_block.trie + child_offset;
            t += child_offset;
            new_block.keyPos++;
            ctr = 9;
            continue;
        }
        leaf_child &= ~mask;
        ctr = leaf_child ? FIRST_BIT_OFFSET_FROM_RIGHT(leaf_child) : 8;
    } while (1);
    new_block.BPT_TRIE_LEN = new_block.copyTrieHalf(tp_cpy, first_key, tp_cpy_len, trie + BPT_TRIE_LEN, 2);
    memcpy(new_block.trie, trie + BPT_TRIE_LEN, new_block.BPT_TRIE_LEN);

    kv_last_pos = getKVLastPos() + BFOS_NODE_SIZE - kv_last_pos;
    int16_t diff = (kv_last_pos - getKVLastPos());

    {
        memmove(new_block.buf + brk_kv_pos + diff, new_block.buf + brk_kv_pos,
                BFOS_NODE_SIZE - brk_kv_pos - diff);
        brk_kv_pos += diff;
        new_block.setPtrDiff(diff);
        new_block.setKVLastPos(brk_kv_pos);
        new_block.setFilledSize(orig_filled_size - brk_idx);
    }

    {
        int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
        memcpy(buf + BFOS_NODE_SIZE - old_blk_new_len,
                new_block.buf + kv_last_pos - diff, old_blk_new_len); // Copy back first half to old block
        diff += (BFOS_NODE_SIZE - brk_kv_pos);
        setPtrDiff(diff);
        setKVLastPos(BFOS_NODE_SIZE - old_blk_new_len);
        setFilledSize(brk_idx);
    }

    if (!isLeaf())
        new_block.setLeaf(false);

    consolidateInitialPrefix(buf);
    new_block.consolidateInitialPrefix(new_block.buf);

    //keyPos = 0;

    return new_block.buf;

}

byte bfos_node_handler::copyKary(byte *t, byte *dest, int lvl, byte *tp,
        byte *brk_key, int16_t brk_key_len, byte whichHalf) {
    byte *orig_dest = dest;
    if (*t & x01) {
        byte len = (*t >> 1) + 1;
        memcpy(dest, t, len);
        dest += len;
        t += len;
    }
    byte *dest_after_prefix = dest;
    byte *limit = trie + (lvl < brk_key_len ? tp[lvl] : 0);
    byte tc;
    byte is_limit = 0;
    do {
        is_limit = (limit == t ? 1 : 0); // && is_limit == 0 ? 1 : 0);
        if (limit == t && whichHalf == 2)
            dest = dest_after_prefix;
        tc = *t;
        if (is_limit) {
            *dest++ = tc;
            byte offset = (lvl < brk_key_len ? brk_key[lvl] & x07 : x07);
            t++;
            byte children = (tc & x02) ? *t++ : 0;
            byte orig_children = children;
            byte leaves = *t++;
            byte orig_leaves = leaves;
            if (whichHalf == 1) {
                children &= ~(((brk_key_len - lvl) == 1 ? xFF : xFE) << offset);
                leaves &= ~(xFE << offset);
                *(dest - 1) |= x04;
                if (tc & x02)
                    *dest++ = children;
                *dest++ = leaves;
                for (int i = 0; i < BIT_COUNT(children); i++)
                    *dest++ = t[i];
                t += BIT_COUNT(orig_children);
                memcpy(dest, t, BIT_COUNT2(leaves));
                dest += BIT_COUNT2(leaves);
                break;
            } else {
                children &= (xFF << offset);
                leaves &= (((brk_key_len - lvl) == 1 ? xFF : xFE) << offset);
                if (tc & x02)
                    *dest++ = children;
                *dest++ = leaves;
                for (int i = BIT_COUNT(orig_children - children); i < BIT_COUNT(orig_children); i++)
                    *dest++ = t[i];
                t += BIT_COUNT(orig_children);
                memcpy(dest, t + BIT_COUNT2(orig_leaves - leaves), BIT_COUNT2(leaves));
                dest += BIT_COUNT2(leaves);
            }
            t += BIT_COUNT2(orig_leaves);
        } else {
            byte len = (tc & x02) ? 3 + BIT_COUNT(t[1]) + BIT_COUNT2(t[2])
                    : 2 + BIT_COUNT2(t[1]);
            memcpy(dest, t, len);
            t += len;
            dest += len;
        }
    } while (!(tc & x04));
    return dest - orig_dest;
}

byte bfos_node_handler::copyTrieHalf(byte *tp, byte *brk_key, int16_t brk_key_len, byte *dest, byte whichHalf) {
    byte *d;
    byte *t = trie;
    byte *new_trie = dest;
    byte tp_child[BX_MAX_PFX_LEN];
    byte child_num[BX_MAX_PFX_LEN];
    int lvl = 0;
    if (*t & x01) {
        byte len = (*t >> 1);
        memset(tp_child, dest - new_trie, len);
        lvl = len;
    }
    byte last_len = copyKary(t, dest, lvl, tp, brk_key, brk_key_len, whichHalf);
    d = dest;
    dest += last_len;
    do {
        byte tc = *d++;
        if (tc & x01) {
            d += (tc >> 1);
            tc = *d++;
        }
        if (tc & x02) {
            byte len = BIT_COUNT(*d++);
            if (len) {
                d++;
                tp_child[lvl] = d - new_trie - 3;
                byte *child = trie + *d;
                *d = dest - d;
                child_num[lvl++] = 0;
                t = child;
                if (*t & x01) {
                    byte len = (*t >> 1);
                    memset(tp_child + lvl, dest - new_trie, len);
                    lvl += len;
                }
                last_len = copyKary(t, dest, lvl, tp, brk_key, brk_key_len, whichHalf);
                d = dest;
                dest += last_len;
                continue;
            } else
                d += (BIT_COUNT2(*d) + 1);
        } else
            d += (BIT_COUNT2(*d) + 1);
        while (tc & x04) {
            do {
                lvl--;
                d = new_trie + tp_child[lvl];
                tc = *d;
            } while (tc & x01);
            if (lvl < 0)
                return dest - new_trie;
            d = new_trie + tp_child[lvl];
            byte len = BIT_COUNT(d[1]);
            byte i = child_num[lvl];
            i++;
            if (i < len) {
                d += (i + 3);
                byte *child = trie + *d;
                *d = dest - d;
                child_num[lvl++] = i;
                t = child;
                if (*t & x01) {
                    byte len = (*t >> 1);
                    memset(tp_child + lvl, dest - new_trie, len);
                    lvl += len;
                }
                last_len = copyKary(t, dest, lvl, tp, brk_key, brk_key_len, whichHalf);
                d = dest;
                dest += last_len;
                break;
            }
            tc = *d;
            if (!(tc & x04)) {
                d += (tc & x02 ? 3 + BIT_COUNT(d[1]) + BIT_COUNT2(d[2]) :
                        2 + BIT_COUNT2(d[1]));
                break;
            }
        }
    } while (1);
    return 0;
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
    byte *t_reader = t;
    if (*t & x01) {
        t_reader += (*t >> 1);
        t_reader++;
    }
    byte *t_writer = t_reader + (*t & x01 ? 0 : 1);
    byte count = 0;
    byte trie_len_diff = 0;
    while ((*t_reader & x01) || ((*t_reader & x02) && (*t_reader & x04) && BIT_COUNT(t_reader[1]) == 1
            && BIT_COUNT(t_reader[2]) == 0 && t_reader[3] == 1)) {
        if (*t_reader & x01) {
            byte len = *t_reader++ >> 1;
            memcpy(t_writer, t_reader, len);
            t_writer += len;
            t_reader += len;
            count += len;
            trie_len_diff++;
        } else {
            *t_writer++ = (*t_reader & xF8) + BIT_COUNT(t_reader[1] - 1);
            t_reader += 4;
            count++;
            trie_len_diff += 3;
        }
    }
    if (t_reader > t_writer) {
        memmove(t_writer, t_reader, BPT_TRIE_LEN - (t_reader - t));
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

int16_t bfos_node_handler::insertAfter() {
    byte key_char;
    byte mask;
    key_char = key[keyPos - 1];
    mask = x01 << (key_char & x07);
    *origPos &= xFB;
    triePos = origPos + (*origPos & x02 ? BIT_COUNT(origPos[1])
            + BIT_COUNT2(origPos[2]) + 3 : BIT_COUNT2(origPos[1]) + 2);
    updatePtrs(triePos, 4);
    insAt(triePos, ((key_char & xF8) | x04), mask, 0, 0);
    return triePos - trie + 2;
}

int16_t bfos_node_handler::insertBefore() {
    byte key_char;
    byte mask;
    key_char = key[keyPos - 1];
    mask = x01 << (key_char & x07);
    updatePtrs(origPos, 4);
    insAt(origPos, (key_char & xF8), mask, 0, 0);
    return origPos - trie + 2;
}

int16_t bfos_node_handler::insertLeaf() {
    byte key_char;
    byte mask;
    key_char = key[keyPos - 1];
    mask = x01 << (key_char & x07);
    triePos = origPos + ((*origPos & x02) ? 2 : 1);
    *triePos |= mask;
    triePos += (BIT_COUNT(*origPos & x02 ? origPos[1] : 0)
            + BIT_COUNT2(*triePos & (mask - 1)) + 1);
    updatePtrs(triePos, 2);
    insAt(triePos, x00, x00);
    return triePos - trie;
}

#if BS_MIDDLE_PREFIX == 1
int16_t bfos_node_handler::insertConvert() {
    byte key_char;
    byte mask;
    int16_t ret = 0;
    key_char = key[keyPos - 1];
    mask = x01 << (key_char & x07);
    byte b, c;
    char cmp_rel;
    int16_t diff;
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
        *triePos++ = mask;
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
        *triePos++ = mask;
        *triePos++ = 3;
        ret = triePos - trie;
        triePos += 2;
        break;
    }
    if (need_count)
        *triePos = (need_count << 1) | x01;
    return ret;
}
#endif

int16_t bfos_node_handler::insertThread() {
    byte key_char;
    byte mask;
    int16_t p, min;
    byte c1, c2;
    byte *childPos;
    int16_t diff;
    int16_t ret, ptr, pos;
    ret = pos = 0;

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
        insAt(childPos, mask, *childPos);
        childPos[2] = (byte) (BPT_TRIE_LEN - (childPos + 2 - trie));
        updatePtrs(childPos, 2);
    }
    p = keyPos;
    min = util::min16(key_len, keyPos + key_at_len);
    triePos = origPos + BIT_COUNT(*childPos) + 3
            + BIT_COUNT2(origPos[2] & (mask - 1));
    ptr = util::getInt(triePos);
    if (p < min) {
        origPos[2] &= ~mask;
        delAt(triePos, 2);
        updatePtrs(triePos, -2);
    } else {
        pos = triePos - trie;
        ret = pos;
    }
#if BS_MIDDLE_PREFIX == 1
    need_count -= 8;
    if (p + need_count == min) {
        if (need_count) {
            need_count--;
        }
    }
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
#if BS_MIDDLE_PREFIX == 1
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
    return ret;
}

int16_t bfos_node_handler::insertEmpty() {
    append((*key & xF8) | x04);
    append(x01 << (*key & x07));
    int16_t ret = BPT_TRIE_LEN;
    append(0);
    append(0);
    keyPos = 1;
    return ret;
}

int16_t bfos_node_handler::insertCurrent() {
    int16_t ret;

    switch (insertState) {
    case INSERT_AFTER:
        ret = insertAfter();
        break;
    case INSERT_BEFORE:
        ret = insertBefore();
        break;
    case INSERT_LEAF:
        ret = insertLeaf();
        break;
#if BS_MIDDLE_PREFIX == 1
    case INSERT_CONVERT:
        ret = insertConvert();
        break;
#endif
    case INSERT_THREAD:
        ret = insertThread();
        break;
    case INSERT_EMPTY:
        ret = insertEmpty();
        break;
    }

    if (BX_MAX_PFX_LEN < keyPos)
        BX_MAX_PFX_LEN = keyPos;

    if (BPT_MAX_KEY_LEN < key_len)
        BPT_MAX_KEY_LEN = key_len;

    return ret;
}

byte *bfos_node_handler::getPtrPos() {
    return trie + BPT_TRIE_LEN;
}

void bfos_node_handler::decodeNeedCount() {
    if (insertState != INSERT_THREAD)
        need_count = need_counts[insertState];
}
const byte bfos_node_handler::need_counts[10] = {0, 4, 4, 2, 4, 0, 7, 0, 0, 0};

void bfos_node_handler::initVars() {
}

int bfos::count1, bfos::count2;
