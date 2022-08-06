#ifndef dft_H
#define dft_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define DFT_UNIT_SIZE 4

#define DFT_HDR_SIZE 9

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class dft : public bpt_trie_handler<dft> {
public:
    const static byte need_counts[10];
    int16_t last_sibling_pos;
    int16_t pos, key_at_pos;

    dft(uint16_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint16_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, byte *block = NULL) :
        bpt_trie_handler<dft>(leaf_block_sz, parent_block_sz, cache_sz, fname, block) {
    }

    inline void setCurrentBlockRoot() {
        current_block = root_block;
        trie = current_block + DFT_HDR_SIZE;
    }

    inline void setCurrentBlock(byte *m) {
        current_block = m;
        trie = current_block + DFT_HDR_SIZE;
    }

    inline int16_t searchCurrentBlock() {
        byte *t = trie;
        keyPos = 0;
        byte key_char = key[keyPos++];
        triePos = 0;
        do {
            origPos = t;
            last_sibling_pos = 0;
            while (key_char > *t) {
                t++;
                last_sibling_pos = t - trie;
    #if DFT_UNIT_SIZE == 3
                int s = *t & x3F;
    #else
                int s = *t & x7F;
    #endif
                if (s) {
                    t += (s * DFT_UNIT_SIZE);
                    t--;
                    origPos = t;
                    continue;
                } else {
    #if DFT_UNIT_SIZE == 3
                        int child = *t & x40;
    #else
                        int child = *t & x80;
    #endif
                        t += DFT_UNIT_SIZE;
                        while (child) {
    #if DFT_UNIT_SIZE == 3
                            s = *t & x3F;
                            child = *t & x40;
    #else
                            s = *t & x7F;
                            child = *t & x80;
    #endif
                            if (s) {
                                t += s * DFT_UNIT_SIZE;
                                child = 1;
                            } else
                                t += DFT_UNIT_SIZE;
                        }
                        insertState = INSERT_AFTER;
                        triePos = t - 1;
                    return -1;
                }
            }
            if (key_char < *t++) {
                triePos = t - 1;
                    insertState = INSERT_BEFORE;
                return -1;
            }
#if DFT_UNIT_SIZE == 3
            byte r_children = *t & x40;
            int16_t ptr = get9bitPtr(t);
#else
            byte r_children = *t & x80;
            int16_t ptr = util::getInt(t + 1);
#endif
            switch ((ptr ? 2 : 0) | ((r_children && keyPos != key_len) ? 1 : 0)) {
            //switch (ptr ?
            //        (r_children ? (keyPos == key_len ? 2 : 1) : 2) :
            //        (r_children ? (keyPos == key_len ? 0 : 1) : 0)) {
            case 0:
                triePos = t - 1;
                    insertState = INSERT_LEAF;
                return -1;
            case 1:
                break;
            case 2:
                int16_t cmp;
                key_at = current_block + ptr;
                key_at_len = *key_at++;
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at, key_at_len);
                if (cmp == 0)
                    return ptr;
                if (cmp < 0) {
                    cmp = -cmp;
                    triePos = t - 1;
                }
                insertState = INSERT_THREAD;
                need_count = (cmp * DFT_UNIT_SIZE) + DFT_UNIT_SIZE * 2;
                return -1;
            }
            t += DFT_UNIT_SIZE;
            t--;
            key_char = key[keyPos++];
        } while (1);
        return -1;
    }

    inline int getHeaderSize() {
        return DFT_HDR_SIZE;
    }

    inline byte *getPtrPos() {
        return NULL;
    }

    inline byte *getChildPtrPos(int16_t search_result) {
        if (triePos++) {
            while (triePos > trie) {
                triePos -= DFT_UNIT_SIZE;
                int16_t p = (DFT_UNIT_SIZE == 3 ? get9bitPtr(triePos) : util::getInt(triePos + 1));
                if (p) {
                    key_at = current_block + p;
                    break;
                }
            }
        } else
            key_at--;
        return key_at;
    }

    void updatePtrs(byte *upto, int diff) {
        byte *t = trie + 1;
        while (t <= upto) {
    #if DFT_UNIT_SIZE == 3
            byte sibling = (*t & x3F);
    #else
            byte sibling = (*t & x7F);
    #endif
            if (sibling && (t + sibling * DFT_UNIT_SIZE) >= upto)
                *t += diff;
            t += DFT_UNIT_SIZE;
        }
    }

    byte insertUnit(byte *t, byte c1, byte s1, int16_t ptr1) {
        memmove(t + DFT_UNIT_SIZE, t, trie + BPT_TRIE_LEN- t);
        t[0] = c1;
        t[1] = s1;
    #if DFT_UNIT_SIZE == 3
                if (ptr1 & x100)
                t[1] |= x80;
                t[2] = ptr1;
    #else
                util::setInt(t + 2, ptr1);
    #endif
                BPT_TRIE_LEN += DFT_UNIT_SIZE;
                return DFT_UNIT_SIZE;
            }

    byte insert2Units(byte *t, byte c1, byte s1, int16_t ptr1,
            byte c2, byte s2, int16_t ptr2) {
        byte size = DFT_UNIT_SIZE * 2;
        memmove(t + size, t, trie + BPT_TRIE_LEN- t);
        t[0] = c1;
        t[1] = s1;
    #if DFT_UNIT_SIZE == 3
        if (ptr1 & x100)
            t[1] |= x80;
        t[2] = ptr1;
        t[3] = c2;
        t[4] = s2;
        if (ptr2 & x100)
            t[4] |= x80;
        t[5] = ptr2;
    #else
        util::setInt(t + 2, ptr1);
        t[4] = c2;
        t[5] = s2;
        util::setInt(t + 6, ptr2);
    #endif
        BPT_TRIE_LEN+= size;
        return size;
    }

    void append(byte b) {
        trie[BPT_TRIE_LEN++] = b;
    }

    void appendPtr(int16_t p) {
    #if DFT_UNIT_SIZE == 3
        trie[BPT_TRIE_LEN] = p;
        if (p & x100)
        trie[BPT_TRIE_LEN - 1] |= x80;
        else
        trie[BPT_TRIE_LEN - 1] &= x7F;
        BPT_TRIE_LEN++;
    #else
        util::setInt(trie + BPT_TRIE_LEN, p);
        BPT_TRIE_LEN += 2;
    #endif
    }

    int16_t get9bitPtr(byte *t) {
        int16_t ptr = (*t++ & x80 ? 256 : 0);
        ptr |= *t;
        return ptr;
    }

    void set9bitPtr(byte *t, int16_t p) {
        *t-- = p;
        if (p & x100)
            *t |= x80;
        else
            *t &= x7F;
    }

    byte *split(byte *first_key, int16_t *first_len_ptr) {
        int16_t orig_filled_size = filledSize();
        const uint16_t DFT_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
        byte *b = allocateBlock(DFT_NODE_SIZE);
        dft new_block(this->leaf_block_size, this->parent_block_size, 0, NULL, b);
        //new_block.setKVLastPos(DFT_NODE_SIZE);
        if (!isLeaf())
            new_block.setLeaf(false);
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        int16_t kv_last_pos = getKVLastPos();
        int16_t halfKVLen = DFT_NODE_SIZE - kv_last_pos + 1;
        halfKVLen /= 2;

        int16_t brk_idx = 0;
        int16_t brk_kv_pos;
        int16_t brk_trie_pos;
        int16_t brk_old_trie_pos;
        int16_t brk_tp_old_pos;
        int16_t tot_len;
        brk_kv_pos = tot_len = 0;
        // (1) move all data to new_block in order
        int16_t idx = 0;
        keyPos = 0;
        byte tp[BPT_MAX_PFX_LEN + 1];
        byte brk_tp_old[BPT_MAX_PFX_LEN + 1];
        byte brk_tp_new[BPT_MAX_PFX_LEN + 1];
        byte *t_end = trie + BPT_TRIE_LEN;
        for (byte *t = trie + 1; t < t_end; t += DFT_UNIT_SIZE) {
    #if DFT_UNIT_SIZE == 3
            int16_t src_idx = get9bitPtr(t);
            byte child = *t & x40;
    #else
            int16_t src_idx = util::getInt(t + 1);
            byte child = *t & x80;
    #endif
            tp[keyPos] = t - trie;
            if (child) {
                keyPos++;
                tp[keyPos] = t - trie;
            }
            if (src_idx) {
                int16_t kv_len = current_block[src_idx];
                kv_len++;
                kv_len += current_block[src_idx + kv_len];
                kv_len++;
                tot_len += kv_len;
                memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
    #if DFT_UNIT_SIZE == 3
                set9bitPtr(t + 1, kv_last_pos);
    #else
                util::setInt(t + 1, kv_last_pos);
    #endif
                kv_last_pos += kv_len;
                if (brk_idx == 0) {
                    //if (tot_len > halfKVLen) {
                    //if (tot_len > halfKVLen || ((t-trie) > (BPT_TRIE_LEN * 2 / 3)) ) {
                    if (tot_len > halfKVLen || idx == (orig_filled_size / 2)
                                || ((t - trie) > (BPT_TRIE_LEN* 2 / 3)) ) {
                        brk_idx = -1;
                        brk_kv_pos = kv_last_pos;
                        brk_tp_old_pos = (child ? keyPos : keyPos + 1);
                        brk_old_trie_pos = t - trie + DFT_UNIT_SIZE - 1;
                        memcpy(brk_tp_old, tp, brk_tp_old_pos);
                    }
                } else if (brk_idx == -1) {
                    brk_idx = idx;
                    brk_trie_pos = t - trie - 1;
                    *first_len_ptr = (child ? keyPos : keyPos + 1);
                    memcpy(brk_tp_new, tp, *first_len_ptr);
                }
                idx++;
            }
            if (!child) {
    #if DFT_UNIT_SIZE == 3
                byte p = *t & x3F;
    #else
                byte p = *t & x7F;
    #endif
                while (p == 0) {
                    keyPos--;
    #if DFT_UNIT_SIZE == 3
                    p = trie[tp[keyPos]] & x3F;
    #else
                    p = trie[tp[keyPos]] & x7F;
    #endif
                }
            }
        }
        memcpy(new_block.trie, trie, BPT_TRIE_LEN);
        new_block.BPT_TRIE_LEN= BPT_TRIE_LEN;
        idx = *first_len_ptr;
        first_key[--idx] = trie[brk_trie_pos];
        int16_t k = brk_trie_pos;
        while (idx--) {
            int16_t j = brk_tp_new[idx] - 1;
            first_key[idx] = trie[j];
            k -= DFT_UNIT_SIZE;
            new_block.trie[k++] = trie[j++];
            new_block.trie[k] = trie[j];
            if (new_block.trie[k] & (DFT_UNIT_SIZE == 3 ? x3F : x7F))
                new_block.trie[k] -= ((k - j) / DFT_UNIT_SIZE);
            k++;
            j++;
            new_block.trie[k] = trie[j];
    #if DFT_UNIT_SIZE == 3
            set9bitPtr(new_block.trie + k, 0);
            k++;
    #else
            new_block.trie[k++] = 0;
            new_block.trie[k++] = 0;
    #endif
            k -= DFT_UNIT_SIZE;
        }
        new_block.BPT_TRIE_LEN-= k;
        memmove(new_block.trie, new_block.trie + k, new_block.BPT_TRIE_LEN);
        BPT_TRIE_LEN= brk_old_trie_pos;
        idx = brk_tp_old_pos;
        while (idx--) {
    #if DFT_UNIT_SIZE == 3
            trie[brk_tp_old[idx]] &= xC0;
    #else
            trie[brk_tp_old[idx]] &= x80;
    #endif
        }
    #if DFT_UNIT_SIZE == 3
        trie[brk_old_trie_pos - DFT_UNIT_SIZE + 1] &= x80;
    #else
        trie[brk_old_trie_pos - DFT_UNIT_SIZE + 1] = 0;
    #endif
        if (!isLeaf()) {
            if (new_block.current_block[brk_kv_pos]) {
                memcpy(first_key + *first_len_ptr, new_block.current_block + brk_kv_pos + 1,
                        new_block.current_block[brk_kv_pos]);
                *first_len_ptr += new_block.current_block[brk_kv_pos];
            }
        }

        {
            kv_last_pos = getKVLastPos();
            int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + DFT_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            setKVLastPos(DFT_NODE_SIZE - old_blk_new_len);
            setFilledSize(brk_idx);
            int16_t diff = DFT_NODE_SIZE - brk_kv_pos;
            t_end = trie + BPT_TRIE_LEN;
            for (byte *t = trie + 1; t < t_end; t += DFT_UNIT_SIZE) {
    #if DFT_UNIT_SIZE == 3
                int16_t ptr = get9bitPtr(t);
    #else
                int16_t ptr = util::getInt(t + 1);
    #endif
                if (ptr) {
                    ptr += diff;
    #if DFT_UNIT_SIZE == 3
                    set9bitPtr(t + 1, ptr);
    #else
                    util::setInt(t + 1, ptr);
    #endif
                }
            }
        }

        int16_t new_size = orig_filled_size - brk_idx;
        new_block.setKVLastPos(brk_kv_pos);
        new_block.setFilledSize(new_size);
        return new_block.current_block;
    }

    void addFirstData() {
        addData(0);
    }

    void addData(int16_t search_result) {

        int16_t ptr = insertCurrent();

        int16_t key_left = key_len - keyPos;
        int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
        setKVLastPos(kv_last_pos);
    #if DFT_UNIT_SIZE == 3
        trie[ptr--] = kv_last_pos;
        if (kv_last_pos & x100)
            trie[ptr] |= x80;
        else
            trie[ptr] &= x7F;
    #else
        util::setInt(trie + ptr, kv_last_pos);
    #endif
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + keyPos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);
        setFilledSize(filledSize() + 1);

    }

    bool isFull(int16_t search_result) {
        decodeNeedCount();
    #if DFT_UNIT_SIZE == 3
        if (BPT_TRIE_LEN > 189 - need_count) {
            //if ((origPos - trie) <= (72 + need_count)) {
            return true;
            //}
        }
    #endif
        if (getKVLastPos() < (DFT_HDR_SIZE + BPT_TRIE_LEN
                + need_count + key_len - keyPos + value_len + 3)) {
            return true;
        }
        if (BPT_TRIE_LEN > 254 - need_count) {
            return true;
        }
        return false;
    }

    int16_t insertCurrent() {
        byte key_char;
        int16_t ret, ptr, pos;
        ret = pos = 0;

        switch (insertState) {
        case INSERT_AFTER:
            key_char = key[keyPos - 1];
            updatePtrs(triePos, 1);
            origPos++;
            *origPos += ((triePos - origPos + 1) / DFT_UNIT_SIZE);
    #if DFT_UNIT_SIZE == 3
            insAt(triePos, key_char, x00, 0);
    #else
            insAt(triePos, key_char, x00, 0, 0);
    #endif
            ret = triePos - trie + 2;
            break;
        case INSERT_BEFORE:
            key_char = key[keyPos - 1];
            updatePtrs(origPos, 1);
            if (last_sibling_pos)
                trie[last_sibling_pos]--;
    #if DFT_UNIT_SIZE == 3
            insAt(origPos, key_char, 1, 0);
    #else
            insAt(origPos, key_char, 1, 0, 0);
    #endif
            ret = origPos - trie + 2;
            break;
        case INSERT_LEAF:
            ret = origPos - trie + 2;
            break;
        case INSERT_THREAD:
            int16_t p, min, unit_ctr;
            byte c1, c2;
            unit_ctr = 0;
            key_char = key[keyPos - 1];
            c1 = c2 = key_char;
            p = keyPos;
            min = util::min16(key_len, keyPos + key_at_len);
            origPos++;
    #if DFT_UNIT_SIZE == 3
            *origPos |= x40;
            ptr = *(origPos + 1);
            if (*origPos & x80)
                ptr |= x100;
    #else
            *origPos |= x80;
            ptr = util::getInt(origPos + 1);
    #endif
            if (p < min) {
    #if DFT_UNIT_SIZE == 3
                *origPos &= x7F;
                origPos++;
                *origPos = 0;
    #else
                origPos++;
                util::setInt(origPos, 0);
                origPos++;
    #endif
            } else {
                origPos++;
                pos = origPos - trie;
                ret = pos;
    #if DFT_UNIT_SIZE == 4
                origPos++;
    #endif
            }
            origPos++;
            triePos = origPos;
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
                    if (swap) {
                        pos = triePos - trie + 2;
                        ret = pos + DFT_UNIT_SIZE;
                    } else {
                        ret = triePos - trie + 2;
                        pos = ret + DFT_UNIT_SIZE;
                    }
                    triePos += insert2Units(triePos, c1, x01, (swap ? ptr : 0), c2,
                            0, (swap ? 0 : ptr));
                    unit_ctr += 2;
                    break;
                case 1:
    #if DFT_UNIT_SIZE == 3
                    triePos += insertUnit(triePos, c1, x40, 0);
    #else
                    triePos += insertUnit(triePos, c1, x80, 0);
    #endif
                    unit_ctr++;
                    break;
                case 2:
                    if (p + 1 == key_len)
                        ret = triePos - trie + 2;
                    else
                        pos = triePos - trie + 2;
    #if DFT_UNIT_SIZE == 3
                    triePos += insertUnit(triePos, c1, x40, 0);
    #else
                    triePos += insertUnit(triePos, c1, x80, 0);
    #endif
                    unit_ctr++;
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
                if (p == key_len) {
                    pos = triePos - trie + 2;
                    keyPos--;
                } else
                    ret = triePos - trie + 2;
                triePos += insertUnit(triePos, c2, x00, 0);
                unit_ctr++;
            }
            updatePtrs(origPos, unit_ctr);
            if (diff < key_at_len)
                diff++;
            if (diff) {
                p = ptr;
                key_at_len -= diff;
                p += diff;
                if (key_at_len >= 0) {
                    current_block[p] = key_at_len;
    #if DFT_UNIT_SIZE == 3
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
            append(x00);
            ret = BPT_TRIE_LEN;
            appendPtr(0);
            keyPos = 1;
            break;
        }

        if (BPT_MAX_PFX_LEN < keyPos)
            BPT_MAX_PFX_LEN = keyPos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

        return ret;
    }

    void decodeNeedCount() {
        if (insertState != INSERT_THREAD)
            need_count = need_counts[insertState];
    }

};

#endif
