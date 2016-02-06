#include "linex.h"

void linex::put(char *key, int key_len, char *value, int value_len) {
    // isPut = true;
    int lastSearchPos[numLevels];
    if (root->filledSize() == 0) {
        root->addData(0, key, key_len, value, value_len);
        total_size++;
    } else {
        int pos = 0;
        linex_block *foundNode = recursiveSearch(key, key_len, root,
                lastSearchPos, &pos);
        recursiveUpdate(foundNode, pos, key, key_len, value, value_len,
                lastSearchPos, numLevels - 1);
    }
    // isPut = false;
}

linex_block *linex::recursiveSearch(char *key, int key_len, linex_block *node,
        int lastSearchPos[], int *pIdx) {
    int level = 0;
    while (!node->isLeaf()) {
        lastSearchPos[level] = node->binarySearch(key, key_len);
        if (lastSearchPos[level] < 0) {
            lastSearchPos[level] = ~lastSearchPos[level];
            if (lastSearchPos[level] >= node->filledSize()) {
                lastSearchPos[level] -= node->filledSize();
                //lastSearchPos--; // required?
                do {
                    node = node->getChild(lastSearchPos[level]);
                    level++;
                    lastSearchPos[level] = 0;
                } while (!node->isLeaf());
                *pIdx = lastSearchPos[level];
                return node;
            }
        }
        node = node->getChild(lastSearchPos[level]);
        level++;
    }
    *pIdx = node->binarySearch(key, key_len);
    return node;
}

char *linex::get(char *key, int key_len, int *pValueLen) {
    int lastSearchPos[numLevels];
    int pos = -1;
    linex_block *foundNode = recursiveSearch(key, key_len, root, lastSearchPos,
            &pos);
    if (pos < 0)
        return null;
    return foundNode->getData(pos, pValueLen);
}

void linex_block::addData(int idx, char *key, int key_len, char *value,
        int value_len) {
    int *kvIdx = sizeof(block_data.hdr);
    if (idx < filledSize())
        memmove(kvIdx+1, kvIdx, sizeof(int));
    block_data.hdr.filledSize++;
    kvIdx[idx] = block_data.hdr.kv_last_pos - key_len - value_len - 2;
}

void linex::recursiveUpdate(linex_block *node, int pos, char *key, int key_len,
        char *value, int value_len, int lastSearchPos[], int level) {
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
            int *kvIdx = sizeof(node->block_data.hdr);
            int breakIdx;
            for (breakIdx = filledSize() - 1; breakIdx >= 0; breakIdx++) {
                int keyLen = node->block_data.buf[kvIdx[breakIdx]];
                keyLen++;
                int valueLen = node->block_data.buf[kvIdx[breakIdx] + keyLen];
                valueLen++;
                kvLen += keyLen;
                kvLen += valueLen;
                if (kvLen > halfKVLen) {
                    new_block->block_data.hdr.filledSize = filledSize()
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
                Node parent = node->parent;
                if (parent.lastSearchPos == parent.filledSize())
                    parent.lastSearchPos = ~(parent.filledSize() + 1);
                else
                    parent.lastSearchPos = ~(parent.lastSearchPos + 1);
                recursiveUpdate(parent, newNode, null);
            }
            if (idx > breakIdx) {
                node = new_block;
                idx -= breakIdx;
            }

        }
        node->addData(&idx, key, key_len, value, value_len);
    } else {
        if (node->isLeaf) {
            int vIdx = idx + mSizeBy2;
            returnValue = (V) arr[vIdx];
            arr[vIdx] = value;
        }
    }
    return returnValue;
}

int linex_block::binarySearchLeaf(Object key, int key_len) {
    int middle, cmp, filledSize();
    filledSize() = node->filledSize();
    middle = GenTree::roots[filledSize()];
    do {
        cmp = node->data[middle].compareTo(key);
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
    middle = GenTree::roots[filledSize];
    do {
        char *key_middle = getKey[middle];
        cmp = mem.compareTo(key);
        if (cmp > 0)
            middle = GenTree::left[middle];
        else if (cmp < 0) {
            Node nextChild = null;
            if (filledSize > middle) {
                nextChild = node->children[middle + 1];
                cmp = nextChild.first.compareTo(key);
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
    spaceLeft -= (filledSize * 2);
    spaceLeft -= sizeof(block_data.hdr);
    if (spaceLeft > kv_len)
        return true;
    return false;
}

int linex_block::filledSize() {
    return block_data.hdr.filledSize;
}

linex_block *linex_block::getChild(int pos) {
}

char *linex_block::getKey(int pos, int *plen) {
    int *kvIdx = sizeof(block_data.hdr);
    *plen = kvIdx[pos];
    return kvIdx+pos+1;
}

char *linex_block::getData(int pos, int *plen) {
    int *kvIdx = sizeof(block_data.hdr);
    kvIdx += kvIdx[pos];
    kvIdx++;
    *plen = kvIdx[pos];
    return kvIdx+pos+1;
}
