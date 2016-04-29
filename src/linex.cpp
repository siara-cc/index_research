#include "linex.h"

char *linex::get(const char *key, int key_len, int *pValueLen) {
    int lastSearchPos[numLevels];
    byte *block_paths[numLevels];
    int pos = -1;
    byte *foundNode = recursiveSearch(key, key_len, root->buf, lastSearchPos,
            block_paths, &pos);
    if (pos < 0)
        return null;
    linex_node node(foundNode);
    return (char *) node.getData(pos, pValueLen);
}

byte *linex::recursiveSearch(const char *key, int key_len, byte *node_data,
        int lastSearchPos[], byte *node_paths[], int *pIdx) {
    int level = 0;
    linex_node node(node_data);
    while (!node.isLeaf()) {
        lastSearchPos[level] = node.locate(key, key_len, level);
        node_paths[level] = node_data;
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
            if (lastSearchPos[level] >= node.filledSize()) {
                lastSearchPos[level] -= node.filledSize();
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

void linex::put(const char *key, int key_len, const char *value,
        int value_len) {
    int lastSearchPos[numLevels];
    byte *block_paths[numLevels];
    if (root->filledSize() == 0) {
        root->addData(0, key, key_len, value, value_len);
        total_size++;
    } else {
        int pos = -1;
        byte *foundNode = recursiveSearch(key, key_len, root->buf,
                lastSearchPos, block_paths, &pos);
        recursiveUpdate(key, key_len, foundNode, pos, value, value_len,
                lastSearchPos, block_paths, numLevels - 1);
    }
}

void linex_node::setKVLastPos(int val) {
    util::setInt(&buf[3], val);
}

int linex_node::getKVLastPos() {
    return util::getInt(buf + 3);
}

void linex_node::addData(int idx, const char *key, int key_len,
        const char *value, int value_len) {

    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += idx;
    kvIdx += idx;
    int filledSz = filledSize();
    if (idx < filledSz)
        memmove(kvIdx + 2, kvIdx, (filledSz - idx) * 2);

    filledSz++;
    util::setInt(buf + 1, filledSz);
    int kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_len;
    memcpy(buf + kv_last_pos + 1, key, key_len);
    buf[kv_last_pos + key_len + 1] = value_len;
    memcpy(buf + kv_last_pos + key_len + 2, value, value_len);
    util::setInt(kvIdx, kv_last_pos);
}

byte *linex_node::split(int *pbrk_idx) {
    int filled_size = filledSize();
    byte *b = util::alignedAlloc(LINEX_NODE_SIZE);
    linex_node new_block(b);
    new_block.init();
    if (!isLeaf())
        new_block.setLeaf(false);
    int kv_last_pos = getKVLastPos();
    int halfKVLen = LINEX_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;
    byte *kv_idx = buf + BLK_HDR_SIZE;
    byte *new_kv_idx = new_block.buf + BLK_HDR_SIZE;
    int new_idx;
    int brk_idx = -1;
    int brk_kv_pos = 0;
    int new_pos = 0;
    int tot_len = 0;
    for (new_idx = 0; new_idx < filled_size; new_idx++) {
        int src_idx = util::getInt(kv_idx + new_pos);
        int key_len = buf[src_idx];
        key_len++;
        int value_len = buf[src_idx + key_len];
        value_len++;
        int kv_len = key_len;
        kv_len += value_len;
        tot_len += kv_len;
        memcpy(new_block.buf + kv_last_pos, buf + src_idx, kv_len);
        util::setInt(new_kv_idx + new_pos, kv_last_pos);
        kv_last_pos += kv_len;
        new_pos += 2;
        if (tot_len > halfKVLen && brk_idx == -1) {
            brk_idx = new_idx;
            brk_kv_pos = kv_last_pos;
        }
    }
    kv_last_pos = getKVLastPos();
    int old_blk_new_len = brk_kv_pos - kv_last_pos;
    memcpy(buf + LINEX_NODE_SIZE - old_blk_new_len, new_block.buf + kv_last_pos,
            old_blk_new_len); // (3)
    new_pos = 0;
    for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
        int src_idx = util::getInt(new_kv_idx + new_pos);
        src_idx += (LINEX_NODE_SIZE - brk_kv_pos);
        util::setInt(kv_idx + new_pos, src_idx);
        if (new_idx == 0)
            setKVLastPos(src_idx); // (6)
        new_pos += 2;
    } // (4)
    int new_size = filled_size - brk_idx - 1;
    memcpy(new_kv_idx, new_kv_idx + new_pos, new_size * 2); // (5)
    new_block.setKVLastPos(util::getInt(new_kv_idx)); // (7)
    filled_size = brk_idx + 1;
    setFilledSize(filled_size); // (8)
    new_block.setFilledSize(new_size); // (9)
    *pbrk_idx = brk_idx;
    return b;
}

void linex::recursiveUpdate(const char *key, int key_len, byte *foundNode,
        int pos, const char *value, int value_len, int lastSearchPos[],
        byte *node_paths[], int level) {
    linex_node node(foundNode);
    int idx = pos; // lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (node.isFull(key_len + value_len)) {
            //std::cout << "Full\n" << std::endl;
            //if (maxKeyCount < block->filledSize())
            //    maxKeyCount = block->filledSize();
            //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
            maxKeyCount += node.filledSize();
            int brk_idx;
            byte *b = node.split(&brk_idx);
            linex_node new_block(b);
            blockCount++;
            if (root->buf == node.buf) {
                byte *buf = util::alignedAlloc(LINEX_NODE_SIZE);
                root->setBuf(buf);
                root->init();
                blockCount++;
                int first_len;
                char *first_key;
                char addr[5];
                util::ptrToFourBytes((unsigned long) foundNode, addr);
                root->addData(0, "", 1, addr, sizeof(char *));
                first_key = (char *) new_block.getKey(0, &first_len);
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                root->addData(1, first_key, first_len, addr, sizeof(char *));
                root->setLeaf(false);
                numLevels++;
            } else {
                int prev_level = level - 1;
                byte *parent = node_paths[prev_level];
                int first_key_len;
                char *new_block_first_key = (char *) new_block.getKey(0,
                        &first_key_len);
                char addr[5];
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                recursiveUpdate(new_block_first_key, first_key_len, parent,
                        ~(lastSearchPos[prev_level] + 1), addr, sizeof(char *),
                        lastSearchPos, node_paths, prev_level);
            }
            brk_idx++;
            if (idx > brk_idx) {
                node.setBuf(new_block.buf);
                idx -= brk_idx;
            }
        }
        node.addData(idx, key, key_len, value, value_len);
    } else {
        //if (node->isLeaf) {
        //    int vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

int linex_node::binarySearchLeaf(const char *key, int key_len) {
    int middle, cmp, filledUpto;
    filledUpto = filledSize() - 1;
    middle = GenTree::roots[filledUpto];
    do {
        int middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        cmp = util::compare(middle_key, middle_key_len, key, key_len);
        if (cmp < 0) {
            middle = GenTree::ryte[middle];
            while (middle > filledUpto)
                middle = GenTree::left[middle];
        } else if (cmp > 0)
            middle = GenTree::left[middle];
        else
            return middle;
    } while (middle >= 0);
    return middle;
}

int linex_node::binarySearchNode(const char *key, int key_len) {
    int middle, cmp, filledUpto;
    filledUpto = filledSize() - 1;
    middle = GenTree::roots[filledUpto];
    do {
        int middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        cmp = util::compare(middle_key, middle_key_len, key, key_len);
        if (cmp > 0)
            middle = GenTree::left[middle];
        else if (cmp < 0) {
            if (filledUpto > middle) {
                int plus1_key_len;
                char *plus1_key = (char *) getKey(middle + 1, &plus1_key_len);
                cmp = util::compare(plus1_key, plus1_key_len, key, key_len);
                if (cmp > 0)
                    return middle;
                else if (cmp < 0) {
                    middle = GenTree::ryte[middle];
                    while (middle > filledUpto)
                        middle = GenTree::left[middle];
                } else
                    return ~(middle + filledUpto + 2);
            } else
                return middle;
        } else
            return ~(middle + filledUpto + 1);
    } while (middle >= 0);
    return middle;
}

int linex_node::locate(const char *key, int key_len, int level) {
    int ret = -1;
    if (isLeaf()) {
        ret = binarySearchLeaf(key, key_len);
    } else {
        ret = binarySearchNode(key, key_len);
    }
    return ret;
}

linex::~linex() {
    delete root;
}

linex::linex() {
    GenTree::generateLists();
    root = new linex_node(util::alignedAlloc(LINEX_NODE_SIZE));
    root->init();
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 0;
    blockCount = 1;
}

linex_node::linex_node(byte *b) {
    setBuf(b);
}

void linex_node::setBuf(byte *b) {
    buf = b;
}

void linex_node::init() {
    memset(buf, '\0', LINEX_NODE_SIZE);
    setLeaf(1);
    setFilledSize(0);
    setKVLastPos(LINEX_NODE_SIZE);
}

bool linex_node::isLeaf() {
    return (buf[0] == 1);
}

void linex_node::setLeaf(char isLeaf) {
    buf[0] = isLeaf;
}

void linex_node::setFilledSize(int filledSize) {
    util::setInt(buf + 1, filledSize);
}

bool linex_node::isFull(int kv_len) {
    kv_len += 4; // one int pointer, 1 byte key len, 1 byte value len
    int spaceLeft = getKVLastPos();
    spaceLeft -= (filledSize() * 2);
    spaceLeft -= BLK_HDR_SIZE;
    if (spaceLeft < kv_len)
        return true;
    return false;
}

int linex_node::filledSize() {
    return util::getInt(buf + 1);
}

byte *linex_node::getChild(int pos) {
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + util::getInt(kvIdx);
    byte *idx = kvIdx;
    idx += kvIdx[0];
    idx += 2;
    unsigned long addr_num = util::fourBytesToPtr(idx);
    byte *ret = (byte *) addr_num;
    return ret;
}

byte *linex_node::getKey(int pos, int *plen) {
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + util::getInt(kvIdx);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *linex_node::getData(int pos, int *plen) {
    byte *kvIdx = buf + BLK_HDR_SIZE;
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
