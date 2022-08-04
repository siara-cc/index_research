#ifndef bft_H
#define bft_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define BFT_UNIT_SIZE 4

#define BFT_HDR_SIZE 9

#define BFT_MAX_KEY_PREFIX_LEN 60

class bft_iterator_status {
public:
    byte *t;
    int keyPos;
    byte is_next;
    byte is_child_pending;
    byte tp[BFT_MAX_KEY_PREFIX_LEN];
    bft_iterator_status(byte *trie, byte prefix_len) {
        t = trie + 1;
        keyPos = prefix_len;
        is_child_pending = is_next = 0;
    }
};

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class bft : public bpt_trie_handler<bft> {
public:
    byte *last_t;
    byte *split_buf;
    const static byte need_counts[10];
    byte last_child_pos;
    byte to_pick_leaf;
    bft(uint16_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint16_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
        bpt_trie_handler(leaf_block_sz, parent_block_sz, cache_sz, fname) {
        split_buf = (byte *) util::alignedAlloc(leaf_block_size > parent_block_size ?
                leaf_block_size : parent_block_size);
    }
    ~bft() {
        delete split_buf;
    }

    inline void setCurrentBlockRoot() {
        current_block = root_block;
        trie = current_block + BFT_HDR_SIZE;
    }

    inline void setCurrentBlock(byte *m) {
        current_block = m;
        trie = current_block + BFT_HDR_SIZE;
    }

    inline int16_t searchCurrentBlock() {
        byte *t;
        t = trie;
        last_t = trie + 1;
        keyPos = 0;
        byte key_char = key[keyPos++];
        last_child_pos = 0;
        do {
            origPos = t;
            while (key_char > *t) {
                last_t = ++t;
    #if BFT_UNIT_SIZE == 3
                if (*t & x40) {
                    byte r_children = *t & x3F;
                    if (r_children)
                        last_t = current_block + getLastPtrOfChild(t + r_children * 3);
                    else
                        last_t = current_block + get9bitPtr(t);
                    t += 2;
                        triePos = t;
                        insertState = INSERT_AFTER;
                    return -1;
                }
                t += 2;
    #else
                if (*t & x80) {
                    byte r_children = *t & x7F;
                    if (r_children)
                        last_t = current_block + getLastPtrOfChild(t + r_children * 4);
                    else
                        last_t = current_block + util::getInt(t + 1);
                    t += 3;
                        triePos = t;
                        insertState = INSERT_AFTER;
                    return -1;
                }
                t += 3;
    #endif
                last_child_pos = 0;
                to_pick_leaf = 0;
                origPos = t;
            }
            if (key_char < *t++) {
                if (!isLeaf())
                   last_t = getLastPtr(last_t);
                    insertState = INSERT_BEFORE;
                return -1;
            }
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
            switch ((ptr ? 2 : 0) | ((r_children && keyPos != key_len) ? 1 : 0)) {
            //switch (ptr ?
            //        (r_children ? (keyPos == key_len ? 2 : 1) : 2) :
            //        (r_children ? (keyPos == key_len ? 0 : 4) : 0)) {
            case 0:
                if (!isLeaf())
                   last_t = getLastPtr(last_t);
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
                if (cmp == 0) {
                    last_t = key_at - 1;
                    return ptr;
                }
                    insertState = INSERT_THREAD;
                    if (cmp < 0) {
                        if (!isLeaf())
                            last_t = getLastPtr(last_t);
                        cmp = -cmp;
                    } else
                        last_t = key_at - 1;
                    need_count = (cmp * BFT_UNIT_SIZE) + BFT_UNIT_SIZE * 2;
                return -1;
            case 3:
                last_t = t;
                to_pick_leaf = 1;
                break;
            }
            last_child_pos = t - trie;
            t += (r_children * BFT_UNIT_SIZE);
            t--;
            key_char = key[keyPos++];
        } while (1);
        return -1;
    }

    inline byte *getPtrPos() {
        return NULL;
    }

    inline byte *getChildPtrPos(int16_t search_result) {
        return last_t;
    }

    inline int getHeaderSize() {
        return BFT_HDR_SIZE;
    }

    int16_t nextPtr(bft_iterator_status& s) {
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
        } while (1); // (s.t - trie) < BPT_TRIE_LEN);
        return 0;
    }

    int16_t getLastPtrOfChild(byte *t) {
        do {
    #if BFT_UNIT_SIZE == 3
            if (*t & x40) {
                byte children = (*t & x3F);
                if (children)
                    t += (children * 3);
                else
                    return get9bitPtr(t);
            } else
                t += 3;
    #else
            if (*t & x80) {
                byte children = (*t & x7F);
                if (children)
                    t += (children * 4);
                else
                    return util::getInt(t + 1);
            } else
                t += 4;
    #endif
        } while (1);
        return -1;
    }

    byte *getLastPtr(byte *last_t) {
    #if BFT_UNIT_SIZE == 3
        byte last_child = (*last_t & x3F);
        if (!last_child || to_pick_leaf)
            return current_block + get9bitPtr(last_t);
        return current_block + getLastPtrOfChild(last_t + (last_child * 3));
    #else
        byte last_child = (*last_t & x7F);
        if (!last_child || to_pick_leaf)
            return current_block + util::getInt(last_t + 1);
        return current_block + getLastPtrOfChild(last_t + (last_child * 4));
    #endif
    }

    void appendPtr(int16_t p) {
    #if BFT_UNIT_SIZE == 3
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

    int16_t deletePrefix(int16_t prefix_len) {
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
            BPT_TRIE_LEN -= 3;
            memmove(delete_start, delete_start + 3, BPT_TRIE_LEN);
            t -= 3;
    #else
            t += (*t & x7F);
            BPT_TRIE_LEN -= 4;
            memmove(delete_start, delete_start + 4, BPT_TRIE_LEN);
            t -= 4;
    #endif
        }
        return prefix_len + 1;
    }

    void addFirstData() {
        addData(0);
    }

    void addData(int16_t search_result) {

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
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + keyPos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);
        setFilledSize(filledSize() + 1);

    }

    bool isFull(int16_t search_result) {
        decodeNeedCount();
    #if BFT_UNIT_SIZE == 3
        if ((BPT_TRIE_LEN + need_count) > 189) {
            //if ((origPos - trie) <= (72 + need_count)) {
                return true;
            //}
        }
    #endif
        if (getKVLastPos() < (BFT_HDR_SIZE + BPT_TRIE_LEN
                + need_count + key_len - keyPos + value_len + 3)) {
            return true;
        }
        if (BPT_TRIE_LEN > 254 - need_count)
            return true;
        return false;
    }

    byte *split(byte *first_key, int16_t *first_len_ptr) {
        int16_t orig_filled_size = filledSize();
        const uint16_t BFT_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
        byte *b = allocateBlock(BFT_NODE_SIZE);
        bft new_block;
        new_block.setCurrentBlock(b);
        new_block.initCurrentBlock();
        new_block.setKVLastPos(BFT_NODE_SIZE);
        if (!isLeaf())
            new_block.setLeaf(0);
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        bft old_block;
        old_block.setCurrentBlock(split_buf);
        old_block.initCurrentBlock();
        if (!isLeaf())
            old_block.setLeaf(0);
        old_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        old_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        bft *ins_block = &old_block;
        int16_t kv_last_pos = getKVLastPos();
        int16_t halfKVLen = BFT_NODE_SIZE - kv_last_pos + 1;
        halfKVLen /= 2;

        int16_t brk_idx = 0;
        int16_t tot_len = 0;
        // (1) move all data to new_block in order
        int16_t idx;
        byte ins_key[BPT_MAX_PFX_LEN], old_first_key[BPT_MAX_PFX_LEN];
        int16_t ins_key_len, old_first_len;
        bft_iterator_status s(trie, 0); //BPT_MAX_PFX_LEN);
        for (idx = 0; idx < orig_filled_size; idx++) {
            int16_t src_idx = nextPtr(s);
            int16_t kv_len = current_block[src_idx];
            ins_key_len = kv_len;
            memcpy(ins_key + s.keyPos + 1, current_block + src_idx + 1, kv_len);
            kv_len++;
            ins_block->value_len = current_block[src_idx + kv_len];
            kv_len++;
            ins_block->value = (const char *) current_block + src_idx + kv_len;
            kv_len += ins_block->value_len;
            tot_len += kv_len;
            for (int i = 0; i <= s.keyPos; i++)
                ins_key[i] = trie[s.tp[i] - 1];
            //for (int i = 0; i < BPT_MAX_PFX_LEN; i++)
            //    ins_key[i] = key[i];
            ins_key_len += s.keyPos;
            ins_key_len++;
            if (idx == 0) {
                memcpy(old_first_key, ins_key, ins_key_len);
                old_first_len = ins_key_len;
            }
            //ins_key[ins_key_len] = 0;
            //cout << ins_key << endl;
            ins_block->key = (char *) ins_key;
            ins_block->key_len = ins_key_len;
            if (idx && brk_idx >= 0)
                ins_block->searchCurrentBlock();
            ins_block->addData(0);
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
                //first_key[*first_len_ptr] = 0;
                //cout << (int) isLeaf() << "First key:" << first_key << endl;
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
        memcpy(current_block, old_block.current_block, BFT_NODE_SIZE);

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
            if (BPT_MAX_PFX_LEN) {
                new_block.deletePrefix(BPT_MAX_PFX_LEN);
                new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
            }
            prefix -= deletePrefix(prefix);
            BPT_MAX_PFX_LEN = prefix;
        }*/

        return new_block.current_block;
    }

    int16_t getFirstPtr() {
        bft_iterator_status s(trie, 0);
        return nextPtr(s);
    }

    int16_t insertCurrent() {
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
            min = util::min16(key_len, keyPos + key_at_len);
            origPos++;
    #if BFT_UNIT_SIZE == 3
            *origPos |= ((BPT_TRIE_LEN - (origPos - trie - 1)) / 3);
            ptr = *(origPos + 1);
            if (*origPos & x80)
                ptr |= x100;
    #else
            *origPos |= ((BPT_TRIE_LEN - (origPos - trie - 1)) / 4);
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
                        pos = BPT_TRIE_LEN;
                        appendPtr(ptr);
                    } else {
                        ret = BPT_TRIE_LEN;
                        appendPtr(0);
                    }
                    append(c2);
                    append(BFT_UNIT_SIZE == 3 ? x40 : x80);
                    if (swap) {
                        ret = BPT_TRIE_LEN;
                        appendPtr(0);
                    } else {
                        pos = BPT_TRIE_LEN;
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
                        ret = BPT_TRIE_LEN;
                        appendPtr(0);
                    } else {
                        pos = BPT_TRIE_LEN;
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
                    pos = BPT_TRIE_LEN;
                    appendPtr(ptr);
                    keyPos--;
                } else {
                    ret = BPT_TRIE_LEN;
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
                    current_block[p] = key_at_len;
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
            ret = BPT_TRIE_LEN;
            appendPtr(0);
            keyPos = 1;
            break;
        }

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

        if (BPT_MAX_PFX_LEN <= keyPos)
            BPT_MAX_PFX_LEN = keyPos;

        return ret;
    }

    void updatePtrs(byte *upto, int diff) {
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

    void decodeNeedCount() {
        if (insertState != INSERT_THREAD)
            need_count = need_counts[insertState];
    }
};

#endif
