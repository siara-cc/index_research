#include "linex.h"
#include <cstring>

void linex::put(char *key, int key_len, char *value, int value_len) {
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

linex_block *linex::recursiveSearch(char *key, int key_len, linex_block *node,
        int lastSearchPos[], linex_block *block_paths[], int *pIdx) {
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

char *linex::get(char *key, int key_len, int *pValueLen) {
    int lastSearchPos[numLevels];
    linex_block *block_paths[numLevels];
    int pos = -1;
    linex_block *foundNode = recursiveSearch(key, key_len, root, lastSearchPos,
            block_paths, &pos);
    if (pos < 0)
        return null;
    return foundNode->getData(pos, pValueLen);
}

void linex_block::addData(int idx, char *key, int key_len, char *value,
        int value_len) {
    int *kvIdx = (int *) (&block_data.hdr + sizeof(block_data.hdr));
    if (idx < filledSize())
        memmove(kvIdx + 1, kvIdx, sizeof(int));
    block_data.hdr.filledSize++;
    kvIdx[idx] = block_data.hdr.kv_last_pos - key_len - value_len - 2;
}

void linex::recursiveUpdate(linex_block *node, int pos, char *key, int key_len,
        char *value, int value_len, int lastSearchPos[],
        linex_block *block_paths[], int level) {
    int idx = lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (node->isFull(key_len + value_len)) {
            linex_block *new_block = new linex_block();
            if (!node->isLeaf())
                new_block->setLeaf(false);
            int halfKVLen = BLK_SIZE - node->block_data.hdr.kv_last_pos + 1;
            halfKVLen /= 2;
            int kvLen = 0;
            int *kvIdx = (int *) (&node->block_data.hdr
                    + sizeof(node->block_data.hdr));
            int breakIdx;
            for (breakIdx = node->filledSize() - 1; breakIdx >= 0; breakIdx++) {
                int keyLen = node->block_data.buf[kvIdx[breakIdx]];
                keyLen++;
                int valueLen = node->block_data.buf[kvIdx[breakIdx] + keyLen];
                valueLen++;
                kvLen += keyLen;
                kvLen += valueLen;
                if (kvLen > halfKVLen) {
                    new_block->block_data.hdr.filledSize = node->filledSize()
                            - breakIdx;
                    break;
                } else {
                    memcpy(
                            new_block->block_data.buf
                                    + new_block->block_data.hdr.kv_last_pos
                                    - kvLen,
                            node->block_data.buf + kvIdx[breakIdx], kvLen);
                    node->block_data.buf[kvIdx[breakIdx]] =
                            ~node->block_data.buf[kvIdx[breakIdx]];
                    new_block->block_data.hdr.kv_last_pos -= kvLen;
                }
            }
            int newLen = 0;
            for (int i = node->block_data.hdr.kv_last_pos; i < BLK_SIZE;) {
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
            byte *kv_last_idx = node->block_data.buf
                    + node->block_data.hdr.kv_last_pos;
            memmove(kv_last_idx + newLen, kv_last_idx, newLen);
            node->block_data.hdr.kv_last_pos += newLen;

            if (root == node) {
                root = new linex_block();
                int len;
                char *key = node->getKey(0, &len);
                root->addData(0, key, len, (char *) &node, sizeof(char *));
                key = new_block->getKey(0, &len);
                root->addData(1, key, len, (char *) &new_block, sizeof(char *));
                root->setLeaf(false);
            } else {
                int prev_level = level - 1;
                linex_block *parent = block_paths[prev_level];
                int first_key_len;
                char *new_block_first_key = new_block->getKey(0,
                        &first_key_len);
                recursiveUpdate(parent, ~(lastSearchPos[prev_level] + 1),
                        new_block_first_key, first_key_len, (char *) &new_block,
                        sizeof(char *), lastSearchPos, block_paths, prev_level);
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

int linex_block::compare(char *v1, int len1, char *v2, int len2) {
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

int linex_block::binarySearchLeaf(char *key, int key_len) {
    int middle, cmp;
    middle = GenTree::roots[filledSize() - 1];
    do {
        int middle_key_len;
        char *middle_key = getKey(middle, &middle_key_len);
        cmp = compare(middle_key, middle_key_len, key, key_len);
        if (cmp < 0) {
            middle = GenTree::ryte[middle];
            while (middle > filledSize())
                middle = GenTree::left[middle];
        } else if (cmp > 0)
            middle = GenTree::left[middle];
        else
            return middle;
    } while (middle >= 0);
    return middle;
}

int linex_block::binarySearchNode(char *key, int key_len) {
    int middle, cmp;
    middle = GenTree::roots[filledSize()];
    do {
        int middle_key_len;
        char *middle_key = getKey(middle, &middle_key_len);
        cmp = compare(middle_key, middle_key_len, key, key_len);
        if (cmp > 0)
            middle = GenTree::left[middle];
        else if (cmp < 0) {
            if (filledSize() > (middle + 1)) {
                int plus1_key_len;
                char *plus1_key = getKey(middle + 1, &plus1_key_len);
                cmp = compare(plus1_key, plus1_key_len, key, key_len);
                if (cmp > 0)
                    return middle;
                else if (cmp < 0) {
                    middle = GenTree::ryte[middle];
                    while (middle > filledSize())
                        middle = GenTree::left[middle];
                } else
                    return ~(middle + 2 + filledSize());
            } else
                return middle;
        } else
            return ~(middle + filledSize() + 1);
    } while (middle >= 0);
    return middle;
}

int linex_block::binarySearch(char *key, int key_len) {
    if (block_data.hdr.isLeaf)
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
    block_data.hdr.isLeaf = 1;
    block_data.hdr.filledSize = 0;
    block_data.hdr.kv_last_pos = sizeof(block_data.buf);
}

bool linex_block::isLeaf() {
    return (block_data.hdr.isLeaf == 1);
}

void linex_block::setLeaf(byte isLeaf) {
    block_data.hdr.isLeaf = isLeaf;
}

bool linex_block::isFull(int kv_len) {
    kv_len += 4; // one int pointer, 1 byte key len, 1 byte value len
    int spaceLeft = sizeof(block_data.buf);
    spaceLeft -= (filledSize() * 2);
    spaceLeft -= sizeof(block_data.hdr);
    if (spaceLeft > kv_len)
        return true;
    return false;
}

int linex_block::filledSize() {
    return block_data.hdr.filledSize;
}

linex_block *linex_block::getChild(int pos) {
    int *kvIdx = (int *) (&block_data.hdr + sizeof(block_data.hdr));
    char *idx = (char *) kvIdx;
    idx += kvIdx[pos];
    idx += 2;
    return (linex_block *) idx;
}

char *linex_block::getKey(int pos, int *plen) {
    int *kvIdx = (int *) (&block_data.hdr + sizeof(block_data.hdr));
    *plen = kvIdx[pos];
    char *ret = (char *) kvIdx;
    return ret + pos + 1;
}

char *linex_block::getData(int pos, int *plen) {
    int *kvIdx = (int *) (&block_data.hdr + sizeof(block_data.hdr));
    char *idx = (char *) kvIdx;
    idx += kvIdx[pos];
    idx++;
    *plen = idx[pos];
    return (char *) (idx + 1);
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
