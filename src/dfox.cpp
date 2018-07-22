#include <math.h>
#include <stdint.h>
#include "dfox.h"

int dfox::getHeaderSize() {
    return DFOX_HDR_SIZE;
}

byte *dfox::getChildPtrPos(int16_t idx) {
    return current_block + getPtr(last_t);
}

byte *dfox::skipChildren(byte *t, int16_t count) {
    while (count) {
        byte tc = *t++;
        switch (tc & x03) {
        case x01:
            t += (tc >> 2);
            continue;
        case x00:
            t += BIT_COUNT2(*t);
            t++;
            break;
        case x02:
            t += *t;
            break;
        case x03:
            count--;
            t++;
            last_t = t - 2;
            continue;
        }
        last_t = t - 2;
        if (tc & x04)
            count--;
    }
    return t;
}

int16_t dfox::searchCurrentBlock() {
    byte key_char = *key;
    byte *t = trie;
    byte trie_char = *t;
    keyPos = 1;
    last_t = 0;
    origPos = t++;
    do {
        switch (((trie_char & x03) == x01) ? 3 :
                (key_char ^ trie_char) > x07 ? (key_char > trie_char ? 0 : 2) : 1) {
        case 0:
            if (trie_char & x02) {
                t += *t;
            } else {
                t += BIT_COUNT2(*t);
                t++;
            }
            last_t = t - 2;
            if (trie_char & x04) {
                    triePos = t;
                    insertState = INSERT_AFTER;
                return -1;
            }
            break;
        case 1:
            if (trie_char & x02) {
                t++;
                byte r_children = *t++;
                byte r_leaves = *t++;
                byte r_mask = x01 << (key_char & x07);
                key_char = (r_leaves & r_mask ? x01 : x00)
                        | (r_children & r_mask ? x02 : x00)
                        | (keyPos == key_len ? x04 : x00);
                r_mask--;
                t = skipChildren(t, BIT_COUNT(r_children & r_mask) + BIT_COUNT(r_leaves & r_mask));
            } else {
                byte r_leaves = *t++;
                byte r_mask = ~(xFF << (key_char & x07));
                t = skipChildren(t, BIT_COUNT(r_leaves & r_mask));
                key_char = (r_leaves & (r_mask + 1) ? x01 : x00);
            }
            switch (key_char) {
            case x01:
            case x05:
                int16_t cmp;
                key_at = getKey(t, &key_at_len);
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at, key_at_len);
                if (cmp == 0) {
                    last_t = t;
                    return 1;
                }
                if (cmp > 0)
                    last_t = t;
                else
                    cmp = -cmp;
                    triePos = t;
                    insertState = INSERT_THREAD;
#if DX_MIDDLE_PREFIX == 1
                    need_count = cmp + 8;
#else
                    need_count = (cmp * 4) + 6;
#endif
                return -1;
            case x02:
                break;
            case x07:
                last_t = t;
                key_at = getKey(t, &key_at_len);
                return 1;
            case x03:
                last_t = t;
                t += 2;
                break;
            case x00:
            case x04:
            case x06:
                    triePos = t;
                    insertState = INSERT_LEAF;
                return -1;
            }
            key_char = key[keyPos++];
            break;
        case 3:
#if DX_MIDDLE_PREFIX == 1
            byte pfx_len;
            pfx_len = (trie_char >> 2);
            while (pfx_len && key_char == *t && keyPos < key_len) {
                key_char = key[keyPos++];
                t++;
                pfx_len--;
            }
            if (!pfx_len)
                break;
            triePos = t;
            if (key_char > *t) {
                t = skipChildren(t + pfx_len, 1);
                key_at = t;
            }
                insertState = INSERT_CONVERT;
            return -1;
#endif
        case 2:
                triePos = origPos;
                insertState = INSERT_BEFORE;
            return -1;
            break;
        }
        trie_char = *t;
        origPos = t++;
    } while (1);
    return -1;
}

int16_t dfox::getPtr(byte *t) {
    return ((*t >> 2) << 8) + t[1];
}

byte *dfox::getKey(byte *t, byte *plen) {
    byte *kvIdx = current_block + getPtr(t);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *dfox::nextKey(byte *first_key, byte *tp, byte *t, char& ctr, byte& tc, byte& child, byte& leaf) {
    do {
        while (ctr > x07) {
            if (tc & x04) {
                keyPos--;
                tc = trie[tp[keyPos]];
                while (x01 == (tc & x03)) {
                    keyPos--;
                    tc = trie[tp[keyPos]];
                }
                child = (tc & x02 ? trie[tp[keyPos] + 2] : 0);
                leaf = trie[tp[keyPos] + (tc & x02 ? 3 : 1)];
                ctr = first_key[keyPos] & 0x07;
                ctr++;
            } else {
                tp[keyPos] = t - trie;
                tc = *t++;
                if (x01 == (tc & x03)) {
                    byte len = tc >> 2;
                    memset(tp + keyPos, t - trie - 1, len);
                    memcpy(first_key + keyPos, t, len);
                    t += len;
                    keyPos += len;
                    tp[keyPos] = t - trie;
                    tc = *t++;
                }
                t += (tc & x02 ? 1 : 0);
                child = (tc & x02 ? *t++ : 0);
                leaf = *t++;
                ctr = 0;
            }
        }
        first_key[keyPos] = (tc & xF8) | ctr;
        byte mask = x01 << ctr;
        if (leaf & mask) {
            leaf &= ~mask;
            if (0 == (child & mask))
                ctr++;
            return t;
        }
        if (child & mask) {
            child = leaf = 0;
            keyPos++;
            ctr = x08;
            tc = 0;
        }
        ctr++;
    } while (1); // (t - trie) < BPT_TRIE_LEN);
    return t;
}

byte *dfox::nextPtr(byte *t) {
    byte tc = *t & x03;
    while (x03 != tc) {
        if (x01 == tc) {
            t += (*t >> 2);
            t++;
            tc = *t & x03;
        }
        t += (tc & x02 ? 4 : 2);
        tc = *t & x03;
    }
    return t;
}

byte *dfox::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t orig_filled_size = filledSize();
    const uint16_t DFOX_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
    byte *b = (byte *) util::alignedAlloc(DFOX_NODE_SIZE);
    dfox new_block;
    new_block.setCurrentBlock(b);
    new_block.initCurrentBlock();
    if (!isLeaf())
        new_block.setLeaf(false);
    new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
    new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = DFOX_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int16_t brk_idx = -1;
    int16_t brk_kv_pos;
    byte *brk_trie_pos = trie;
    int16_t tot_len;
    brk_kv_pos = tot_len = 0;
    char ctr = x08;
    byte tp[BPT_MAX_PFX_LEN + 1];
    byte *t = trie;
    byte tc, child, leaf;
    tc = child = leaf = 0;
    //if (!isLeaf())
    //   cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
    keyPos = 0;
    // (1) move all data to new_block in order
    int16_t idx;
    for (idx = 0; idx < orig_filled_size; idx++) {
        if (brk_idx == -1)
            t = nextKey(first_key, tp, t, ctr, tc, child, leaf);
        else
            t = nextPtr(t);
        int16_t src_idx = getPtr(t);
        //if (brk_idx == -1) {
        //    memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
        //   first_key[keyPos+1+current_block[src_idx]] = 0;
        //   cout << first_key << endl;
        //}
        t += 2;
        int16_t kv_len = current_block[src_idx];
        kv_len++;
        kv_len += current_block[src_idx + kv_len];
        kv_len++;
        tot_len += kv_len;
        memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
        kv_last_pos += kv_len;
        if (brk_idx == -1) {
            //brk_key_len = nextKey(s);
            //if (tot_len > halfKVLen) {
            if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                //memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
                //first_key[keyPos+1+current_block[src_idx]] = 0;
                //cout << first_key << ":";
                brk_idx = idx + 1;
                brk_kv_pos = kv_last_pos;
                brk_trie_pos = t;
            }
        }
    }
    kv_last_pos = getKVLastPos() + DFOX_NODE_SIZE - kv_last_pos;
    new_block.setKVLastPos(kv_last_pos);
    memmove(new_block.current_block + kv_last_pos, new_block.current_block + getKVLastPos(), DFOX_NODE_SIZE - kv_last_pos);
    brk_kv_pos += (kv_last_pos - getKVLastPos());
    int16_t diff = DFOX_NODE_SIZE - brk_kv_pos;
    t = trie;
    for (idx = 0; idx < orig_filled_size; idx++) {
        t = nextPtr(t);
        setPtr(t, kv_last_pos + (idx < brk_idx ? diff : 0));
        t += 2;
        kv_last_pos += new_block.current_block[kv_last_pos];
        kv_last_pos++;
        kv_last_pos += new_block.current_block[kv_last_pos];
        kv_last_pos++;
    }
    kv_last_pos = new_block.getKVLastPos();
    memcpy(new_block.trie, trie, BPT_TRIE_LEN);
    new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;

    {
        int16_t last_key_len = keyPos + 1;
        byte last_key[last_key_len + 1];
        memcpy(last_key, first_key, last_key_len);
        deleteTrieLastHalf(keyPos, first_key, tp);
        new_block.keyPos = keyPos;
        t = new_block.trie + (brk_trie_pos - trie);
        t = new_block.nextKey(first_key, tp, t, ctr, tc, child, leaf);
        keyPos = new_block.keyPos;
        //src_idx = getPtr(idx + 1);
        //memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
        //first_key[keyPos+1+current_block[src_idx]] = 0;
        //cout << first_key << endl;
        if (isLeaf()) {
            // *first_len_ptr = keyPos + 1;
            *first_len_ptr = util::compare((const char *) first_key, keyPos + 1, (const char *) last_key, last_key_len);
        } else {
            new_block.key_at = new_block.getKey(t, &new_block.key_at_len);
            keyPos++;
            if (new_block.key_at_len) {
                memcpy(first_key + keyPos, new_block.key_at,
                        new_block.key_at_len);
                *first_len_ptr = keyPos + new_block.key_at_len;
            } else
                *first_len_ptr = keyPos;
            keyPos--;
        }
        new_block.deleteTrieFirstHalf(keyPos, first_key, tp);
    }

    {
        int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
        memcpy(current_block + DFOX_NODE_SIZE - old_blk_new_len,
                new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
        setKVLastPos(DFOX_NODE_SIZE - old_blk_new_len);
        setFilledSize(brk_idx);
    }

    {
        int16_t new_size = orig_filled_size - brk_idx;
        new_block.setKVLastPos(brk_kv_pos);
        new_block.setFilledSize(new_size);
    }

    return new_block.current_block;
}

void dfox::updatePrefix() {

}

void dfox::deleteTrieLastHalf(int16_t brk_key_len, byte *first_key, byte *tp) {
    for (int idx = brk_key_len; idx >= 0; idx--) {
        byte *t = trie + tp[idx];
        byte tc = *t;
        if (x01 == (tc & x03))
            continue;
        byte offset = first_key[idx] & x07;
        *t++ = (tc | x04);
        if (tc & x02) {
            byte children = t[1];
            children &= ~((idx == brk_key_len ? xFF : xFE) << offset);
                            // ryte_mask[offset] : ryte_incl_mask[offset]);
            t[1] = children;
            byte leaves = t[2] & ~(xFE << offset);
            t[2] = leaves; // ryte_incl_mask[offset];
            byte *new_t = skipChildren(t + 3, BIT_COUNT(children) + BIT_COUNT(leaves));
            *t = new_t - t;
            t = new_t;
        } else {
            byte leaves = *t;
            leaves &= ~(xFE << offset); // ryte_incl_mask[offset];
            *t++ = leaves;
            t += BIT_COUNT2(leaves);
        }
        if (idx == brk_key_len)
            BPT_TRIE_LEN = t - trie;
    }
}

int dfox::deleteSegment(byte *delete_end, byte *delete_start) {
    int count = delete_end - delete_start;
    if (count) {
        BPT_TRIE_LEN -= count;
        memmove(delete_start, delete_end, trie + BPT_TRIE_LEN - delete_start);
    }
    return count;
}

void dfox::deleteTrieFirstHalf(int16_t brk_key_len, byte *first_key, byte *tp) {
    for (int idx = brk_key_len; idx >= 0; idx--) {
        byte *t = trie + tp[idx];
        byte tc = *t++;
        if (x01 == (tc & x03)) {
            byte len = tc >> 2;
            deleteSegment(trie + tp[idx + 1], t + len);
            idx -= len;
            idx++;
            continue;
        }
        byte offset = first_key[idx] & 0x07;
        if (tc & x02) {
            byte children = t[1];
            byte leaves = t[2];
            byte mask = (xFF << offset);
            children &= mask; // left_incl_mask[offset];
            leaves &= ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
            int16_t count = BIT_COUNT(t[1]) - BIT_COUNT(children) + BIT_COUNT(t[2]) - BIT_COUNT(leaves); // ryte_mask[offset];
            t[1] = children;
            t[2] = leaves;
            byte *new_t = skipChildren(t + 3, count);
            deleteSegment((idx < brk_key_len) ? (trie + tp[idx + 1]) : new_t, t + 3);
            new_t = skipChildren(t + 3, BIT_COUNT(children) + BIT_COUNT(leaves));
            *t = new_t - t;
        } else {
            byte leaves = *t;
            leaves &= ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
            deleteSegment(t + 1 + BIT_COUNT2(*t) - BIT_COUNT2(leaves), t + 1);
            *t = leaves;
        }
    }
    deleteSegment(trie + tp[0], trie);
}

void dfox::addFirstData() {
    addData(0);
}

void dfox::addData(int16_t idx) {

    byte *ptr = insertCurrent();

    int16_t key_left = key_len - keyPos;
    int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
    setKVLastPos(kv_last_pos);
    current_block[kv_last_pos] = key_left;
    if (key_left)
        memcpy(current_block + kv_last_pos + 1, key + keyPos, key_left);
    current_block[kv_last_pos + key_left + 1] = value_len;
    memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);

    setPtr(ptr, kv_last_pos);
    setFilledSize(filledSize() + 1);

}

bool dfox::isFull() {
    decodeNeedCount();
    if (getKVLastPos() < (DFOX_HDR_SIZE + BPT_TRIE_LEN +
            need_count + key_len + value_len + 3))
        return true;
    if (BPT_TRIE_LEN > (252 - need_count))
        return true;
    return false;
}

void dfox::setPtr(byte *t, int16_t ptr) {
    *t++ = ((ptr >> 8) << 2) | x03;
    *t = ptr & xFF;
}

void dfox::updateSkipLens(byte *loop_upto, byte *covering_upto, int diff) {
    byte *t = trie;
    byte tc = *t++;
    loop_upto++;
    while (t <= loop_upto) {
        switch (tc & x03) {
        case x01:
            t += (tc >> 2);
            break;
        case x02:
            if ((t + *t) > covering_upto) {
                *t = *t + diff;
                t += 3;
            } else
                t += *t;
            break;
        case x00:
            t += BIT_COUNT2(*t);
            t++;
            break;
        case x03:
            t++;
        }
        tc = *t++;
    }
}

byte *dfox::insertCurrent() {
    byte key_char, mask;
    int16_t diff;
    byte *ret;

    key_char = key[keyPos - 1];
    mask = x01 << (key_char & x07);
    switch (insertState) {
    case INSERT_AFTER:
        *origPos &= xFB;
        insAt(triePos, ((key_char & xF8) | x04), mask, x00, x00);
        ret = triePos + 2;
        if (keyPos > 1)
            updateSkipLens(origPos - 1, triePos - 1, 4);
        break;
    case INSERT_BEFORE:
        insAt(triePos, key_char & xF8, mask, x00, x00);
        ret = triePos + 2;
        if (keyPos > 1)
            updateSkipLens(triePos, triePos + 1, 4);
        break;
    case INSERT_LEAF:
        if (*origPos & x02)
            origPos[3] |= mask;
        else
            origPos[1] |= mask;
        insAt(triePos, x00, x00);
        ret = triePos;
        updateSkipLens(origPos, triePos - 1, 2);
        break;
#if DX_MIDDLE_PREFIX == 1
    case INSERT_CONVERT:
        byte b, c;
        char cmp_rel;
        diff = triePos - origPos;
        // 3 possible relationships between key_char and *triePos, 4 possible positions of triePos
        c = *triePos;
        cmp_rel = ((c ^ key_char) > x07 ? (c < key_char ? 0 : 1) : 2);
        if (cmp_rel == 0) {
            insAt(key_at, (key_char & xF8) | 0x04, 1 << (key_char & x07), x00, x00);
            ret = key_at + 2;
        }
        if (diff == 1)
            triePos = origPos;
        b = (cmp_rel == 2 ? x04 : x00) | (cmp_rel == 1 ? x00 : x02);
        need_count = (*origPos >> 2) - diff;
        diff--;
        *triePos++ = ((cmp_rel == 0 ? c : key_char) & xF8) | b;
        b = (cmp_rel == 1 ? 6 : (cmp_rel == 2 ? (c < key_char ? 2 : 4) : 2));
        if (diff) {
            *origPos = (diff << 2) | x01;
            b++;
        }
        if (need_count)
            b++;
        insBytes(triePos + (diff ? 0 : 1), b);
        updateSkipLens(origPos, triePos, b + (cmp_rel == 0 ? 4
                : (cmp_rel == 2 && c < key_char ? 2 : 0)));
        if (cmp_rel == 1) {
            *triePos++ = 1 << (key_char & x07);
            ret = triePos;
            triePos += 2;
            *triePos++ = (c & xF8) | x06;
        }
        key_at = skipChildren(triePos + (cmp_rel == 2 ? (c < key_char ? 3 : 5) : 3)
                + need_count + (need_count ? 1 : 0), 1);
        *triePos = key_at - triePos + (cmp_rel == 2 && c < key_char ? 2 : 0);
        triePos++;
        *triePos++ = 1 << (c & x07);
        if (cmp_rel == 2) {
            *triePos++ = 1 << (key_char & x07);
            if (c >= key_char) {
                ret = triePos;
                triePos += 2;
            }
        } else
            *triePos++ = 0;
        if (need_count)
            *triePos = (need_count << 2) | x01;
        if (cmp_rel == 0)
            ret += b;
        if (cmp_rel == 2 && c < key_char) {
            insAt(key_at, x00, x00);
            ret = key_at;
        }
        break;
#endif
    case INSERT_THREAD:
        int16_t p, min, ptr;
        byte c1, c2;
        byte *ptrPos;
        ret = ptrPos = 0;
        if (*origPos & x02) {
            origPos[2] |= mask;
        } else {
            insAt(origPos + 1, BIT_COUNT2(origPos[1]) + 3, mask);
            triePos += 2;
        }
        c1 = c2 = key_char;
        p = keyPos;
        min = util::min16(key_len, keyPos + key_at_len);
        ptr = getPtr(triePos);
        if (p < min)
            origPos[3] &= ~mask;
        else {
            if (p == key_len)
                ret = triePos;
            else
                ptrPos = triePos;
            triePos += 2;
        }
#if DX_MIDDLE_PREFIX == 1
        need_count -= 9;
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
        insBytes(triePos, diff);
#else
        need_count = (p == min ? 4 : 0);
        while (p < min) {
            c1 = key[p];
            c2 = key_at[p - keyPos];
            need_count += ((c1 ^ c2) > x07 ? 6 : (c1 == c2 ? (p + 1 == min ? 8 : 4) : 4));
            if (c1 != c2)
                break;
            p++;
        }
        p = keyPos;
        insBytes(triePos, need_count);
        diff = need_count;
        need_count += (p == min ? 0 : 2);
#endif
        if (!ret)
            ret = triePos + diff - (p < min ? 0 : 2);
        if (!ptrPos)
            ptrPos = triePos + diff - (p < min ? 0 : 2);
#if DX_MIDDLE_PREFIX == 1
        if (need_count) {
            *triePos++ = (need_count << 2) | x01;
            memcpy(triePos, key + keyPos, need_count);
            triePos += need_count;
            p += need_count;
            //count1 += need_count;
        }
#endif
        while (p < min) {
            c1 = key[p];
            c2 = key_at[p - keyPos];
            if (c1 > c2) {
                byte swap = c1;
                c1 = c2;
                c2 = swap;
            }
#if DX_MIDDLE_PREFIX == 1
            switch ((c1 ^ c2) > x07 ? 0 : (c1 == c2 ? 3 : 1)) {
#else
            switch ((c1 ^ c2) > x07 ?
                    0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
            case 2:
                need_count -= 4;
                *triePos++ = (c1 & xF8) | x06;
                *triePos++ = need_count + 3;
                *triePos++ = x01 << (c1 & x07);
                *triePos++ = 0;
                break;
#endif
            case 0:
                *triePos++ = c1 & xF8;
                *triePos++ = x01 << (c1 & x07);
                if (c1 == key[p])
                    ret = triePos;
                else
                    ptrPos = triePos;
                triePos += 2;
                *triePos++ = (c2 & xF8) | x04;
                *triePos = x01 << (c2 & x07);
                break;
            case 1:
                *triePos++ = (c1 & xF8) | x04;
                *triePos++ = (x01 << (c1 & x07)) | (x01 << (c2 & x07));
                if (c1 == key[p])
                    ret = triePos;
                else
                    ptrPos = triePos;
                break;
            case 3:
                *triePos++ = (c1 & xF8) | x06;
                *triePos++ = 9;
                *triePos++ = x01 << (c1 & x07);
                *triePos++ = x01 << (c1 & x07);
                if (p + 1 == key_len)
                    ret = triePos;
                else
                    ptrPos = triePos;
                triePos += 2;
                break;
            }
            if (c1 != c2)
                break;
            p++;
        }
        if (c1 == c2) {
            c2 = (p == key_len ? key_at[p - keyPos] : key[p]);
            *triePos++ = (c2 & xF8) | x04;
            *triePos = x01 << (c2 & x07);
        }
        updateSkipLens(origPos - 1, origPos, diff + (*origPos & x02 ? 0 : 2));
        origPos[1] += diff;
        *origPos |= x02;
        diff = p - keyPos;
        keyPos = p + (p < key_len ? 1 : 0);
        if (diff < key_at_len)
            diff++;
        if (diff) {
            key_at_len -= diff;
            ptr += diff;
            if (key_at_len >= 0)
                current_block[ptr] = key_at_len;
        }
        if (ptrPos)
            setPtr(ptrPos, ptr);
        break;
    case INSERT_EMPTY:
        *trie = (key_char & xF8) | x04;
        trie[1] = mask;
        BPT_TRIE_LEN = 4;
        ret = trie + 2;
        break;
    }

    if (BPT_MAX_PFX_LEN < (isLeaf() ? keyPos : key_len))
        BPT_MAX_PFX_LEN = (isLeaf() ? keyPos : key_len);

    if (BPT_MAX_KEY_LEN < key_len)
        BPT_MAX_KEY_LEN = key_len;

    return ret;

}

void dfox::insBytes(byte *ptr, int16_t len) {
    memmove(ptr + len, ptr, trie + BPT_TRIE_LEN - ptr);
    BPT_TRIE_LEN += len;
}

byte *dfox::getPtrPos() {
    return NULL;
}

void dfox::decodeNeedCount() {
    if (insertState != INSERT_THREAD)
        need_count = need_counts[insertState];
}
const byte dfox::need_counts[10] = {0, 4, 4, 2, 4, 0, 7, 0, 0, 0};
