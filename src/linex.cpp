#include "linex.h"

linex_var *linex::newVar() {
    return new linex_var();
}

linex_node *linex::newNode() {
    return new linex_node();
}

void linex_node::setKVLastPos(int val) {
    util::setInt(&buf[3], val);
}

int linex_node::getKVLastPos() {
    return util::getInt(buf + 3);
}

void linex_node::addData(int idx, const char *value, int value_len,
        bplus_tree_var *v) {
    addData(idx, value, value_len, (linex_var *) v);
}

void linex_node::addData(int idx, const char *value, int value_len,
        linex_var *v) {

    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += idx;
    kvIdx += idx;
    int filledSz = filledSize();
    if (idx < filledSz)
        memmove(kvIdx + 2, kvIdx, (filledSz - idx) * 2);

    filledSz++;
    util::setInt(buf + 1, filledSz);
    int kv_last_pos = getKVLastPos() - (v->key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = v->key_len;
    memcpy(buf + kv_last_pos + 1, v->key, v->key_len);
    buf[kv_last_pos + v->key_len + 1] = value_len;
    memcpy(buf + kv_last_pos + v->key_len + 2, value, value_len);
    util::setInt(kvIdx, kv_last_pos);
}

linex_node *linex_node::split(int *pbrk_idx) {
    int filled_size = filledSize();
    linex_node *new_block = new linex_node();
    if (!isLeaf())
        new_block->setLeaf(false);
    int kv_last_pos = getKVLastPos();
    int halfKVLen = LINEX_BLK_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;
    byte *kv_idx = buf + BLK_HDR_SIZE;
    byte *new_kv_idx = new_block->buf + BLK_HDR_SIZE;
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
        memcpy(new_block->buf + kv_last_pos, buf + src_idx, kv_len);
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
    memcpy(buf + LINEX_BLK_SIZE - old_blk_new_len, new_block->buf + kv_last_pos,
            old_blk_new_len); // (3)
    new_pos = 0;
    for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
        int src_idx = util::getInt(new_kv_idx + new_pos);
        src_idx += (LINEX_BLK_SIZE - brk_kv_pos);
        util::setInt(kv_idx + new_pos, src_idx);
        if (new_idx == 0)
            setKVLastPos(src_idx); // (6)
        new_pos += 2;
    } // (4)
    int new_size = filled_size - brk_idx - 1;
    memcpy(new_kv_idx, new_kv_idx + new_pos, new_size * 2); // (5)
    new_block->setKVLastPos(util::getInt(new_kv_idx)); // (7)
    filled_size = brk_idx + 1;
    setFilledSize(filled_size); // (8)
    new_block->setFilledSize(new_size); // (9)
    *pbrk_idx = brk_idx;
    return new_block;
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

int linex_node::locate(bplus_tree_var *v) {
    return locate((linex_var *) v);
}

int linex_node::locate(linex_var *v) {
    if (isLeaf())
        return binarySearchLeaf(v->key, v->key_len);
    return binarySearchNode(v->key, v->key_len);
}

linex::linex() {
    GenTree::generateLists();
    root = newNode();
    total_size = 0;
    numLevels = 1;
    maxKeyCount = 0;
    blockCount = 1;
}

linex_node::linex_node() {
    memset(buf, '\0', sizeof(buf));
    setLeaf(1);
    setFilledSize(0);
    setKVLastPos(sizeof(buf));
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

bool linex_node::isFull(int kv_len, bplus_tree_var *v) {
    return isFull(kv_len, (linex_var *) v);
}

bool linex_node::isFull(int kv_len, linex_var *v) {
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

linex_node *linex_node::getChild(int pos) {
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + util::getInt(kvIdx);
    byte *idx = kvIdx;
    idx += kvIdx[0];
    idx += 2;
    unsigned long addr_num = util::fourBytesToPtr(idx);
    linex_node *ret = (linex_node *) addr_num;
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
