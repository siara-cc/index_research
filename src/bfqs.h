#ifndef bfqs_H
#define bfqs_H
#ifdef ARDUINO
#if (defined(__AVR__))
#include <avr/pgmspace.h>
#else
#include <pgmspace.h>
#endif
#else
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#if defined(ARDUINO)
#define BIT_COUNT_LF_CH(x) pgm_read_byte_near(util::bit_count_lf_ch + (x))
#else
#define BIT_COUNT_LF_CH(x) util::bit_count_lf_ch[x]
//#define BIT_COUNT_LF_CH(x) (BIT_COUNT(x & xAA) + BIT_COUNT2(x & x55))
#endif

#define BQ_MIDDLE_PREFIX 1

#define BFQS_HDR_SIZE 9

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class bfqs: public bpt_trie_handler<bfqs> {
public:
    const static uint8_t need_counts[10];
    const static uint8_t switch_map[8];
    const static uint8_t shift_mask[8];
    uint8_t *last_t;
    uint8_t last_leaf_child;

    bfqs(uint16_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint16_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL, uint8_t *block = NULL) :
        bpt_trie_handler<bfqs>(leaf_block_sz, parent_block_sz, cache_sz, fname, block) {
    }

    inline void setCurrentBlockRoot() {
        current_block = root_block;
        trie = current_block + BFQS_HDR_SIZE;
    }

    inline void setCurrentBlock(uint8_t *m) {
        current_block = m;
        trie = current_block + BFQS_HDR_SIZE;
    }

    inline uint8_t *getLastPtr() {
        //keyPos = 0;
        while ((last_leaf_child & xAA) > (last_leaf_child & x55)) {
                last_t += BIT_COUNT_LF_CH(last_leaf_child & xAA) + 1;
                last_t += *last_t;
                while (*last_t & x01) {
                    last_t += (*last_t >> 1);
                    last_t++;
                }
                while (!(*last_t & x02))
                    last_t += BIT_COUNT_LF_CH(last_t[1]) + 2;
                last_leaf_child = last_t[1];
        }
        return current_block + util::getInt(last_t +
                BIT_COUNT_LF_CH((last_t[1] & xAA) | (last_leaf_child & x55)));
    //    do {
    //        int rslt = ((last_leaf_child & xAA) > (last_leaf_child & x55) ? 2 : 1);
    //        switch (rslt) {
    //        case 1:
    //            return current_block + util::getInt(last_t +
    //                    BIT_COUNT_LF_CH((last_t[1] & xAA) | (last_leaf_child & x55)));
    //        case 2:
    //            last_t += BIT_COUNT_LF_CH(last_leaf_child & xAA) + 1;
    //            last_t += *last_t;
    //            while (*last_t & x01) {
    //                last_t += (*last_t >> 1);
    //                last_t++;
    //            }
    //            while (!(*last_t & x02))
    //                last_t += BIT_COUNT_LF_CH(last_t[1]) + 2;
    //            last_leaf_child = last_t[1];
    //        }
    //    } while (1);
        return 0;
    }

    inline void setPrefixLast(uint8_t key_char, uint8_t *t, uint8_t pfx_rem_len) {
        if (key_char > *t) {
            t += pfx_rem_len;
            while (!(*t & x02))
                t += BIT_COUNT_LF_CH(t[1]) + 2;
            last_t = t;
            last_leaf_child = t[1];
        }
    }

    inline int16_t searchCurrentBlock() {
        uint8_t *t = trie;
        uint8_t trie_char = *t;
        origPos = t++;
        //if (keyPos) {
        //    if (trie_char & x01) {
        //        keyPos--;
        //        uint8_t pfx_len = trie_char >> 1;
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
        uint8_t key_char = *key; //[keyPos++];
        keyPos = 1;
        do {
    #if BQ_MIDDLE_PREFIX == 1
            switch (trie_char & x01 ? 3 : (key_char ^ trie_char) > x03
                    ? (key_char > trie_char ? 0 : 2) : 1) {
    #else
            switch ((key_char ^ trie_char) > x03 ?
                    (key_char > trie_char ? 0 : 2) : 1) {
    #endif
            case 0:
                last_t = origPos;
                last_leaf_child = *t++;
                t += BIT_COUNT_LF_CH(last_leaf_child);
                if (trie_char & x02) {
                    //if (!isLeaf())
                    //    last_t = getLastPtr();
                        insertState = INSERT_AFTER;
                    return -1;
                }
                break;
            case 1:
                uint8_t r_shft, r_leaves_children;
                r_leaves_children = *t++;
                key_char = (key_char & x03) << 1;
                r_shft = ~(xFF << key_char); //shift_mask[key_char];
                if (!isLeaf() && (r_leaves_children & r_shft)) {
                    last_t = origPos;
                    last_leaf_child = r_leaves_children & r_shft;
                }
                trie_char = switch_map[((r_leaves_children >> key_char) & x03) | (keyPos == key_len ? 4 : 0)];
                switch (trie_char) {
                case 0:
                //case 4:
                //case 6:
                       insertState = INSERT_LEAF;
                    //if (!isLeaf())
                    //    last_t = getLastPtr();
                    return -1;
                case 2:
                    break;
                case 1:
                //case 5:
                //case 7:
                    int16_t cmp;
                    t += BIT_COUNT_LF_CH(r_leaves_children & (r_shft | xAA));
                    key_at = current_block + util::getInt(t);
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
    #if BQ_MIDDLE_PREFIX == 1
                        need_count = cmp + 6;
    #else
                        need_count = (cmp * 4) + 10;
    #endif
                    return -1;
                case 3:
                    if (!isLeaf()) {
                        last_t = origPos;
                        last_leaf_child = (r_leaves_children & r_shft) | (x01 << key_char);
                    }
                    break;
                }
                t += BIT_COUNT_LF_CH(r_leaves_children & r_shft & xAA);
                t += *t;
                key_char = key[keyPos++];
                break;
            case 2:
                //if (!isLeaf())
                //    last_t = getLastPtr();
                    insertState = INSERT_BEFORE;
                return -1;
    #if BQ_MIDDLE_PREFIX == 1
            case 3:
                uint8_t pfx_len;
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

    inline uint8_t *getChildPtrPos(int16_t search_result) {
        return last_t - current_block < getKVLastPos() ? getLastPtr() : last_t;
    }

    inline uint8_t *getPtrPos() {
        return trie + BPT_TRIE_LEN;
    }

    inline int getHeaderSize() {
        return BFQS_HDR_SIZE;
    }

    uint8_t copyKary(uint8_t *t, uint8_t *dest, int lvl, uint8_t *tp,
            uint8_t *brk_key, int16_t brk_key_len, uint8_t whichHalf) {
        uint8_t *orig_dest = dest;
        if (*t & x01) {
            uint8_t len = (*t >> 1) + 1;
            memcpy(dest, t, len);
            dest += len;
            t += len;
        }
        uint8_t *dest_after_prefix = dest;
        uint8_t *limit = trie + (lvl < brk_key_len ? tp[lvl] : 0);
        uint8_t tc;
        uint8_t is_limit = 0;
        do {
            is_limit = (limit == t ? 1 : 0); // && is_limit == 0 ? 1 : 0);
            if (limit == t && whichHalf == 2)
                dest = dest_after_prefix;
            tc = *t;
            if (is_limit) {
                *dest = tc;
                uint8_t offset = (lvl < brk_key_len ? (brk_key[lvl] & x03) << 1 : x03);
                t++;
                uint8_t leaves_children = *t++;
                uint8_t orig_leaves_children = leaves_children;
                if (whichHalf == 1) {
                    leaves_children &= ~(((brk_key_len - lvl) == 1 ? xFE : xFC) << offset);
                    *dest++ |= x02;
                    *dest++ = leaves_children;
                    for (int i = 0; i < BIT_COUNT_LF_CH(leaves_children & xAA); i++)
                        *dest++ = t[i];
                    t += BIT_COUNT_LF_CH(orig_leaves_children & xAA);
                    memcpy(dest, t, BIT_COUNT_LF_CH(leaves_children & x55));
                    dest += BIT_COUNT_LF_CH(leaves_children & x55);
                    break;
                } else {
                    leaves_children &= (((brk_key_len - lvl) == 1 ? xFF : xFE) << offset);
                    dest++;
                    *dest++ = leaves_children;
                    for (int i = BIT_COUNT_LF_CH(xAA & (orig_leaves_children - leaves_children));
                            i < BIT_COUNT_LF_CH(xAA & orig_leaves_children); i++)
                        *dest++ = t[i];
                    t += BIT_COUNT_LF_CH(orig_leaves_children & xAA);
                    memcpy(dest, t + BIT_COUNT_LF_CH(x55 & (orig_leaves_children - leaves_children)),
                            BIT_COUNT_LF_CH(x55 & leaves_children));
                    dest += BIT_COUNT_LF_CH(x55 & leaves_children);
                }
                t += BIT_COUNT_LF_CH(orig_leaves_children & x55);
            } else {
                uint8_t len = BIT_COUNT_LF_CH(t[1]) + 2;
                memcpy(dest, t, len);
                t += len;
                dest += len;
            }
        } while (!(tc & x02));
        return dest - orig_dest;
    }

    uint8_t copyTrieHalf(uint8_t *tp, uint8_t *brk_key, int16_t brk_key_len, uint8_t *dest, uint8_t whichHalf) {
        uint8_t *d;
        uint8_t *t = trie;
        uint8_t *new_trie = dest;
        uint8_t tp_child[BPT_MAX_PFX_LEN];
        uint8_t child_num[BPT_MAX_PFX_LEN];
        int lvl = 0;
        if (*t & x01) {
            uint8_t len = (*t >> 1);
            memset(tp_child, dest - new_trie, len);
            lvl = len;
        }
        uint8_t last_len = copyKary(t, dest, lvl, tp, brk_key, brk_key_len, whichHalf);
        d = dest;
        dest += last_len;
        do {
            uint8_t tc = *d++;
            if (tc & x01) {
                d += (tc >> 1);
                tc = *d++;
            }
            uint8_t len = BIT_COUNT_LF_CH(xAA & *d);
            if (len) {
                d++;
                tp_child[lvl] = d - new_trie - 2;
                uint8_t *child = trie + *d;
                *d = dest - d;
                child_num[lvl++] = 0;
                t = child;
                if (*t & x01) {
                    uint8_t len = (*t >> 1);
                    memset(tp_child + lvl, dest - new_trie, len);
                    lvl += len;
                }
                last_len = copyKary(t, dest, lvl, tp, brk_key, brk_key_len, whichHalf);
                d = dest;
                dest += last_len;
                continue;
            } else
                d += (BIT_COUNT_LF_CH(*d) + 1);
            while (tc & x02) {
                do {
                    lvl--;
                    d = new_trie + tp_child[lvl];
                    tc = *d;
                } while (tc & x01);
                if (lvl < 0)
                    return dest - new_trie;
                d = new_trie + tp_child[lvl];
                uint8_t len = BIT_COUNT_LF_CH(xAA & d[1]);
                uint8_t i = child_num[lvl];
                i++;
                if (i < len) {
                    d += (i + 2);
                    uint8_t *child = trie + *d;
                    *d = dest - d;
                    child_num[lvl++] = i;
                    t = child;
                    if (*t & x01) {
                        uint8_t len = (*t >> 1);
                        memset(tp_child + lvl, dest - new_trie, len);
                        lvl += len;
                    }
                    last_len = copyKary(t, dest, lvl, tp, brk_key, brk_key_len, whichHalf);
                    d = dest;
                    dest += last_len;
                    break;
                }
                tc = *d;
                if (!(tc & x02)) {
                    d += BIT_COUNT_LF_CH(d[1]) + 2;
                    break;
                }
            }
        } while (1);
        return 0;
    }

    void setPtrDiff(int16_t diff) {
        uint8_t *t = trie;
        uint8_t *t_end = trie + BPT_TRIE_LEN;
        while (t < t_end) {
            uint8_t tc = *t++;
            if (tc & x01) {
                t += (tc >> 1);
                continue;
            }
            uint8_t leaves_count = BIT_COUNT_LF_CH(*t & x55);
            t += BIT_COUNT_LF_CH(*t & xAA);
            t++;
            for (int i = 0; i < leaves_count; i += 2) {
                util::setInt(t, util::getInt(t) + diff);
                t += 2;
            }
        }
    }

    void consolidateInitialPrefix(uint8_t *t) {
        t += BFQS_HDR_SIZE;
        uint8_t *t1 = t;
        if (*t & x01) {
            t1 += (*t >> 1);
            t1++;
        }
        uint8_t *t2 = t1 + (*t & x01 ? 0 : 1);
        uint8_t count = 0;
        uint8_t trie_len_diff = 0;
        while ((*t1 & x01) || ((*t1 & x02) && BIT_COUNT_LF_CH(t1[1]) == 1 && t1[2] == 1)) {
            if (*t1 & x01) {
                uint8_t len = *t1++ >> 1;
                memcpy(t2, t1, len);
                t2 += len;
                t1 += len;
                count += len;
                trie_len_diff++;
            } else {
                *t2++ = (*t1 & xFC) + (BIT_COUNT(t1[1] - 1) >> 1);
                t1 += 3;
                count++;
                trie_len_diff += 2;
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

    int16_t insertAfter() {
        uint8_t key_char;
        uint8_t mask;
        key_char = key[keyPos - 1];
        mask = x01 << ((key_char & x03) << 1);
        *origPos &= xFC;
        triePos = origPos + 2 + BIT_COUNT_LF_CH(origPos[1]);
        updatePtrs(triePos, 4);
        insAt(triePos, ((key_char & xFC) | x02), mask, 0, 0);
        return triePos - trie + 2;
    }

    int16_t insertBefore() {
        uint8_t key_char;
        uint8_t mask;
        key_char = key[keyPos - 1];
        mask = x01 << ((key_char & x03) << 1);
        updatePtrs(origPos, 4);
        insAt(origPos, (key_char & xFC), mask, 0, 0);
        return origPos - trie + 2;
    }

    int16_t insertLeaf() {
        uint8_t key_char;
        uint8_t mask;
        key_char = key[keyPos - 1];
        mask = x01 << ((key_char & x03) << 1);
        triePos = origPos + 1;
        *triePos++ |= mask;
        triePos += BIT_COUNT_LF_CH(origPos[1])
                - BIT_COUNT_LF_CH(origPos[1] & (0x55 << ((key_char & x03) << 1)));
        updatePtrs(triePos, 2);
        insAt(triePos, x00, x00);
        return triePos - trie;
    }

    #if BQ_MIDDLE_PREFIX == 1
    int16_t insertConvert() {
        uint8_t key_char;
        uint8_t mask;
        int16_t ret = 0;
        key_char = key[keyPos - 1];
        mask = x01 << ((key_char & x03) << 1);
        uint8_t b, c;
        char cmp_rel;
        int16_t diff;
        diff = triePos - origPos;
        // 3 possible relationships between key_char and *triePos, 4 possible positions of triePos
        c = *triePos;
        cmp_rel = ((c ^ key_char) > x03 ? (c < key_char ? 0 : 1) : 2);
        if (diff == 1)
            triePos = origPos;
        b = (cmp_rel == 2 ? x02 : x00); // | (cmp_rel == 1 ? x00 : x02);
        need_count = (*origPos >> 1) - diff;
        diff--;
        *triePos++ = ((cmp_rel == 0 ? c : key_char) & xFC) | b;
        b = (cmp_rel == 2 ? 3 : 5);
        if (diff) {
            b++;
            *origPos = (diff << 1) | x01;
        }
        if (need_count)
            b++;
        insBytes(triePos, b);
        updatePtrs(triePos, b);
        switch (cmp_rel) {
        case 0:
            *triePos++ = x02 << ((c & x03) << 1);
            *triePos++ = 5;
            *triePos++ = (key_char & xFC) | 0x02;
            *triePos++ = mask;
            ret = triePos - trie;
            triePos += 2;
            break;
        case 1:
            *triePos++ = mask;
            ret = triePos - trie;
            triePos += 2;
            *triePos++ = (c & xFC) | x02;
            *triePos++ = x02 << ((c & x03) << 1);
            *triePos++ = 1;
            break;
        case 2:
            *triePos++ = (x02 << ((c & x03) << 1)) | mask;
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

    int16_t insertThread() {
        uint8_t key_char;
        uint8_t mask;
        int16_t p, min;
        uint8_t c1, c2;
        int16_t diff;
        int16_t ret, ptr, pos;
        ret = pos = 0;

        key_char = key[keyPos - 1];
        mask = x01 << ((key_char & x03) << 1);
        c1 = c2 = key_char;
        {
            triePos = origPos + 2 + BIT_COUNT_LF_CH(origPos[1] & xAA & (mask - 1));
            insAt(triePos, (uint8_t) (BPT_TRIE_LEN - (triePos - trie) + 1));
            updatePtrs(triePos, 1);
            origPos[1] |= (mask << 1);
        }
        p = keyPos;
        min = util::min16(key_len, keyPos + key_at_len);
        triePos = origPos + 2 + (BIT_COUNT_LF_CH(origPos[1])
                - BIT_COUNT_LF_CH(origPos[1] & (0x55 << ((key_char & x03) << 1))));
        ptr = util::getInt(triePos);
        if (p < min) {
            origPos[1] &= ~mask;
            delAt(triePos, 2);
            updatePtrs(triePos, -2);
        } else {
            pos = triePos - trie;
            ret = pos;
        }
    #if BQ_MIDDLE_PREFIX == 1
        need_count -= 7;
        if (p + need_count == min) {
            if (need_count) {
                need_count--;
            }
        }
        if (need_count) {
            append((need_count << 1) | x01);
            append(key + keyPos, need_count);
            p += need_count;
            //count1 += need_count;
        }
    #endif
        while (p < min) {
            bool isSwapped = false;
            c1 = key[p];
            c2 = key_at[p - keyPos];
            if (c1 > c2) {
                uint8_t swap = c1;
                c1 = c2;
                c2 = swap;
                isSwapped = true;
            }
    #if BQ_MIDDLE_PREFIX == 1
            switch ((c1 ^ c2) > x03 ? 0 : (c1 == c2 ? 3 : 1)) {
    #else
            switch ((c1 ^ c2) > x03 ?
                    0 : (c1 == c2 ? (p + 1 == min ? 3 : 2) : 1)) {
            case 2:
                append((c1 & xFC) | x02);
                append(x02 << ((c1 & x03) << 1));
                append(1);
                break;
    #endif
            case 0:
                append(c1 & xFC);
                append(x01 << ((c1 & x03) << 1));
                ret = isSwapped ? ret : BPT_TRIE_LEN;
                pos = isSwapped ? BPT_TRIE_LEN : pos;
                appendPtr(isSwapped ? ptr : 0);
                append((c2 & xFC) | x02);
                append(x01 << ((c2 & x03) << 1));
                ret = isSwapped ? BPT_TRIE_LEN : ret;
                pos = isSwapped ? pos : BPT_TRIE_LEN;
                appendPtr(isSwapped ? 0 : ptr);
                break;
            case 1:
                append((c1 & xFC) | x02);
                append((x01 << ((c1 & x03) << 1) | (x01 << ((c2 & x03) << 1))));
                ret = isSwapped ? ret : BPT_TRIE_LEN;
                pos = isSwapped ? BPT_TRIE_LEN : pos;
                appendPtr(isSwapped ? ptr : 0);
                ret = isSwapped ? BPT_TRIE_LEN : ret;
                pos = isSwapped ? pos : BPT_TRIE_LEN;
                appendPtr(isSwapped ? 0 : ptr);
                break;
            case 3:
                append((c1 & xFC) | x02);
                append(x03 << ((c1 & x03) << 1));
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
            append((c2 & xFC) | x02);
            append(x01 << ((c2 & x03) << 1));
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
                current_block[p] = key_at_len;
                util::setInt(trie + pos, p);
            }
        }
        return ret;
    }

    int16_t insertEmpty() {
        append((*key & xFC) | x02);
        append(x01 << ((*key & x03) << 1));
        int16_t ret = BPT_TRIE_LEN;
        keyPos = 1;
        append(0);
        append(0);
        return ret;
    }

    void addFirstData() {
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
        if (getKVLastPos() < (BFQS_HDR_SIZE + BPT_TRIE_LEN +
                need_count + key_len - keyPos + value_len + 3))
            return true;
        if (BPT_TRIE_LEN > 254 - need_count)
            return true;
        return false;
    }

    uint8_t *split(uint8_t *first_key, int16_t *first_len_ptr) {
        int16_t orig_filled_size = filledSize();
        const uint16_t BFQS_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
        uint8_t *b = allocateBlock(BFQS_NODE_SIZE, isLeaf(), current_block[0] & 0x1F);
        bfqs new_block(this->leaf_block_size, this->parent_block_size, 0, NULL, b);
        new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
        new_block.BPT_MAX_PFX_LEN = BPT_MAX_PFX_LEN;
        int16_t kv_last_pos = getKVLastPos();
        int16_t halfKVPos = kv_last_pos + (BFQS_NODE_SIZE - kv_last_pos) / 2;

        int16_t brk_idx, brk_kv_pos;
        brk_idx = brk_kv_pos = 0;
        // (1) move all data to new_block in order
        int16_t idx = 0;
        uint8_t alloc_size = BPT_MAX_PFX_LEN + 1;
        uint8_t curr_key[alloc_size];
        uint8_t tp[alloc_size];
        uint8_t tp_cpy[alloc_size];
        int16_t tp_cpy_len = 0;
        uint8_t *t = new_block.trie;
        //if (!isLeaf())
        //   cout << "Trie len:" << (int) BPT_TRIE_LEN << ", filled:" << orig_filled_size << ", max:" << (int) DX_MAX_KEY_LEN << endl;
        new_block.keyPos = 0;
        memcpy(new_block.trie, trie, BPT_TRIE_LEN);
        new_block.BPT_TRIE_LEN = BPT_TRIE_LEN;
        uint8_t tc, leaf_child;
        tc = leaf_child = 0;
        uint8_t ctr = 5;
        do {
            while (ctr == x04) {
                if (tc & x02) {
                    new_block.keyPos--;
                    t = new_block.trie + tp[new_block.keyPos];
                    if (*t & x01) {
                        new_block.keyPos -= (*t >> 1);
                        t = new_block.trie + tp[new_block.keyPos];
                    }
                    tc = *t++;
                    leaf_child = *t++;
                    ctr = curr_key[new_block.keyPos] & x03;
                    leaf_child &= (xFC << (ctr * 2));
                    ctr = leaf_child ? FIRST_BIT_OFFSET_FROM_RIGHT(leaf_child) >> 1 : 4;
                } else {
                    t += BIT_COUNT_LF_CH(*(t - 1));
                    ctr = 0x05;
                    break;
                }
            }
            if (ctr > x03) {
                tp[new_block.keyPos] = t - new_block.trie;
                tc = *t++;
                if (tc & x01) {
                    uint8_t len = tc >> 1;
                    memset(tp + new_block.keyPos, t - new_block.trie - 1, len);
                    memcpy(curr_key + new_block.keyPos, t, len);
                    t += len;
                    new_block.keyPos += len;
                    tp[new_block.keyPos] = t - new_block.trie;
                    tc = *t++;
                }
                leaf_child = *t++;
                ctr = FIRST_BIT_OFFSET_FROM_RIGHT(leaf_child) >> 1;
            }
            curr_key[new_block.keyPos] = (tc & xFC) | ctr;
            uint8_t mask = x01 << (ctr * 2);
            if (leaf_child & mask) {
                uint8_t *leaf_ptr = t + BIT_COUNT_LF_CH(*(t - 1) & ((mask - 1) | xAA));
                int16_t src_idx = util::getInt(leaf_ptr);
                int16_t kv_len = current_block[src_idx];
                kv_len++;
                kv_len += current_block[src_idx + kv_len];
                kv_len++;
                memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
                util::setInt(leaf_ptr, kv_last_pos);
                //memcpy(curr_key + new_block.keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
                //curr_key[new_block.keyPos+1+current_block[src_idx]] = 0;
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
                        memcpy(first_key + new_block.keyPos, current_block + src_idx + 1, current_block[src_idx]);
                        *first_len_ptr = new_block.keyPos + current_block[src_idx];
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
                        //memcpy(first_key + keyPos + 1, current_block + src_idx + 1, current_block[src_idx]);
                        //first_key[keyPos+1+current_block[src_idx]] = 0;
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
            if (leaf_child & (x02 << (ctr * 2))) {
                t += BIT_COUNT_LF_CH(*(t - 1) & xAA & (mask - 1));
                uint8_t child_offset = *t;
                *t = t - new_block.trie + child_offset;
                t += child_offset;
                new_block.keyPos++;
                ctr = 5;
                continue;
            }
            leaf_child &= ~(x03 << (ctr * 2));
            ctr = leaf_child ? FIRST_BIT_OFFSET_FROM_RIGHT(leaf_child) >> 1 : 4;
        } while (1);
        new_block.BPT_TRIE_LEN = new_block.copyTrieHalf(tp_cpy, first_key, tp_cpy_len, trie + BPT_TRIE_LEN, 2);
        memcpy(new_block.trie, trie + BPT_TRIE_LEN, new_block.BPT_TRIE_LEN);

        kv_last_pos = getKVLastPos() + BFQS_NODE_SIZE - kv_last_pos;
        int16_t diff = (kv_last_pos - getKVLastPos());

        {
            memmove(new_block.current_block + brk_kv_pos + diff, new_block.current_block + brk_kv_pos,
                    BFQS_NODE_SIZE - brk_kv_pos - diff);
            brk_kv_pos += diff;
            new_block.setPtrDiff(diff);
            new_block.setKVLastPos(brk_kv_pos);
            new_block.setFilledSize(orig_filled_size - brk_idx);
        }

        {
            int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + BFQS_NODE_SIZE - old_blk_new_len, // Copy back first half to old block
                    new_block.current_block + kv_last_pos - diff, old_blk_new_len);
            diff += (BFQS_NODE_SIZE - brk_kv_pos);
            setPtrDiff(diff);
            setKVLastPos(BFQS_NODE_SIZE - old_blk_new_len);
            setFilledSize(brk_idx);
        }

        if (!isLeaf())
            new_block.setLeaf(false);

        consolidateInitialPrefix(current_block);
        new_block.consolidateInitialPrefix(new_block.current_block);

        //keyPos = 0;

        return new_block.current_block;

    }

    int16_t insertCurrent() {
        int16_t ret;

        switch (insertState) {
        case INSERT_AFTER:
            ret = insertAfter();
            break;
        case INSERT_BEFORE:
            ret = insertBefore();
            break;
        case INSERT_THREAD:
            ret = insertThread();
            break;
        case INSERT_LEAF:
            ret = insertLeaf();
            break;
    #if BQ_MIDDLE_PREFIX == 1
        case INSERT_CONVERT:
            ret = insertConvert();
            break;
    #endif
        case INSERT_EMPTY:
            ret = insertEmpty();
            break;
        }

        if (BPT_MAX_PFX_LEN < keyPos)
            BPT_MAX_PFX_LEN = keyPos;

        if (BPT_MAX_KEY_LEN < key_len)
            BPT_MAX_KEY_LEN = key_len;

        return ret;
    }
    void updatePtrs(uint8_t *upto, int diff) {
        uint8_t *t = trie;
        uint8_t tc = *t++;
        while (t <= upto) {
            if (tc & x01) {
                t += (tc >> 1);
                tc = *t++;
                continue;
            }
            int leaf_count = BIT_COUNT_LF_CH(*t & x55);
            int child_count = BIT_COUNT_LF_CH(*t & xAA);
            t++;
            while (child_count--) {
                if (t < upto && (t + *t) >= upto)
                    *t += diff;
                // todo: avoid inside loops
                if (insertState == INSERT_BEFORE && keyPos > 1 && (t + *t) == (origPos + 4))
                    *t -= 4;
                t++;
            }
            t += leaf_count;
            tc = *t++;
        }
    }

    void decodeNeedCount() {
        if (insertState != INSERT_THREAD)
            need_count = need_counts[insertState];
    }

};

#endif
