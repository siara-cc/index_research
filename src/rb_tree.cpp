#include "rb_tree.h"
#include <math.h>

char *rb_tree::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    int16_t pos = -1;
    byte *node_data = root_data;
    rb_tree_node_handler node(node_data);
    node.key = key;
    node.key_len = key_len;
    recursiveSearchForGet(&node, &pos);
    if (pos < 0)
        return null;
    return (char *) node.getData(pos, pValueLen);
}

void rb_tree::recursiveSearchForGet(rb_tree_node_handler *node, int16_t *pIdx) {
    int16_t level = 0;
    int16_t pos = -1;
    byte *node_data = node->buf;
    node->depth = 0;
    //node->initVars();
    while (!node->isLeaf()) {
        pos = node->locate(level);
        if (pos < 0)
            pos = ~pos;
//        } else {
//            do {
//                node_data = node->getChild(pos);
//                node->setBuf(node_data);
//                level++;
//                pos = node->getFirst();
//            } while (!node->isLeaf());
//            *pIdx = pos;
//            return;
//        }
        node_data = node->getChild(pos);
        node->setBuf(node_data);
        //node->initVars();
        level++;
    }
    pos = node->locate(level);
    if (root->depth < node->depth)
        root->depth = node->depth;
    *pIdx = pos;
    return;
}

void rb_tree::recursiveSearch(rb_tree_node_handler *node,
        int16_t lastSearchPos[], byte *node_paths[], int16_t *pIdx) {
    int16_t level = 0;
    byte *node_data = node->buf;
    //node->initVars();
    while (!node->isLeaf()) {
        lastSearchPos[level] = node->locate(level);
        node_paths[level] = node->buf;
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
        } else {
            do {
                node_data = node->getChild(lastSearchPos[level]);
                node->setBuf(node_data);
                level++;
                node_paths[level] = node->buf;
                lastSearchPos[level] = 0;
            } while (!node->isLeaf());
            *pIdx = lastSearchPos[level];
            return;
        }
        node_data = node->getChild(lastSearchPos[level]);
        node->setBuf(node_data);
        //node->initVars();
        level++;
    }
    node_paths[level] = node_data;
    lastSearchPos[level] = node->locate(level);
    *pIdx = lastSearchPos[level];
    return;
}

void rb_tree::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    int16_t lastSearchPos[10];
    byte *block_paths[10];
    rb_tree_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    node.value = value;
    node.value_len = value_len;
    node.isPut = true;
    if (node.filledUpto() == -1) {
        node.addData(0);
        total_size++;
    } else {
        int16_t pos = -1;
        recursiveSearch(&node, lastSearchPos, block_paths, &pos);
        recursiveUpdate(&node, pos, lastSearchPos, block_paths, numLevels - 1);
    }
}

void rb_tree::recursiveUpdate(rb_tree_node_handler *node, int16_t pos,
        int16_t lastSearchPos[], byte *node_paths[], int16_t level) {
    int16_t idx = pos; // lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (node->isFull(node->key_len + node->value_len)) {
            //std::cout << "Full\n" << std::endl;
            //if (maxKeyCount < block->filledSize())
            //    maxKeyCount = block->filledSize();
            //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
            maxKeyCount += node->filledUpto();
            int16_t brk_idx;
            byte *b = node->split(&brk_idx);
            rb_tree_node_handler new_block(b);
            blockCount++;
            bool isRoot = false;
            if (root_data == node->buf)
                isRoot = true;
            int16_t first_key_len = node->key_len;
            byte *old_buf = node->buf;
            const char *new_block_first_key = (char *) new_block.getFirstKey(
                    &first_key_len);
            int16_t cmp = util::compare(new_block_first_key, first_key_len,
                    node->key, node->key_len, 0);
            if (cmp <= 0)
                node->setBuf(new_block.buf);
            if (isRoot) {
                root_data = util::alignedAlloc(RB_TREE_NODE_SIZE);
                rb_tree_node_handler root(root_data);
                root.initBuf();
                blockCount++;
                char addr[5];
                util::ptrToFourBytes((unsigned long) old_buf, addr);
                root.key = "";
                root.key_len = 1;
                root.value = addr;
                root.value_len = sizeof(char *);
                root.addData(0);
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                root.key = new_block_first_key;
                root.key_len = first_key_len;
                root.value = addr;
                root.value_len = sizeof(char *);
                root.addData(1);
                root.setLeaf(false);
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent = node_paths[prev_level];
                rb_tree_node_handler parent_node(parent);
                char addr[5];
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                parent_node.key = new_block_first_key;
                parent_node.key_len = first_key_len;
                parent_node.value = addr;
                parent_node.value_len = sizeof(char *);
                recursiveUpdate(&parent_node, ~(lastSearchPos[prev_level] + 1),
                        lastSearchPos, node_paths, prev_level);
            }
        }
        node->addData(idx);
    } else {
//if (node->isLeaf) {
//    int16_t vIdx = idx + mSizeBy2;
//    returnValue = (V) arr[vIdx];
//    arr[vIdx] = value;
//}
    }
}

void rb_tree_node_handler::addData(int16_t idx) {

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

    if (getRoot() == 0) {
        setRoot(inserted_node);
    } else {
        int16_t n = getRoot();
        while (1) {
            int16_t key_at_len;
            char *key_at = (char *) getKey(n, &key_at_len);
            int16_t comp_result = util::compare(key, key_len, key_at,
                    key_at_len, 0);
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
    }
    insertCase1(inserted_node);

}

byte *rb_tree_node_handler::split(int16_t *pbrk_idx) {
    int16_t filled_upto = filledUpto();
    rb_tree_node_handler new_block(util::alignedAlloc(RB_TREE_NODE_SIZE));
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
    int16_t new_block_root1;
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
            new_block.addData(9999);
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

    *pbrk_idx = brk_idx;
    return new_block.buf;
}

int16_t rb_tree_node_handler::getDataEndPos() {
    return util::getInt(buf + DATA_END_POS);
}

void rb_tree_node_handler::setDataEndPos(int16_t pos) {
    util::setInt(buf + DATA_END_POS, pos);
}

int16_t rb_tree_node_handler::binarySearchLeaf(const char *key,
        int16_t key_len) {
    register int16_t middle;
    register int16_t new_middle = getRoot();
    int16_t d = 1;
    do {
        middle = new_middle;
        int16_t middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        int16_t cmp = util::compare(middle_key, middle_key_len, key, key_len);
        if (cmp > 0)
            new_middle = getLeft(middle);
        else if (cmp < 0)
            new_middle = getRight(middle);
        else {
            if (d > depth)
                depth = d;
            return middle;
        }
        d++;
    } while (new_middle > 0);
    if (d > depth)
        depth = d;
    return ~middle;
}

int16_t rb_tree_node_handler::binarySearchNode(const char *key,
        int16_t key_len) {
    register int16_t middle;
    register int16_t new_middle = getRoot();
    do {
        middle = new_middle;
        int16_t middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        int16_t cmp = util::compare(middle_key, middle_key_len, key, key_len);
        if (cmp > 0)
            new_middle = getLeft(middle);
        else if (cmp < 0) {
            int16_t next = getNext(middle);
            if (next != 0) {
                int16_t plus1_key_len;
                char *plus1_key = (char *) getKey(next, &plus1_key_len);
                cmp = util::compare(plus1_key, plus1_key_len, key, key_len);
                if (cmp > 0)
                    return ~middle;
                else if (cmp < 0)
                    new_middle = getRight(middle);
                else
                    return next;
            } else
                return ~middle;
        } else
            return middle;
    } while (new_middle > 0);
    return ~middle;
}

int16_t rb_tree_node_handler::locate(int16_t level) {
    int16_t ret = -1;
    if (isLeaf()) {
        ret = binarySearchLeaf(key, key_len);
    } else {
        ret = binarySearchNode(key, key_len);
    }
    return ret;
}

rb_tree::~rb_tree() {
    delete root_data;
}

rb_tree::rb_tree() {
    GenTree::generateLists();
    root_data = util::alignedAlloc(RB_TREE_NODE_SIZE);
    rb_tree_node_handler *r = new rb_tree_node_handler(root_data);
    r->initBuf();
    root = r;
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 0;
    blockCount = 1;
}

rb_tree_node_handler::rb_tree_node_handler(byte *b) {
    setBuf(b);
}

void rb_tree_node_handler::setBuf(byte *b) {
    buf = b;
}

void rb_tree_node_handler::initBuf() {
    memset(buf, '\0', RB_TREE_NODE_SIZE);
    setLeaf(1);
    setFilledUpto(-1);
    setDataEndPos(RB_TREE_HDR_SIZE);
}

bool rb_tree_node_handler::isLeaf() {
    return (buf[IS_LEAF_POS] == 1);
}

void rb_tree_node_handler::setLeaf(char isLeaf) {
    buf[IS_LEAF_POS] = isLeaf;
}

void rb_tree_node_handler::setFilledUpto(int16_t filledSize) {
    util::setInt(buf + FILLED_UPTO_POS, filledSize);
}

int16_t rb_tree_node_handler::filledUpto() {
    return util::getInt(buf + FILLED_UPTO_POS);
}

bool rb_tree_node_handler::isFull(int16_t kv_len) {
    kv_len += 10; // 3 int16_t pointer, 1 byte key len, 1 byte value len, 1 flag
    int16_t spaceLeft = RB_TREE_NODE_SIZE - util::getInt(buf + DATA_END_POS);
    spaceLeft -= RB_TREE_HDR_SIZE;
    if (spaceLeft <= kv_len)
        return true;
    return false;
}

byte *rb_tree_node_handler::getChild(int16_t pos) {
    byte *kvIdx = buf + pos + KEY_LEN_POS;
    kvIdx += kvIdx[0];
    kvIdx += 2;
    unsigned long addr_num = util::fourBytesToPtr(kvIdx);
    byte *ret = (byte *) addr_num;
    return ret;
}

byte *rb_tree_node_handler::getKey(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + pos + KEY_LEN_POS;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
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

byte *rb_tree_node_handler::getData(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + pos + KEY_LEN_POS;
    *plen = kvIdx[0];
    kvIdx += *plen;
    kvIdx++;
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
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
    return util::getInt(buf + n + 1);
}

int16_t rb_tree_node_handler::getRight(int16_t n) {
    return util::getInt(buf + n + 3);
}

int16_t rb_tree_node_handler::getParent(int16_t n) {
    return util::getInt(buf + n + 5);
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
    return buf[n];
}

void rb_tree_node_handler::setLeft(int16_t n, int16_t l) {
    util::setInt(buf + n + LEFT_PTR_POS, l);
}

void rb_tree_node_handler::setRight(int16_t n, int16_t r) {
    util::setInt(buf + n + RYTE_PTR_POS, r);
}

void rb_tree_node_handler::setParent(int16_t n, int16_t p) {
    util::setInt(buf + n + PARENT_PTR_POS, p);
}

void rb_tree_node_handler::setRoot(int16_t n) {
    util::setInt(buf + ROOT_NODE_POS, n);
}

void rb_tree_node_handler::setColor(int16_t n, byte c) {
    buf[n] = c;
}
