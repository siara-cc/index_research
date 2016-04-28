#include "rb_tree.h"
#include <assert.h>

rb_tree_var *rb_tree::newVar() {
    return new rb_tree_var();
}

rb_tree_node *rb_tree::newNode() {
    return new rb_tree_node();
}

void rb_tree_node::addData(int idx, const char *value, int value_len,
        bplus_tree_var *v) {
    addData(idx, value, value_len, (rb_tree_var *) v);
}

void rb_tree_node::addData(int idx, const char *value, int value_len,
        rb_tree_var *v) {

    byte *kvIdx = buf + idx;
    int filledSz = filledSize();
    filledSz++;
    util::setInt(buf + FILLED_SIZE_POS, filledSz);
    kvIdx[0] = v->key_len;
    memcpy(kvIdx + 1, v->key, v->key_len);
    kvIdx[v->key_len + 1] = value_len;
    memcpy(kvIdx + v->key_len + 2, value, value_len);

    int inserted_node = newNode(RB_RED, 0, 0, 0);
    if (getRoot == 0) {
        setRoot(inserted_node);
    } else {
        int n = getRoot();
        while (1) {
            int comp_result = compare(key, n->key);
            if (comp_result == 0) {
                setValue(n, value);
                return;
            } else if (comp_result < 0) {
                if (getLeft(n) == NULL) {
                    setLeft(n, inserted_node);
                    break;
                } else {
                    n = getLeft(n);
                }
            } else {
                assert(comp_result > 0);
                if (getRight(n) == 0) {
                    setRight(n, inserted_node);
                    break;
                } else {
                    n = getRight(n);
                }
            }
        }
        setParent(inserted_node, n);
    }
    insertCase1(inserted_node);

}

rb_tree_node *rb_tree_node::split(int *pbrk_idx) {
    int filled_size = filledSize();
    rb_tree_node *new_block = new rb_tree_node();
    if (!isLeaf())
        new_block->setLeaf(false);

    int data_end_pos = util::getInt(buf + DATA_END_POS);
    int halfKVLen = data_end_pos;
    halfKVLen /= 2;
    byte *new_kv_idx = new_block->buf + RB_TREE_HDR_SIZE;
    int new_idx;
    int brk_idx = -1;
    int brk_kv_pos = 0;
    int tot_len = 0;
    int stack[log2(filled_size) + 1];
    int level = 0;
    int node = getRoot();
    for (new_idx = 0; new_idx < filled_size; new_idx++) {
        if (node == 0) {
            node = stack[--level];
            int src_idx = node;
            int key_len = buf[src_idx + KEY_LEN_POS];
            key_len++;
            int value_len = buf[src_idx + KEY_LEN_POS + key_len];
            value_len++;
            int kv_len = key_len;
            kv_len += value_len;
            tot_len += kv_len;
            tot_len += KEY_LEN_POS;
            memcpy(new_kv_idx, buf + src_idx, kv_len);
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
    new_block->setFilledSize(brk_idx);
    new_block->setDataEndPos(brk_kv_pos);

    int old_blk_new_len = data_end_pos - brk_kv_pos;
    memcpy(buf + RB_TREE_HDR_SIZE, new_block->buf + brk_kv_pos,
            old_blk_new_len);
    setFilledSize(filled_size - brk_idx);

    // from new block, update pointers for remaining items
    *pbrk_idx = brk_idx;
    return new_block;
}

int rb_tree_node::getDataEndPos() {
    return util::getInt(buf + DATA_END_POS);
}

void rb_tree_node::setDataEndPos(int pos) {
    util::setInt(buf + DATA_END_POS, pos);
}

int rb_tree_node::binarySearchLeaf(const char *key, int key_len) {
    int middle = getRoot();
    do {
        int middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        int cmp = util::compare(middle_key, middle_key_len, key, key_len);
        if (cmp < 0)
            middle = getLeft(middle);
        else if (cmp > 0)
            middle = getRight(middle);
        else
            return middle;
    } while (middle > 0);
    return middle;
}

int rb_tree_node::binarySearchNode(const char *key, int key_len) {
    int middle = getRoot();
    do {
        int middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        int cmp = util::compare(middle_key, middle_key_len, key, key_len);
        if (cmp > 0)
            middle = getLeft(middle);
        else if (cmp < 0) {
            int right = getRight(middle);
            if (right != 0) {
                int plus1_key_len;
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

int rb_tree_node::locate(bplus_tree_var *v, int level) {
    return locate((rb_tree_var *) v, level);
}

int rb_tree_node::locate(rb_tree_var *v, int level) {
    int ret = -1;
    if (isLeaf()) {
        ret = binarySearchLeaf(v->key, v->key_len);
    } else {
        ret = binarySearchNode(v->key, v->key_len);
        if (ret < 0)
            ret--;
    }
    return ret;
}

rb_tree::rb_tree() {
    root = newNode();
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 0;
    blockCount = 1;
}

rb_tree_node::rb_tree_node() {
    buf = new byte[RB_TREE_NODE_SIZE];
    memset(buf, '\0', RB_TREE_NODE_SIZE);
    setLeaf(1);
    setFilledSize(0);
    //setDataEnd
    //setRoot
}

bool rb_tree_node::isLeaf() {
    return (buf[IS_LEAF_POS] == 1);
}

void rb_tree_node::setLeaf(char isLeaf) {
    buf[IS_LEAF_POS] = isLeaf;
}

void rb_tree_node::setFilledSize(int filledSize) {
    util::setInt(buf + FILLED_SIZE_POS, filledSize);
}

bool rb_tree_node::isFull(int kv_len, bplus_tree_var *v) {
    return isFull(kv_len, (rb_tree_var *) v);
}

bool rb_tree_node::isFull(int kv_len, rb_tree_var *v) {
    kv_len += 9; // 3 int pointer, 1 byte key len, 1 byte value len, 1 flag
    int spaceLeft = RB_TREE_NODE_SIZE - util::getInt(buf + DATA_END_POS);
    spaceLeft -= RB_NODE_HDR_SIZE;
    if (spaceLeft < kv_len)
        return true;
    return false;
}

int rb_tree_node::filledSize() {
    return util::getInt(buf + FILLED_SIZE_POS);
}

rb_tree_node *rb_tree_node::getChild(int pos) {
    byte *kvIdx = buf + RB_NODE_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + util::getInt(kvIdx);
    byte *idx = kvIdx;
    idx += kvIdx[0];
    idx += 2;
    unsigned long addr_num = util::fourBytesToPtr(idx);
    rb_tree_node *ret = (rb_tree_node *) addr_num;
    return ret;
}

byte *rb_tree_node::getKey(int pos, int *plen) {
    byte *kvIdx = buf + RB_NODE_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + util::getInt(kvIdx);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *rb_tree_node::getData(int pos, int *plen) {
    byte *kvIdx = buf + RB_NODE_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + util::getInt(kvIdx);
    byte *ret = kvIdx;
    ret += kvIdx[0];
    ret++;
    *plen = ret[0];
    ret++;
    return ret;
}

void rb_tree_node::rotateLeft(int n) {
    int r = getRight(n);
    replaceNode(n, r);
    setRight(n, getLeft(r));
    if (getLeft(r) != 0) {
        setParent(getLeft(r), n);
    }
    setLeft(r, n);
    setParent(n, r);
}

void rb_tree_node::rotateRight(int n) {
    int l = getLeft(n);
    replaceNode(n, l);
    setLeft(n, getRight(l));
    if (getRight(l) != 0) {
        setParent(getRight(l), n);
    }
    setRight(l, n);
    setParent(n, l);
}

void rb_tree_node::replaceNode(int oldn, int newn) {
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

void rb_tree_node::insertCase1(int n) {
    if (getParent(n) == NULL)
        setColor(n, RB_BLACK);
    else
        insertCase2(n);
}

void rb_tree_node::insertCase2(int n) {
    if (getColor(getParent(n)) == RB_BLACK)
        return;
    else
        insertCase3(n);
}

void rb_tree_node::insertCase3(int n) {
    if (getColor(getUncle(n)) == RB_RED) {
        setColor(getParent(n), RB_BLACK);
        setColor(getUncle(n), RB_BLACK);
        setColor(getGrandParent(n), RB_RED);
        insertCase1(getGrandParent(n));
    } else {
        insertCase4(n);
    }
}

void rb_tree_node::insertCase4(int n) {
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

void rb_tree_node::insertCase5(int n) {
    setColor(getParent(n), RB_BLACK);
    setColor(getGrandParent(n), RB_RED);
    if (n == getLeft(getParent(n))
            && getParent(n) == getLeft(getGrandParent(n))) {
        rotateRight(getGrandParent(n));
    } else {
        assert(
                n == getRight(getParent(n))
                        && getParent(n) == getRight(getGrandParent(n)));
        rotateLeft(getGrandParent(n));
    }
}

int rb_tree_node::getLeft(int n) {
    return util::getInt(buf + n + 1);
}

int rb_tree_node::getRight(int n) {
    return util::getInt(buf + n + 3);
}

int rb_tree_node::getParent(int n) {
    return util::getInt(buf + n + 5);
}

int rb_tree_node::getSibling(int n) {
    assert(n != 0);
    assert(getParent(n) != 0);
    if (n == getLeft(getParent(n)))
        return getRight(getParent(n));
    else
        return getLeft(getParent(n));
}

int rb_tree_node::getUncle(int n) {
    assert(n != NULL);
    assert(getParent(n) != 0);
    assert(getParent(getParent(n)) != 0);
    return getSibling(getParent(n));
}

int rb_tree_node::getGrandParent(int n) {
    assert(n != NULL);
    assert(getParent(n) != 0);
    assert(getParent(getParent(n)) != 0);
    return getParent(getParent(n));
}

int rb_tree_node::getRoot() {
    return util::getInt(buf + ROOT_NODE_POS);
}

int rb_tree_node::getColor(int n) {
    return buf[n];
}

void rb_tree_node::setLeft(int n, int l) {
    util::setInt(buf + n + 1, l);
}

void rb_tree_node::setRight(int n, int r) {
    util::setInt(buf + n + 3, r);
}

void rb_tree_node::setParent(int n, int p) {
    util::setInt(buf + n + 5, p);
}

void rb_tree_node::setRoot(int n) {
    util::setInt(buf + ROOT_NODE_POS, n);
}

void rb_tree_node::setColor(int n, byte c) {
    buf[n] = c;
}

int rb_tree_node::newNode(byte n_color, int left, int right, int parent) {
    int n = util::getInt(buf + DATA_END_POS);
    byte *node = buf + n;

    node[COLOR_POS] = n_color;
    util::setInt(node + LEFT_PTR_POS, left);
    util::setInt(node + RYTE_PTR_POS, right);
    util::setInt(node + PARENT_PTR_POS, parent);
    util::setInt(buf + DATA_END_POS, kv_last_pos);
    return n;
}
