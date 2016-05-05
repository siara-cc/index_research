#include "linex.h"

char *linex::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    int16_t pos = -1;
    byte *foundNode = recursiveSearchForGet(key, key_len, &pos);
    if (pos < 0)
        return null;
    linex_node node(foundNode);
    return (char *) node.getData(pos, pValueLen);
}

byte *linex::recursiveSearchForGet(const char *key, int16_t key_len,
        int16_t *pIdx) {
    int16_t level = 0;
    int pos = -1;
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

byte *linex::recursiveSearch(const char *key, int16_t key_len, byte *node_data,
        int16_t lastSearchPos[], byte *node_paths[], int16_t *pIdx) {
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

void linex::put(const char *key, int16_t key_len, const char *value,
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

void linex_node::setKVLastPos(int16_t val) {
    util::setInt(&buf[3], val);
}

int16_t linex_node::getKVLastPos() {
    return util::getInt(buf + 3);
}

void linex_node::addData(int16_t idx, const char *key, int16_t key_len,
        const char *value, int16_t value_len) {

    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += idx;
    kvIdx += idx;
    int16_t filled_upto = filledUpto();
    filled_upto++;
    if (idx < filled_upto)
        memmove(kvIdx + 2, kvIdx, (filled_upto - idx) * 2);

    util::setInt(buf + 1, filled_upto);
    int16_t kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_len;
    memcpy(buf + kv_last_pos + 1, key, key_len);
    buf[kv_last_pos + key_len + 1] = value_len;
    memcpy(buf + kv_last_pos + key_len + 2, value, value_len);
    util::setInt(kvIdx, kv_last_pos);
}

byte *linex_node::split(int16_t *pbrk_idx) {
    int16_t filled_upto = filledUpto();
    byte *b = util::alignedAlloc(LINEX_NODE_SIZE);
    linex_node new_block(b);
    new_block.init();
    if (!isLeaf())
        new_block.setLeaf(false);
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = LINEX_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;
    byte *kv_idx = buf + BLK_HDR_SIZE;
    byte *new_kv_idx = new_block.buf + BLK_HDR_SIZE;
    int16_t new_idx;
    int16_t brk_idx = -1;
    int16_t brk_kv_pos = 0;
    int16_t new_pos = 0;
    int16_t tot_len = 0;
    // Copy all data to new block in ascending order
    for (new_idx = 0; new_idx <= filled_upto; new_idx++) {
        int16_t src_idx = util::getInt(kv_idx + new_pos);
        int16_t key_len = buf[src_idx];
        key_len++;
        int16_t value_len = buf[src_idx + key_len];
        value_len++;
        int16_t kv_len = key_len;
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
    memset(buf + BLK_HDR_SIZE, '\0', LINEX_NODE_SIZE - BLK_HDR_SIZE);
    kv_last_pos = getKVLastPos();
    int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
    memcpy(buf + LINEX_NODE_SIZE - old_blk_new_len, new_block.buf + kv_last_pos,
            old_blk_new_len); // Copy back first half to old block
    memset(new_block.buf + kv_last_pos, '\0', old_blk_new_len);
    new_pos = 0;
    for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
        int16_t src_idx = util::getInt(new_kv_idx + new_pos);
        src_idx += (LINEX_NODE_SIZE - brk_kv_pos);
        util::setInt(kv_idx + new_pos, src_idx);
        if (new_idx == 0) // Set KV Last pos in old block
            setKVLastPos(src_idx);
        new_pos += 2;
    } // Set index of copied first half in old block
    int16_t new_size = filled_upto - brk_idx;
    // Move index of second half to first half in new block
    memcpy(new_kv_idx, new_kv_idx + new_pos, new_size * 2);
    memset(new_kv_idx + new_size * 2, '\0', new_pos);
    // Set KV Last pos for new block
    new_block.setKVLastPos(util::getInt(new_kv_idx));
    filled_upto = brk_idx;
    setFilledUpto(filled_upto); // Set filled upto for old block
    new_block.setFilledUpto(new_size - 1); // Set filled upto for new block
    *pbrk_idx = brk_idx;
    return b;
}

void linex::recursiveUpdate(const char *key, int16_t key_len, byte *foundNode,
        int16_t pos, const char *value, int16_t value_len,
        int16_t lastSearchPos[], byte *node_paths[], int16_t level) {
    linex_node node(foundNode);
    int16_t idx = pos; // lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (node.isFull(key_len + value_len)) {
            //std::cout << "Full\n" << std::endl;
            //if (maxKeyCount < block->filledSize())
            //    maxKeyCount = block->filledSize();
            //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
            maxKeyCount += node.filledUpto();
            int16_t brk_idx;
            byte *b = node.split(&brk_idx);
            linex_node new_block(b);
            blockCount++;
            bool isRoot = false;
            if (root->buf == node.buf)
                isRoot = true;
            int16_t first_key_len = key_len;
            const char *new_block_first_key = key;
            if (idx > brk_idx) {
                node.setBuf(new_block.buf);
                idx -= brk_idx;
                idx--;
                if (idx > 0) {
                    new_block_first_key = (char *) new_block.getKey(0,
                            &first_key_len);
                }
            } else {
                new_block_first_key = (char *) new_block.getKey(0,
                        &first_key_len);
            }
            if (isRoot) {
                byte *buf = util::alignedAlloc(LINEX_NODE_SIZE);
                root->setBuf(buf);
                root->init();
                blockCount++;
                char addr[5];
                util::ptrToFourBytes((unsigned long) foundNode, addr);
                root->addData(0, "", 1, addr, sizeof(char *));
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                root->addData(1, new_block_first_key, first_key_len, addr,
                        sizeof(char *));
                root->setLeaf(false);
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent = node_paths[prev_level];
                char addr[5];
                util::ptrToFourBytes((unsigned long) new_block.buf, addr);
                recursiveUpdate(new_block_first_key, first_key_len, parent,
                        ~(lastSearchPos[prev_level] + 1), addr, sizeof(char *),
                        lastSearchPos, node_paths, prev_level);
            }
        }
        node.addData(idx, key, key_len, value, value_len);
    } else {
        //if (node->isLeaf) {
        //    int16_t vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

int16_t linex_node::binarySearchLeaf(const char *key, int16_t key_len) {
    register int16_t middle, cmp, filled_upto;
    filled_upto = filledUpto();
    middle = GenTree::roots[filled_upto];
    do {
        int16_t middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        cmp = util::compare(middle_key, middle_key_len, key, key_len);
        if (cmp < 0) {
            middle = GenTree::ryte[middle];
            while (middle > filled_upto)
                middle = GenTree::left[middle];
        } else if (cmp > 0)
            middle = GenTree::left[middle];
        else
            return middle;
    } while (middle >= 0);
    return middle;
}

int16_t linex_node::binarySearchNode(const char *key, int16_t key_len) {
    register int16_t middle, cmp, filled_upto;
    filled_upto = filledUpto();
    middle = GenTree::roots[filled_upto];
    do {
        int16_t middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        cmp = util::compare(middle_key, middle_key_len, key, key_len);
        if (cmp > 0)
            middle = GenTree::left[middle];
        else if (cmp < 0) {
            if (filled_upto > middle) {
                int16_t plus1_key_len;
                char *plus1_key = (char *) getKey(middle + 1, &plus1_key_len);
                cmp = util::compare(plus1_key, plus1_key_len, key, key_len);
                if (cmp > 0)
                    return ~middle;
                else if (cmp < 0) {
                    middle = GenTree::ryte[middle];
                    while (middle > filled_upto)
                        middle = GenTree::left[middle];
                } else
                    return (middle + 1);
            } else
                return ~middle;
        } else
            return middle;
    } while (middle >= 0);
    return middle;
}

int16_t linex_node::locate(const char *key, int16_t key_len, int16_t level) {
    int16_t ret = -1;
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
    setFilledUpto(-1);
    setKVLastPos(LINEX_NODE_SIZE);
}

bool linex_node::isLeaf() {
    return (buf[0] == 1);
}

void linex_node::setLeaf(char isLeaf) {
    buf[0] = isLeaf;
}

void linex_node::setFilledUpto(int16_t filledUpto) {
    util::setInt(buf + 1, filledUpto);
}

bool linex_node::isFull(int16_t kv_len) {
    kv_len += 4; // one int16_t pointer, 1 byte key len, 1 byte value len
    int16_t spaceLeft = getKVLastPos();
    spaceLeft -= (filledUpto() * 2);
    spaceLeft -= 2;
    spaceLeft -= BLK_HDR_SIZE;
    if (spaceLeft < kv_len)
        return true;
    return false;
}

int16_t linex_node::filledUpto() {
    return util::getInt(buf + 1);
}

byte *linex_node::getChild(int16_t pos) {
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

byte *linex_node::getKey(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + util::getInt(kvIdx);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *linex_node::getData(int16_t pos, int16_t *plen) {
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
