#ifndef RB_TREE_H
#define RB_TREE_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "gen_tree.h"
#include "bplus_tree_handler.h"

#if RB_TREE_NODE_SIZE == 512
#define RB_TREE_HDR_SIZE 7
#define DATA_END_POS 2
#define ROOT_NODE_POS 4
#else
#define RB_TREE_HDR_SIZE 8
#define DATA_END_POS 3
#define ROOT_NODE_POS 5
#endif

#define RB_RED 0
#define RB_BLACK 1

#if RB_TREE_NODE_SIZE == 512
#define RBT_BITMAP_POS 0
#define LEFT_PTR_POS 1
#define RYTE_PTR_POS 2
#define PARENT_PTR_POS 3
#define KEY_LEN_POS 4
#else
#define COLOR_POS 0
#define LEFT_PTR_POS 1
#define RYTE_PTR_POS 3
#define PARENT_PTR_POS 5
#define KEY_LEN_POS 7
#endif

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class rb_tree : public bplus_tree_handler<rb_tree> {
private:
    int binary_search(const uint8_t *key, int key_len);
    inline int get_left(int n);
    inline int get_right(int n);
    inline int get_parent(int n);
    int get_sibling(int n);
    inline int get_uncle(int n);
    inline int get_grand_parent(int n);
    inline int get_root();
    inline int get_color(int n);
    inline void set_left(int n, int l);
    inline void set_right(int n, int r);
    inline void set_parent(int n, int p);
    inline void set_root(int n);
    inline void set_color(int n, uint8_t c);
    void rotate_left(int n);
    void rotate_right(int n);
    void replace_node(int oldn, int newn);
    void insert_case1(int n);
    void insert_case2(int n);
    void insert_case3(int n);
    void insert_case4(int n);
    void insert_case5(int n);
public:
    int pos;
    uint8_t last_direction;

    rb_tree(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
       bplus_tree_handler<rb_tree>(leaf_block_sz, parent_block_sz, cache_sz, fname) {
        gen_tree::generate_lists();
        init_buf();
    }

    rb_tree(uint32_t block_sz, uint8_t *block, bool is_leaf) :
      bplus_tree_handler<rb_tree>(block_sz, block, is_leaf) {
        init_stats();
    }

    void init_derived() {
    }
    void cleanup() {
    }
    uint8_t *find_split_source(int search_result) {
        return NULL;
    }

    int get_data_end_pos();
    void set_data_end_pos(int pos);
    int filled_upto();
    void set_filled_upto(int filled_upto);
    int get_ptr(int pos);
    void set_ptr(int pos, int ptr);
    bool is_full(int search_result);
    void set_current_block_root();
    void set_current_block(uint8_t *m);
    int search_current_block(bptree_iter_ctx *ctx = NULL);
    void add_data(int search_result);
    void add_first_data();
    uint8_t *get_key(int pos, int *plen);
    uint8_t *get_first_key(int *plen);
    int get_first();
    int get_next(int n);
    int get_previous(int n);
    uint8_t *split(uint8_t *first_key, int *first_len_ptr);
    void init_vars();
    uint8_t *get_child_ptr_pos(int search_result);
    uint8_t *get_value_at(int *vlen);
    using bplus_tree_handler::get_child_ptr;
    using bplus_tree_handler::get;
    using bplus_tree_handler::put;
    uint8_t *get_child_ptr(int pos);
    uint8_t *get_ptr_pos();
    int get_header_size();
    void init_buf();
};

#endif
