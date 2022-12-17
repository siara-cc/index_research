#include "rb_tree.h"
#include <math.h>

int rb_tree::getHeaderSize() {
    return RB_TREE_HDR_SIZE;
}

void rb_tree::addFirstData() {
    addData(0);
}

void rb_tree::addData(int16_t search_result) {

    //idx = -1; // !!!!!

    int16_t filled_upto = filledUpto();
    filled_upto++;
    setFilledUpto(filled_upto);

    int16_t inserted_node = util::getInt(current_block + DATA_END_POS);
    int16_t n = inserted_node;
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
    util::setInt(current_block + DATA_END_POS, n);

    if (getRoot() == 0) {
        setRoot(inserted_node);
    } else {
        if (pos <= 0) {
            n = getRoot();
            while (1) {
                int16_t key_at_len;
                uint8_t *key_at = getKey(n, &key_at_len);
                int16_t comp_result = util::compare(key, key_len, key_at,
                        key_at_len);
                if (comp_result == 0) {
                    //setValue(n, value);
                    return;
                } else if (comp_result < 0) {
                    if (getLeft(n) == 0) {
                        setLeft(n, inserted_node);
                        break;
                    } else
                        n = getLeft(n);
                } else {
                    if (getRight(n) == 0) {
                        setRight(n, inserted_node);
                        break;
                    } else
                        n = getRight(n);
                }
            }
            setParent(inserted_node, n);
        } else {
            if (last_direction == 'l')
                setLeft(pos, inserted_node);
            else if (last_direction == 'r')
                setRight(pos, inserted_node);
            setParent(inserted_node, pos);
        }
    }
    insertCase1(inserted_node);

}

uint8_t *rb_tree::split(uint8_t *first_key, int16_t *first_len_ptr) {
    int16_t filled_upto = filledUpto();
    const uint16_t RB_TREE_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
    uint8_t *b = allocateBlock(RB_TREE_NODE_SIZE, isLeaf(), current_block[0] & 0x1F);
    rb_tree new_block(this->leaf_block_size, this->parent_block_size, 0, NULL, b);
    int16_t data_end_pos = util::getInt(current_block + DATA_END_POS);
    int16_t halfKVLen = data_end_pos;
    halfKVLen /= 2;

    int16_t new_idx;
    int16_t brk_idx = -1;
    int16_t brk_kv_pos = 0;
    int16_t tot_len = 0;
    int16_t stack[filled_upto]; //(int) log2(filled_upto) + 1];
    int16_t level = 0;
    int16_t node = getRoot();
    int16_t new_block_root1 = 0;
    for (new_idx = 0; new_idx <= filled_upto;) {
        if (node == 0) {
            node = stack[--level];
            int16_t src_idx = node;
            src_idx += KEY_LEN_POS;
            int16_t key_len = current_block[src_idx];
            src_idx++;
            new_block.key = current_block + src_idx;
            new_block.key_len = key_len;
            src_idx += key_len;
            key_len++;
            int16_t value_len = current_block[src_idx];
            src_idx++;
            new_block.value = current_block + src_idx;
            new_block.value_len = value_len;
            value_len++;
            int16_t kv_len = key_len;
            kv_len += value_len;
            tot_len += kv_len;
            tot_len += KEY_LEN_POS;
            pos = -1;
            if (brk_idx != -1 && new_block.getRoot() == 0) {
                memcpy(first_key, new_block.key, new_block.key_len);
                *first_len_ptr = new_block.key_len;
            }
            new_block.addData(0);
            if (tot_len > halfKVLen && brk_idx == -1) {
                brk_idx = new_idx;
                brk_kv_pos = RB_TREE_HDR_SIZE + tot_len;
                new_block_root1 = new_block.getRoot();
                new_block.setRoot(0);
            }
            new_idx++;
            node = getRight(node);
        } else {
            stack[level++] = node;
            node = getLeft(node);
        }
    }
    new_block.setFilledUpto(brk_idx);
    new_block.setDataEndPos(brk_kv_pos);
    memcpy(current_block, new_block.current_block, RB_TREE_NODE_SIZE);

    int16_t new_blk_new_len = data_end_pos - brk_kv_pos;
    memcpy(new_block.current_block + RB_TREE_HDR_SIZE, current_block + brk_kv_pos, new_blk_new_len);
    int16_t n = filled_upto - brk_idx - 1;
    new_block.setFilledUpto(n);
    int16_t pos = RB_TREE_HDR_SIZE;
    brk_kv_pos -= RB_TREE_HDR_SIZE;
    for (int i = 0; i <= n; i++) {
        int16_t ptr = new_block.getLeft(pos);
        if (ptr)
            new_block.setLeft(pos, ptr - brk_kv_pos);
        ptr = new_block.getRight(pos);
        if (ptr)
            new_block.setRight(pos, ptr - brk_kv_pos);
        ptr = new_block.getParent(pos);
        if (ptr)
            new_block.setParent(pos, ptr - brk_kv_pos);
        pos += KEY_LEN_POS;
        pos += new_block.current_block[pos];
        pos++;
        pos += new_block.current_block[pos];
        pos++;
    }
    new_block.setRoot(getRoot() - brk_kv_pos);
    setRoot(new_block_root1);
    new_block.setDataEndPos(pos);
    //uint8_t *nb_first_key = new_block.getFirstKey(first_len_ptr);
    //memcpy(first_key, nb_first_key, *first_len_ptr);

    return new_block.current_block;
}

int16_t rb_tree::getDataEndPos() {
    return util::getInt(current_block + DATA_END_POS);
}

void rb_tree::setDataEndPos(int16_t pos) {
    util::setInt(current_block + DATA_END_POS, pos);
}

void rb_tree::setCurrentBlockRoot() {
    setCurrentBlock(root_block);
}

void rb_tree::setCurrentBlock(uint8_t *m) {
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

int16_t rb_tree::binarySearch(const uint8_t *key, int16_t key_len) {
    int middle;
    int new_middle = getRoot();
    do {
        middle = new_middle;
        int16_t middle_key_len;
        uint8_t *middle_key = getKey(middle, &middle_key_len);
        int16_t cmp = util::compare(middle_key, middle_key_len, key,
                key_len);
        if (cmp > 0) {
            new_middle = getLeft(middle);
            last_direction = 'l';
        } else if (cmp < 0) {
            new_middle = getRight(middle);
            last_direction = 'r';
        } else
            return middle;
    } while (new_middle > 0);
    return ~middle;
}

int16_t rb_tree::searchCurrentBlock() {
    pos = binarySearch(key, key_len);
    return pos;
}

void rb_tree::initBuf() {
    //memset(buf, '\0', RB_TREE_NODE_SIZE);
    setLeaf(1);
    setFilledUpto(-1);
    setRoot(0);
    setDataEndPos(RB_TREE_HDR_SIZE);
}

void rb_tree::setFilledUpto(int16_t filledSize) {
#if RB_TREE_NODE_SIZE == 512
    *(BPT_FILLED_SIZE) = filledSize;
#else
    util::setInt(BPT_FILLED_SIZE, filledSize);
#endif
}

int16_t rb_tree::filledUpto() {
#if RB_TREE_NODE_SIZE == 512
    if (*(BPT_FILLED_SIZE) == 0xFF)
        return -1;
    return *(BPT_FILLED_SIZE);
#else
    return util::getInt(BPT_FILLED_SIZE);
#endif
}

bool rb_tree::isFull(int16_t search_result) {
    int16_t kv_len = key_len + value_len + 9; // 3 int16_t pointer, 1 uint8_t key len, 1 uint8_t value len, 1 flag
    uint16_t RB_TREE_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
    int16_t spaceLeft = RB_TREE_NODE_SIZE - util::getInt(current_block + DATA_END_POS);
    spaceLeft -= RB_TREE_HDR_SIZE;
    if (spaceLeft <= kv_len)
        return true;
    return false;
}

uint8_t *rb_tree::getChildPtr(int16_t pos) {
    uint8_t *kvIdx = current_block + pos + KEY_LEN_POS;
    kvIdx += kvIdx[0];
    kvIdx += 2;
    unsigned long addr_num = util::bytesToPtr(kvIdx);
    uint8_t *ret = (uint8_t *) addr_num;
    return ret;
}

uint8_t *rb_tree::getKey(int16_t pos, int16_t *plen) {
    uint8_t *kvIdx = current_block + pos + KEY_LEN_POS;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

int16_t rb_tree::getFirst() {
    int16_t filled_upto = filledUpto();
    int16_t stack[filled_upto]; //(int) log2(filled_upto) + 1];
    int16_t level = 0;
    int16_t node = getRoot();
    while (node) {
        stack[level++] = node;
        node = getLeft(node);
    }
    //assert(level > 0);
    return stack[--level];
}

uint8_t *rb_tree::getFirstKey(int16_t *plen) {
    int16_t filled_upto = filledUpto();
    int16_t stack[filled_upto]; //(int) log2(filled_upto) + 1];
    int16_t level = 0;
    int16_t node = getRoot();
    while (node) {
        stack[level++] = node;
        node = getLeft(node);
    }
    //assert(level > 0);
    uint8_t *kvIdx = current_block + stack[--level] + KEY_LEN_POS;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

int16_t rb_tree::getNext(int16_t n) {
    int16_t r = getRight(n);
    if (r) {
        int16_t l;
        do {
            l = getLeft(r);
            if (l)
                r = l;
        } while (l);
        return r;
    }
    int16_t p = getParent(n);
    while (p && n == getRight(p)) {
        n = p;
        p = getParent(p);
    }
    return p;
}

int16_t rb_tree::getPrevious(int16_t n) {
    int16_t l = getLeft(n);
    if (l) {
        int16_t r;
        do {
            r = getRight(l);
            if (r)
                l = r;
        } while (r);
        return l;
    }
    int16_t p = getParent(n);
    while (p && n == getLeft(p)) {
        n = p;
        p = getParent(p);
    }
    return p;
}

void rb_tree::rotateLeft(int16_t n) {
    int16_t r = getRight(n);
    replaceNode(n, r);
    setRight(n, getLeft(r));
    if (getLeft(r) != 0) {
        setParent(getLeft(r), n);
    }
    setLeft(r, n);
    setParent(n, r);
}

void rb_tree::rotateRight(int16_t n) {
    int16_t l = getLeft(n);
    replaceNode(n, l);
    setLeft(n, getRight(l));
    if (getRight(l) != 0) {
        setParent(getRight(l), n);
    }
    setRight(l, n);
    setParent(n, l);
}

void rb_tree::replaceNode(int16_t oldn, int16_t newn) {
    if (getParent(oldn) == 0) {
        setRoot(newn);
    } else {
        if (oldn == getLeft(getParent(oldn)))
            setLeft(getParent(oldn), newn);
        else
            setRight(getParent(oldn), newn);
    }
    if (newn != 0) {
        setParent(newn, getParent(oldn));
    }
}

void rb_tree::insertCase1(int16_t n) {
    if (getParent(n) == 0)
        setColor(n, RB_BLACK);
    else
        insertCase2(n);
}

void rb_tree::insertCase2(int16_t n) {
    if (getColor(getParent(n)) == RB_BLACK)
        return;
    else
        insertCase3(n);
}

void rb_tree::insertCase3(int16_t n) {
    int16_t u = getUncle(n);
    if (u && getColor(u) == RB_RED) {
        setColor(getParent(n), RB_BLACK);
        setColor(u, RB_BLACK);
        setColor(getGrandParent(n), RB_RED);
        insertCase1(getGrandParent(n));
    } else {
        insertCase4(n);
    }
}

void rb_tree::insertCase4(int16_t n) {
    if (n == getRight(getParent(n))
            && getParent(n) == getLeft(getGrandParent(n))) {
        rotateLeft(getParent(n));
        n = getLeft(n);
    } else if (n == getLeft(getParent(n))
            && getParent(n) == getRight(getGrandParent(n))) {
        rotateRight(getParent(n));
        n = getRight(n);
    }
    insertCase5(n);
}

void rb_tree::insertCase5(int16_t n) {
    setColor(getParent(n), RB_BLACK);
    setColor(getGrandParent(n), RB_RED);
    if (n == getLeft(getParent(n))
            && getParent(n) == getLeft(getGrandParent(n))) {
        rotateRight(getGrandParent(n));
    } else {
//assert(
//        n == getRight(getParent(n))
//                && getParent(n) == getRight(getGrandParent(n)));
        rotateLeft(getGrandParent(n));
    }
}

int16_t rb_tree::getLeft(int16_t n) {
#if RB_TREE_NODE_SIZE == 512
    return current_block[n + LEFT_PTR_POS] + ((current_block[n + RBT_BITMAP_POS] & 0x02) << 7);
#else
    return util::getInt(current_block + n + LEFT_PTR_POS);
#endif
}

int16_t rb_tree::getRight(int16_t n) {
#if RB_TREE_NODE_SIZE == 512
    return current_block[n + RYTE_PTR_POS] + ((current_block[n + RBT_BITMAP_POS] & 0x04) << 6);
#else
    return util::getInt(current_block + n + RYTE_PTR_POS);
#endif
}

int16_t rb_tree::getParent(int16_t n) {
#if RB_TREE_NODE_SIZE == 512
    return current_block[n + PARENT_PTR_POS] + ((current_block[n + RBT_BITMAP_POS] & 0x08) << 5);
#else
    return util::getInt(current_block + n + PARENT_PTR_POS);
#endif
}

int16_t rb_tree::getSibling(int16_t n) {
    //assert(n != 0);
    //assert(getParent(n) != 0);
    if (n == getLeft(getParent(n)))
        return getRight(getParent(n));
    else
        return getLeft(getParent(n));
}

int16_t rb_tree::getUncle(int16_t n) {
    //assert(n != NULL);
    //assert(getParent(n) != 0);
    //assert(getParent(getParent(n)) != 0);
    return getSibling(getParent(n));
}

int16_t rb_tree::getGrandParent(int16_t n) {
    //assert(n != NULL);
    //assert(getParent(n) != 0);
    //assert(getParent(getParent(n)) != 0);
    return getParent(getParent(n));
}

int16_t rb_tree::getRoot() {
    return util::getInt(current_block + ROOT_NODE_POS);
}

int16_t rb_tree::getColor(int16_t n) {
#if RB_TREE_NODE_SIZE == 512
    return current_block[n + RBT_BITMAP_POS] & 0x01;
#else
    return current_block[n];
#endif
}

void rb_tree::setLeft(int16_t n, int16_t l) {
#if RB_TREE_NODE_SIZE == 512
    current_block[n + LEFT_PTR_POS] = l;
    if (l >= 256)
        current_block[n + RBT_BITMAP_POS] |= 0x02;
    else
        current_block[n + RBT_BITMAP_POS] &= ~0x02;
#else
    util::setInt(current_block + n + LEFT_PTR_POS, l);
#endif
}

void rb_tree::setRight(int16_t n, int16_t r) {
#if RB_TREE_NODE_SIZE == 512
    current_block[n + RYTE_PTR_POS] = r;
    if (r >= 256)
        current_block[n + RBT_BITMAP_POS] |= 0x04;
    else
        current_block[n + RBT_BITMAP_POS] &= ~0x04;
#else
    util::setInt(current_block + n + RYTE_PTR_POS, r);
#endif
}

void rb_tree::setParent(int16_t n, int16_t p) {
#if RB_TREE_NODE_SIZE == 512
    current_block[n + PARENT_PTR_POS] = p;
    if (p >= 256)
        current_block[n + RBT_BITMAP_POS] |= 0x08;
    else
        current_block[n + RBT_BITMAP_POS] &= ~0x08;
#else
    util::setInt(current_block + n + PARENT_PTR_POS, p);
#endif
}

void rb_tree::setRoot(int16_t n) {
    util::setInt(current_block + ROOT_NODE_POS, n);
}

void rb_tree::setColor(int16_t n, uint8_t c) {
#if RB_TREE_NODE_SIZE == 512
    if (c)
        current_block[n + RBT_BITMAP_POS] |= 0x01;
    else
        current_block[n + RBT_BITMAP_POS] &= ~0x01;
#else
    current_block[n] = c;
#endif
}

uint8_t *rb_tree::getChildPtrPos(int16_t search_result) {
    if (search_result < 0) {
        search_result = ~search_result;
        if (last_direction == 'l') {
            int16_t p = getPrevious(search_result);
            if (p)
                search_result = p;
        }
    }
    return current_block + pos + KEY_LEN_POS;
}

uint8_t *rb_tree::getPtrPos() {
    return NULL;
}
