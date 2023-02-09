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
#include <map>
#include <unordered_map>
#include "univix_util.h"
#include "lru_cache.h"
#include "bas_blk.h"

using namespace std;

#define BPT_IS_LEAF_BYTE current_block[0] & 0x80
#define BPT_IS_CHANGED current_block[0] & 0x40
#define BPT_LEVEL (current_block[0] & 0x1F)
#define BPT_FILLED_SIZE current_block + 1
#define BPT_LAST_DATA_PTR current_block + 3
#define BPT_MAX_KEY_LEN current_block[5]
#define BPT_TRIE_LEN_PTR current_block + 6
#define BPT_TRIE_LEN current_block[7]
#define BPT_MAX_PFX_LEN current_block[8]

#define BPT_LEAF0_LVL 14
#define BPT_STAGING_LVL 15
#define BPT_PARENT0_LVL 16

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
#define BPT_9_BIT_PTR 0

#if (defined(__AVR_ATmega328P__))
#define DEFAULT_PARENT_BLOCK_SIZE 512
#define DEFAULT_LEAF_BLOCK_SIZE 512
#elif (defined(__AVR_ATmega2560__))
#define DEFAULT_PARENT_BLOCK_SIZE 2048
#define DEFAULT_LEAF_BLOCK_SIZE 2048
#else
#define DEFAULT_PARENT_BLOCK_SIZE 4096
#define DEFAULT_LEAF_BLOCK_SIZE 4096
#endif

typedef unordered_map<int, bas_blk*> staging_map;

template<class T> // CRTP
class bplus_tree_handler {
protected:
    long total_size;
    int numLevels;
    int maxKeyCountNode;
    int maxKeyCountLeaf;
    int blockCountNode;
    int blockCountLeaf;
    long count1, count2;
    int max_key_len;
    int is_block_given;
    int root_page_num;

public:
    lru_cache *cache;
    uint8_t *root_block;
    uint8_t *current_block;
    unsigned long current_page;
    const uint8_t *key;
    int key_len;
    uint8_t *key_at;
    int key_at_len;
    const uint8_t *value;
    int16_t value_len;
    bool is_btree;
#if BPT_9_BIT_PTR == 1
#if BPT_INT64MAP == 1
    uint64_t *bitmap;
#else
    uint32_t *bitmap1;
    uint32_t *bitmap2;
#endif
#endif

    size_t leaf_block_size, parent_block_size;
    int cache_size;
    const char *filename;
    bool demote_blocks;
    bplus_tree_handler(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz_mb = 0,
            const char *fname = NULL, int start_page_num = 0, bool whether_btree = false) :
            leaf_block_size (leaf_block_sz), parent_block_size (parent_block_sz),
            cache_size (cache_sz_mb & 0xFFFF), filename (fname) {
        static_cast<T*>(this)->init_derived();
        init_stats();
        is_block_given = 0;
        root_page_num = start_page_num;
        is_btree = whether_btree;
        demote_blocks = false;
        if (cache_size > 0) {
            cache = new lru_cache(leaf_block_size, cache_size, filename,
                    &static_cast<T*>(this)->isBlockChanged, &static_cast<T*>(this)->setBlockChanged,
                    start_page_num, util::alignedAlloc);
            root_block = current_block = cache->get_disk_page_in_cache(start_page_num);
            if (cache->is_empty()) {
                static_cast<T*>(this)->setLeaf(1);
                static_cast<T*>(this)->initCurrentBlock();
            }
        } else {
            root_block = current_block = (uint8_t *) util::alignedAlloc(leaf_block_size);
            static_cast<T*>(this)->setLeaf(1);
            static_cast<T*>(this)->setCurrentBlock(root_block);
            static_cast<T*>(this)->initCurrentBlock();
        }
    }

    bplus_tree_handler(uint32_t block_sz, uint8_t *block, bool is_leaf, bool should_init = true) :
            leaf_block_size (block_sz), parent_block_size (block_sz),
            cache_size (0), filename (NULL) {
        is_block_given = 1;
        root_block = current_block = block;
        if (should_init) {
            static_cast<T*>(this)->setLeaf(is_leaf ? 1 : 0);
            static_cast<T*>(this)->setCurrentBlock(block);
            static_cast<T*>(this)->initCurrentBlock();
        }
    }

    ~bplus_tree_handler() {
        static_cast<T*>(this)->cleanup();
        if (cache_size > 0)
            delete cache;
        else if (!is_block_given)
            free(root_block);
    }

    void initCurrentBlock() {
        //memset(current_block, '\0', BFOS_NODE_SIZE);
        //cout << "Tree init block" << endl;
        if (!is_block_given) {
            static_cast<T*>(this)->setFilledSize(0);
            BPT_MAX_KEY_LEN = 0;
            static_cast<T*>(this)->setKVLastPos(
                static_cast<T*>(this)->isLeaf() ? leaf_block_size : parent_block_size);
        }
    }

    void init_stats() {
        total_size = maxKeyCountLeaf = maxKeyCountNode = blockCountNode = 0;
        numLevels = blockCountLeaf = 1;
        count1 = count2 = 0;
        max_key_len = 0;
    }

    inline uint8_t *getValueAt(uint8_t *key_ptr, int16_t *vlen) {
        key_ptr += *(key_ptr - 1);
        if (vlen != NULL)
            *vlen = *key_ptr;
        return (uint8_t *) key_ptr + 1;
    }

    inline uint8_t *getValueAt(int16_t *vlen) {
        if (vlen != NULL)
            *vlen = key_at[key_at_len];
        return (uint8_t *) key_at + key_at_len + 1;
    }

    void setCurrentBlockRoot();
    void setCurrentBlock(uint8_t *m);
    uint8_t *getCurrentBlock() {
        return current_block;
    }

    char *get(const char *key, int key_len, int16_t *pValueLen) {
        return (char *) get((uint8_t *) key, key_len, pValueLen);
    }
    uint8_t *get(const uint8_t *key, int key_len, int16_t *pValueLen) {
        static_cast<T*>(this)->setCurrentBlockRoot();
        this->key = key;
        this->key_len = key_len;
        current_page = root_page_num;
        int16_t search_result;
        if (static_cast<T*>(this)->isLeaf())
            search_result = static_cast<T*>(this)->searchCurrentBlock();
        else
            search_result = traverseToLeaf();
        if (search_result < 0)
            return NULL;
        return static_cast<T*>(this)->getValueAt(pValueLen);
    }

    inline bool isLeaf() {
        return BPT_IS_LEAF_BYTE;
    }

    uint8_t *skipChildren(uint8_t *t, uint8_t count);
    int16_t searchCurrentBlock();
    void setPrefixLast(uint8_t key_char, uint8_t *t, uint8_t pfx_rem_len);

    inline uint8_t *getKey(int16_t pos, int *plen) {
        uint8_t *kvIdx = current_block + getPtr(pos);
        *plen = *kvIdx;
        return kvIdx + 1;
    }
    uint8_t *getKey(uint8_t *t, int *plen);

    inline int getPtr(int16_t pos) {
#if BPT_9_BIT_PTR == 1
        uint16_t ptr = *(static_cast<T*>(this)->getPtrPos() + pos);
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
        return util::getInt(static_cast<T*>(this)->getPtrPos() + (pos << 1));
#endif
    }
    uint8_t *getPtrPos();

    int16_t traverseToLeaf(int8_t *plevel_count = NULL, uint8_t *node_paths[] = NULL) {
        current_page = root_page_num;
        uint8_t prev_lvl_split_count = 0;
        uint8_t *btree_rec_ptr = NULL;
        uint8_t *btree_found_blk = NULL;
        while (!static_cast<T*>(this)->isLeaf()) {
            if (node_paths) {
                *node_paths++ = cache_size > 0 ? (uint8_t *) current_page : current_block;
                (*plevel_count)++;
            }
            int16_t search_result = static_cast<T*>(this)->searchCurrentBlock();
            if (search_result >= 0 && is_btree) {
                btree_rec_ptr = key_at;
                btree_found_blk = current_block;
            }
            uint8_t *child_ptr_loc = static_cast<T*>(this)->getChildPtrPos(search_result);
            if (demote_blocks) {
                prev_lvl_split_count = child_ptr_loc[*child_ptr_loc + 1 + child_ptr_loc[*child_ptr_loc+1]];
            }
            uint8_t *child_ptr;
            if (cache_size > 0) {
                current_page = static_cast<T*>(this)->getChildPage(child_ptr_loc);
                child_ptr = cache->get_disk_page_in_cache(current_page, btree_found_blk);
            } else
                child_ptr = static_cast<T*>(this)->getChildPtr(child_ptr_loc);
            static_cast<T*>(this)->setCurrentBlock(child_ptr);
            //if (demote_blocks && current_block[5] < 255 && prev_lvl_split_count != current_block[5] - 1) {
            //    cout << "Split count not matching: " << (int) prev_lvl_split_count << " " << (int) current_block[5] << " " << (int) (current_block[0] & 0x1F) << endl;
            //}
        }
        if (btree_found_blk != NULL) {
            key_at = btree_rec_ptr;
            return 0;
        }
        return static_cast<T*>(this)->searchCurrentBlock();
    }

    uint8_t *getLastPtr();
    uint8_t *getChildPtrPos(int16_t search_result);
    inline uint8_t *getChildPtr(uint8_t *ptr) {
        ptr += (*ptr + 1);
        return (uint8_t *) (demote_blocks ? util::bytesToPtr(ptr, *ptr - 1) : util::bytesToPtr(ptr));
    }

    inline int getChildPage(uint8_t *ptr) {
        ptr += (*ptr + 1);
        return (demote_blocks ? util::bytesToPtr(ptr, *ptr - 1) : util::bytesToPtr(ptr));
    }

    inline int16_t filledSize() {
        return util::getInt(BPT_FILLED_SIZE);
    }

    inline uint16_t getKVLastPos() {
        return util::getInt(BPT_LAST_DATA_PTR);
    }

    uint8_t *allocateBlock(int size, int is_leaf, int lvl) {
        uint8_t *new_page;
        if (cache_size > 0) {
            new_page = cache->get_new_page(current_block);
            *new_page = 0x40; // Set changed so it gets written next time
        } else
            new_page = (uint8_t *) util::alignedAlloc(size);
        util::setInt(new_page + 3, size - (size == 65536 ? 1 : 0));
        util::setInt(new_page + 1, 0);
        if (is_leaf)
            *new_page = 0xC0 + lvl;
        else
            *new_page = 0x40 + lvl;
        new_page[5] = 1;
        return new_page;
    }

    char *put(const char *key, int key_len, const char *value,
            int16_t value_len, int16_t *pValueLen = NULL) {
        return (char *) put((const uint8_t *) key, key_len, (const uint8_t *) value, value_len, pValueLen);
    }
    uint8_t *put(const uint8_t *key, int key_len, const uint8_t *value,
            int16_t value_len, int16_t *pValueLen = NULL, bool only_if_not_full = false) {
        static_cast<T*>(this)->setCurrentBlockRoot();
        this->key = key;
        this->key_len = key_len;
        if (max_key_len < key_len)
            max_key_len = key_len;
        this->value = value;
        this->value_len = value_len;
        if (static_cast<T*>(this)->filledSize() == 0) {
            static_cast<T*>(this)->addFirstData();
            static_cast<T*>(this)->setChanged(1);
        } else {
            current_page = root_page_num;
            uint8_t **node_paths = (uint8_t **) malloc(8 * sizeof(void *));
            int8_t level_count = 1;
            int16_t search_result = static_cast<T*>(this)->isLeaf() ?
                    static_cast<T*>(this)->searchCurrentBlock() :
                    traverseToLeaf(&level_count, node_paths);
            numLevels = level_count;
            if (only_if_not_full) {
                if (static_cast<T*>(this)->isFull(~search_result)) {
                    *pValueLen = 9999;
                    free(node_paths);
                    return NULL;
                }
            }
            recursiveUpdate(search_result, node_paths, level_count - 1);
            free(node_paths);
        }
        total_size++;
        return NULL;
    }

    int get_level(uint8_t *block, int block_size) {
        return block[0] & 0x1F;
    }

    void set_level(uint8_t *block, int block_size, int lvl) {
        block[0] = (block[0] & 0xE0) + lvl;
    }

    int16_t compare_first_key(const uint8_t *key1, int k_len1,
                         const uint8_t *key2, int k_len2) {
        return util::compare(key1, k_len1, key2, k_len2);
    }

    void recursiveUpdate(int16_t search_result, uint8_t *node_paths[], uint8_t level) {
        //int16_t search_result = pos; // lastSearchPos[level];
        if (search_result < 0) {
            search_result = ~search_result;
            if (static_cast<T*>(this)->isFull(search_result)) {
                updateSplitStats();
                uint8_t first_key[200]; // is max_pfx_len sufficient?
                int16_t first_len;
                uint8_t *old_block = current_block;
                uint8_t *new_block = static_cast<T*>(this)->split(first_key, &first_len);
                static_cast<T*>(this)->setChanged(1);
                int lvl = static_cast<T*>(this)->get_level(old_block, static_cast<T*>(this)->isLeaf() ? leaf_block_size : parent_block_size);
                static_cast<T*>(this)->set_level(new_block, static_cast<T*>(this)->isLeaf() ? leaf_block_size : parent_block_size, lvl);
                int new_page = 0;
                if (cache_size > 0)
                    new_page = cache->get_page_count() - 1;
                int16_t cmp = static_cast<T*>(this)->compare_first_key(first_key, first_len, key, key_len);
                if (cmp <= 0)
                    static_cast<T*>(this)->setCurrentBlock(new_block);
                search_result = ~static_cast<T*>(this)->searchCurrentBlock();
                static_cast<T*>(this)->addData(search_result);
                //cout << "FK:" << level << ":" << first_key << endl;
                if (root_block == old_block) {
                    int new_lvl = lvl;
                    if (new_lvl == BPT_LEAF0_LVL)
                      new_lvl = BPT_PARENT0_LVL;
                    else if (new_lvl >= BPT_PARENT0_LVL)
                      new_lvl++;
                    blockCountNode++;
                    int old_page = 0;
                    if (cache_size > 0) {
                        old_block = cache->get_new_page(new_block);
                        old_page = cache->get_page_count() - 1;
                        memcpy(old_block, root_block,
                                static_cast<T*>(this)->isLeaf() ? leaf_block_size : parent_block_size);
                        static_cast<T*>(this)->setBlockChanged(old_block,
                                static_cast<T*>(this)->isLeaf() ? leaf_block_size : parent_block_size, 1);
                    } else
                        root_block = (uint8_t *) util::alignedAlloc(parent_block_size);
                    static_cast<T*>(this)->setCurrentBlock(root_block);
                    static_cast<T*>(this)->setLeaf(0);
                    static_cast<T*>(this)->initCurrentBlock();
                    static_cast<T*>(this)->setChanged(1);
                    static_cast<T*>(this)->set_level(current_block, static_cast<T*>(this)->isLeaf() ? leaf_block_size : parent_block_size, new_lvl);
                    static_cast<T*>(this)->add_first_kv_to_root(first_key, first_len,
                        cache_size > 0 ? (unsigned long) old_page : (unsigned long) old_block,
                        cache_size > 0 ? (unsigned long) new_page : (unsigned long) new_block);
                    numLevels++;
                } else {
                    int16_t prev_level = level - 1;
                    current_page = (unsigned long) node_paths[prev_level];
                    uint8_t *parent_data = cache_size > 0 ? cache->get_disk_page_in_cache(current_page) : node_paths[prev_level];
                    static_cast<T*>(this)->setCurrentBlock(parent_data);
                    uint8_t addr[9];
                    search_result = static_cast<T*>(this)->prepare_kv_to_add_to_parent(first_key, first_len, 
                                        cache_size > 0 ? (unsigned long) new_page : (unsigned long) new_block, addr);
                    recursiveUpdate(search_result, node_paths, prev_level);
                }
            } else {
                static_cast<T*>(this)->addData(search_result);
                static_cast<T*>(this)->setChanged(1);
            }
        } else {
            static_cast<T*>(this)->updateData();
        }
    }

    void add_first_kv_to_root(uint8_t *first_key, int16_t first_len, 
            unsigned long old_block_addr, unsigned long new_block_addr) {
        uint8_t addr[9];
        key = (uint8_t *) "";
        key_len = 1;
        value = (uint8_t *) addr;
        value_len = util::ptrToBytes(old_block_addr, addr);
        if (demote_blocks) {
            addr[value_len++] = 0;
        }
        //printf("value: %d, value_len1:%d\n", old_page, value_len);
        static_cast<T*>(this)->addFirstData();
        key = (uint8_t *) first_key;
        key_len = first_len;
        value = (uint8_t *) addr;
        value_len = util::ptrToBytes(new_block_addr, addr);
        if (demote_blocks) {
            addr[value_len++] = 0;
        }
        //printf("value: %d, value_len2:%d\n", new_page, value_len);
        int16_t search_result = ~static_cast<T*>(this)->searchCurrentBlock();
        static_cast<T*>(this)->addData(search_result);
    }

    int16_t prepare_kv_to_add_to_parent(uint8_t *first_key, int16_t first_len, unsigned long new_block_addr, uint8_t *addr) {
        key = (uint8_t *) first_key;
        key_len = first_len;
        value = (uint8_t *) addr;
        value_len = util::ptrToBytes(new_block_addr, addr);
        //printf("value: %d, value_len3:%d\n", new_page, value_len);
        int16_t search_result = static_cast<T*>(this)->searchCurrentBlock();
        if (demote_blocks) {
            uint8_t *split_count = static_cast<T*>(this)->findSplitSource(search_result);
            if (split_count != NULL) {
                if (*split_count < 254)
                    (*split_count)++;
                //if (*split_count == 255)
                //    *split_count = 1;
                /*if (*split_count == 254)
                    *split_count = 192;
                if (*split_count == 255)
                    *split_count = 64;*/
                addr[value_len++] = *split_count;
            } else {
                addr[value_len++] = 0;
            }
        }
        return search_result;
    }

    void updateData() {
        if (static_cast<T*>(this)->isLeaf()) {
            this->key_at += this->key_at_len;
            if (*key_at == this->value_len)
                memcpy((uint8_t *) key_at + 1, this->value, this->value_len);
            static_cast<T*>(this)->setChanged(1);
        } else {
            cout << "searchResult >=0 for parent" << endl;
        }
    }

    bool isFull(int16_t search_result);
    void addFirstData();
    void addData(int16_t search_result);
    void insertCurrent();

    void setValue(const uint8_t *val, int16_t len) {
        value = val;
        value_len = len;
    }

    inline void setFilledSize(int16_t filledSize) {
        util::setInt(BPT_FILLED_SIZE, filledSize);
    }

    inline void insPtr(int16_t pos, uint16_t kv_pos) {
        int16_t filledSz = static_cast<T*>(this)->filledSize();
#if BPT_9_BIT_PTR == 1
        uint8_t *kvIdx = static_cast<T*>(this)->getPtrPos() + pos;
        memmove(kvIdx + 1, kvIdx, filledSz - pos);
        *kvIdx = kv_pos;
#if BPT_INT64MAP == 1
        insBit(bitmap, pos, kv_pos);
#else
        if (pos & 0xFFE0) {
            insBit(bitmap2, pos - 32, kv_pos);
        } else {
            uint8_t last_bit = (*bitmap1 & 0x01);
            insBit(bitmap1, pos, kv_pos);
            *bitmap2 >>= 1;
            if (last_bit)
            *bitmap2 |= MASK32(0);
        }
#endif
#else
        uint8_t *kvIdx = static_cast<T*>(this)->getPtrPos() + (pos << 1);
        memmove(kvIdx + 2, kvIdx, (filledSz - pos) * 2);
        util::setInt(kvIdx, kv_pos);
#endif
        static_cast<T*>(this)->setFilledSize(filledSz + 1);

    }

    inline void delPtr(int16_t pos) {
        int16_t filledSz = static_cast<T*>(this)->filledSize() - 1;
#if BPT_9_BIT_PTR == 1
        uint8_t *kvIdx = static_cast<T*>(this)->getPtrPos() + pos;
        memmove(kvIdx, kvIdx + 1, filledSz - pos);
#if BPT_INT64MAP == 1
        delBit(bitmap, pos);
#else
        if (pos & 0xFFE0) {
            delBit(bitmap2, pos - 32);
        } else {
            uint8_t first_bit = (*bitmap2 >> 7);
            delBit(bitmap1, pos);
            *bitmap2 <<= 1;
            if (first_bit)
                *bitmap1 |= 0x01;
        }
#endif
#else
        uint8_t *kvIdx = static_cast<T*>(this)->getPtrPos() + (pos << 1);
        memmove(kvIdx, kvIdx + 2, (filledSz - pos) * 2);
#endif
        static_cast<T*>(this)->setFilledSize(filledSz);
        *current_block |= 0x20;
    }

    inline void setPtr(int16_t pos, uint16_t ptr) {
#if BPT_9_BIT_PTR == 1
        *(static_cast<T*>(this)->getPtrPos() + pos) = ptr;
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
        uint8_t *kvIdx = static_cast<T*>(this)->getPtrPos() + (pos << 1);
        return util::setInt(kvIdx, ptr);
#endif
    }

    inline void updateSplitStats() {
        if (static_cast<T*>(this)->isLeaf()) {
            maxKeyCountLeaf += static_cast<T*>(this)->filledSize();
            blockCountLeaf++;
        } else {
            maxKeyCountNode += static_cast<T*>(this)->filledSize();
            blockCountNode++;
        }
    }

    inline void setLeaf(char isLeaf) {
        if (isLeaf) {
            current_block[0] = (current_block[0] & 0x60) | BPT_LEAF0_LVL | 0x80;
        } else
            current_block[0] &= 0x7F;
    }

    void setChanged(bool isChanged) {
        if (isChanged)
            current_block[0] |= 0x40;
        else
            current_block[0] &= 0xBF;
    }

    bool isChanged() {
        return current_block[0] & 0x40;
    }

    static void setBlockChanged(uint8_t *block, int block_size, bool isChanged) {
        if (isChanged)
            block[0] |= 0x40;
        else
            block[0] &= 0xBF;
    }

    static bool isBlockChanged(uint8_t *block, int block_size) {
        return block[0] & 0x40;
    }

    inline void setKVLastPos(uint16_t val) {
        if (val == 0)
           val = 65535;
        util::setInt(BPT_LAST_DATA_PTR, val);
    }

    inline void insBit(uint32_t *ui32, int pos, uint16_t kv_pos) {
        uint32_t ryte_part = (*ui32) & RYTE_MASK32(pos);
        ryte_part >>= 1;
        if (kv_pos >= 256)
            ryte_part |= MASK32(pos);
        (*ui32) = (ryte_part | ((*ui32) & LEFT_MASK32(pos)));

    }

    inline void delBit(uint32_t *ui32, int pos) {
        uint32_t ryte_part = (*ui32) & RYTE_MASK32(pos);
        ryte_part <<= 1;
        (*ui32) = (ryte_part | ((*ui32) & LEFT_MASK32(pos)));
    }

    uint8_t *split(uint8_t *first_key, int16_t *first_len_ptr);

#if BPT_INT64MAP == 1
    inline void insBit(uint64_t *ui64, int pos, uint16_t kv_pos) {
        uint64_t ryte_part = (*ui64) & RYTE_MASK64(pos);
        ryte_part >>= 1;
        if (kv_pos >= 256)
            ryte_part |= MASK64(pos);
        (*ui64) = (ryte_part | ((*ui64) & LEFT_MASK64(pos)));

    }
    inline void delBit(uint64_t *ui64, int pos) {
        uint64_t ryte_part = (*ui64) & RYTE_MASK64(pos);
        ryte_part <<= 1;
        (*ui64) = (ryte_part | ((*ui64) & LEFT_MASK64(pos)));

    }
#endif

    int getNumLevels() {
        return numLevels;
    }

    void printStats(long num_entries) {
        util::print("Block Count:");
        util::print((long) blockCountNode);
        util::print(", ");
        util::print((long) blockCountLeaf);
        util::endl();
        util::print("Avg Block Count:");
        util::print((long) (num_entries / blockCountLeaf));
        util::print(" = ");
        util::print((long) num_entries);
        util::print(" / ");
        util::print((long) blockCountLeaf);
        util::endl();
        util::print("Avg Max Count:");
        util::print((long) (maxKeyCountNode / (blockCountNode ? blockCountNode : 1)));
        util::print(", ");
        util::print((long) (maxKeyCountLeaf / blockCountLeaf));
        util::print(", ");
        util::print((long) max_key_len);
        util::endl();
    }
    void printNumLevels() {
        util::print("Level Count:");
        util::print((long) numLevels);
        util::endl();
    }
    void printCounts() {
        util::print("Count1:");
        util::print(count1);
        util::print(", Count2:");
        util::print(count2);
        util::print("\n");
        util::endl();
    }
    long size() {
        return total_size;
    }
    int get_max_key_len() {
        return max_key_len;
    }
    cache_stats get_cache_stats() {
        return cache->get_cache_stats();
    }

};

template<class T>
class bpt_trie_handler: public bplus_tree_handler<T> {

protected:
    int maxTrieLenNode;
    int maxTrieLenLeaf;

    bpt_trie_handler<T>(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
       bplus_tree_handler<T>(leaf_block_sz, parent_block_sz, cache_sz, fname) {
        init_stats();
    }

    bpt_trie_handler<T>(uint32_t block_sz, uint8_t *block, bool is_leaf) :
       bplus_tree_handler<T>(block_sz, block, is_leaf) {
        init_stats();
    }

    void init_stats() {
        maxTrieLenLeaf = maxTrieLenNode = 0;
    }

    inline void updateSplitStats() {
        if (static_cast<T*>(this)->isLeaf()) {
            bplus_tree_handler<T>::maxKeyCountLeaf += static_cast<T*>(this)->filledSize();
            maxTrieLenLeaf += bplus_tree_handler<T>::BPT_TRIE_LEN + (*(bplus_tree_handler<T>::BPT_TRIE_LEN_PTR) << 8);
            bplus_tree_handler<T>::blockCountLeaf++;
        } else {
            bplus_tree_handler<T>::maxKeyCountNode += static_cast<T*>(this)->filledSize();
            maxTrieLenNode += bplus_tree_handler<T>::BPT_TRIE_LEN + (*(bplus_tree_handler<T>::BPT_TRIE_LEN_PTR) << 8);
            bplus_tree_handler<T>::blockCountNode++;
        }
    }

    inline void delAt(uint8_t *ptr) {
        bplus_tree_handler<T>::BPT_TRIE_LEN--;
        memmove(ptr, ptr + 1, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
    }

    inline void delAt(uint8_t *ptr, int16_t count) {
        bplus_tree_handler<T>::BPT_TRIE_LEN -= count;
        memmove(ptr, ptr + count, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
    }

    inline uint8_t insAt(uint8_t *ptr, uint8_t b) {
        memmove(ptr + 1, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        *ptr = b;
        bplus_tree_handler<T>::BPT_TRIE_LEN++;
        return 1;
    }

    inline uint8_t insAt(uint8_t *ptr, uint8_t b1, uint8_t b2) {
        memmove(ptr + 2, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr = b2;
        bplus_tree_handler<T>::BPT_TRIE_LEN += 2;
        return 2;
    }

    inline uint8_t insAt(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3) {
        memmove(ptr + 3, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        bplus_tree_handler<T>::BPT_TRIE_LEN += 3;
        return 3;
    }

    inline uint8_t insAt(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
        memmove(ptr + 4, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        bplus_tree_handler<T>::BPT_TRIE_LEN += 4;
        return 4;
    }

    inline void insAt(uint8_t *ptr, uint8_t b, const uint8_t *s, uint8_t len) {
        memmove(ptr + 1 + len, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        *ptr++ = b;
        memcpy(ptr, s, len);
        bplus_tree_handler<T>::BPT_TRIE_LEN += len;
        bplus_tree_handler<T>::BPT_TRIE_LEN++;
    }

    inline void insAt(uint8_t *ptr, const uint8_t *s, uint8_t len) {
        memmove(ptr + len, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        memcpy(ptr, s, len);
        bplus_tree_handler<T>::BPT_TRIE_LEN += len;
    }

    void insBytes(uint8_t *ptr, int16_t len) {
        memmove(ptr + len, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        bplus_tree_handler<T>::BPT_TRIE_LEN += len;
    }

    inline void setAt(uint8_t pos, uint8_t b) {
        trie[pos] = b;
    }

    inline void append(uint8_t b) {
        trie[bplus_tree_handler<T>::BPT_TRIE_LEN++] = b;
    }

    inline void appendPtr(uint16_t p) {
        util::setInt(trie + bplus_tree_handler<T>::BPT_TRIE_LEN, p);
        bplus_tree_handler<T>::BPT_TRIE_LEN += 2;
    }

    void append(const uint8_t *s, int16_t need_count) {
        memcpy(trie + bplus_tree_handler<T>::BPT_TRIE_LEN, s, need_count);
        bplus_tree_handler<T>::BPT_TRIE_LEN += need_count;
    }

public:
    uint8_t *trie;
    uint8_t *triePos;
    uint8_t *origPos;
    uint8_t need_count;
    int16_t insertState;
    uint8_t keyPos;
    static const uint8_t x00 = 0;
    static const uint8_t x01 = 1;
    static const uint8_t x02 = 2;
    static const uint8_t x03 = 3;
    static const uint8_t x04 = 4;
    static const uint8_t x05 = 5;
    static const uint8_t x06 = 6;
    static const uint8_t x07 = 7;
    static const uint8_t x08 = 8;
    static const uint8_t x0F = 0x0F;
    static const uint8_t x10 = 0x10;
    static const uint8_t x11 = 0x11;
    static const uint8_t x3F = 0x3F;
    static const uint8_t x40 = 0x40;
    static const uint8_t x41 = 0x41;
    static const uint8_t x55 = 0x55;
    static const uint8_t x7F = 0x7F;
    static const uint8_t x80 = 0x80;
    static const uint8_t x81 = 0x81;
    static const uint8_t xAA = 0xAA;
    static const uint8_t xBF = 0xBF;
    static const uint8_t xC0 = 0xC0;
    static const uint8_t xF8 = 0xF8;
    static const uint8_t xFB = 0xFB;
    static const uint8_t xFC = 0xFC;
    static const uint8_t xFD = 0xFD;
    static const uint8_t xFE = 0xFE;
    static const uint8_t xFF = 0xFF;
    static const int16_t x100 = 0x100;
    ~bpt_trie_handler() {}
    void printStats(long num_entries) {
        bplus_tree_handler<T>::printStats(num_entries);
        util::print("Avg Trie Len:");
        util::print((long) (maxTrieLenNode
                / (bplus_tree_handler<T>::blockCountNode ? bplus_tree_handler<T>::blockCountNode : 1)));
        util::print(", ");
        util::print((long) (maxTrieLenLeaf / bplus_tree_handler<T>::blockCountLeaf));
        util::endl();
    }
    void initCurrentBlock() {
        //cout << "Trie init block" << endl;
        bplus_tree_handler<T>::initCurrentBlock();
        *(bplus_tree_handler<T>::BPT_TRIE_LEN_PTR) = 0;
        bplus_tree_handler<T>::BPT_TRIE_LEN = 0;
        bplus_tree_handler<T>::BPT_MAX_PFX_LEN = 1;
        keyPos = 1;
        insertState = INSERT_EMPTY;
    }

};

#endif
