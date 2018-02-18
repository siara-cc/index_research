#include "rb_tree.h"
#include <math.h>

char *rb_tree::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    byte *node_data = root_data;
    rb_tree_node_handler node(node_data);
    node.key = key;
    node.key_len = key_len;
    if (!node.isLeaf())
        node.traverseToLeaf();
    if (node.locate() < 0)
        return null;
    return node.getValueAt(pValueLen);
}

void rb_tree_node_handler::traverseToLeaf(byte *node_paths[]) {
    byte level;
    level = 1;
    if (isPut)
        *node_paths = buf;
    while (!isLeaf()) {
        int16_t idx = locate();
        if (idx < 0) {
            idx = ~idx;
            if (last_direction == 'l') {
                int16_t p = getPrevious(idx);
                if (p)
                    idx = p;
            }
        }
        setBuf(getChildPtr(idx));
        if (isPut)
            node_paths[level++] = buf;
    }
}

void rb_tree::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    byte *node_paths[7];
    rb_tree_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    node.value = value;
    node.value_len = value_len;
    node.isPut = true;
    if (node.filledUpto() == -1) {
        node.pos = 0;
        node.addData();
        total_size++;
    } else {
        if (!node.isLeaf())
            node.traverseToLeaf(node_paths);
        node.locate();
        recursiveUpdate(&node, node.pos, node_paths, numLevels - 1);
    }
}

void rb_tree::recursiveUpdate(rb_tree_node_handler *node, int16_t pos,
        byte *node_paths[], int16_t level) {
    int16_t idx = pos;
    if (idx < 0) {
        idx = ~idx;
        if (node->isFull(node->key_len + node->value_len)) {
            //std::cout << "Full\n" << std::endl;
            //if (maxKeyCount < block->filledSize())
            //    maxKeyCount = block->filledSize();
            //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
            //cout << (int) node->TRIE_LEN << endl;
            if (node->isLeaf()) {
                maxKeyCountLeaf += node->filledSize();
                blockCountLeaf++;
            } else {
                maxKeyCountNode += node->filledSize();
                blockCountNode++;
            }
            //    maxKeyCount += node->TRIE_LEN;
            //maxKeyCount += node->PREFIX_LEN;
            byte first_key[64];
            int16_t first_len;
            byte *b = node->split(first_key, &first_len);
            rb_tree_node_handler new_block(b);
            new_block.isPut = true;
            int16_t cmp = util::compare((char *) first_key, first_len,
                    node->key, node->key_len);
            if (cmp <= 0) {
                new_block.initVars();
                new_block.key = node->key;
                new_block.key_len = node->key_len;
                new_block.value = node->value;
                new_block.value_len = node->value_len;
                new_block.pos = ~new_block.locate();
                new_block.addData();
            } else {
                node->initVars();
                node->pos = ~node->locate();
                node->addData();
            }
            if (root_data == node->buf) {
                blockCountNode++;
                root_data = (byte *) util::alignedAlloc(RB_TREE_NODE_SIZE);
                rb_tree_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                root.setLeaf(0);
                byte addr[9];
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = (char *) addr;
                root.value_len = util::ptrToBytes((unsigned long) node->buf, addr);
                root.pos = -1;
                root.addData();
                root.initVars();
                root.key = (char *) first_key;
                root.key_len = first_len;
                root.value = (char *) addr;
                root.value_len = util::ptrToBytes((unsigned long) new_block.buf, addr);
                root.pos = -1; //~root.locate();
                root.addData();
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                rb_tree_node_handler parent(parent_data);
                byte addr[9];
                parent.initVars();
                parent.isPut = true;
                parent.key = (char *) first_key;
                parent.key_len = first_len;
                parent.value = (char *) addr;
                parent.value_len = util::ptrToBytes((unsigned long) new_block.buf, addr);
                parent.pos = parent.locate();
                recursiveUpdate(&parent, parent.pos, node_paths, prev_level);
            }
        } else {
            node->pos = idx;
            node->addData();
        }
    } else {
        //if (node->isLeaf) {
        //    int16_t vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

void rb_tree_node_handler::addData() {

    //idx = -1; // !!!!!

    int16_t filled_upto = filledUpto();
    filled_upto++;
    setFilledUpto(filled_upto);

    int16_t inserted_node = util::getInt(buf + DATA_END_POS);
    int16_t n = inserted_node;
    buf[n] = RB_RED;
    memset(buf + n + 1, 0, 6);
    n += KEY_LEN_POS;
    buf[n] = key_len;
    n++;
    memcpy(buf + n, key, key_len);
    n += key_len;
    buf[n] = value_len;
    n++;
    memcpy(buf + n, value, value_len);
    n += value_len;
    util::setInt(buf + DATA_END_POS, n);

    if (getRoot() == 0) {
        setRoot(inserted_node);
    } else {
        if (pos <= 0) {
            n = getRoot();
            while (1) {
                int16_t key_at_len;
                char *key_at = (char *) getKey(n, &key_at_len);
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

byte *rb_tree_node_handler::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t filled_upto = filledUpto();
    rb_tree_node_handler new_block(
            (byte *) util::alignedAlloc(RB_TREE_NODE_SIZE));
    new_block.initBuf();
    if (!isLeaf())
        new_block.setLeaf(false);
    int16_t data_end_pos = util::getInt(buf + DATA_END_POS);
    int16_t halfKVLen = data_end_pos;
    halfKVLen /= 2;

    int16_t new_idx;
    int16_t brk_idx = -1;
    int16_t brk_kv_pos = 0;
    int16_t tot_len = 0;
    int16_t stack[(int) log2(filled_upto) + 1];
    int16_t level = 0;
    int16_t node = getRoot();
    int16_t new_block_root1 = 0;
    for (new_idx = 0; new_idx <= filled_upto;) {
        if (node == 0) {
            node = stack[--level];
            int16_t src_idx = node;
            src_idx += KEY_LEN_POS;
            int16_t key_len = buf[src_idx];
            src_idx++;
            new_block.key = (char *) buf + src_idx;
            new_block.key_len = key_len;
            src_idx += key_len;
            key_len++;
            int16_t value_len = buf[src_idx];
            src_idx++;
            new_block.value = (char *) buf + src_idx;
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
            new_block.addData();
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
    memcpy(buf, new_block.buf, RB_TREE_NODE_SIZE);

    int16_t new_blk_new_len = data_end_pos - brk_kv_pos;
    memcpy(new_block.buf + RB_TREE_HDR_SIZE, buf + brk_kv_pos, new_blk_new_len);
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
        pos += new_block.buf[pos];
        pos++;
        pos += new_block.buf[pos];
        pos++;
    }
    new_block.setRoot(getRoot() - brk_kv_pos);
    setRoot(new_block_root1);
    new_block.setDataEndPos(pos);
    //byte *nb_first_key = new_block.getFirstKey(first_len_ptr);
    //memcpy(first_key, nb_first_key, *first_len_ptr);

    return new_block.buf;
}

int16_t rb_tree_node_handler::getDataEndPos() {
    return util::getInt(buf + DATA_END_POS);
}

void rb_tree_node_handler::setDataEndPos(int16_t pos) {
    util::setInt(buf + DATA_END_POS, pos);
}

int16_t rb_tree_node_handler::binarySearch(const char *key, int16_t key_len) {
    register int middle;
    register int new_middle = getRoot();
    do {
        middle = new_middle;
        register int16_t middle_key_len;
        register char *middle_key = (char *) getKey(middle, &middle_key_len);
        register int16_t cmp = util::compare(middle_key, middle_key_len, key,
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

int16_t rb_tree_node_handler::locate() {
    pos = binarySearch(key, key_len);
    return pos;
}

rb_tree::~rb_tree() {
    delete root_data;
}

rb_tree::rb_tree() {
    GenTree::generateLists();
    root_data = (byte *) util::alignedAlloc(RB_TREE_NODE_SIZE);
    root = new rb_tree_node_handler(root_data);
    root->initBuf();
    total_size = maxKeyCountLeaf = maxKeyCountNode = 0;
    numLevels = blockCountLeaf = blockCountNode = 1;
}

rb_tree_node_handler::rb_tree_node_handler(byte *b) {
    setBuf(b);
    isPut = false;
}

void rb_tree_node_handler::setBuf(byte *b) {
    buf = b;
}

void rb_tree_node_handler::initBuf() {
    //memset(buf, '\0', RB_TREE_NODE_SIZE);
    setLeaf(1);
    setFilledUpto(-1);
    setRoot(0);
    setDataEndPos(RB_TREE_HDR_SIZE);
}

void rb_tree_node_handler::setFilledUpto(int16_t filledSize) {
#if RB_TREE_NODE_SIZE == 512
    *(BPT_FILLED_SIZE) = filledSize;
#else
    util::setInt(BPT_FILLED_SIZE, filledSize);
#endif
}

int16_t rb_tree_node_handler::filledUpto() {
#if RB_TREE_NODE_SIZE == 512
    if (*(BPT_FILLED_SIZE) == 0xFF)
        return -1;
    return *(BPT_FILLED_SIZE);
#else
    return util::getInt(BPT_FILLED_SIZE);
#endif
}

bool rb_tree_node_handler::isFull(int16_t kv_len) {
    kv_len += 9; // 3 int16_t pointer, 1 byte key len, 1 byte value len, 1 flag
    int16_t spaceLeft = RB_TREE_NODE_SIZE - util::getInt(buf + DATA_END_POS);
    spaceLeft -= RB_TREE_HDR_SIZE;
    if (spaceLeft <= kv_len)
        return true;
    return false;
}

byte *rb_tree_node_handler::getChildPtr(int16_t pos) {
    byte *kvIdx = buf + pos + KEY_LEN_POS;
    kvIdx += kvIdx[0];
    kvIdx += 2;
    unsigned long addr_num = util::bytesToPtr(kvIdx);
    byte *ret = (byte *) addr_num;
    return ret;
}

byte *rb_tree_node_handler::getKey(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + pos + KEY_LEN_POS;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

char *rb_tree_node_handler::getValueAt(int16_t *vlen) {
    key_at = buf + pos + KEY_LEN_POS;
    key_at += *key_at;
    key_at++;
    *vlen = *key_at;
    key_at++;
    return (char *) key_at;
}

int16_t rb_tree_node_handler::getFirst() {
    int16_t filled_upto = filledUpto();
    int16_t stack[(int) log2(filled_upto) + 1];
    int16_t level = 0;
    int16_t node = getRoot();
    while (node) {
        stack[level++] = node;
        node = getLeft(node);
    }
    //assert(level > 0);
    return stack[--level];
}

byte *rb_tree_node_handler::getFirstKey(int16_t *plen) {
    int16_t filled_upto = filledUpto();
    int16_t stack[(int) log2(filled_upto) + 1];
    int16_t level = 0;
    int16_t node = getRoot();
    while (node) {
        stack[level++] = node;
        node = getLeft(node);
    }
    //assert(level > 0);
    byte *kvIdx = buf + stack[--level] + KEY_LEN_POS;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

int16_t rb_tree_node_handler::getNext(int16_t n) {
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

int16_t rb_tree_node_handler::getPrevious(int16_t n) {
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

void rb_tree_node_handler::rotateLeft(int16_t n) {
    int16_t r = getRight(n);
    replaceNode(n, r);
    setRight(n, getLeft(r));
    if (getLeft(r) != 0) {
        setParent(getLeft(r), n);
    }
    setLeft(r, n);
    setParent(n, r);
}

void rb_tree_node_handler::rotateRight(int16_t n) {
    int16_t l = getLeft(n);
    replaceNode(n, l);
    setLeft(n, getRight(l));
    if (getRight(l) != 0) {
        setParent(getRight(l), n);
    }
    setRight(l, n);
    setParent(n, l);
}

void rb_tree_node_handler::replaceNode(int16_t oldn, int16_t newn) {
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

void rb_tree_node_handler::insertCase1(int16_t n) {
    if (getParent(n) == 0)
        setColor(n, RB_BLACK);
    else
        insertCase2(n);
}

void rb_tree_node_handler::insertCase2(int16_t n) {
    if (getColor(getParent(n)) == RB_BLACK)
        return;
    else
        insertCase3(n);
}

void rb_tree_node_handler::insertCase3(int16_t n) {
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

void rb_tree_node_handler::insertCase4(int16_t n) {
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

void rb_tree_node_handler::insertCase5(int16_t n) {
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

int16_t rb_tree_node_handler::getLeft(int16_t n) {
#if RB_TREE_NODE_SIZE == 512
    return buf[n + LEFT_PTR_POS] + ((buf[n + RBT_BITMAP_POS] & 0x02) << 7);
#else
    return util::getInt(buf + n + LEFT_PTR_POS);
#endif
}

int16_t rb_tree_node_handler::getRight(int16_t n) {
#if RB_TREE_NODE_SIZE == 512
    return buf[n + RYTE_PTR_POS] + ((buf[n + RBT_BITMAP_POS] & 0x04) << 6);
#else
    return util::getInt(buf + n + RYTE_PTR_POS);
#endif
}

int16_t rb_tree_node_handler::getParent(int16_t n) {
#if RB_TREE_NODE_SIZE == 512
    return buf[n + PARENT_PTR_POS] + ((buf[n + RBT_BITMAP_POS] & 0x08) << 5);
#else
    return util::getInt(buf + n + PARENT_PTR_POS);
#endif
}

int16_t rb_tree_node_handler::getSibling(int16_t n) {
    //assert(n != 0);
    //assert(getParent(n) != 0);
    if (n == getLeft(getParent(n)))
        return getRight(getParent(n));
    else
        return getLeft(getParent(n));
}

int16_t rb_tree_node_handler::getUncle(int16_t n) {
    //assert(n != NULL);
    //assert(getParent(n) != 0);
    //assert(getParent(getParent(n)) != 0);
    return getSibling(getParent(n));
}

int16_t rb_tree_node_handler::getGrandParent(int16_t n) {
    //assert(n != NULL);
    //assert(getParent(n) != 0);
    //assert(getParent(getParent(n)) != 0);
    return getParent(getParent(n));
}

int16_t rb_tree_node_handler::getRoot() {
    return util::getInt(buf + ROOT_NODE_POS);
}

int16_t rb_tree_node_handler::getColor(int16_t n) {
#if RB_TREE_NODE_SIZE == 512
    return buf[n + RBT_BITMAP_POS] & 0x01;
#else
    return buf[n];
#endif
}

void rb_tree_node_handler::setLeft(int16_t n, int16_t l) {
#if RB_TREE_NODE_SIZE == 512
    buf[n + LEFT_PTR_POS] = l;
    if (l >= 256)
        buf[n + RBT_BITMAP_POS] |= 0x02;
    else
        buf[n + RBT_BITMAP_POS] &= ~0x02;
#else
    util::setInt(buf + n + LEFT_PTR_POS, l);
#endif
}

void rb_tree_node_handler::setRight(int16_t n, int16_t r) {
#if RB_TREE_NODE_SIZE == 512
    buf[n + RYTE_PTR_POS] = r;
    if (r >= 256)
        buf[n + RBT_BITMAP_POS] |= 0x04;
    else
        buf[n + RBT_BITMAP_POS] &= ~0x04;
#else
    util::setInt(buf + n + RYTE_PTR_POS, r);
#endif
}

void rb_tree_node_handler::setParent(int16_t n, int16_t p) {
#if RB_TREE_NODE_SIZE == 512
    buf[n + PARENT_PTR_POS] = p;
    if (p >= 256)
        buf[n + RBT_BITMAP_POS] |= 0x08;
    else
        buf[n + RBT_BITMAP_POS] &= ~0x08;
#else
    util::setInt(buf + n + PARENT_PTR_POS, p);
#endif
}

void rb_tree_node_handler::setRoot(int16_t n) {
    util::setInt(buf + ROOT_NODE_POS, n);
}

void rb_tree_node_handler::setColor(int16_t n, byte c) {
#if RB_TREE_NODE_SIZE == 512
    if (c)
        buf[n + RBT_BITMAP_POS] |= 0x01;
    else
        buf[n + RBT_BITMAP_POS] &= ~0x01;
#else
    buf[n] = c;
#endif
}

void rb_tree_node_handler::initVars() {
}
