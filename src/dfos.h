#ifndef dfos_H
#define dfos_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#define DS_MIDDLE_PREFIX 1

#if BPT_9_BIT_PTR == 1
#define DS_MAX_PTR_BITMAP_BYTES 8
#define DS_MAX_PTRS 63
#else
#define DS_MAX_PTR_BITMAP_BYTES 0
#define DS_MAX_PTRS 240
#endif
#define DFOS_HDR_SIZE 9
//#define MID_KEY_LEN buf[DS_MAX_PTR_BITMAP_BYTES+6]

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class dfos : public bpt_trie_handler<dfos> {
public:
    byte pos, key_at_pos;
    const static byte need_counts[10];
    const static byte switch_map[8];

    dfos(uint16_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint16_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE) :
        bpt_trie_handler<dfos>(leaf_block_sz, parent_block_sz) {
    }

    void insAtWithPtrs(byte *ptr, const char *s, byte len) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
    #else
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
    #endif
        memcpy(ptr, s, len);
        BPT_TRIE_LEN += len;
    }

    void insAtWithPtrs(byte *ptr, byte b, const char *s, byte len) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 1 + len, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
    #else
        memmove(ptr + 1 + len, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
    #endif
        *ptr++ = b;
        memcpy(ptr, s, len);
        BPT_TRIE_LEN += len;
        BPT_TRIE_LEN++;
    }

    byte insAtWithPtrs(byte *ptr, byte b1, byte b2) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 2, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
    #else
        memmove(ptr + 2, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr = b2;
        BPT_TRIE_LEN += 2;
        return 2;
    }

    byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 3, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
    #else
        memmove(ptr + 3, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        BPT_TRIE_LEN += 3;
        return 3;
    }

    byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 4, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
    #else
        memmove(ptr + 4, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        BPT_TRIE_LEN += 4;
        return 4;
    }

    byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4,
            byte b5) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 5, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
    #else
        memmove(ptr + 5, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr++ = b4;
        *ptr = b5;
        BPT_TRIE_LEN += 5;
        return 5;
    }

    byte insAtWithPtrs(byte *ptr, byte b1, byte b2, byte b3, byte b4,
            byte b5, byte b6) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 6, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
    #else
        memmove(ptr + 6, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr++ = b4;
        *ptr++ = b5;
        *ptr = b6;
        BPT_TRIE_LEN += 6;
        return 6;
    }

    void insBytesWithPtrs(byte *ptr, int16_t len) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
    #else
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
    #endif
        BPT_TRIE_LEN += len;
    }

    byte insChildAndLeafAt(byte *ptr, byte b1, byte b2) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + 5, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
    #else
        memmove(ptr + 5, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
    #endif
        *ptr++ = b1;
        *ptr++ = x02;
        *ptr++ = x05;
        *ptr++ = b2;
        *ptr = b2;
        BPT_TRIE_LEN += 5;
        return 5;
    }

    void append(byte b) {
        trie[BPT_TRIE_LEN++] = b;
    }

    byte *nextKey(byte *first_key, byte *tp, byte *t, char& ctr, byte& tc, byte& child, byte& leaf) {
        do {
            while (ctr > x07) {
                if (tc & x04) {
                    keyPos--;
                    tc = trie[tp[keyPos]];
                    while (tc & x01) {
                        keyPos--;
                        tc = trie[tp[keyPos]];
                    }
                    child = (tc & x02 ? trie[tp[keyPos] + 3] : 0);
                    leaf = trie[tp[keyPos] + (tc & x02 ? 4 : 1)];
                    ctr = first_key[keyPos] & 0x07;
                    ctr++;
                } else {
                    tp[keyPos] = t - trie;
                    tc = *t++;
                    if (tc & x01) {
                        byte len = tc >> 1;
                        memset(tp + keyPos, t - trie - 1, len);
                        memcpy(first_key + keyPos, t, len);
                        t += len;
                        keyPos += len;
                        tp[keyPos] = t - trie;
                        tc = *t++;
                    }
                    t += (tc & x02 ? 2 : 0);
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
                keyPos++;
                ctr = x08;
                tc = 0;
            }
            ctr++;
        } while (1); // (t - trie) < BPT_TRIE_LEN);
        return t;
    }

    void updateSkipLens(byte *loop_upto, byte *covering_upto, int diff) {
        byte *t = trie;
        byte tc = *t++;
        loop_upto++;
        while (t <= loop_upto) {
            if (tc & x01) {
                t += (tc >> 1);
            } else if (tc & x02) {
                t++;
                if ((t + *t) > covering_upto) {
                    *t = *t + diff;
                    (*(t-1))++;
                    t += 3;
                } else
                    t += *t;
            } else
                t++;
            tc = *t++;
        }
    }
    void movePtrList(byte orig_trie_len) {
    #if BPT_9_BIT_PTR == 1
        memmove(trie + BPT_TRIE_LEN, trie + orig_trie_len, filledSize());
    #else
        memmove(trie + BPT_TRIE_LEN, trie + orig_trie_len, filledSize() << 1);
    #endif
    }

    void deleteTrieLastHalf(int16_t brk_key_len, byte *first_key, byte *tp) {
        byte orig_trie_len = BPT_TRIE_LEN;
        for (int idx = brk_key_len; idx >= 0; idx--) {
            byte *t = trie + tp[idx];
            byte tc = *t;
            if (tc & x01)
                continue;
            byte offset = first_key[idx] & x07;
            *t++ = (tc | x04);
            if (tc & x02) {
                byte children = t[2];
                children &= ~((idx == brk_key_len ? xFF : xFE) << offset);
                                // ryte_mask[offset] : ryte_incl_mask[offset]);
                t[2] = children;
                byte leaves = t[3] & ~(xFE << offset);
                t[3] = leaves; // ryte_incl_mask[offset];
                pos = BIT_COUNT(leaves);
                byte *new_t = skipChildren(t + 4, BIT_COUNT(children));
                *t++ = pos;
                *t = new_t - t;
                t = new_t;
            } else
                *t++ &= ~(xFE << offset); // ryte_incl_mask[offset];
            if (idx == brk_key_len)
                BPT_TRIE_LEN = t - trie;
        }
        movePtrList(orig_trie_len);
    }

    int deleteSegment(byte *delete_end, byte *delete_start) {
        int count = delete_end - delete_start;
        if (count) {
            BPT_TRIE_LEN -= count;
            memmove(delete_start, delete_end, trie + BPT_TRIE_LEN - delete_start);
        }
        return count;
    }

    void deleteTrieFirstHalf(int16_t brk_key_len, byte *first_key, byte *tp) {
        byte orig_trie_len = BPT_TRIE_LEN;
        for (int idx = brk_key_len; idx >= 0; idx--) {
            byte *t = trie + tp[idx];
            byte tc = *t++;
            if (tc & x01) {
                byte len = tc >> 1;
                deleteSegment(trie + tp[idx + 1], t + len);
                idx -= len;
                idx++;
                continue;
            }
            byte offset = first_key[idx] & 0x07;
            if (tc & x02) {
                byte children = t[2];
                int16_t count = BIT_COUNT(children & ~(xFF << offset)); // ryte_mask[offset];
                children &= (xFF << offset); // left_incl_mask[offset];
                t[2] = children;
                byte leaves = t[3] & ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
                t[3] = leaves;
                byte *new_t = skipChildren(t + 4, count);
                deleteSegment((idx < brk_key_len) ? (trie + tp[idx + 1]) : new_t, t + 4);
                pos = BIT_COUNT(leaves);
                new_t = skipChildren(t + 4, BIT_COUNT(children));
                *t++ = pos;
                *t = new_t - t;
            } else
                *t &= ((idx == brk_key_len ? xFF : xFE) << offset); // left_incl_mask[offset] : left_mask[offset]);
        }
        deleteSegment(trie + tp[0], trie);
        movePtrList(orig_trie_len);
    }

    void consolidateInitialPrefix(byte *t) {
        t += DFOS_HDR_SIZE;
        byte *t_reader = t;
        if (*t & x01) {
            t_reader += (*t >> 1);
            t_reader++;
        }
        byte *t_writer = t_reader + (*t & x01 ? 0 : 1);
        byte count = 0;
        byte trie_len_diff = 0;
        while ((*t_reader & x01) || ((*t_reader & x02) && (*t_reader & x04) && BIT_COUNT(t_reader[3]) == 1
                && BIT_COUNT(t_reader[4]) == 0)) {
            if (*t_reader & x01) {
                byte len = *t_reader++ >> 1;
                memcpy(t_writer, t_reader, len);
                t_writer += len;
                t_reader += len;
                count += len;
                trie_len_diff++;
            } else {
                *t_writer++ = (*t_reader & xF8) + BIT_COUNT(t_reader[3] - 1);
                t_reader += 5;
                count++;
                trie_len_diff += 4;
            }
        }
        if (t_reader > t_writer) {
            memmove(t_writer, t_reader, BPT_TRIE_LEN - (t_reader - t) + filledSize() * 2);
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

    void addFirstData() {
        addData(0);
    }

    void addData(int16_t idx) {

        insertCurrent();

        int16_t key_left = key_len - keyPos;
        int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
        setKVLastPos(kv_last_pos);
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + keyPos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);

        insPtr(idx, kv_last_pos);

    }

    bool isFull() {
        decodeNeedCount();
        int16_t ptr_size = filledSize() + 1;
    #if BPT_9_BIT_PTR == 0
        ptr_size <<= 1;
    #endif
        if (getKVLastPos()
                < (DFOS_HDR_SIZE + DS_MAX_PTR_BITMAP_BYTES + BPT_TRIE_LEN
                        + need_count + ptr_size + key_len - keyPos + value_len + 3))
            return true;
        if (filledSize() > DS_MAX_PTRS)
            return true;
        if (BPT_TRIE_LEN > (254 - need_count))
            return true;
        return false;
    }

    byte *split(byte *first_key, int16_t *first_len_ptr) {
        int16_t orig_filled_size = filledSize();
        const uint16_t DFOS_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
        byte *b = (byte *) util::alignedAlloc(DFOS_NODE_SIZE);
        dfos new_block;
        new_block.setCurrentBlock(b);
        new_block.initCurrentBlock();
        new_block.setKVLastPos(DFOS_NODE_SIZE);
        if (!isLeaf())
            new_block.setLeaf(false);
        memcpy(new_block.trie, trie, BPT_TRIE_LEN);
        new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        int16_t kv_last_pos = getKVLastPos();
        int16_t halfKVLen = DFOS_NODE_SIZE - kv_last_pos + 1;
        halfKVLen /= 2;

        int16_t brk_idx = -1;
        int16_t brk_kv_pos;
        int16_t tot_len;
        brk_kv_pos = tot_len = 0;
        char ctr = x08;
        byte tp[BPT_MAX_PFX_LEN + 1];
        byte last_key[BPT_MAX_PFX_LEN + 1];
        int16_t last_key_len = 0;
        byte *t = trie;
        byte tc, child, leaf;
        tc = child = leaf = 0;
        //if (!isLeaf())
        //   cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DS_MAX_KEY_LEN << endl;
        keyPos = 0;
        // (1) move all data to new_block in order
        int16_t idx;
        for (idx = 0; idx < orig_filled_size; idx++) {
            int16_t src_idx = getPtr(idx);
            int16_t kv_len = current_block[src_idx];
            kv_len++;
            kv_len += current_block[src_idx + kv_len];
            kv_len++;
            tot_len += kv_len;
            memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
            kv_last_pos += kv_len;
            if (brk_idx == -1) {
                t = nextKey(first_key, tp, t, ctr, tc, child, leaf);
                //brk_key_len = nextKey(s);
                //if (tot_len > halfKVLen) {
                //memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
                //first_key[keyPos+1+current_block[src_idx]] = 0;
                //cout << first_key << endl;
                if (tot_len > halfKVLen || idx == (orig_filled_size / 2)) {
                    //memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[keyPos+1+current_block[src_idx]] = 0;
                    //cout << first_key << ":";
                    brk_idx = idx + 1;
                    brk_kv_pos = kv_last_pos;
                    last_key_len = keyPos + 1;
                    memcpy(last_key, first_key, last_key_len);
                    deleteTrieLastHalf(keyPos, first_key, tp);
                    new_block.keyPos = keyPos;
                    t = new_block.trie + (t - trie);
                    t = new_block.nextKey(first_key, tp, t, ctr, tc, child, leaf);
                    keyPos = new_block.keyPos;
                    //src_idx = getPtr(idx + 1);
                    //memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[keyPos+1+current_block[src_idx]] = 0;
                    //cout << first_key << endl;
                }
            }
        }
        kv_last_pos = getKVLastPos() + DFOS_NODE_SIZE - kv_last_pos;
        new_block.setKVLastPos(kv_last_pos);
        memmove(new_block.current_block + kv_last_pos, new_block.current_block + getKVLastPos(), DFOS_NODE_SIZE - kv_last_pos);
        brk_kv_pos += (kv_last_pos - getKVLastPos());
        int16_t diff = DFOS_NODE_SIZE - brk_kv_pos;
        for (idx = 0; idx < orig_filled_size; idx++) {
            new_block.insPtr(idx, kv_last_pos + (idx < brk_idx ? diff : 0));
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
        }
        kv_last_pos = new_block.getKVLastPos();

    #if BPT_9_BIT_PTR == 1
        memcpy(current_block + DFOS_HDR_SIZE, new_block.current_block + DFOS_HDR_SIZE, DS_MAX_PTR_BITMAP_BYTES);
        memcpy(trie + BPT_TRIE_LEN, new_block.trie + new_block.BPT_TRIE_LEN, brk_idx);
    #else
        memcpy(trie + BPT_TRIE_LEN, new_block.trie + new_block.BPT_TRIE_LEN, (brk_idx << 1));
    #endif

        {
            if (isLeaf()) {
                // *first_len_ptr = keyPos + 1;
                *first_len_ptr = util::compare((const char *) first_key, keyPos + 1, (const char *) last_key, last_key_len);
            } else {
                new_block.key_at = new_block.getKey(brk_idx, &new_block.key_at_len);
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
            memcpy(current_block + DFOS_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            setKVLastPos(DFOS_NODE_SIZE - old_blk_new_len);
            setFilledSize(brk_idx);
        }

        {
    #if BPT_9_BIT_PTR == 1
    #if BPT_INT64MAP == 1
            (*new_block.bitmap) <<= brk_idx;
    #else
            if (brk_idx & 0xFFE0)
            *new_block.bitmap1 = *new_block.bitmap2 << (brk_idx - 32);
            else {
                *new_block.bitmap1 <<= brk_idx;
                *new_block.bitmap1 |= (*new_block.bitmap2 >> (32 - brk_idx));
            }
    #endif
    #endif
            int16_t new_size = orig_filled_size - brk_idx;
            byte *block_ptrs = new_block.trie + new_block.BPT_TRIE_LEN;
    #if BPT_9_BIT_PTR == 1
            memmove(block_ptrs, block_ptrs + brk_idx, new_size);
    #else
            memmove(block_ptrs, block_ptrs + (brk_idx << 1), new_size << 1);
    #endif
            new_block.setKVLastPos(brk_kv_pos);
            new_block.setFilledSize(new_size);
        }

        consolidateInitialPrefix(current_block);
        new_block.consolidateInitialPrefix(new_block.current_block);

        return new_block.current_block;

    }

    void insertCurrent() {
        byte key_char, mask;
        int16_t diff;

        key_char = key[keyPos - 1];
        mask = x01 << (key_char & x07);
        switch (insertState) {
        case INSERT_AFTER:
            *origPos &= xFB;
            triePos = ((*origPos & x02) ? origPos + origPos[2] : origPos) + 2;
            insAtWithPtrs(triePos, ((key_char & xF8) | x04), mask);
            if (keyPos > 1)
                updateSkipLens(origPos - 1, triePos - 1, 2);
            break;
        case INSERT_BEFORE:
            insAtWithPtrs(origPos, key_char & xF8, mask);
            if (keyPos > 1)
                updateSkipLens(origPos, origPos + 1, 2);
            break;
        case INSERT_LEAF:
            if (*origPos & x02)
                origPos[4] |= mask;
            else
                origPos[1] |= mask;
            updateSkipLens(origPos, origPos + 1, 0);
            break;
    #if DS_MIDDLE_PREFIX == 1
        case INSERT_CONVERT:
            byte b, c;
            char cmp_rel;
            diff = triePos - origPos;
            // 3 possible relationships between key_char and *triePos, 4 possible positions of triePos
            c = *triePos;
            cmp_rel = ((c ^ key_char) > x07 ? (c < key_char ? 0 : 1) : 2);
            if (cmp_rel == 0)
                insAtWithPtrs(triePos + key_at_pos, (key_char & xF8) | 0x04, 1 << (key_char & x07));
            if (diff == 1)
                triePos = origPos;
            b = (cmp_rel == 2 ? x04 : x00) | (cmp_rel == 1 ? x00 : x02);
            need_count = (*origPos >> 1) - diff;
            diff--;
            *triePos++ = ((cmp_rel == 0 ? c : key_char) & xF8) | b;
            b = (cmp_rel == 1 ? (diff ? 6 : 5) : (diff ? 4 : 3));
            if (diff)
                *origPos = (diff << 1) | x01;
            if (need_count)
                b++;
            insBytesWithPtrs(triePos + (diff ? 0 : 1), b);
            updateSkipLens(origPos, triePos, b + (cmp_rel == 0 ? 2 : 0));
            if (cmp_rel == 1) {
                *triePos++ = 1 << (key_char & x07);
                *triePos++ = (c & xF8) | x06;
            }
            b = pos;
            pos = (cmp_rel == 2 ? 1 : 0);
            triePos[1] = skipChildren(triePos + need_count + (need_count ? 5 : 4), 1) - triePos - 1;
            *triePos++ = pos;
            triePos++;
            pos = b;
            *triePos++ = 1 << (c & x07);
            if (cmp_rel == 2)
                *triePos++ = 1 << (key_char & x07);
            else
                *triePos++ = 0;
            if (need_count)
                *triePos = (need_count << 1) | x01;
            break;
    #endif
        case INSERT_THREAD:
            int16_t p, min;
            byte c1, c2;
            byte *fromPos;
            fromPos = triePos;
            if (*origPos & x02) {
                origPos[3] |= mask;
                origPos[1]++;
            } else {
                insAtWithPtrs(origPos + 1, BIT_COUNT(origPos[1]) + 1, x00, mask);
                triePos += 3;
                *origPos |= x02;
            }
            c1 = c2 = key_char;
            p = keyPos;
            min = util::min16(key_len, keyPos + key_at_len);
            if (p < min)
                origPos[4] &= ~mask;
    #if DS_MIDDLE_PREFIX == 1
            need_count -= 9;
            diff = p + need_count;
            if (diff == min && need_count) {
                need_count--;
                diff--;
            }
            insBytesWithPtrs(triePos, need_count + (need_count ? 1 : 0)
                    + (diff == min ? 2 : (key[diff] ^ key_at[diff - keyPos]) > x07 ? 4 :
                            (key[diff] == key_at[diff - keyPos] ? 7 : 2)));
            if (need_count) {
                *triePos++ = (need_count << 1) | x01;
                memcpy(triePos, key + keyPos, need_count);
                triePos += need_count;
                p += need_count;
                //count1 += need_count;
            }
    #else
                need_count = (p == min ? 2 : 0);
                while (p < min) {
                    c1 = key[p];
                    c2 = key_at[p - keyPos];
                    need_count += ((c1 ^ c2) > x07 ? 4 : (c1 == c2 ? (p + 1 == min ? 7 : 5) : 2));
                    if (c1 != c2)
                        break;
                    p++;
                }
                p = keyPos;
                insAtWithPtrs(triePos, (const char *) current_block, need_count);
    #endif
            while (p < min) {
                c1 = key[p];
                c2 = key_at[p - keyPos];
                if (c1 > c2) {
                    byte swap = c1;
                    c1 = c2;
                    c2 = swap;
                }
    #if DS_MIDDLE_PREFIX == 1
                switch ((c1 ^ c2) > x07 ? 0 : (c1 == c2 ? 3 : 1)) {
    #else
                switch ((c1 ^ c2) > x07 ?
                        0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
                case 2:
                    need_count -= 5;
                    *triePos++ = (c1 & xF8) | x06;
                    *triePos++ = 2;
                    *triePos++ = need_count + 3;
                    *triePos++ = x01 << (c1 & x07);
                    *triePos++ = 0;
                    break;
    #endif
                case 0:
                    *triePos++ = c1 & xF8;
                    *triePos++ = x01 << (c1 & x07);
                    *triePos++ = (c2 & xF8) | x04;
                    *triePos++ = x01 << (c2 & x07);
                    break;
                case 1:
                    *triePos++ = (c1 & xF8) | x04;
                    *triePos++ = (x01 << (c1 & x07)) | (x01 << (c2 & x07));
                    break;
                case 3:
                    *triePos++ = (c1 & xF8) | x06;
                    *triePos++ = x02;
                    *triePos++ = x05;
                    *triePos++ = x01 << (c1 & x07);
                    *triePos++ = x01 << (c1 & x07);
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
                *triePos++ = (c2 & xF8) | x04;
                *triePos++ = x01 << (c2 & x07);
                if (p == key_len)
                    keyPos--;
            }
            p = triePos - fromPos;
            updateSkipLens(origPos - 2, origPos, p);
            origPos[2] += p;
            if (diff < key_at_len)
                diff++;
            if (diff) {
                p = getPtr(key_at_pos);
                key_at_len -= diff;
                p += diff;
                if (key_at_len >= 0) {
                    current_block[p] = key_at_len;
                    setPtr(key_at_pos, p);
                }
            }
            break;
        case INSERT_EMPTY:
            append((key_char & xF8) | x04);
            append(mask);
            break;
        }

        if (BPT_MAX_PFX_LEN <= (isLeaf() ? keyPos : key_len))
            BPT_MAX_PFX_LEN = (isLeaf() ? keyPos + 1 : key_len);

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

    }

    inline byte *skipChildren(byte *t, byte count) {
        while (count) {
            byte tc = *t++;
            if (tc & x01) {
                t += (tc >> 1);
                continue;
            }
            if (tc & x04)
                count--;
            if (tc & x02) {
                pos += *t++;
                t += *t;
            } else
                pos += BIT_COUNT(*t++);
        }
        return t;
    }

    inline int16_t searchCurrentBlock() {
        byte key_char = *key;
        byte *t = trie;
        byte trie_char = *t;
        keyPos = 1;
        origPos = t++;
        pos = 0;
        do {
#if DS_MIDDLE_PREFIX == 1
            if (trie_char & x01) {
                byte pfx_len;
                pfx_len = (trie_char >> 1);
                while (pfx_len && key_char == *t && keyPos < key_len) {
                    key_char = key[keyPos++];
                    t++;
                    pfx_len--;
                }
                if (pfx_len) {
                    triePos = t;
                    if (key_char > *t) {
                        t = skipChildren(t + pfx_len, 1);
                        key_at_pos = t - triePos;
                    }
                    insertState = INSERT_CONVERT;
                    return ~pos;
                }
                trie_char = *t;
                origPos = t++;
            }
#endif
            while ((key_char & xF8) > trie_char) {
                if (trie_char & x02) {
                    pos += *t++;
                    t += *t;
                } else
                    pos += BIT_COUNT(*t++);
                if (trie_char & x04) {
                    insertState = INSERT_AFTER;
                    return ~pos;
                }
                trie_char = *t;
                origPos = t++;
            }
            if ((key_char ^ trie_char) & xF8) {
                insertState = INSERT_BEFORE;
                return ~pos;
            }
            if (trie_char & x02) {
                t += 2;
                byte r_children = *t++;
                byte r_leaves = *t++;
                byte r_mask = x01 << (key_char & x07);
                key_char = (r_leaves & r_mask ? x02 : x00) |
                        ((r_children & r_mask) && keyPos != key_len ? x01 : x00);
                //key_char = r_leaves & r_mask ?
                //        (r_children & r_mask ? (keyPos == key_len ? x01 : x03) : x01) :
                //        (r_children & r_mask ? (keyPos == key_len ? x00 : x02) : x00);
                r_mask--;
                pos += BIT_COUNT(r_leaves & r_mask);
                t = skipChildren(t, BIT_COUNT(r_children & r_mask));
            } else {
                byte r_leaves = *t++;
                byte r_mask = x01 << (key_char & x07);
                key_char = (r_leaves & r_mask) ? x02 : x00;
                pos += BIT_COUNT(r_leaves & (r_mask - 1));
            }
            switch (key_char) {
            case 0:
                triePos = t;
                insertState = INSERT_LEAF;
                return ~pos;
            case 1:
                break;
            case 2:
                int16_t cmp;
                key_at = getKey(pos, &key_at_len);
                cmp = util::compare(key + keyPos, key_len - keyPos,
                        (char *) key_at, key_at_len);
                if (cmp == 0)
                    return pos;
                key_at_pos = pos;
                if (cmp > 0)
                    pos++;
                else
                    cmp = -cmp;
                triePos = t;
                insertState = INSERT_THREAD;
#if DS_MIDDLE_PREFIX == 1
                need_count = cmp + 8;
#else
                need_count = (cmp * 5) + 5;
#endif
                return ~pos;
            case 3:
                pos++;
                break;
            }
            key_char = key[keyPos++];
            trie_char = *t;
            origPos = t++;
        } while (1);
        return ~pos;
    }

    inline byte *getChildPtrPos(int16_t idx) {
        if (idx < 0) {
            idx++;
            idx = ~idx;
        }
        return current_block + getPtr(idx);
    }

    inline int getHeaderSize() {
        return DFOS_HDR_SIZE + DS_MAX_PTR_BITMAP_BYTES;
    }

    inline byte *getPtrPos() {
        return trie + BPT_TRIE_LEN;
    }

    void decodeNeedCount() {
        if (insertState != INSERT_THREAD)
            need_count = need_counts[insertState];
    }

};

#endif
