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
void linex_block::setKVLastPos(int val) {
    linex::setInt(&block_data.buf[2], val);
}
int linex_block::getKVLastPos() {
    return linex::getInt(block_data.buf + 2);
}

void linex_block::addData(int idx, const char *key, int key_len,
        const char *value, int value_len) {

    byte *kvIdx = block_data.buf + BLK_HDR_SIZE;
    kvIdx += idx;
    kvIdx += idx;
    int filledSz = filledSize();
    if (idx < filledSz)
        memmove(kvIdx + 2, kvIdx, (filledSz - idx) * 2);

    block_data.buf[1]++;
    int kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    block_data.buf[kv_last_pos] = key_len;
    memcpy(block_data.buf + kv_last_pos + 1, key, key_len);
    block_data.buf[kv_last_pos + key_len + 1] = value_len;
    memcpy(block_data.buf + kv_last_pos + key_len + 2, value, value_len);
    linex::setInt(kvIdx, kv_last_pos);
}

void linex::recursiveUpdate(linex_block *node, int pos, const char *key,
        int key_len, const char *value, int value_len, int lastSearchPos[],
        linex_block *block_paths[], int level) {
    int idx = lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (node->isFull(key_len + value_len)) {
            printf("Full\n");
            linex_block *new_block = new linex_block();
            if (!node->isLeaf())
                new_block->setLeaf(false);
            int halfKVLen = BLK_SIZE - node->getKVLastPos() + 1;
            halfKVLen /= 2;
            int kvLen = 0;
            byte *kvIdx = node->block_data.buf + BLK_HDR_SIZE;
            int breakIdx;
            for (breakIdx = node->filledSize() - 1; breakIdx >= 0; breakIdx++) {
                int keyLen = node->block_data.buf[getInt(kvIdx + breakIdx * 2)];
                keyLen++;
                int valueLen = node->block_data.buf[getInt(kvIdx + breakIdx * 2)
                        + keyLen];
                valueLen++;
                kvLen += keyLen;
                kvLen += valueLen;
                if (kvLen > halfKVLen) {
                    new_block->setFilledSize(node->filledSize() - breakIdx);
                    break;
                } else {
                    memcpy(
                            new_block->block_data.buf
                                    + new_block->getKVLastPos() - kvLen,
                            node->block_data.buf + getInt(kvIdx + breakIdx * 2),
                            kvLen);
                    node->block_data.buf[getInt(kvIdx + breakIdx * 2)] =
                            ~node->block_data.buf[getInt(kvIdx + breakIdx * 2)];
                    setInt(new_block->block_data.buf + 2,
                            new_block->getKVLastPos() - kvLen);
                }
            }
            int newLen = 0;
            for (int i = node->getKVLastPos(); i < BLK_SIZE;) {
                int keyLen = node->block_data.buf[i];
                int kvLen = 0;
                if (keyLen < 0) {
                    keyLen = ~keyLen;
                    kvLen = keyLen;
                    kvLen++;
                    kvLen += node->block_data.buf[i];
                    kvLen++;
                    memmove(node->block_data.buf + i,
                            node->block_data.buf + i + kvLen, kvLen);
                } else {
                    kvLen = keyLen;
                    kvLen++;
                    kvLen += node->block_data.buf[i];
                    kvLen++;
                    newLen += kvLen;
                }
                i += kvLen;
            }
            byte *kv_last_idx = node->block_data.buf + node->getKVLastPos();
            memmove(kv_last_idx + newLen, kv_last_idx, newLen);
            node->setKVLastPos(node->getKVLastPos() + newLen);

            if (root == node) {
                root = new linex_block();
                int len;
                byte *key = node->getKey(0, &len);
                //root->addData(0, key, len, (const char *) &node, sizeof(char *));
                key = new_block->getKey(0, &len);
                //root->addData(1, key, len, (const char *) &new_block, sizeof(char *));
                root->setLeaf(false);
                numLevels++;
            } else {
                int prev_level = level - 1;
                linex_block *parent = block_paths[prev_level];
                int first_key_len;
                byte *new_block_first_key = new_block->getKey(0,
                        &first_key_len);
                //recursiveUpdate(parent, ~(lastSearchPos[prev_level] + 1),
                //        new_block_first_key, first_key_len, (char *) &new_block,
                //        sizeof(char *), lastSearchPos, block_paths, prev_level);
            }
            if (idx > breakIdx) {
                node = new_block;
                idx -= breakIdx;
            }

        }
        node->addData(idx, key, key_len, value, value_len);
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
    memset(block_data.buf, '\0', sizeof(block_data.buf));
    setLeaf(1);
    setFilledSize(0);
    setKVLastPos(sizeof(block_data.buf));
}

bool linex_block::isLeaf() {
    return (block_data.buf[0] == 1);
}

void linex_block::setLeaf(char isLeaf) {
    block_data.buf[0] = isLeaf;
}
void linex_block::setFilledSize(char filledSize) {
    block_data.buf[1] = filledSize;
}

bool linex_block::isFull(int kv_len) {
    kv_len += 4; // one int pointer, 1 byte key len, 1 byte value len
    int spaceLeft = sizeof(block_data.buf);
    spaceLeft -= (filledSize() * 2);
    spaceLeft -= BLK_HDR_SIZE;
    if (spaceLeft < kv_len)
        return true;
    return false;
}

int linex_block::filledSize() {
    return block_data.buf[1];
}

linex_block *linex_block::getChild(int pos) {
    byte *kvIdx = block_data.buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = block_data.buf + linex::getInt(kvIdx);
    byte *idx = kvIdx;
    idx += kvIdx[pos];
    idx += 2;
    return (linex_block *) *idx;
}

byte *linex_block::getKey(int pos, int *plen) {
    byte *kvIdx = block_data.buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = block_data.buf + linex::getInt(kvIdx);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *linex_block::getData(int pos, int *plen) {
    byte *kvIdx = block_data.buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    kvIdx = block_data.buf + linex::getInt(kvIdx);
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
