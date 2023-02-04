#ifndef SUCCIX_H
#define SUCCIX_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

#define SCX_NODE_COUNT_P current_block + 5
#define SCX_PTR_COUNT_P current_block + 7

using namespace std;

#define HAS_CHILD_MASK 0x01
#define IS_LEAF_MASK 0x02
#define CHILD_LEAF_MASK 0x03
#define LAST_CHILD_MASK 0x04

#define SUCCIX_HDR_SIZE 9

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class succix : public bpt_trie_handler<succix> {
public:
    uint8_t *last_t;
    uint8_t *split_buf;
    const static uint8_t need_counts[10];
    uint8_t last_child_pos;
    uint8_t to_pick_leaf;
    succix(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
        bpt_trie_handler(leaf_block_sz, parent_block_sz, cache_sz, fname) {
        split_buf = (uint8_t *) util::alignedAlloc(leaf_block_size > parent_block_size ?
                leaf_block_size : parent_block_size);
    }

    succix(uint32_t block_sz, uint8_t *block) :
      bpt_trie_handler<succix>(block_sz, block) {
        init_stats();
    }

    ~succix() {
        delete split_buf;
    }

    inline void setCurrentBlockRoot() {
        current_block = root_block;
        trie = current_block + SUCCIX_HDR_SIZE;
    }

    inline void setCurrentBlock(uint8_t *m) {
        current_block = m;
        trie = current_block + SUCCIX_HDR_SIZE;
    }

    inline int16_t searchCurrentBlock() {
        uint8_t *elouds_bm = current_block + 9;
        int node_count = util::getInt(SCX_NODE_COUNT_P);
        uint8_t *nodes_ptr = elouds_bm + (node_count >> 1) + (node_count & 0x01);
        int cur_node = 0;
        int has_child_count = 0;
        int last_child_count = 0;
        uint8_t key_char = key[0];
        do {
            uint8_t trie_char = nodes_ptr[cur_node];
            uint8_t flags = elouds_bm[cur_node >> 1] << (cur_node & 0x01 ? 0 : 4);
            has_child_count += (flags & HAS_CHILD_MASK);
            if (key_char == trie_char) {
                int sw = (flags & IS_LEAF_MASK ? 2 : 0) | (flags & HAS_CHILD_MASK && keyPos != key_len ? 1 : 0);
                switch (sw) {
                    case 0:
                        insertState = INSERT_LEAF;
                        return -1;
                    case 2:
                        int16_t cmp;
                        int16_t ptr = nodes_ptr[++cur_node];
                        uint8_t ptr_flags = elouds_bm[cur_node >> 1] << (cur_node & 0x01 ? 0 : 4);
                        cur_node++;
                        if ((ptr_flags & CHILD_LEAF_MASK) == 0)
                          ptr += nodes_ptr[cur_node];
                        key_at = current_block + ptr;
                        key_at_len = *key_at++;
                        cmp = util::compare(key + keyPos, key_len - keyPos, key_at, key_at_len);
                        if (cmp == 0) {
                            last_t = key_at - 1;
                            return ptr;
                        }
                        insertState = INSERT_THREAD;
                        if (cmp < 0)
                            cmp = -cmp;
                        need_count = cmp + cmp / 2;
                        return -1;
                }
                int diff = last_child_count - has_child_count;
                do {
                    cur_node++;
                    flags = elouds_bm[cur_node >> 1] >> (cur_node & 0x01 ? 0 : 4);
                    has_child_count += (flags & HAS_CHILD_MASK);
                    if (flags & LAST_CHILD_MASK) {
                        diff--;
                        last_child_count++;
                    }
                } while (diff);
                continue;
            } else if (key_char < trie_char) {
                insertState = INSERT_BEFORE;
                return -1;
            } else {
                if (flags & LAST_CHILD_MASK) { // is_last_child
                    insertState = INSERT_AFTER;
                    return -1;
                }
            }
            last_child_count += flags & LAST_CHILD_MASK ? 1 : 0;
        } while (++cur_node < node_count);
        return -1;
    }

    inline uint8_t *getPtrPos() {
        return NULL;
    }

    inline uint8_t *getChildPtrPos(int16_t search_result) {
        return last_t;
    }

    inline int getHeaderSize() {
        return SUCCIX_HDR_SIZE;
    }

    int16_t getLastPtrOfChild(uint8_t *t) {
        do {
            if (*t & x80) {
                uint8_t children = (*t & x7F);
                if (children)
                    t += (children * 4);
                else
                    return util::getInt(t + 1);
            } else
                t += 4;
        } while (1);
        return -1;
    }

    uint8_t *getLastPtr(uint8_t *last_t) {
        uint8_t last_child = (*last_t & x7F);
        if (!last_child || to_pick_leaf)
            return current_block + util::getInt(last_t + 1);
        return current_block + getLastPtrOfChild(last_t + (last_child * 4));
    }

    void appendPtr(int16_t p) {
        util::setInt(trie + BPT_TRIE_LEN, p);
        BPT_TRIE_LEN += 2;
    }

    void addFirstData() {
        util::setInt(SCX_NODE_COUNT_P, 0);
        util::setInt(SCX_PTR_COUNT_P, 0);
        addData(0);
    }

    void addData(int16_t search_result) {

        int16_t ptr = insertCurrent();

        int16_t key_left = key_len - keyPos;
        int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
        setKVLastPos(kv_last_pos);
        util::setInt(trie + ptr, kv_last_pos);
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + keyPos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);
        setFilledSize(filledSize() + 1);

    }

    bool isFull(int16_t search_result) {
        decodeNeedCount();
        if (getKVLastPos() < (SUCCIX_HDR_SIZE + BPT_TRIE_LEN
                + need_count + key_len - keyPos + value_len + 3)) {
            return true;
        }
        if (BPT_TRIE_LEN > 254 - need_count)
            return true;
        return false;
    }

    uint8_t *split(uint8_t *first_key, int16_t *first_len_ptr) {
        int16_t orig_filled_size = filledSize();
        const uint16_t SUCCIX_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
        int lvl = current_block[0] & 0x1F;
        uint8_t *b = allocateBlock(SUCCIX_NODE_SIZE, isLeaf(), lvl);
        succix new_block(SUCCIX_NODE_SIZE, b);
        new_block.setKVLastPos(SUCCIX_NODE_SIZE);
        if (!isLeaf())
            new_block.setLeaf(0);
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        succix old_block(SUCCIX_NODE_SIZE, split_buf);
        if (!isLeaf())
            old_block.setLeaf(0);
        old_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        old_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        succix *ins_block = &old_block;
        int16_t kv_last_pos = getKVLastPos();
        int16_t halfKVLen = SUCCIX_NODE_SIZE - kv_last_pos + 1;
        halfKVLen /= 2;

        int16_t brk_idx = 0;
        int16_t tot_len = 0;
        // (1) move all data to new_block in order
        int16_t idx;
        uint8_t ins_key[BPT_MAX_PFX_LEN], old_first_key[BPT_MAX_PFX_LEN];
        int16_t ins_key_len, old_first_len;
        succix_iterator_status s(trie, 0); //BPT_MAX_PFX_LEN);
        for (idx = 0; idx < orig_filled_size; idx++) {
            int16_t src_idx = nextPtr(s);
            int16_t kv_len = current_block[src_idx];
            ins_key_len = kv_len;
            memcpy(ins_key + s.keyPos + 1, current_block + src_idx + 1, kv_len);
            kv_len++;
            ins_block->value_len = current_block[src_idx + kv_len];
            kv_len++;
            ins_block->value = current_block + src_idx + kv_len;
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
            ins_block->key = ins_key;
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
        memcpy(current_block, old_block.current_block, SUCCIX_NODE_SIZE);

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
        succix_iterator_status s(trie, 0);
        return nextPtr(s);
    }

    int16_t insertCurrent() {
        uint8_t key_char;
        int16_t ret, ptr, pos;
        ret = pos = 0;

        switch (insertState) {
        case INSERT_AFTER:
            key_char = key[keyPos - 1];
            origPos++;
            *origPos &= (SUCCIX_UNIT_SIZE == 3 ? xBF : x7F);
            updatePtrs(triePos, 1);
            insAt(triePos, key_char, x80, 0, 0);
            ret = triePos - trie + 2;
            break;
        case INSERT_BEFORE:
            key_char = key[keyPos - 1];
            updatePtrs(origPos, 1);
            if (keyPos > 1 && last_child_pos)
                trie[last_child_pos]--;
            insAt(origPos, key_char, x00, 0, 0);
            ret = origPos - trie + 2;
            break;
        case INSERT_LEAF:
            key_char = key[keyPos - 1];
            ret = origPos - trie + 2;
            break;
        case INSERT_THREAD:
            int16_t p, min;
            uint8_t c1, c2;
            key_char = key[keyPos - 1];
            c1 = c2 = key_char;
            p = keyPos;
            min = util::min16(key_len, keyPos + key_at_len);
            origPos++;
            *origPos |= ((BPT_TRIE_LEN - (origPos - trie - 1)) / 4);
            ptr = util::getInt(origPos + 1);
            if (p < min) {
                util::setInt(origPos + 1, 0);
            } else {
                origPos++;
                pos = origPos - trie;
                ret = pos;
            }
            while (p < min) {
                uint8_t swap = 0;
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
                    append(SUCCIX_UNIT_SIZE == 3 ? x40 : x80);
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
                    append(SUCCIX_UNIT_SIZE == 3 ? x41 : x81);
                    appendPtr(0);
                    break;
                case 2:
                    append(c1);
                    append(SUCCIX_UNIT_SIZE == 3 ? x41 : x81);
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
                append(SUCCIX_UNIT_SIZE == 3 ? x40 : x80);
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
                    util::setInt(trie + pos, p);
                }
            }
            break;
        case INSERT_EMPTY:
            key_char = *key;
            append(key_char);
            append(SUCCIX_UNIT_SIZE == 3 ? x40 : x80);
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

    void decodeNeedCount() {
        if (insertState != INSERT_THREAD)
            need_count = need_counts[insertState];
    }

    void cleanup() {
    }

};

#endif
