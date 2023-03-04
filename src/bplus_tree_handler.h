#ifndef BP_TREE_H
#define BP_TREE_H
#ifdef ARDUINO
#include <Hardware_serial.h>
#include <string.h>
#else
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include <stdint.h>
#include "univix_util.h"
#include "lru_cache.h"

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

#define descendant static_cast<T*>(this)

union page_ptr {
    unsigned long page;
    uint8_t *ptr;
};

#define MAX_LVL_COUNT 10
class bptree_iter_ctx {
    public:
        page_ptr pages[MAX_LVL_COUNT];
        int found_page_pos;
        int8_t last_page_lvl;
        int8_t found_page_idx;
        void init(unsigned long page, uint8_t *ptr, int cache_size) {
            last_page_lvl = 0;
            found_page_idx = 0;
            found_page_pos = -1;
            set(page, ptr, cache_size);
        }
        void set(unsigned long page, uint8_t *ptr, int cache_size) {
            if (cache_size > 0)
                pages[last_page_lvl].page = page;
            else
                pages[last_page_lvl].ptr = ptr;
        }
};

template<class T> // CRTP
class bplus_tree_handler {
protected:
    long total_size;
    int num_levels;
    int max_key_count_node;
    int max_key_count_leaf;
    int block_count_node;
    int block_count_leaf;
    long count1, count2;
    int max_key_len;
    int is_block_given;
    int root_page_num;
    bool is_closed;

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
    int value_len;
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
    bool to_demote_blocks;
    bplus_tree_handler(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz_mb = 0,
            const char *fname = NULL, int start_page_num = 0, bool whether_btree = false) :
            leaf_block_size (leaf_block_sz), parent_block_size (parent_block_sz),
            cache_size (cache_sz_mb & 0xFFFF), filename (fname) {
        descendant->init_derived();
        init_stats();
        is_closed = false;
        is_block_given = 0;
        root_page_num = start_page_num;
        is_btree = whether_btree;
        to_demote_blocks = false;
        if (cache_size > 0) {
            cache = new lru_cache(leaf_block_size, cache_size, filename,
                    &descendant->is_block_changed, &descendant->set_block_changed,
                    start_page_num, util::aligned_alloc);
            root_block = current_block = cache->get_disk_page_in_cache(start_page_num);
            if (cache->is_empty()) {
                descendant->set_leaf(1);
                descendant->init_current_block();
            }
        } else {
            root_block = current_block = (uint8_t *) util::aligned_alloc(leaf_block_size);
            descendant->set_leaf(1);
            descendant->set_current_block(root_block);
            descendant->init_current_block();
        }
    }

    bplus_tree_handler(uint32_t block_sz, uint8_t *block, bool is_leaf, bool should_init = true) :
            leaf_block_size (block_sz), parent_block_size (block_sz),
            cache_size (0), filename (NULL) {
        is_block_given = 1;
        is_closed = false;
        root_block = current_block = block;
        if (should_init) {
            descendant->set_leaf(is_leaf ? 1 : 0);
            descendant->set_current_block(block);
            descendant->init_current_block();
        } else
            descendant->set_current_block(block);
    }

    ~bplus_tree_handler() {
        if (!is_closed)
            close();
    }

    void close() {
        descendant->cleanup();
        if (cache_size > 0)
            delete cache;
        else if (!is_block_given)
            free(root_block);
    }

    void init_current_block() {
        //memset(current_block, '\0', BFOS_NODE_SIZE);
        //cout << "Tree init block" << endl;
        if (!is_block_given) {
            descendant->set_filled_size(0);
            BPT_MAX_KEY_LEN = 0;
            descendant->set_kv_last_pos(
                descendant->is_leaf() ? leaf_block_size : parent_block_size);
        }
    }

    void init_stats() {
        total_size = max_key_count_leaf = max_key_count_node = block_count_node = 0;
        num_levels = block_count_leaf = 1;
        count1 = count2 = 0;
        max_key_len = 0;
    }

    inline uint8_t *get_value_at(uint8_t *key_ptr, int *vlen) {
        key_ptr += *(key_ptr - 1);
        if (vlen != NULL)
            *vlen = *key_ptr;
        return (uint8_t *) key_ptr + 1;
    }

    inline uint8_t *get_value_at(int *vlen) {
        if (vlen != NULL)
            *vlen = key_at[key_at_len];
        return (uint8_t *) key_at + key_at_len + 1;
    }

    void copy_value(uint8_t *val, int *vlen) {
        if (vlen != NULL)
            *vlen = key_at[key_at_len];
        memcpy(val, key_at + key_at_len + 1, *vlen);
    }

    uint8_t *get_current_block() {
        return current_block;
    }

    std::string get_string(std::string& key, std::string& not_found_value) {
        bool ret = get(key.c_str(), key.length(), NULL, NULL);
        if (ret) {
            uint8_t *val = (uint8_t *) malloc(key_at_len);
            int val_len;
            descendant->copy_value(val, &val_len);
            return std::string((const char *) val, val_len);
        }
        return not_found_value;
    }

    bool get(const char *key, int key_len, int *in_size_out_val_len = NULL,
                 char *val = NULL, bptree_iter_ctx *ctx = NULL) {
        return get((uint8_t *) key, key_len, in_size_out_val_len, (uint8_t *) val, ctx);
    }
    bool get(const uint8_t *key, int key_len, int *in_size_out_val_len = NULL,
                 uint8_t *val = NULL, bptree_iter_ctx *ctx = NULL) {
        descendant->set_current_block_root();
        current_page = root_page_num;
        if (ctx)
            ctx->init(current_page, current_block, cache_size);
        this->key = key;
        this->key_len = key_len;
        int search_result = traverse_to_leaf(ctx);
        if (search_result < 0)
            return false;
        if (in_size_out_val_len != NULL)
            *in_size_out_val_len = key_at_len;
        if (val != NULL)
            descendant->copy_value(val, in_size_out_val_len);
        return true;
    }

    inline bool is_leaf() {
        return BPT_IS_LEAF_BYTE;
    }

    uint8_t *skip_children(uint8_t *t, uint8_t count);
    int search_current_block();
    void set_prefix_last(uint8_t key_char, uint8_t *t, uint8_t pfx_rem_len);

    inline uint8_t *get_key(int pos, int *plen) {
        uint8_t *kv_idx = current_block + get_ptr(pos);
        *plen = *kv_idx;
        return kv_idx + 1;
    }
    uint8_t *get_key(uint8_t *t, int *plen);

    inline int get_ptr(int pos) {
#if BPT_9_BIT_PTR == 1
        int ptr = *(descendant->get_ptr_pos() + pos);
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
        return util::get_int(descendant->get_ptr_pos() + (pos << 1));
#endif
    }

    int traverse_to_leaf(bptree_iter_ctx *ctx = NULL) {
        uint8_t prev_lvl_split_count = 0;
        while (!descendant->is_leaf()) {
            if (ctx) {
                ctx->set(current_page, current_block, cache_size);
                ctx->last_page_lvl++;
            }
            int search_result = descendant->search_current_block(ctx);
            if (search_result >= 0 && is_btree)
                return search_result;
            uint8_t *child_ptr_loc = descendant->get_child_ptr_pos(search_result);
            if (to_demote_blocks) {
                prev_lvl_split_count = child_ptr_loc[*child_ptr_loc + 1 + child_ptr_loc[*child_ptr_loc+1]];
            }
            uint8_t *child_ptr;
            if (cache_size > 0) {
                current_page = descendant->get_child_page(child_ptr_loc);
                child_ptr = cache->get_disk_page_in_cache(current_page);
            } else
                child_ptr = descendant->get_child_ptr(child_ptr_loc);
            descendant->set_current_block(child_ptr);
            //if (to_demote_blocks && current_block[5] < 255 && prev_lvl_split_count != current_block[5] - 1) {
            //    cout << "Split count not matching: " << (int) prev_lvl_split_count << " " << (int) current_block[5] << " " << (int) (current_block[0] & 0x1F) << endl;
            //}
        }
        return descendant->search_current_block(ctx);
    }

    inline uint8_t *get_child_ptr(uint8_t *ptr) {
        ptr += (*ptr + 1);
        return (uint8_t *) (to_demote_blocks ? util::bytes_to_ptr(ptr, *ptr - 1) : util::bytes_to_ptr(ptr));
    }

    inline int get_child_page(uint8_t *ptr) {
        ptr += (*ptr + 1);
        return (to_demote_blocks ? util::bytes_to_ptr(ptr, *ptr - 1) : util::bytes_to_ptr(ptr));
    }

    inline int filled_size() {
        return util::get_int(BPT_FILLED_SIZE);
    }

    inline int get_kv_last_pos() {
        return util::get_int(BPT_LAST_DATA_PTR);
    }

    #define BPT_BLK_TYPE_INTERIOR 0
    #define BPT_BLK_TYPE_LEAF 128
    #define BPT_BLK_TYPE_OVFL 255
    uint8_t *allocate_block(int size, int type, int lvl) {
        uint8_t *new_page;
        if (cache_size > 0) {
            new_page = cache->get_new_page(current_block);
            *new_page = 0x40; // Set changed so it gets written next time
        } else
            new_page = (uint8_t *) util::aligned_alloc(size);
        util::set_int(new_page + 3, size - (size == 65536 ? 1 : 0));
        util::set_int(new_page + 1, 0);
        if (type != BPT_BLK_TYPE_OVFL) {
            if (type == BPT_BLK_TYPE_INTERIOR)
                *new_page = 0x40 + lvl;
            else
                *new_page = 0xC0 + lvl;
        }
        new_page[5] = 1;
        return new_page;
    }

    bool put_string(std::string& key, std::string& value) {
        return put(key.c_str(), key.length(), value.c_str(), value.length());
    }
    bool put(const char *key, int key_len, const char *value,
            int value_len, bptree_iter_ctx *ctx = NULL) {
        return put((const uint8_t *) key, key_len, (const uint8_t *) value, value_len, ctx);
    }
    bool put(const uint8_t *key, int key_len, const uint8_t *value,
            int value_len, bptree_iter_ctx *ctx = NULL, bool only_if_not_full = false) {
        descendant->set_current_block_root();
        this->key = key;
        this->key_len = key_len;
        if (max_key_len < key_len)
            max_key_len = key_len;
        this->value = value;
        this->value_len = value_len;
        current_page = root_page_num;
        bool is_ctx_given = true;
        if (ctx == NULL) {
            ctx = new bptree_iter_ctx();
            is_ctx_given = false;
        }
        ctx->init(current_page, current_block, cache_size);
        if (descendant->filled_size() == 0) {
            descendant->add_first_data();
            descendant->set_changed(1);
        } else {
            int search_result = traverse_to_leaf(ctx);
            num_levels = ctx->last_page_lvl + 1;
            if (only_if_not_full) {
                if (descendant->is_full(~search_result)) {
                    if (is_ctx_given)
                        delete ctx;
                    return false;
                }
            }
            recursive_update(search_result, ctx, ctx->last_page_lvl);
            if (search_result >= 0) {
                if (is_ctx_given)
                    delete ctx;
                descendant->set_changed(1);
                return true;
            }
        }
        total_size++;
        if (is_ctx_given)
            delete ctx;
        return false;
    }

    int get_level(uint8_t *block, int block_size) {
        return block[0] & 0x1F;
    }

    void set_level(uint8_t *block, int block_size, int lvl) {
        block[0] = (block[0] & 0xE0) + lvl;
    }

    int compare_first_key(const uint8_t *key1, int k_len1,
                         const uint8_t *key2, int k_len2) {
        return util::compare(key1, k_len1, key2, k_len2);
    }

    int get_first_key_len() {
        return 255;
    }

    void recursive_update(int search_result, bptree_iter_ctx *ctx, int prev_level) {
        if (search_result < 0) {
            search_result = ~search_result;
            if (descendant->is_full(search_result)) {
                update_split_stats();
                uint8_t first_key[descendant->get_first_key_len()]; // is max_pfx_len sufficient?
                int first_len;
                uint8_t *old_block = current_block;
                uint8_t *new_block = descendant->split(first_key, &first_len);
                descendant->set_changed(1);
                int lvl = descendant->get_level(old_block, descendant->is_leaf() ? leaf_block_size : parent_block_size);
                descendant->set_level(new_block, descendant->is_leaf() ? leaf_block_size : parent_block_size, lvl);
                int new_page = 0;
                if (cache_size > 0)
                    new_page = cache->get_page_count() - 1;
                int cmp = descendant->compare_first_key(first_key, first_len, key, key_len);
                if (cmp <= 0)
                    descendant->set_current_block(new_block);
                search_result = ~descendant->search_current_block();
                descendant->add_data(search_result);
                //cout << "FK:" << level << ":" << first_key << endl;
                if (root_block == old_block) {
                    int new_lvl = lvl;
                    if (new_lvl == BPT_LEAF0_LVL)
                      new_lvl = BPT_PARENT0_LVL;
                    else if (new_lvl >= BPT_PARENT0_LVL)
                      new_lvl++;
                    block_count_node++;
                    int old_page = 0;
                    if (cache_size > 0) {
                        int block_size = descendant->is_leaf() ? leaf_block_size : parent_block_size;
                        old_block = descendant->allocate_block(block_size, descendant->is_leaf(), new_lvl);
                        old_page = cache->get_page_count() - 1;
                        memcpy(old_block, root_block, block_size);
                        descendant->set_block_changed(old_block, block_size, true);
                    } else
                        root_block = (uint8_t *) util::aligned_alloc(parent_block_size);
                    descendant->set_current_block(root_block);
                    descendant->set_leaf(0);
                    descendant->init_current_block();
                    descendant->set_changed(1);
                    descendant->set_level(current_block, descendant->is_leaf() ? leaf_block_size : parent_block_size, new_lvl);
                    descendant->add_first_kv_to_root(first_key, first_len,
                        cache_size > 0 ? (unsigned long) old_page : (unsigned long) old_block,
                        cache_size > 0 ? (unsigned long) new_page : (unsigned long) new_block);
                    num_levels++;
                } else {
                    prev_level = prev_level - 1;
                    current_page = ctx->pages[prev_level].page;
                    uint8_t *parent_data = cache_size > 0 ? cache->get_disk_page_in_cache(current_page) : ctx->pages[prev_level].ptr;
                    descendant->set_current_block(parent_data);
                    uint8_t addr[9];
                    search_result = descendant->prepare_kv_to_add_to_parent(first_key, first_len, 
                                        cache_size > 0 ? (unsigned long) new_page : (unsigned long) new_block, addr);
                    recursive_update(search_result, ctx, prev_level);
                }
            } else {
                descendant->add_data(search_result);
                descendant->set_changed(1);
            }
        } else {
            descendant->update_data();
        }
    }

    void add_first_kv_to_root(uint8_t *first_key, int first_len, 
            unsigned long old_block_addr, unsigned long new_block_addr) {
        uint8_t addr[9];
        key = (uint8_t *) "";
        key_len = 1;
        value = (uint8_t *) addr;
        value_len = util::ptr_to_bytes(old_block_addr, addr);
        if (to_demote_blocks) {
            addr[value_len++] = 0;
        }
        //printf("value: %d, value_len1:%d\n", old_page, value_len);
        descendant->add_first_data();
        key = (uint8_t *) first_key;
        key_len = first_len;
        value = (uint8_t *) addr;
        value_len = util::ptr_to_bytes(new_block_addr, addr);
        if (to_demote_blocks) {
            addr[value_len++] = 0;
        }
        //printf("value: %d, value_len2:%d\n", new_page, value_len);
        int search_result = ~descendant->search_current_block();
        descendant->add_data(search_result);
    }

    int prepare_kv_to_add_to_parent(uint8_t *first_key, int first_len, unsigned long new_block_addr, uint8_t *addr) {
        key = (uint8_t *) first_key;
        key_len = first_len;
        value = (uint8_t *) addr;
        value_len = util::ptr_to_bytes(new_block_addr, addr);
        //printf("value: %d, value_len3:%d\n", new_page, value_len);
        int search_result = descendant->search_current_block();
        if (to_demote_blocks) {
            uint8_t *split_count = descendant->find_split_source(search_result);
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

    void update_data() {
        if (descendant->is_leaf()) {
            uint8_t *value_at = this->key_at;
            value_at += this->key_at_len;
            if (*value_at == this->value_len)
                memcpy((uint8_t *) value_at + 1, this->value, this->value_len);
            descendant->set_changed(1);
        } else {
            cout << "search_result >=0 for parent" << endl;
        }
    }

    bool is_full(int search_result);
    void add_first_data();
    void add_data(int search_result);
    void insert_current();

    void set_value(const uint8_t *val, int len) {
        value = val;
        value_len = len;
    }

    inline void set_filled_size(int filled_size) {
        util::set_int(BPT_FILLED_SIZE, filled_size);
    }

    inline void ins_ptr(int pos, int kv_pos) {
        int filled_sz = descendant->filled_size();
#if BPT_9_BIT_PTR == 1
        uint8_t *kv_idx = descendant->get_ptr_pos() + pos;
        memmove(kv_idx + 1, kv_idx, filled_sz - pos);
        *kv_idx = kv_pos;
#if BPT_INT64MAP == 1
        ins_bit(bitmap, pos, kv_pos);
#else
        if (pos & 0xFFE0) {
            ins_bit(bitmap2, pos - 32, kv_pos);
        } else {
            uint8_t last_bit = (*bitmap1 & 0x01);
            ins_bit(bitmap1, pos, kv_pos);
            *bitmap2 >>= 1;
            if (last_bit)
            *bitmap2 |= MASK32(0);
        }
#endif
#else
        uint8_t *kv_idx = descendant->get_ptr_pos() + (pos << 1);
        memmove(kv_idx + 2, kv_idx, (filled_sz - pos) * 2);
        util::set_int(kv_idx, kv_pos);
#endif
        descendant->set_filled_size(filled_sz + 1);

    }

    inline void del_ptr(int pos) {
        int filled_sz = descendant->filled_size() - 1;
#if BPT_9_BIT_PTR == 1
        uint8_t *kv_idx = descendant->get_ptr_pos() + pos;
        memmove(kv_idx, kv_idx + 1, filled_sz - pos);
#if BPT_INT64MAP == 1
        del_bit(bitmap, pos);
#else
        if (pos & 0xFFE0) {
            del_bit(bitmap2, pos - 32);
        } else {
            uint8_t first_bit = (*bitmap2 >> 7);
            del_bit(bitmap1, pos);
            *bitmap2 <<= 1;
            if (first_bit)
                *bitmap1 |= 0x01;
        }
#endif
#else
        uint8_t *kv_idx = descendant->get_ptr_pos() + (pos << 1);
        memmove(kv_idx, kv_idx + 2, (filled_sz - pos) * 2);
#endif
        descendant->set_filled_size(filled_sz);
        *current_block |= 0x20;
    }

    inline void set_ptr(int pos, int ptr) {
#if BPT_9_BIT_PTR == 1
        *(descendant->get_ptr_pos() + pos) = ptr;
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
        uint8_t *kv_idx = descendant->get_ptr_pos() + (pos << 1);
        return util::set_int(kv_idx, ptr);
#endif
    }

    inline void update_split_stats() {
        if (descendant->is_leaf()) {
            max_key_count_leaf += descendant->filled_size();
            block_count_leaf++;
        } else {
            max_key_count_node += descendant->filled_size();
            block_count_node++;
        }
    }

    inline void set_leaf(char is_leaf) {
        if (is_leaf) {
            current_block[0] = (current_block[0] & 0x60) | BPT_LEAF0_LVL | 0x80;
        } else
            current_block[0] &= 0x7F;
    }

    void set_changed(bool is_changed) {
        if (is_changed)
            current_block[0] |= 0x40;
        else
            current_block[0] &= 0xBF;
    }

    bool is_changed() {
        return current_block[0] & 0x40;
    }

    static void set_block_changed(uint8_t *block, int block_size, bool is_changed) {
        if (is_changed)
            block[0] |= 0x40;
        else
            block[0] &= 0xBF;
    }

    static bool is_block_changed(uint8_t *block, int block_size) {
        return block[0] & 0x40;
    }

    inline void set_kv_last_pos(int val) {
        if (val == 0)
           val = 65535;
        util::set_int(BPT_LAST_DATA_PTR, val);
    }

    inline void ins_bit(uint32_t *ui32, int pos, int kv_pos) {
        uint32_t ryte_part = (*ui32) & RYTE_MASK32(pos);
        ryte_part >>= 1;
        if (kv_pos >= 256)
            ryte_part |= MASK32(pos);
        (*ui32) = (ryte_part | ((*ui32) & LEFT_MASK32(pos)));

    }

    inline void del_bit(uint32_t *ui32, int pos) {
        uint32_t ryte_part = (*ui32) & RYTE_MASK32(pos);
        ryte_part <<= 1;
        (*ui32) = (ryte_part | ((*ui32) & LEFT_MASK32(pos)));
    }

#if BPT_INT64MAP == 1
    inline void ins_bit(uint64_t *ui64, int pos, int kv_pos) {
        uint64_t ryte_part = (*ui64) & RYTE_MASK64(pos);
        ryte_part >>= 1;
        if (kv_pos >= 256)
            ryte_part |= MASK64(pos);
        (*ui64) = (ryte_part | ((*ui64) & LEFT_MASK64(pos)));

    }
    inline void del_bit(uint64_t *ui64, int pos) {
        uint64_t ryte_part = (*ui64) & RYTE_MASK64(pos);
        ryte_part <<= 1;
        (*ui64) = (ryte_part | ((*ui64) & LEFT_MASK64(pos)));

    }
#endif

    int get_num_levels() {
        return num_levels;
    }

    void print_stats(long num_entries) {
        std::cout << "Block Count:";
        std::cout << (long) block_count_node;
        std::cout << ", ";
        std::cout << (long) block_count_leaf;
        std::cout << std::endl;
        std::cout << "Avg Block Count:";
        std::cout << (long) (num_entries / block_count_leaf);
        std::cout << " = ";
        std::cout << (long) num_entries;
        std::cout << " / ";
        std::cout << (long) block_count_leaf;
        std::cout << std::endl;
        std::cout << "Avg Max Count:";
        std::cout << (long) (max_key_count_node / (block_count_node ? block_count_node : 1));
        std::cout << ", ";
        std::cout << (long) (max_key_count_leaf / block_count_leaf);
        std::cout << ", ";
        std::cout << (long) max_key_len;
        std::cout << std::endl;
    }
    void print_num_levels() {
        std::cout << "Level Count:";
        std::cout << (long) num_levels;
        std::cout << std::endl;
    }
    void print_counts() {
        std::cout << "Count1:";
        std::cout << count1;
        std::cout << ", Count2:";
        std::cout << count2;
        std::cout << "\n";
        std::cout << std::endl;
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
    int max_trie_len_node;
    int max_trie_len_leaf;

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
        max_trie_len_leaf = max_trie_len_node = 0;
    }

    inline void update_split_stats() {
        if (descendant->is_leaf()) {
            bplus_tree_handler<T>::max_key_count_leaf += descendant->filled_size();
            max_trie_len_leaf += bplus_tree_handler<T>::BPT_TRIE_LEN + (*(bplus_tree_handler<T>::BPT_TRIE_LEN_PTR) << 8);
            bplus_tree_handler<T>::block_count_leaf++;
        } else {
            bplus_tree_handler<T>::max_key_count_node += descendant->filled_size();
            max_trie_len_node += bplus_tree_handler<T>::BPT_TRIE_LEN + (*(bplus_tree_handler<T>::BPT_TRIE_LEN_PTR) << 8);
            bplus_tree_handler<T>::block_count_node++;
        }
    }

    inline void del_at(uint8_t *ptr) {
        bplus_tree_handler<T>::BPT_TRIE_LEN--;
        memmove(ptr, ptr + 1, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
    }

    inline void del_at(uint8_t *ptr, int count) {
        bplus_tree_handler<T>::BPT_TRIE_LEN -= count;
        memmove(ptr, ptr + count, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
    }

    inline uint8_t ins_at(uint8_t *ptr, uint8_t b) {
        memmove(ptr + 1, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        *ptr = b;
        bplus_tree_handler<T>::BPT_TRIE_LEN++;
        return 1;
    }

    inline uint8_t ins_at(uint8_t *ptr, uint8_t b1, uint8_t b2) {
        memmove(ptr + 2, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr = b2;
        bplus_tree_handler<T>::BPT_TRIE_LEN += 2;
        return 2;
    }

    inline uint8_t ins_at(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3) {
        memmove(ptr + 3, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr = b3;
        bplus_tree_handler<T>::BPT_TRIE_LEN += 3;
        return 3;
    }

    inline uint8_t ins_at(uint8_t *ptr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
        memmove(ptr + 4, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        *ptr++ = b1;
        *ptr++ = b2;
        *ptr++ = b3;
        *ptr = b4;
        bplus_tree_handler<T>::BPT_TRIE_LEN += 4;
        return 4;
    }

    inline void ins_at(uint8_t *ptr, uint8_t b, const uint8_t *s, uint8_t len) {
        memmove(ptr + 1 + len, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        *ptr++ = b;
        memcpy(ptr, s, len);
        bplus_tree_handler<T>::BPT_TRIE_LEN += len;
        bplus_tree_handler<T>::BPT_TRIE_LEN++;
    }

    inline void ins_at(uint8_t *ptr, const uint8_t *s, uint8_t len) {
        memmove(ptr + len, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        memcpy(ptr, s, len);
        bplus_tree_handler<T>::BPT_TRIE_LEN += len;
    }

    void ins_bytes(uint8_t *ptr, int len) {
        memmove(ptr + len, ptr, trie + bplus_tree_handler<T>::BPT_TRIE_LEN - ptr);
        bplus_tree_handler<T>::BPT_TRIE_LEN += len;
    }

    inline void set_at(uint8_t pos, uint8_t b) {
        trie[pos] = b;
    }

    inline void append(uint8_t b) {
        trie[bplus_tree_handler<T>::BPT_TRIE_LEN++] = b;
    }

    inline void append_ptr(int p) {
        util::set_int(trie + bplus_tree_handler<T>::BPT_TRIE_LEN, p);
        bplus_tree_handler<T>::BPT_TRIE_LEN += 2;
    }

    void append(const uint8_t *s, int need_count) {
        memcpy(trie + bplus_tree_handler<T>::BPT_TRIE_LEN, s, need_count);
        bplus_tree_handler<T>::BPT_TRIE_LEN += need_count;
    }

public:
    uint8_t *trie;
    uint8_t *trie_pos;
    uint8_t *orig_pos;
    uint8_t need_count;
    int insert_state;
    uint8_t key_pos;
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
    static const int x100 = 0x100;
    ~bpt_trie_handler() {}
    void print_stats(long num_entries) {
        bplus_tree_handler<T>::print_stats(num_entries);
        std::cout << "Avg Trie Len:";
        std::cout << (long) (max_trie_len_node
                / (bplus_tree_handler<T>::block_count_node ? bplus_tree_handler<T>::block_count_node : 1));
        std::cout << ", ";
        std::cout << (long) (max_trie_len_leaf / bplus_tree_handler<T>::block_count_leaf);
        std::cout << std::endl;
    }
    void init_current_block() {
        //cout << "Trie init block" << endl;
        bplus_tree_handler<T>::init_current_block();
        *(bplus_tree_handler<T>::BPT_TRIE_LEN_PTR) = 0;
        bplus_tree_handler<T>::BPT_TRIE_LEN = 0;
        bplus_tree_handler<T>::BPT_MAX_PFX_LEN = 1;
        key_pos = 1;
        insert_state = INSERT_EMPTY;
    }

};

#endif
