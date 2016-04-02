#include "linex.h"
#include <cstdio>
#include <cstring>

void linex::put(const char *key, int key_len, const char *value,
        int value_len) {
    // isPut = true;
    int lastSearchPos[numLevels];
    linex_block *block_paths[numLevels];
    if (root->filledSize() == 0) {
        root->addData(0, key, key_len, value, value_len);
        total_size++;
    } else {
        int pos = 0;
        linex_block *foundNode = recursiveSearch(key, key_len, root,
                lastSearchPos, block_paths, &pos);
        recursiveUpdate(foundNode, pos, key, key_len, value, value_len,
                lastSearchPos, block_paths, numLevels - 1);
    }
    // isPut = false;
}

linex_block *linex::recursiveSearch(const char *key, int key_len,
        linex_block *node, int lastSearchPos[], linex_block *block_paths[],
        int *pIdx) {
    int level = 0;
    while (!node->isLeaf()) {
        lastSearchPos[level] = node->binarySearch(key, key_len);
        block_paths[level] = node;
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
            if (lastSearchPos[level] >= node->filledSize()) {
                lastSearchPos[level] -= node->filledSize();
                //lastSearchPos--; // required?
                do {
                    node = node->getChild(lastSearchPos[level]);
                    level++;
                    block_paths[level] = node;
                    lastSearchPos[level] = 0;
                } while (!node->isLeaf());
                *pIdx = lastSearchPos[level];
                return node;
            }
        }
        node = node->getChild(lastSearchPos[level]);
        level++;
    }
    block_paths[level] = node;
    *pIdx = node->binarySearch(key, key_len);
    lastSearchPos[level] = *pIdx; // required?
    return node;
}

char *linex::get(const char *key, int key_len, int *pValueLen) {
    int lastSearchPos[numLevels];
    linex_block *block_paths[numLevels];
    int pos = -1;
    linex_block *foundNode = recursiveSearch(key, key_len, root, lastSearchPos,
            block_paths, &pos);
    if (pos < 0)
        return null;
    return (char *) foundNode->getData(pos, pValueLen);
}
int linex::getInt(byte *pos) {
    int ret = *pos * 256;
    pos++;
    ret += *pos;
    return ret;
}
void linex::setInt(byte *pos, int val) {
    *pos = val / 256;
    pos++;
    *pos = val % 256;
}
void linex::cvtAddr(unsigned long addr_num, char *addr) {
    addr[3] = (addr_num & 0xFF);
    addr_num >>= 8;
    addr[2] = (addr_num & 0xFF);
    addr_num >>= 8;
    addr[1] = (addr_num & 0xFF);
    addr_num >>= 8;
    addr[0] = addr_num;
    addr[4] = 0;
}
unsigned long linex::cvtAddrToLong(byte *addr) {
    unsigned long ret = 0;
    ret = addr[0];
    ret <<= 8;
    ret |= addr[1];
    ret <<= 8;
    ret |= addr[2];
    ret <<= 8;
    ret |= addr[3];
    return ret;
}
void linex_block::setKVLastPos(int val) {
    linex::setInt(&buf[2], val);
}
int linex_block::getKVLastPos() {
    return linex::getInt(buf + 2);
}

void linex_block::addData(int idx, const char *key, int key_len,
        const char *value, int value_len) {

    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += idx;
    kvIdx += idx;
    int filledSz = filledSize();
    if (idx < filledSz)
        memmove(kvIdx + 2, kvIdx, (filledSz - idx) * 2);

    buf[1]++;
    int kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_len;
    memcpy(buf + kv_last_pos + 1, key, key_len);
    buf[kv_last_pos + key_len + 1] = value_len;
    memcpy(buf + kv_last_pos + key_len + 2, value, value_len);
    linex::setInt(kvIdx, kv_last_pos);
}

void linex::recursiveUpdate(linex_block *block, int pos, const char *key,
        int key_len, const char *value, int value_len, int lastSearchPos[],
        linex_block *blocks_path[], int level) {
    int idx = pos; // lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (block->isFull(key_len + value_len)) {
            //printf("Full\n");
            linex_block *new_block = new linex_block();
            if (!block->isLeaf())
                new_block->setLeaf(false);
            int kv_last_pos = block->getKVLastPos();
            int halfKVLen = BLK_SIZE - kv_last_pos + 1;
            halfKVLen /= 2;
            byte *kv_idx = block->buf + BLK_HDR_SIZE;
            byte *new_kv_idx = new_block->buf + BLK_HDR_SIZE;
            int new_idx;
            int brk_idx = -1;
            int brk_kv_pos;
            int new_pos = 0;
            int tot_len = 0;
            int filled_size = block->filledSize();
            for (new_idx = 0; new_idx < filled_size; new_idx++) {
                int src_idx = getInt(kv_idx + new_pos);
                int key_len = block->buf[src_idx];
                key_len++;
                int value_len = block->buf[src_idx + key_len];
                value_len++;
                int kv_len = key_len;
                kv_len += value_len;
                tot_len += kv_len;
                memcpy(new_block->buf + kv_last_pos, block->buf + src_idx,
                        kv_len);
                linex::setInt(new_kv_idx + new_pos, kv_last_pos);
                kv_last_pos += kv_len;
                new_pos += 2;
                if (tot_len > halfKVLen && brk_idx == -1) {
                    brk_idx = new_idx;
                    brk_kv_pos = kv_last_pos;
                }
            }
            kv_last_pos = block->getKVLastPos();
            // no need memset after everything works
            // memset(block->buf, '\n', sizeof(block->buf));
            int old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(block->buf + BLK_SIZE - old_blk_new_len,
                    new_block->buf + kv_last_pos, old_blk_new_len); // (3)
            new_pos = 0;
            for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
                int src_idx = getInt(new_kv_idx + new_pos);
                src_idx += (BLK_SIZE - brk_kv_pos);
                linex::setInt(kv_idx + new_pos, src_idx);
                if (new_idx == 0)
                    block->setKVLastPos(src_idx); // (6)
                new_pos += 2;
            } // (4)
            int new_size = filled_size - brk_idx - 1;
            memcpy(new_kv_idx, new_kv_idx + new_pos, new_size * 2); // (5)
            new_block->setKVLastPos(getInt(new_kv_idx)); // (7)
            block->setFilledSize(brk_idx + 1); // (8)
            new_block->setFilledSize(new_size); // (9)

            if (root == block) {
                root = new linex_block();
                int first_len;
                char *first_key = (char *) block->getKey(0, &first_len);
                char addr[5];
                linex::cvtAddr((unsigned long) block, addr);
                root->addData(0, first_key, first_len, addr, sizeof(char *));
                first_key = (char *) new_block->getKey(0, &first_len);
                linex::cvtAddr((unsigned long) new_block, addr);
                root->addData(1, first_key, first_len, addr, sizeof(char *));
                root->setLeaf(false);
                numLevels++;
            } else {
                int prev_level = level - 1;
                linex_block *parent = blocks_path[prev_level];
                int first_key_len;
                char *new_block_first_key = (char *) new_block->getKey(0,
                        &first_key_len);
                char addr[5];
                linex::cvtAddr((unsigned long) new_block, addr);
                recursiveUpdate(parent, ~(lastSearchPos[prev_level] + 1),
                        new_block_first_key, first_key_len, addr,
                        sizeof(char *), lastSearchPos, blocks_path, prev_level);
            }
            if (idx > new_idx) {
                block = new_block;
                idx -= new_idx;
            }

        }
        block->addData(idx, key, key_len, value, value_len);
    } else {
        //if (node->isLeaf) {
        //    int vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

int linex_block::compare(const char *v1, int len1, const char *v2, int len2) {
    int lim = len1;
    if (len2 < len1)
        lim = len2;
    int k = 0;
    while (k < lim) {
        char c1 = v1[k];
        char c2 = v2[k];
        if (c1 != c2) {
            return c1 - c2;
        }
        k++;
    }
    return len1 - len2;
}

int linex_block::binarySearchLeaf(const char *key, int key_len) {
    int middle, cmp, filledUpto;
    filledUpto = filledSize() - 1;
    middle = GenTree::roots[filledUpto];
    do {
        int middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        cmp = compare(middle_key, middle_key_len, key, key_len);
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

int linex_block::binarySearchNode(const char *key, int key_len) {
    int middle, cmp, filledUpto;
    filledUpto = filledSize() - 1;
    middle = GenTree::roots[filledUpto];
    do {
        int middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        cmp = compare(middle_key, middle_key_len, key, key_len);
        if (cmp > 0)
            middle = GenTree::left[middle];
        else if (cmp < 0) {
            if (filledUpto > middle) {
                int plus1_key_len;
                char *plus1_key = (char *) getKey(middle + 1, &plus1_key_len);
                cmp = compare(plus1_key, plus1_key_len, key, key_len);
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

int linex_block::binarySearch(const char *key, int key_len) {
    if (isLeaf())
        return binarySearchLeaf(key, key_len);
    return binarySearchNode(key, key_len);
}

long linex::size() {
    return total_size;
}

linex::linex() {
    GenTree::generateLists();
    root = new linex_block();
    total_size = 0;
    numLevels = 1;
}

linex_block::linex_block() {
    memset(buf, '\0', sizeof(buf));
    setLeaf(1);
    setFilledSize(0);
    setKVLastPos(sizeof(buf));
}

bool linex_block::isLeaf() {
    return (buf[0] == 1);
}

void linex_block::setLeaf(char isLeaf) {
    buf[0] = isLeaf;
}
void linex_block::setFilledSize(char filledSize) {
    buf[1] = filledSize;
}

bool linex_block::isFull(int kv_len) {
    kv_len += 4; // one int pointer, 1 byte key len, 1 byte value len
    int spaceLeft = getKVLastPos();
    spaceLeft -= (filledSize() * 2);
    spaceLeft -= BLK_HDR_SIZE;
    if (spaceLeft < kv_len)
        return true;
    return false;
}

int linex_block::filledSize() {
    return buf[1];
}

linex_block *linex_block::getChild(int pos) {
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + linex::getInt(kvIdx);
    byte *idx = kvIdx;
    idx += kvIdx[0];
    idx += 2;
    unsigned long addr_num = linex::cvtAddrToLong(idx);
    linex_block *ret = (linex_block *) addr_num;
    return ret;
}

byte *linex_block::getKey(int pos, int *plen) {
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + linex::getInt(kvIdx);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *linex_block::getData(int pos, int *plen) {
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = buf + linex::getInt(kvIdx);
    byte *ret = kvIdx;
    ret += kvIdx[0];
    ret++;
    *plen = ret[0];
    ret++;
    return ret;
}

byte GenTree::bit_count[256];
int *GenTree::roots;
int *GenTree::left;
int *GenTree::ryte;
int *GenTree::parent;
int GenTree::ixRoots;
int GenTree::ixLeft;
int GenTree::ixRyte;
int GenTree::ixPrnt;
