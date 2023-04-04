#include "rb_tree.h"
#include <math.h>

int rb_tree::get_header_size() {
    return RB_TREE_HDR_SIZE;
}

void rb_tree::add_first_data() {
    add_data(0);
}

void rb_tree::add_data(int search_result) {

    //idx = -1; // !!!!!

    int filled_up2 = filled_upto();
    filled_up2++;
    set_filled_upto(filled_up2);

    int inserted_node = util::get_int(current_block + DATA_END_POS);
    int n = inserted_node;
    current_block[n] = RB_RED;
    memset(current_block + n + 1, 0, 6);
    n += KEY_LEN_POS;
    current_block[n] = key_len;
    n++;
    memcpy(current_block + n, key, key_len);
    n += key_len;
    current_block[n] = value_len;
    n++;
    memcpy(current_block + n, value, value_len);
    n += value_len;
    util::set_int(current_block + DATA_END_POS, n);

    if (get_root() == 0) {
        set_root(inserted_node);
    } else {
        if (pos <= 0) {
            n = get_root();
            while (1) {
                int key_at_len;
                uint8_t *key_at = get_key(n, &key_at_len);
                int comp_result = util::compare(key, key_len, key_at,
                        key_at_len);
                if (comp_result == 0) {
                    //set_value(n, value);
                    return;
                } else if (comp_result < 0) {
                    if (get_left(n) == 0) {
                        set_left(n, inserted_node);
                        break;
                    } else
                        n = get_left(n);
                } else {
                    if (get_right(n) == 0) {
                        set_right(n, inserted_node);
                        break;
                    } else
                        n = get_right(n);
                }
            }
            set_parent(inserted_node, n);
        } else {
            if (last_direction == 'l')
                set_left(pos, inserted_node);
            else if (last_direction == 'r')
                set_right(pos, inserted_node);
            set_parent(inserted_node, pos);
        }
    }
    insert_case1(inserted_node);

}

uint8_t *rb_tree::split(uint8_t *first_key, int *first_len_ptr) {
    int filled_up2 = filled_upto();
    const int RB_TREE_NODE_SIZE = is_leaf() ? block_size : parent_block_size;
    uint8_t *b = allocate_block(RB_TREE_NODE_SIZE, is_leaf(), current_block[0] & 0x1F);
    rb_tree new_block(RB_TREE_NODE_SIZE, b, is_leaf());
    int data_end_pos = util::get_int(current_block + DATA_END_POS);
    int half_kVLen = data_end_pos;
    half_kVLen /= 2;

    int new_idx;
    int brk_idx = -1;
    int brk_kv_pos = 0;
    int tot_len = 0;
    int stack[filled_up2]; //(int) log2(filled_upto) + 1];
    int level = 0;
    int node = get_root();
    int new_block_root1 = 0;
    for (new_idx = 0; new_idx <= filled_up2;) {
        if (node == 0) {
            node = stack[--level];
            int src_idx = node;
            src_idx += KEY_LEN_POS;
            int key_len = current_block[src_idx];
            src_idx++;
            new_block.key = current_block + src_idx;
            new_block.key_len = key_len;
            src_idx += key_len;
            key_len++;
            int value_len = current_block[src_idx];
            src_idx++;
            new_block.value = current_block + src_idx;
            new_block.value_len = value_len;
            value_len++;
            int kv_len = key_len;
            kv_len += value_len;
            tot_len += kv_len;
            tot_len += KEY_LEN_POS;
            pos = -1;
            if (brk_idx != -1 && new_block.get_root() == 0) {
                memcpy(first_key, new_block.key, new_block.key_len);
                *first_len_ptr = new_block.key_len;
            }
            new_block.add_data(0);
            if (tot_len > half_kVLen && brk_idx == -1) {
                brk_idx = new_idx;
                brk_kv_pos = RB_TREE_HDR_SIZE + tot_len;
                new_block_root1 = new_block.get_root();
                new_block.set_root(0);
            }
            new_idx++;
            node = get_right(node);
        } else {
            stack[level++] = node;
            node = get_left(node);
        }
    }
    new_block.set_filled_upto(brk_idx);
    new_block.set_data_end_pos(brk_kv_pos);
    memcpy(current_block, new_block.current_block, RB_TREE_NODE_SIZE);

    int new_blk_new_len = data_end_pos - brk_kv_pos;
    memcpy(new_block.current_block + RB_TREE_HDR_SIZE, current_block + brk_kv_pos, new_blk_new_len);
    int n = filled_up2 - brk_idx - 1;
    new_block.set_filled_upto(n);
    int pos = RB_TREE_HDR_SIZE;
    brk_kv_pos -= RB_TREE_HDR_SIZE;
    for (int i = 0; i <= n; i++) {
        int ptr = new_block.get_left(pos);
        if (ptr)
            new_block.set_left(pos, ptr - brk_kv_pos);
        ptr = new_block.get_right(pos);
        if (ptr)
            new_block.set_right(pos, ptr - brk_kv_pos);
        ptr = new_block.get_parent(pos);
        if (ptr)
            new_block.set_parent(pos, ptr - brk_kv_pos);
        pos += KEY_LEN_POS;
        pos += new_block.current_block[pos];
        pos++;
        pos += new_block.current_block[pos];
        pos++;
    }
    new_block.set_root(get_root() - brk_kv_pos);
    set_root(new_block_root1);
    new_block.set_data_end_pos(pos);
    //uint8_t *nb_first_key = new_block.get_first_key(first_len_ptr);
    //memcpy(first_key, nb_first_key, *first_len_ptr);

    return new_block.current_block;
}

int rb_tree::get_data_end_pos() {
    return util::get_int(current_block + DATA_END_POS);
}

void rb_tree::set_data_end_pos(int pos) {
    util::set_int(current_block + DATA_END_POS, pos);
}

void rb_tree::set_current_block_root() {
    set_current_block(root_block);
}

void rb_tree::set_current_block(uint8_t *m) {
    current_block = m;
#if BPT_9_BIT_PTR == 1
#if BPT_INT64MAP == 1
    bitmap = (uint64_t *) (current_block + get_header_size() - 8);
#else
    bitmap1 = (uint32_t *) (current_block + get_header_size() - 8);
    bitmap2 = bitmap1 + 1;
#endif
#endif
}

int rb_tree::binary_search(const uint8_t *key, int key_len) {
    int middle;
    int new_middle = get_root();
    do {
        middle = new_middle;
        int middle_key_len;
        uint8_t *middle_key = get_key(middle, &middle_key_len);
        int cmp = util::compare(middle_key, middle_key_len, key,
                key_len);
        if (cmp > 0) {
            new_middle = get_left(middle);
            last_direction = 'l';
        } else if (cmp < 0) {
            new_middle = get_right(middle);
            last_direction = 'r';
        } else
            return middle;
    } while (new_middle > 0);
    return ~middle;
}

int rb_tree::search_current_block(bptree_iter_ctx *ctx) {
    pos = binary_search(key, key_len);
    return pos;
}

void rb_tree::init_buf() {
    //memset(buf, '\0', RB_TREE_NODE_SIZE);
    set_leaf(1);
    set_filled_upto(-1);
    set_root(0);
    set_data_end_pos(RB_TREE_HDR_SIZE);
}

void rb_tree::set_filled_upto(int filled_size) {
#if RB_TREE_NODE_SIZE == 512
    *(BPT_FILLED_SIZE) = filled_size;
#else
    util::set_int(BPT_FILLED_SIZE, filled_size);
#endif
}

int rb_tree::filled_upto() {
#if RB_TREE_NODE_SIZE == 512
    if (*(BPT_FILLED_SIZE) == 0xFF)
        return -1;
    return *(BPT_FILLED_SIZE);
#else
    return util::get_int(BPT_FILLED_SIZE);
#endif
}

bool rb_tree::is_full(int search_result) {
    int kv_len = key_len + value_len + 9; // 3 int pointer, 1 uint8_t key len, 1 uint8_t value len, 1 flag
    int RB_TREE_NODE_SIZE = is_leaf() ? block_size : parent_block_size;
    int space_left = RB_TREE_NODE_SIZE - util::get_int(current_block + DATA_END_POS);
    space_left -= RB_TREE_HDR_SIZE;
    if (space_left <= kv_len)
        return true;
    return false;
}

uint8_t *rb_tree::get_child_ptr(int pos) {
    uint8_t *kv_idx = current_block + pos + KEY_LEN_POS;
    kv_idx += kv_idx[0];
    kv_idx += 2;
    unsigned long addr_num = util::bytes_to_ptr(kv_idx);
    uint8_t *ret = (uint8_t *) addr_num;
    return ret;
}

uint8_t *rb_tree::get_key(int pos, int *plen) {
    uint8_t *kv_idx = current_block + pos + KEY_LEN_POS;
    *plen = kv_idx[0];
    kv_idx++;
    return kv_idx;
}

int rb_tree::get_first() {
    int filled_up2 = filled_upto();
    int stack[filled_up2]; //(int) log2(filled_upto) + 1];
    int level = 0;
    int node = get_root();
    while (node) {
        stack[level++] = node;
        node = get_left(node);
    }
    //assert(level > 0);
    return stack[--level];
}

uint8_t *rb_tree::get_first_key(int *plen) {
    int filled_up2 = filled_upto();
    int stack[filled_up2]; //(int) log2(filled_upto) + 1];
    int level = 0;
    int node = get_root();
    while (node) {
        stack[level++] = node;
        node = get_left(node);
    }
    //assert(level > 0);
    uint8_t *kv_idx = current_block + stack[--level] + KEY_LEN_POS;
    *plen = kv_idx[0];
    kv_idx++;
    return kv_idx;
}

int rb_tree::get_next(int n) {
    int r = get_right(n);
    if (r) {
        int l;
        do {
            l = get_left(r);
            if (l)
                r = l;
        } while (l);
        return r;
    }
    int p = get_parent(n);
    while (p && n == get_right(p)) {
        n = p;
        p = get_parent(p);
    }
    return p;
}

int rb_tree::get_previous(int n) {
    int l = get_left(n);
    if (l) {
        int r;
        do {
            r = get_right(l);
            if (r)
                l = r;
        } while (r);
        return l;
    }
    int p = get_parent(n);
    while (p && n == get_left(p)) {
        n = p;
        p = get_parent(p);
    }
    return p;
}

void rb_tree::rotate_left(int n) {
    int r = get_right(n);
    replace_node(n, r);
    set_right(n, get_left(r));
    if (get_left(r) != 0) {
        set_parent(get_left(r), n);
    }
    set_left(r, n);
    set_parent(n, r);
}

void rb_tree::rotate_right(int n) {
    int l = get_left(n);
    replace_node(n, l);
    set_left(n, get_right(l));
    if (get_right(l) != 0) {
        set_parent(get_right(l), n);
    }
    set_right(l, n);
    set_parent(n, l);
}

void rb_tree::replace_node(int oldn, int newn) {
    if (get_parent(oldn) == 0) {
        set_root(newn);
    } else {
        if (oldn == get_left(get_parent(oldn)))
            set_left(get_parent(oldn), newn);
        else
            set_right(get_parent(oldn), newn);
    }
    if (newn != 0) {
        set_parent(newn, get_parent(oldn));
    }
}

void rb_tree::insert_case1(int n) {
    if (get_parent(n) == 0)
        set_color(n, RB_BLACK);
    else
        insert_case2(n);
}

void rb_tree::insert_case2(int n) {
    if (get_color(get_parent(n)) == RB_BLACK)
        return;
    else
        insert_case3(n);
}

void rb_tree::insert_case3(int n) {
    int u = get_uncle(n);
    if (u && get_color(u) == RB_RED) {
        set_color(get_parent(n), RB_BLACK);
        set_color(u, RB_BLACK);
        set_color(get_grand_parent(n), RB_RED);
        insert_case1(get_grand_parent(n));
    } else {
        insert_case4(n);
    }
}

void rb_tree::insert_case4(int n) {
    if (n == get_right(get_parent(n))
            && get_parent(n) == get_left(get_grand_parent(n))) {
        rotate_left(get_parent(n));
        n = get_left(n);
    } else if (n == get_left(get_parent(n))
            && get_parent(n) == get_right(get_grand_parent(n))) {
        rotate_right(get_parent(n));
        n = get_right(n);
    }
    insert_case5(n);
}

void rb_tree::insert_case5(int n) {
    set_color(get_parent(n), RB_BLACK);
    set_color(get_grand_parent(n), RB_RED);
    if (n == get_left(get_parent(n))
            && get_parent(n) == get_left(get_grand_parent(n))) {
        rotate_right(get_grand_parent(n));
    } else {
//assert(
//        n == get_right(get_parent(n))
//                && get_parent(n) == get_right(get_grand_parent(n)));
        rotate_left(get_grand_parent(n));
    }
}

int rb_tree::get_left(int n) {
#if RB_TREE_NODE_SIZE == 512
    return current_block[n + LEFT_PTR_POS] + ((current_block[n + RBT_BITMAP_POS] & 0x02) << 7);
#else
    return util::get_int(current_block + n + LEFT_PTR_POS);
#endif
}

int rb_tree::get_right(int n) {
#if RB_TREE_NODE_SIZE == 512
    return current_block[n + RYTE_PTR_POS] + ((current_block[n + RBT_BITMAP_POS] & 0x04) << 6);
#else
    return util::get_int(current_block + n + RYTE_PTR_POS);
#endif
}

int rb_tree::get_parent(int n) {
#if RB_TREE_NODE_SIZE == 512
    return current_block[n + PARENT_PTR_POS] + ((current_block[n + RBT_BITMAP_POS] & 0x08) << 5);
#else
    return util::get_int(current_block + n + PARENT_PTR_POS);
#endif
}

int rb_tree::get_sibling(int n) {
    //assert(n != 0);
    //assert(get_parent(n) != 0);
    if (n == get_left(get_parent(n)))
        return get_right(get_parent(n));
    else
        return get_left(get_parent(n));
}

int rb_tree::get_uncle(int n) {
    //assert(n != NULL);
    //assert(get_parent(n) != 0);
    //assert(get_parent(get_parent(n)) != 0);
    return get_sibling(get_parent(n));
}

int rb_tree::get_grand_parent(int n) {
    //assert(n != NULL);
    //assert(get_parent(n) != 0);
    //assert(get_parent(get_parent(n)) != 0);
    return get_parent(get_parent(n));
}

int rb_tree::get_root() {
    return util::get_int(current_block + ROOT_NODE_POS);
}

int rb_tree::get_color(int n) {
#if RB_TREE_NODE_SIZE == 512
    return current_block[n + RBT_BITMAP_POS] & 0x01;
#else
    return current_block[n];
#endif
}

void rb_tree::set_left(int n, int l) {
#if RB_TREE_NODE_SIZE == 512
    current_block[n + LEFT_PTR_POS] = l;
    if (l >= 256)
        current_block[n + RBT_BITMAP_POS] |= 0x02;
    else
        current_block[n + RBT_BITMAP_POS] &= ~0x02;
#else
    util::set_int(current_block + n + LEFT_PTR_POS, l);
#endif
}

void rb_tree::set_right(int n, int r) {
#if RB_TREE_NODE_SIZE == 512
    current_block[n + RYTE_PTR_POS] = r;
    if (r >= 256)
        current_block[n + RBT_BITMAP_POS] |= 0x04;
    else
        current_block[n + RBT_BITMAP_POS] &= ~0x04;
#else
    util::set_int(current_block + n + RYTE_PTR_POS, r);
#endif
}

void rb_tree::set_parent(int n, int p) {
#if RB_TREE_NODE_SIZE == 512
    current_block[n + PARENT_PTR_POS] = p;
    if (p >= 256)
        current_block[n + RBT_BITMAP_POS] |= 0x08;
    else
        current_block[n + RBT_BITMAP_POS] &= ~0x08;
#else
    util::set_int(current_block + n + PARENT_PTR_POS, p);
#endif
}

void rb_tree::set_root(int n) {
    util::set_int(current_block + ROOT_NODE_POS, n);
}

void rb_tree::set_color(int n, uint8_t c) {
#if RB_TREE_NODE_SIZE == 512
    if (c)
        current_block[n + RBT_BITMAP_POS] |= 0x01;
    else
        current_block[n + RBT_BITMAP_POS] &= ~0x01;
#else
    current_block[n] = c;
#endif
}

uint8_t *rb_tree::get_child_ptr_pos(int search_result) {
    if (search_result < 0) {
        search_result = ~search_result;
        if (last_direction == 'l') {
            int p = get_previous(search_result);
            if (p)
                search_result = p;
        }
    }
    return current_block + pos + KEY_LEN_POS;
}

uint8_t *rb_tree::get_ptr_pos() {
    return NULL;
}
