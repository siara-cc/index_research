#ifndef dfqx_H
#define dfqx_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#if BPT_9_BIT_PTR == 1
#define DQ_MAX_PTR_BITMAP_BYTES 8
#define DQ_MAX_PTRS 63
#else
#define DQ_MAX_PTR_BITMAP_BYTES 0
#define DQ_MAX_PTRS 240
#endif
#define DFQX_HDR_SIZE 9

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class dfqx: public bpt_trie_handler<dfqx> {
public:
    const static uint8_t need_counts[10];
    static uint8_t left_mask[4];
    static uint8_t left_incl_mask[4];
    static uint8_t ryte_mask[4];
    static uint8_t dbl_ryte_mask[4];
    static uint8_t ryte_incl_mask[4];
    static uint8_t first_bit_offset[16];
    static uint8_t bit_count[16];
#if (defined(__AVR__))
    const static PROGMEM uint16_t dbl_bit_count[256];
#else
    const static uint16_t dbl_bit_count[256];
#endif
public:
    uint8_t pos, key_at_pos;

    dfqx(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, uint8_t *block = NULL) :
        bpt_trie_handler<dfqx>(leaf_block_sz, parent_block_sz, cache_sz, fname, block) {
    }

    inline void setCurrentBlockRoot() {
        setCurrentBlock(root_block);
    }

    inline void setCurrentBlock(uint8_t *m) {
        current_block = m;
        trie = current_block + DFQX_HDR_SIZE;
#if BPT_9_BIT_PTR == 1
#if BPT_INT64MAP == 1
        bitmap = (uint64_t *) (current_block + getHeaderSize() - 8);
#else
        bitmap1 = (uint32_t *) (current_block + getHeaderSize() - 8);
        bitmap2 = bitmap1 + 1;
#endif
#endif
    }

    //static uint8_t first_bit_offset4[16];
    //static uint8_t bit_count_16[16];
    inline uint8_t *skipChildren(uint8_t *t, uint16_t& count) {
        while (count & xFF) {
            uint8_t tc = *t++;
            count -= tc & x01;
            count += (tc & x02 ? *t++ << 8 : dbl_bit_count[*t++]);
            t += (tc & x02 ? *t : 0);
        }
        return t;
    }
    inline int16_t searchCurrentBlock() {
        uint8_t *t = trie;
        uint16_t to_skip = 0;
        uint8_t key_char = *key;
        keyPos = 1;
        do {
            uint8_t trie_char = *t;
            switch ((key_char ^ trie_char) > x03 ?
                    (key_char > trie_char ? 0 : 2) : 1) {
            case 0:
                origPos = t++;
                to_skip += (trie_char & x02 ? *t++ << 8 : dbl_bit_count[*t++]);
                t = (trie_char & x02 ? t + *t : skipChildren(t, to_skip));
                if (trie_char & x01) {
                        triePos = t;
                        insertState = INSERT_AFTER;
                    pos = to_skip >> 8;
                    return ~pos;
                }
                break;
            case 1:
                uint8_t r_leaves_children;
                origPos = t;
                t += (trie_char & x02 ? 3 : 1);
                r_leaves_children = *t++;
                key_char &= x03;
                //to_skip += dbl_bit_count[r_leaves_children & dbl_ryte_mask[key_char]];
                to_skip += dbl_bit_count[r_leaves_children & ((0xEECC8800 >> (key_char << 3)) & xFF)];
                t = skipChildren(t, to_skip);
                switch (((r_leaves_children << key_char) & 0x88) | (keyPos == key_len ? x01 : x00)) {
                case 0x80: // 10000000
                case 0x81: // 10000001
                    int16_t cmp;
                    pos = to_skip >> 8;
                    key_at = getKey(pos, &key_at_len);
                    cmp = util::compare(key + keyPos, key_len - keyPos, key_at, key_at_len);
                    if (cmp == 0)
                        return pos;
                    key_at_pos = pos;
                    if (cmp > 0)
                        pos++;
                    else
                        cmp = -cmp;
                        triePos = t;
                        insertState = INSERT_THREAD;
                        need_count = (cmp * 2) + 4;
                    return ~pos;
                case 0x08: // 00001000
                    break;
                case 0x89: // 10001001
                    pos = to_skip >> 8;
                    key_at = getKey(pos, &key_at_len);
                    return pos;
                case 0x88: // 10001000
                    to_skip += 0x100;
                    break;
                case 0x00: // 00000000
                case 0x01: // 00000001
                case 0x09: // 00001001
                        triePos = t;
                        insertState = INSERT_LEAF;
                    pos = to_skip >> 8;
                    return ~pos;
                }
                key_char = key[keyPos++];
                break;
            case 2:
                    triePos = t;
                    insertState = INSERT_BEFORE;
                pos = to_skip >> 8;
                return ~pos;
            }
        } while (1);
        return -1; // dummy - will never reach here
    }

    inline uint8_t *getChildPtrPos(int16_t search_result) {
        if (search_result < 0) {
            search_result++;
            search_result = ~search_result;
        }
        return current_block + getPtr(search_result);
    }

    inline uint8_t *getPtrPos() {
        return trie + BPT_TRIE_LEN;
    }

    inline int getHeaderSize() {
        return DFQX_HDR_SIZE + DQ_MAX_PTR_BITMAP_BYTES;
    }

    void append(uint8_t b) {
        trie[BPT_TRIE_LEN++] = b;
    }

    uint8_t *nextKey(uint8_t *first_key, uint8_t *tp, uint8_t *t, char& ctr, uint8_t& tc, uint8_t& child_leaf) {
        do {
            while (ctr > x03) {
                if (tc & x01) {
                    keyPos--;
                    tc = trie[tp[keyPos]];
                    child_leaf = trie[tp[keyPos] + (tc & x02 ? 3 : 1)];
                    ctr = first_key[keyPos] & 0x03;
                    ctr++;
                } else {
                    tp[keyPos] = t - trie;
                    tc = *t;
                    t += (tc & x02 ? 3 : 1);
                    child_leaf = *t++;
                    ctr = 0;
                }
            }
            first_key[keyPos] = (tc & xFC) | ctr;
            uint8_t mask = x80 >> ctr;
            if (child_leaf & mask) {
                child_leaf &= ~mask;
                if (0 == (child_leaf & (x08 >> ctr)))
                    ctr++;
                return t;
            }
            if (child_leaf & (x08 >> ctr)) {
                keyPos++;
                ctr = x04;
                tc = 0;
            }
            ctr++;
        } while (1); // (t - trie) < BPT_TRIE_LEN);
        return t;
    }

    void movePtrList(uint8_t orig_trie_len) {
    #if BPT_9_BIT_PTR == 1
        memmove(trie + BPT_TRIE_LEN, trie + orig_trie_len, filledSize());
    #else
        memmove(trie + BPT_TRIE_LEN, trie + orig_trie_len, filledSize() << 1);
    #endif
    }

    void deleteTrieLastHalf(int16_t brk_key_len, uint8_t *first_key, uint8_t *tp) {
        uint8_t orig_trie_len = BPT_TRIE_LEN;
        for (int idx = brk_key_len; idx >= 0; idx--) {
            uint8_t *t = trie + tp[idx];
            uint8_t tc = *t;
            uint8_t offset = first_key[idx] & x03;
            *t = (tc | x01);
            t += (tc & x02 ? 3 : 1);
            uint8_t children = *t & x0F;
            children &= (idx == brk_key_len ? ryte_mask[offset] : ryte_incl_mask[offset]);
            uint8_t child_leaf = (*t & (ryte_incl_mask[offset] << 4)) + children;
            *t++ = child_leaf;
            if (tc & x02 || idx == brk_key_len) {
                uint16_t count = bit_count[children] + (bit_count[child_leaf >> 4] << 8);
                uint8_t *new_t = skipChildren(t, count);
                if (tc & x02) {
                    *(t - 3) = count >> 8;
                    *(t - 2) = new_t - t + 2;
                }
                t = new_t;
                if (idx == brk_key_len)
                    BPT_TRIE_LEN = t - trie;
            }
        }
        movePtrList(orig_trie_len);
    }

    int deleteSegment(uint8_t *delete_end, uint8_t *delete_start) {
        int count = delete_end - delete_start;
        if (count) {
            BPT_TRIE_LEN -= count;
            memmove(delete_start, delete_end, trie + BPT_TRIE_LEN - delete_start);
        }
        return count;
    }

    void deleteTrieFirstHalf(int16_t brk_key_len, uint8_t *first_key, uint8_t *tp) {
        uint8_t orig_trie_len = BPT_TRIE_LEN;
        for (int idx = brk_key_len; idx >= 0; idx--) {
            uint8_t *t = trie + tp[idx];
            uint8_t tc = *t;
            t += (tc & x02 ? 3 : 1);
            uint8_t offset = first_key[idx] & 0x03;
            uint8_t children = *t & x0F;
            uint16_t count = bit_count[children & ryte_mask[offset]];
            children &= left_incl_mask[offset];
            uint8_t leaves = (*t & ((idx == brk_key_len ? left_incl_mask[offset] : left_mask[offset]) << 4));
            *t++ = (children + leaves);
            uint8_t *new_t = skipChildren(t, count);
            deleteSegment(idx < brk_key_len ? trie + tp[idx + 1] : new_t, t);
            if (tc & x02) {
                count = bit_count[children] + (bit_count[leaves >> 4] << 8);
                new_t = skipChildren(t, count);
                *(t - 3) = count >> 8;
                *(t - 2) = new_t - t + 2;
            }
        }
        deleteSegment(trie + tp[0], trie);
        movePtrList(orig_trie_len);
    }

    void addFirstData() {
        addData(0);
    }

    void addData(int16_t search_result) {

        insertCurrent();

        int16_t key_left = key_len - keyPos;
        int16_t kv_last_pos = getKVLastPos() - (key_left + value_len + 2);
        setKVLastPos(kv_last_pos);
        current_block[kv_last_pos] = key_left;
        if (key_left)
            memcpy(current_block + kv_last_pos + 1, key + keyPos, key_left);
        current_block[kv_last_pos + key_left + 1] = value_len;
        memcpy(current_block + kv_last_pos + key_left + 2, value, value_len);

        insPtr(search_result, kv_last_pos);

    }

    bool isFull(int16_t search_result) {
        decodeNeedCount();
        int16_t ptr_size = filledSize() + 1;
    #if BPT_9_BIT_PTR == 0
        ptr_size <<= 1;
    #endif
        if (getKVLastPos()
                < (DFQX_HDR_SIZE + DQ_MAX_PTR_BITMAP_BYTES + BPT_TRIE_LEN
                        + need_count + ptr_size + key_len - keyPos + value_len + 3))
            return true;
        if (filledSize() > DQ_MAX_PTRS)
            return true;
        if (BPT_TRIE_LEN > 252 - need_count)
            return true;
        return false;
    }

    void insBytesWithPtrs(uint8_t *ptr, int16_t len) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
    #else
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
    #endif
        BPT_TRIE_LEN += len;
    }

    void insAtWithPtrs(uint8_t *ptr, const uint8_t *s, uint8_t len) {
    #if BPT_9_BIT_PTR == 1
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN + filledSize() - ptr);
    #else
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN + filledSize() * 2 - ptr);
    #endif
        memcpy(ptr, s, len);
        BPT_TRIE_LEN += len;
    }

    void insAtWithPtrs(uint8_t *ptr, uint8_t b, const uint8_t *s, uint8_t len) {
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

    uint8_t insAtWithPtrs(uint8_t *ptr, uint8_t b1, uint8_t b2) {
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

    uint8_t insAtWithPtrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3) {
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

    uint8_t insAtWithPtrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
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

    uint8_t insAtWithPtrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
            uint8_t b5) {
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

    uint8_t insAtWithPtrs(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
            uint8_t b5, uint8_t b6) {
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

    void updateSkipLens(uint8_t *loop_upto, uint8_t *covering_upto, int diff) {
        uint8_t *t = trie;
        uint8_t tc = *t++;
        loop_upto++;
        while (t <= loop_upto) {
            if (tc & x02) {
                t++;
                if ((t + *t) > covering_upto) {
                    *t = *t + diff;
                    (*(t-1))++;
                    t += 2;
                } else
                    t += *t;
            } else
                t++;
            tc = *t++;
        }
    }

    void insertCurrent() {
        uint8_t key_char;
        uint8_t mask;

        key_char = key[keyPos - 1];
        mask = x80 >> (key_char & x03);
        switch (insertState) {
        case INSERT_AFTER:
            *origPos &= xFE;
            insAtWithPtrs(triePos, ((key_char & xFC) | x01), mask);
            if (keyPos > 1)
                updateSkipLens(origPos - 1, triePos - 1, 2);
            break;
        case INSERT_BEFORE:
            insAtWithPtrs(triePos, (key_char & xFC), mask);
            if (keyPos > 1)
                updateSkipLens(origPos, triePos + 1, 2);
            break;
        case INSERT_LEAF:
            if (*origPos & x02)
                origPos[3] |= mask;
            else
                origPos[1] |= mask;
            updateSkipLens(origPos, origPos + 1, 0);
            break;
        case INSERT_THREAD:
            uint16_t p, min;
            uint8_t c1, c2;
            uint8_t *fromPos;
            fromPos = triePos;
            if (*origPos & x02) {
                origPos[1]++;
            } else {
                if (origPos[1] & x0F || need_count > 4) {
                    p = dbl_bit_count[origPos[1]];
                    uint8_t *new_t = skipChildren(origPos + 2, p);
                    insAtWithPtrs(origPos + 1, (p >> 8) + 1, new_t - origPos - 2);
                    triePos += 2;
                    *origPos |= x02;
                }
            }
            origPos[(*origPos & x02) ? 3 : 1] |= (x08 >> (key_char & x03));
            c1 = c2 = key_char;
            p = keyPos;
            min = util::min16(key_len, keyPos + key_at_len);
            if (p < min)
                origPos[(*origPos & x02) ? 3 : 1] &= ~mask;
            need_count -= 6;
            need_count /= 2;
            int16_t diff;
            diff = p + need_count;
            if (diff == min && need_count) {
                need_count--;
                diff--;
            }
            insBytesWithPtrs(triePos, (diff == min ? 2 :
                    (need_count * 2) + ((key[diff] ^ key_at[diff - keyPos]) > x03 ? 4 :
                            (key[diff] == key_at[diff - keyPos] ? 6 : 2))));
            while (p < min) {
                c1 = key[p];
                c2 = key_at[p - keyPos];
                if (c1 > c2) {
                    uint8_t swap = c1;
                    c1 = c2;
                    c2 = swap;
                }
                switch ((c1 ^ c2) > x03 ?
                        0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
                case 0:
                    *triePos++ = c1 & xFC;
                    *triePos++ = x80 >> (c1 & x03);
                    *triePos++ = (c2 & xFC) | x01;
                    *triePos++ = x80 >> (c2 & x03);
                    break;
                case 1:
                    *triePos++ = (c1 & xFC) | x01;
                    *triePos++ = (x80 >> (c1 & x03)) | (x80 >> (c2 & x03));
                    break;
                case 2:
                    *triePos++ = (c1 & xFC) | x01;
                    *triePos++ = x08 >> (c1 & x03);
                    break;
                case 3:
                    *triePos++ = (c1 & xFC) | x03;
                    *triePos++ = 2;
                    *triePos++ = 4;
                    *triePos++ = 0x88 >> (c1 & x03);
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
                *triePos++ = (c2 & xFC) | x01;
                *triePos++ = x80 >> (c2 & x03);
                if (p == key_len)
                    keyPos--;
            }
            p = triePos - fromPos;
            updateSkipLens(origPos - 1, origPos, p);
            if (*origPos & x02)
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
            append((key_char & xFC) | x01);
            append(mask);
            break;
        }

        if (BPT_MAX_PFX_LEN < keyPos)
            BPT_MAX_PFX_LEN = keyPos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

    }

    uint8_t *split(uint8_t *first_key, int16_t *first_len_ptr) {
        int16_t orig_filled_size = filledSize();
        const uint16_t DFQX_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
        uint8_t *b = allocateBlock(DFQX_NODE_SIZE, isLeaf(), current_block[0] & 0x1F);
        dfqx new_block(this->leaf_block_size, this->parent_block_size, 0, NULL, b);
        memcpy(new_block.trie, trie, BPT_TRIE_LEN);
        new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        int16_t kv_last_pos = getKVLastPos();
        int16_t halfKVLen = DFQX_NODE_SIZE - kv_last_pos + 1;
        halfKVLen /= 2;

        int16_t brk_idx = -1;
        int16_t brk_kv_pos;
        int16_t tot_len;
        brk_kv_pos = tot_len = 0;
        char ctr = 4;
        uint8_t tp[BPT_MAX_PFX_LEN + 1];
        uint8_t *t = trie;
        uint8_t tc, child_leaf;
        tc = child_leaf = 0;
        //if (!isLeaf())
        //   cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled size:" << orig_filled_size << endl;
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
                t = nextKey(first_key, tp, t, ctr, tc, child_leaf);
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
                    deleteTrieLastHalf(keyPos, first_key, tp);
                    new_block.keyPos = keyPos;
                    t = new_block.trie + (t - trie);
                    t = new_block.nextKey(first_key, tp, t, ctr, tc, child_leaf);
                    keyPos = new_block.keyPos;
                    //src_idx = getPtr(idx + 1);
                    //memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
                    //first_key[keyPos+1+current_block[src_idx]] = 0;
                    //cout << first_key << endl;
                }
            }
        }
        kv_last_pos = getKVLastPos() + DFQX_NODE_SIZE - kv_last_pos;
        new_block.setKVLastPos(kv_last_pos);
        memmove(new_block.current_block + kv_last_pos, new_block.current_block + getKVLastPos(), DFQX_NODE_SIZE - kv_last_pos);
        brk_kv_pos += (kv_last_pos - getKVLastPos());
        int16_t diff = DFQX_NODE_SIZE - brk_kv_pos;
        for (idx = 0; idx < orig_filled_size; idx++) {
            new_block.insPtr(idx, kv_last_pos + (idx < brk_idx ? diff : 0));
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
            kv_last_pos += new_block.current_block[kv_last_pos];
            kv_last_pos++;
        }
        kv_last_pos = new_block.getKVLastPos();
    #if BPT_9_BIT_PTR == 1
        memcpy(current_block + DFQX_HDR_SIZE, new_block.current_block + DFQX_HDR_SIZE, DQ_MAX_PTR_BITMAP_BYTES);
        memcpy(trie + BPT_TRIE_LEN, new_block.trie + new_block.BPT_TRIE_LEN, brk_idx);
    #else
        memcpy(trie + BPT_TRIE_LEN, new_block.trie + new_block.BPT_TRIE_LEN, (brk_idx << 1));
    #endif

        {
            new_block.key_at = new_block.getKey(brk_idx, &new_block.key_at_len);
            keyPos++;
            if (isLeaf())
                *first_len_ptr = keyPos;
            else {
                if (new_block.key_at_len) {
                    memcpy(first_key + keyPos, new_block.key_at,
                            new_block.key_at_len);
                    *first_len_ptr = keyPos + new_block.key_at_len;
                } else
                    *first_len_ptr = keyPos;
            }
            keyPos--;
            new_block.deleteTrieFirstHalf(keyPos, first_key, tp);
        }

        {
            int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + DFQX_NODE_SIZE - old_blk_new_len,
                    new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
            setKVLastPos(DFQX_NODE_SIZE - old_blk_new_len);
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
            uint8_t *block_ptrs = new_block.trie + new_block.BPT_TRIE_LEN;
    #if BPT_9_BIT_PTR == 1
            memmove(block_ptrs, block_ptrs + brk_idx, new_size);
    #else
            memmove(block_ptrs, block_ptrs + (brk_idx << 1), new_size << 1);
    #endif
            new_block.setKVLastPos(brk_kv_pos);
            new_block.setFilledSize(new_size);
        }

        return new_block.current_block;
    }

    void decodeNeedCount() {
        if (insertState != INSERT_THREAD)
            need_count = need_counts[insertState];
    }

};

#endif
