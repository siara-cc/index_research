#ifndef BP_TREE_H
#define BP_TREE_H
#ifdef ARDUINO
#include <HardwareSerial.h>
#include <string.h>
#else
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include <stdint.h>
#include "univix_util.h"

using namespace std;

#define BPT_IS_LEAF_BYTE current_block[0]
#define BPT_FILLED_SIZE current_block + 1
#define BPT_LAST_DATA_PTR current_block + 3
#define BPT_MAX_KEY_LEN current_block[5]
#define BPT_TRIE_LEN current_block[6]
#define BPT_MAX_PFX_LEN current_block[7]

#define INSERT_AFTER 1
#define INSERT_BEFORE 2
#define INSERT_LEAF 3
#define INSERT_EMPTY 4
#define INSERT_THREAD 5
#define INSERT_CONVERT 6

#ifdef ARDUINO
#define BPT_INT64MAP 0
#else
#define BPT_INT64MAP 1
#endif
#define BPT_9_BIT_PTR 1

class bplus_tree_handler {
protected:
    long total_size;
    int numLevels;
    int maxKeyCountNode;
    int maxKeyCountLeaf;
    int blockCountNode;
    int blockCountLeaf;
    long count1, count2;

public:
    byte *root_block;
    byte *current_block;
    const char *key;
    byte key_len;
    byte *key_at;
    byte key_at_len;
    const char *value;
    int16_t value_len;
#if BPT_9_BIT_PTR == 1
#if BPT_INT64MAP == 1
    uint64_t *bitmap;
#else
    uint32_t *bitmap1;
    uint32_t *bitmap2;
#endif
#endif

    const int16_t leaf_block_size, parent_block_size;
    bplus_tree_handler(int16_t leaf_block_sz = 512, int16_t parent_block_sz = 512) :
            leaf_block_size (leaf_block_sz), parent_block_size (parent_block_sz) {
        root_block = current_block = (byte *) util::alignedAlloc(leaf_block_size);
        init_stats();
        initCurrentBlock();
    }

    ~bplus_tree_handler() {
    }

    void init_stats() {
        total_size = maxKeyCountLeaf = maxKeyCountNode = blockCountNode = 0;
        numLevels = blockCountLeaf = 1;
        count1 = count2 = 0;
    }

    virtual byte *getChildPtrPos(int16_t idx) = 0;
    char *get(const char *key, int8_t key_len, int16_t *pValueLen) {
        setCurrentBlock(root_block);
        this->key = key;
        this->key_len = key_len;
        if (traverseToLeaf() < 0)
            return null;
        return getValueAt(pValueLen);
    }

    int16_t traverseToLeaf(int8_t *plevel_count = NULL, byte *node_paths[] = NULL) {
        while (!isLeaf()) {
            if (node_paths) {
                *node_paths++ = current_block;
                (*plevel_count)++;
            }
            int16_t idx = searchCurrentBlock();
            setCurrentBlock(getChildPtr(getChildPtrPos(idx)));
        }
        return searchCurrentBlock();
    }

    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len) {
        setCurrentBlock(root_block);
        this->key = key;
        this->key_len = key_len;
        this->value = value;
        this->value_len = value_len;
        if (filledSize() == 0) {
            addFirstData();
        } else {
            byte *node_paths[7];
            int8_t level_count = 1;
            int16_t idx = traverseToLeaf(&level_count, node_paths);
            recursiveUpdate(idx, node_paths, level_count - 1);
        }
        total_size++;
    }

    virtual inline void updateSplitStats() {
        //std::cout << "Full\n" << std::endl;
        //if (maxKeyCount < block->filledSize())
        //    maxKeyCount = block->filledSize();
        //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
        //cout << (int) node->TRIE_LEN << endl;
        if (isLeaf()) {
            maxKeyCountLeaf += filledSize();
            blockCountLeaf++;
        } else {
            maxKeyCountNode += filledSize();
            blockCountNode++;
        }
            //maxKeyCount += node->BPT_TRIE_LEN;
        //maxKeyCount += node->PREFIX_LEN;
    }

    void recursiveUpdate(int16_t idx, byte *node_paths[], int16_t level) {
        //int16_t idx = pos; // lastSearchPos[level];
        if (idx < 0) {
            idx = ~idx;
            if (isFull()) {
                updateSplitStats();
                byte first_key[BPT_MAX_KEY_LEN]; // is max_pfx_len sufficient?
                int16_t first_len;
                byte *old_block = current_block;
                byte *new_block = split(first_key, &first_len);
                int16_t cmp = util::compare((char *) first_key, first_len,
                        key, key_len);
                if (cmp <= 0) {
                    if (strcmp((const char *)first_key, "iu") != 0) {
                    setCurrentBlock(new_block);
                    idx = ~searchCurrentBlock();
                    addData(idx);
                    }
                } else {
                    idx = ~searchCurrentBlock();
                    addData(idx);
                }
                //cout << "FK:" << level << ":" << first_key << endl;
                if (root_block == old_block) {
                    blockCountNode++;
                    root_block = (byte *) util::alignedAlloc(parent_block_size);
                    setCurrentBlock(root_block);
                    initCurrentBlock();
                    setLeaf(0);
                    setKVLastPos(parent_block_size);
                    byte addr[9];
                    key = "";
                    key_len = 1;
                    value = (char *) addr;
                    value_len = util::ptrToBytes((unsigned long) old_block, addr);
                    addFirstData();
                    key = (char *) first_key;
                    key_len = first_len;
                    value = (char *) addr;
                    value_len = util::ptrToBytes((unsigned long) new_block, addr);
                    idx = ~searchCurrentBlock();
                    addData(idx);
                    numLevels++;
                } else {
                    int16_t prev_level = level - 1;
                    byte *parent_data = node_paths[prev_level];
                    setCurrentBlock(parent_data);
                    byte addr[9];
                    key = (char *) first_key;
                    key_len = first_len;
                    value = (char *) addr;
                    value_len = util::ptrToBytes((unsigned long) new_block, addr);
                    idx = searchCurrentBlock();
                    recursiveUpdate(idx, node_paths, prev_level);
                }
            } else
                addData(idx);
        } else {
            //if (node->isLeaf) {
            //    int16_t vIdx = idx + mSizeBy2;
            //    returnValue = (V) arr[vIdx];
            //    arr[vIdx] = value;
            //}
        }
    }

    virtual void setCurrentBlock(byte *m) {
        current_block = m;
#if BPT_9_BIT_PTR == 1
#if BPT_INT64MAP == 1
        bitmap = (uint64_t *) (current_block + getHeaderSize() - 8);
#else
        bitmap1 = (uint32_t *) (current_block + getHeaderSize() - 8);
        bitmap2 = bitmap1 + 1;
#endif
#endif
    }

    virtual void initCurrentBlock() {
        //memset(current_block, '\0', BFOS_NODE_SIZE);
        setLeaf(1);
        setFilledSize(0);
        BPT_MAX_KEY_LEN = 1;
        setKVLastPos(leaf_block_size);
    }

    inline bool isLeaf() {
        return BPT_IS_LEAF_BYTE;
    }

    inline void setLeaf(char isLeaf) {
        BPT_IS_LEAF_BYTE = isLeaf;
    }

    inline void setFilledSize(int16_t filledSize) {
        util::setInt(BPT_FILLED_SIZE, filledSize);
    }

    inline int16_t filledSize() {
        return util::getInt(BPT_FILLED_SIZE);
    }

    inline void setKVLastPos(uint16_t val) {
        util::setInt(BPT_LAST_DATA_PTR, val);
    }

    inline uint16_t getKVLastPos() {
        return util::getInt(BPT_LAST_DATA_PTR);
    }

    inline byte *getKey(int16_t pos, byte *plen) {
        byte *kvIdx = current_block + getPtr(pos);
        *plen = *kvIdx;
        return kvIdx + 1;
    }

    virtual inline byte *getChildPtr(byte *ptr) {
        ptr += (*ptr + 1);
        return (byte *) util::bytesToPtr(ptr);
    }

    virtual inline char *getValueAt(int16_t *vlen) {
        key_at += key_at_len;
        *vlen = *key_at;
        return (char *) key_at + 1;
    }

    inline int getPtr(int16_t pos) {
#if BPT_9_BIT_PTR == 1
        uint16_t ptr = *(getPtrPos() + pos);
#if BPT_INT64MAP == 1
        if (*bitmap & MASK64(pos))
        ptr |= 256;
#else
        if (pos & 0xFFE0) {
            if (*bitmap2 & MASK32(pos - 32))
            ptr |= 256;
        } else {
            if (*bitmap1 & MASK32(pos))
            ptr |= 256;
        }
#endif
        return ptr;
#else
        return util::getInt(getPtrPos() + (pos << 1));
#endif
    }

    void insPtr(int16_t pos, uint16_t kv_pos) {
        int16_t filledSz = filledSize();
#if BPT_9_BIT_PTR == 1
        byte *kvIdx = getPtrPos() + pos;
        memmove(kvIdx + 1, kvIdx, filledSz - pos);
        *kvIdx = kv_pos;
#if BPT_INT64MAP == 1
        insBit(bitmap, pos, kv_pos);
#else
        if (pos & 0xFFE0) {
            insBit(bitmap2, pos - 32, kv_pos);
        } else {
            byte last_bit = (*bitmap1 & 0x01);
            insBit(bitmap1, pos, kv_pos);
            *bitmap2 >>= 1;
            if (last_bit)
            *bitmap2 |= MASK32(0);
        }
#endif
#else
        byte *kvIdx = getPtrPos() + (pos << 1);
        memmove(kvIdx + 2, kvIdx, (filledSz - pos) * 2);
        util::setInt(kvIdx, kv_pos);
#endif
        setFilledSize(filledSz + 1);

    }

    void setPtr(int16_t pos, uint16_t ptr) {
#if BPT_9_BIT_PTR == 1
        *(getPtrPos() + pos) = ptr;
#if BPT_INT64MAP == 1
        if (ptr >= 256)
        *bitmap |= MASK64(pos);
        else
        *bitmap &= ~MASK64(pos);
#else
        if (pos & 0xFFE0) {
            pos -= 32;
            if (ptr >= 256)
            *bitmap2 |= MASK32(pos);
            else
            *bitmap2 &= ~MASK32(pos);
        } else {
            if (ptr >= 256)
            *bitmap1 |= MASK32(pos);
            else
            *bitmap1 &= ~MASK32(pos);
        }
#endif
#else
        byte *kvIdx = getPtrPos() + (pos << 1);
        return util::setInt(kvIdx, ptr);
#endif
    }

    inline void insBit(uint32_t *ui32, int pos, uint16_t kv_pos) {
        uint32_t ryte_part = (*ui32) & RYTE_MASK32(pos);
        ryte_part >>= 1;
        if (kv_pos >= 256)
            ryte_part |= MASK32(pos);
        (*ui32) = (ryte_part | ((*ui32) & LEFT_MASK32(pos)));

    }

#if BPT_INT64MAP == 1
    inline void insBit(uint64_t *ui64, int pos, uint16_t kv_pos) {
        uint64_t ryte_part = (*ui64) & RYTE_MASK64(pos);
        ryte_part >>= 1;
        if (kv_pos >= 256)
            ryte_part |= MASK64(pos);
        (*ui64) = (ryte_part | ((*ui64) & LEFT_MASK64(pos)));

    }
#endif

    virtual bool isFull() = 0;
    virtual void addFirstData() = 0;
    virtual void addData(int16_t idx) = 0;
    virtual int16_t searchCurrentBlock() = 0;
    virtual byte *split(byte *first_key, int16_t *first_len_ptr) = 0;
    virtual byte *getPtrPos() = 0;
    virtual int getHeaderSize() = 0;

    void printMaxKeyCount(long num_entries) {
        util::print("Block Count:");
        util::print((long) blockCountNode);
        util::print(", ");
        util::print((long) blockCountLeaf);
        util::endl();
        util::print("Avg Block Count:");
        util::print((long) (num_entries / blockCountLeaf));
        util::endl();
        util::print("Avg Max Count:");
        util::print((long) (maxKeyCountNode / (blockCountNode ? blockCountNode : 1)));
        util::print(", ");
        util::print((long) (maxKeyCountLeaf / blockCountLeaf));
        util::endl();
    }
    void printNumLevels() {
        util::print("Level Count:");
        util::print((long) numLevels);
    }
    void printCounts() {
        util::print("Count1:");
        util::print(count1);
        util::print(", Count2:");
        util::print(count2);
        util::print("\n");
    }
    long size() {
        return total_size;
    }

};

class bpt_trie_handler: public bplus_tree_handler {

protected:
    const static byte need_counts[10];
    virtual void decodeNeedCount() = 0;

    int maxTrieLenNode;
    int maxTrieLenLeaf;

    bpt_trie_handler(int16_t leaf_block_sz = 512, int16_t parent_block_sz = 512) :
        bplus_tree_handler(leaf_block_sz, parent_block_sz) {
        initCurrentBlock();
        init_stats();
    }

    void init_stats() {
        maxTrieLenLeaf = maxTrieLenNode = 0;
    }

    virtual void initCurrentBlock() {
        //memset(current_block, '\0', BFOS_NODE_SIZE);
        setLeaf(1);
        setFilledSize(0);
        BPT_MAX_KEY_LEN = 1;
        BPT_TRIE_LEN = 0;
        BPT_MAX_PFX_LEN = 1;
        keyPos = 1;
        setKVLastPos(leaf_block_size);
        insertState = INSERT_EMPTY;
    }

    virtual void setCurrentBlock(byte *m) {
        current_block = m;
        trie = current_block + getHeaderSize();
#if BPT_9_BIT_PTR == 1
#if BPT_INT64MAP == 1
        bitmap = (uint64_t *) (current_block + getHeaderSize() - 8);
#else
        bitmap1 = (uint32_t *) (current_block + getHeaderSize() - 8);
        bitmap2 = bitmap1 + 1;
#endif
#endif
    }

    virtual inline void updateSplitStats() {
        //std::cout << "Full\n" << std::endl;
        //if (maxKeyCount < block->filledSize())
        //    maxKeyCount = block->filledSize();
        //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->BPT_TRIE_LEN);
        //cout << (int) node->BPT_TRIE_LEN << endl;
        if (isLeaf()) {
            maxKeyCountLeaf += filledSize();
            maxTrieLenLeaf += BPT_TRIE_LEN;
            blockCountLeaf++;
        } else {
            maxKeyCountNode += filledSize();
            maxTrieLenNode += BPT_TRIE_LEN;
            blockCountNode++;
        }
        //    maxKeyCount += node->BPT_TRIE_LEN;
        //maxKeyCount += node->PREFIX_LEN;
    }

    inline void delAt(byte *ptr) {
        BPT_TRIE_LEN--;
        memmove(ptr, ptr + 1, trie + BPT_TRIE_LEN - ptr);
    }

    inline void delAt(byte *ptr, int16_t count) {
        BPT_TRIE_LEN -= count;
        memmove(ptr, ptr + count, trie + BPT_TRIE_LEN - ptr);
    }

    inline byte insAt(byte *ptr, byte b) {
        memmove(ptr + 1, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr = b;
        BPT_TRIE_LEN++;
        return 1;
    }

    inline byte insAt(byte *ptr, byte b1, byte b2) {
        memmove(ptr + 2, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr = b2;
        BPT_TRIE_LEN += 2;
        return 2;
    }

    inline byte insAt(byte *ptr, byte b1, byte b2, byte b3) {
        memmove(ptr + 3, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        BPT_TRIE_LEN += 3;
        return 3;
    }

    inline byte insAt(byte *ptr, byte b1, byte b2, byte b3, byte b4) {
        memmove(ptr + 4, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        BPT_TRIE_LEN += 4;
        return 4;
    }

    inline void insAt(byte *ptr, byte b, const char *s, byte len) {
        memmove(ptr + 1 + len, ptr, trie + BPT_TRIE_LEN - ptr);
        *ptr++ = b;
        memcpy(ptr, s, len);
        BPT_TRIE_LEN += len;
        BPT_TRIE_LEN++;
    }

    inline void insAt(byte *ptr, const char *s, byte len) {
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN - ptr);
        memcpy(ptr, s, len);
        BPT_TRIE_LEN += len;
    }

    void insBytes(byte *ptr, int16_t len) {
        memmove(ptr + len, ptr, trie + BPT_TRIE_LEN - ptr);
        BPT_TRIE_LEN += len;
    }

    inline void setAt(byte pos, byte b) {
        trie[pos] = b;
    }

    inline void append(byte b) {
        trie[BPT_TRIE_LEN++] = b;
    }

    inline void append(byte b1, byte b2) {
        trie[BPT_TRIE_LEN++] = b1;
        trie[BPT_TRIE_LEN++] = b2;
    }

    inline void append(byte b1, byte b2, byte b3) {
        trie[BPT_TRIE_LEN++] = b1;
        trie[BPT_TRIE_LEN++] = b2;
        trie[BPT_TRIE_LEN++] = b3;
    }

    inline void appendPtr(int16_t p) {
        util::setInt(trie + BPT_TRIE_LEN, p);
        BPT_TRIE_LEN += 2;
    }

    void append(const char *s, int16_t need_count) {
        memcpy(trie + BPT_TRIE_LEN, s, need_count);
        BPT_TRIE_LEN += need_count;
    }

public:
    byte *trie;
    byte *triePos;
    byte *origPos;
    byte need_count;
    byte insertState;
    byte keyPos;
    static const byte x00 = 0;
    static const byte x01 = 1;
    static const byte x02 = 2;
    static const byte x03 = 3;
    static const byte x04 = 4;
    static const byte x05 = 5;
    static const byte x06 = 6;
    static const byte x07 = 7;
    static const byte x08 = 8;
    static const byte x0F = 0x0F;
    static const byte x10 = 0x10;
    static const byte x11 = 0x11;
    static const byte x3F = 0x3F;
    static const byte x40 = 0x40;
    static const byte x41 = 0x41;
    static const byte x55 = 0x55;
    static const byte x7F = 0x7F;
    static const byte x80 = 0x80;
    static const byte x81 = 0x81;
    static const byte xAA = 0xAA;
    static const byte xBF = 0xBF;
    static const byte xC0 = 0xC0;
    static const byte xF8 = 0xF8;
    static const byte xFB = 0xFB;
    static const byte xFC = 0xFC;
    static const byte xFD = 0xFD;
    static const byte xFE = 0xFE;
    static const byte xFF = 0xFF;
    static const int16_t x100 = 0x100;
    virtual ~bpt_trie_handler() {}
    void printMaxKeyCount(long num_entries) {
        bplus_tree_handler::printMaxKeyCount(num_entries);
        util::print("Avg Trie Len:");
        util::print((long) (maxTrieLenNode / (blockCountNode ? blockCountNode : 1)));
        util::print(", ");
        util::print((long) (maxTrieLenLeaf / blockCountLeaf));
        util::endl();
    }

};

#endif
