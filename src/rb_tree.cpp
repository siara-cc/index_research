#include "rb_tree.h"

char *rb_tree::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    int16_t pos = -1;
    byte *foundNode = recursiveSearchForGet(key, key_len, &pos);
    if (pos < 0)
        return null;
    linex_node node(foundNode);
    return (char *) node.getData(pos, pValueLen);
}

byte *rb_tree::recursiveSearchForGet(const char *key, int16_t key_len,
        int16_t *pIdx) {
    int16_t level = 0;
    int16_t pos = -1;
    byte *node_data = root->buf;
    linex_node node(node_data);
    while (!node.isLeaf()) {
        pos = node.locate(key, key_len, level);
        if (pos < 0) {
            pos = ~pos;
        } else {
            do {
                node_data = node.getChild(pos);
                node.setBuf(node_data);
                level++;
                pos = 0;
            } while (!node.isLeaf());
            *pIdx = pos;
            return node_data;
        }
        node_data = node.getChild(pos);
        node.setBuf(node_data);
        level++;
    }
    pos = node.locate(key, key_len, level);
    *pIdx = pos;
    return node_data;
}

byte *rb_tree::recursiveSearch(const char *key, int16_t key_len,
        byte *node_data, int16_t lastSearchPos[], byte *node_paths[],
        int16_t *pIdx) {
    int16_t level = 0;
    linex_node node(node_data);
    while (!node.isLeaf()) {
        lastSearchPos[level] = node.locate(key, key_len, level);
        node_paths[level] = node_data;
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
        } else {
            do {
                node_data = node.getChild(lastSearchPos[level]);
                node.setBuf(node_data);
                level++;
                node_paths[level] = node.buf;
                lastSearchPos[level] = 0;
            } while (!node.isLeaf());
            *pIdx = lastSearchPos[level];
            return node_data;
        }
        node_data = node.getChild(lastSearchPos[level]);
        node.setBuf(node_data);
        level++;
    }
    node_paths[level] = node_data;
    lastSearchPos[level] = node.locate(key, key_len, level);
    *pIdx = lastSearchPos[level];
    return node_data;
}

void rb_tree::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    int16_t lastSearchPos[numLevels];
    byte *block_paths[numLevels];
    if (root->filledUpto() == -1) {
        root->addData(0, key, key_len, value, value_len);
        total_size++;
    } else {
        int16_t pos = -1;
        byte *foundNode = recursiveSearch(key, key_len, root->buf,
                lastSearchPos, block_paths, &pos);
        recursiveUpdate(key, key_len, foundNode, pos, value, value_len,
                lastSearchPos, block_paths, numLevels - 1);
    }
}

void rb_tree_node::addData(int16_t idx, const char *key, int16_t key_len,
        const char *value, int16_t value_len) {

    int16_t filled_upto = filledUpto();
    filled_upto++;
    util::setInt(buf + FILLED_UPTO_POS, filled_upto);

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

    if (getRoot == 0) {
        setRoot(inserted_node);
    } else {
        int16_t n = getRoot();
        while (1) {
            int16_t key_at_len;
            char *key_at = getKey(n, &key_at_len);
            int16_t comp_result = util::compare(key, key_len, key_at,
                    key_at_len, 0);
            if (comp_result == 0) {
                //setValue(n, value);
                return;
            } else if (comp_result < 0) {
                if (getLeft(n) == NULL) {
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
    }
    insertCase1(inserted_node);

}

byte *rb_tree_node::split(int16_t *pbrk_idx) {
    int16_t filled_upto = filledUpto();
    rb_tree_node *new_block = new rb_tree_node(
            util::alignedAlloc(RB_TREE_NODE_SIZE));
    if (!isLeaf())
        new_block->setLeaf(false);
    int16_t data_end_pos = util::getInt(buf + DATA_END_POS);
    int16_t halfKVLen = data_end_pos;
    halfKVLen /= 2;
    byte *new_kv_idx = new_block->buf + RB_TREE_HDR_SIZE;
    int16_t new_idx;
    int16_t brk_idx = -1;
    int16_t brk_kv_pos = 0;
    int16_t tot_len = 0;
    int16_t stack[log2(filled_upto) + 1];
    int16_t level = 0;
    int16_t node = getRoot();
    for (new_idx = 0; new_idx < filled_upto; new_idx++) {
        if (node == 0) {
            node = stack[--level];
            int16_t src_idx = node;
            int16_t key_len = buf[src_idx + KEY_LEN_POS];
            key_len++;
            int16_t value_len = buf[src_idx + KEY_LEN_POS + key_len];
            value_len++;
            int16_t kv_len = key_len;
            kv_len += value_len;
            tot_len += kv_len;
            tot_len += KEY_LEN_POS;
            memcpy(new_kv_idx, buf + src_idx, kv_len + KEY_LEN_POS);
            if (tot_len > halfKVLen && brk_idx == -1) {
                brk_idx = new_idx;
                brk_kv_pos = RB_TREE_HDR_SIZE + tot_len;
            }
            node = getRight(node);
        } else {
            stack[level++] = node;
            node = getLeft(node);
        }
    }
    new_block->setFilledUpto(brk_idx);
    new_block->setDataEndPos(brk_kv_pos);

    int16_t old_blk_new_len = data_end_pos - brk_kv_pos;
    memcpy(buf + RB_TREE_HDR_SIZE, new_block->buf + brk_kv_pos,
            old_blk_new_len);
    setFilledUpto(filled_upto - brk_idx);

    // from new block, update pointers for remaining items
    *pbrk_idx = brk_idx;
    return new_block;
}

int16_t rb_tree_node::getDataEndPos() {
    return util::getInt(buf + DATA_END_POS);
}

void rb_tree_node::setDataEndPos(int16_t pos) {
    util::setInt(buf + DATA_END_POS, pos);
}

int16_t rb_tree_node::binarySearchLeaf(const char *key, int16_t key_len) {
    int16_t middle = getRoot();
    do {
        int16_t middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        int16_t cmp = util::compare(middle_key, middle_key_len, key, key_len);
        if (cmp < 0)
            middle = getLeft(middle);
        else if (cmp > 0)
            middle = getRight(middle);
        else
            return middle;
    } while (middle > 0);
    return middle;
}

int16_t rb_tree_node::binarySearchNode(const char *key, int16_t key_len) {
    int16_t middle = getRoot();
    do {
        int16_t middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        int16_t cmp = util::compare(middle_key, middle_key_len, key, key_len);
        if (cmp > 0)
            middle = getLeft(middle);
        else if (cmp < 0) {
            int16_t right = getRight(middle);
            if (right != 0) {
                int16_t plus1_key_len;
                char *plus1_key = (char *) getKey(right, &plus1_key_len);
                cmp = util::compare(plus1_key, plus1_key_len, key, key_len);
                if (cmp > 0)
                    return middle;
                else if (cmp < 0)
                    middle = right;
                else
                    return ~(middle + filledUpto + 2);
            } else
                return middle;
        } else
            return ~(middle + filledUpto + 1);
    } while (middle > 0);
    return middle;
}

int16_t rb_tree_node::locate(const char *key, int16_t key_len, int16_t level) {
    int16_t ret = -1;
    if (isLeaf()) {
        ret = binarySearchLeaf(key, key_len);
    } else {
        ret = binarySearchNode(key, key_len);
    }
    return ret;
}

rb_tree::~rb_tree() {
    delete root;
}

rb_tree::rb_tree() {
    GenTree::generateLists();
    root = new rb_tree_node(util::alignedAlloc(RB_TREE_NODE_SIZE));
    root->init();
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 0;
    blockCount = 1;
}

rb_tree_node::rb_tree_node(byte *b) {
    setBuf(b);
}

void rb_tree_node::setBuf(byte *b) {
    buf = b;
}

void rb_tree_node::init() {
    memset(buf, '\0', RB_TREE_NODE_SIZE);
    setLeaf(1);
    setFilledUpto(-1);
    setDataEndPos(RB_TREE_NODE_SIZE);
}

bool rb_tree_node::isLeaf() {
    return (buf[IS_LEAF_POS] == 1);
}

void rb_tree_node::setLeaf(char isLeaf) {
    buf[IS_LEAF_POS] = isLeaf;
}

void rb_tree_node::setFilledUpto(int16_t filledSize) {
    util::setInt(buf + FILLED_UPTO_POS, filledSize);
}

int16_t rb_tree_node::filledUpto() {
    return util::getInt(buf + FILLED_UPTO_POS);
}

bool rb_tree_node::isFull(int16_t kv_len) {
    kv_len += 9; // 3 int16_t pointer, 1 byte key len, 1 byte value len, 1 flag
    int16_t spaceLeft = RB_TREE_NODE_SIZE - util::getInt(buf + DATA_END_POS);
    spaceLeft -= RB_TREE_HDR_SIZE;
    if (spaceLeft < kv_len)
        return true;
    return false;
}

byte *rb_tree_node::getChild(int16_t pos) {
    byte *kvIdx = buf + pos;
    kvIdx += kvIdx[KEY_LEN_POS];
    kvIdx += 2;
    unsigned long addr_num = util::fourBytesToPtr(kvIdx);
    byte *ret = (byte *) addr_num;
    return ret;
}

byte *rb_tree_node::getKey(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + pos;
    *plen = kvIdx[KEY_LEN_POS];
    kvIdx += *plen;
    kvIdx++;
    return kvIdx;
}

byte *rb_tree_node::getData(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + pos;
    *plen = kvIdx[KEY_LEN_POS];
    kvIdx += *plen;
    kvIdx++;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

void rb_tree_node::rotateLeft(int16_t n) {
    int16_t r = getRight(n);
    replaceNode(n, r);
    setRight(n, getLeft(r));
    if (getLeft(r) != 0) {
        setParent(getLeft(r), n);
    }
    setLeft(r, n);
    setParent(n, r);
}

void rb_tree_node::rotateRight(int16_t n) {
    int16_t l = getLeft(n);
    replaceNode(n, l);
    setLeft(n, getRight(l));
    if (getRight(l) != 0) {
        setParent(getRight(l), n);
    }
    setRight(l, n);
    setParent(n, l);
}

void rb_tree_node::replaceNode(int16_t oldn, int16_t newn) {
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

void rb_tree_node::insertCase1(int16_t n) {
    if (getParent(n) == NULL)
        setColor(n, RB_BLACK);
    else
        insertCase2(n);
}

void rb_tree_node::insertCase2(int16_t n) {
    if (getColor(getParent(n)) == RB_BLACK)
        return;
    else
        insertCase3(n);
}

void rb_tree_node::insertCase3(int16_t n) {
    if (getColor(getUncle(n)) == RB_RED) {
        setColor(getParent(n), RB_BLACK);
        setColor(getUncle(n), RB_BLACK);
        setColor(getGrandParent(n), RB_RED);
        insertCase1(getGrandParent(n));
    } else {
        insertCase4(n);
    }
}

void rb_tree_node::insertCase4(int16_t n) {
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

void rb_tree_node::insertCase5(int16_t n) {
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

int16_t rb_tree_node::getLeft(int16_t n) {
    return util::getInt(buf + n + 1);
}

int16_t rb_tree_node::getRight(int16_t n) {
    return util::getInt(buf + n + 3);
}

int16_t rb_tree_node::getParent(int16_t n) {
    return util::getInt(buf + n + 5);
}

int16_t rb_tree_node::getSibling(int16_t n) {
    //assert(n != 0);
    //assert(getParent(n) != 0);
    if (n == getLeft(getParent(n)))
        return getRight(getParent(n));
    else
        return getLeft(getParent(n));
}

int16_t rb_tree_node::getUncle(int16_t n) {
    //assert(n != NULL);
    //assert(getParent(n) != 0);
    //assert(getParent(getParent(n)) != 0);
    return getSibling(getParent(n));
}

int16_t rb_tree_node::getGrandParent(int16_t n) {
    //assert(n != NULL);
    //assert(getParent(n) != 0);
    //assert(getParent(getParent(n)) != 0);
    return getParent(getParent(n));
}

int16_t rb_tree_node::getRoot() {
    return util::getInt(buf + ROOT_NODE_POS);
}

int16_t rb_tree_node::getColor(int16_t n) {
    return buf[n];
}

void rb_tree_node::setLeft(int16_t n, int16_t l) {
    util::setInt(buf + n + 1, l);
}

void rb_tree_node::setRight(int16_t n, int16_t r) {
    util::setInt(buf + n + 3, r);
}

void rb_tree_node::setParent(int16_t n, int16_t p) {
    util::setInt(buf + n + 5, p);
}

void rb_tree_node::setRoot(int16_t n) {
    util::setInt(buf + ROOT_NODE_POS, n);
}

void rb_tree_node::setColor(int16_t n, byte c) {
    buf[n] = c;
}
