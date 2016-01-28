#include "linex.h"

void linex::put(char *key, int key_len, char *value, int value_len) {
    // isPut = true;
    int lastSearchPos[numLevels];
    if (root->filledSize() == 0) {
        root->addData(0, key, key_len, value, value_len, lastSearchPos, 0);
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

int linex::get(char *key, int key_len, char *value) {
    int lastSearchPos[numLevels];
    int pos = -1;
    linex_block *foundNode = recursiveSearch(key, key_len, root, lastSearchPos,
            &pos);
    if (pos < 0)
        return 0;
    return foundNode->getData(pos, value);
}

linex_block *linex_block::addData(int *pIdx, char *key, int key_len,
        char *value, int value_len, int lastInsertPos[], int level) {
    if (isFull(key_len + value_len)) {
        linex_block *newnode = new linex_block();
        if (!isLeaf())
            newnode->setLeaf(false);
        int halfKVLen = BLK_SIZE - block_data.hdr.kv_last_pos + 1;
        halfKVLen /= 2;
        int kvLen = 0;
        int *kvIdx = sizeof(block_data.hdr);
        for (int i = filledSize() - 1; i >= 0; i++) {
            int keyLen = block_data.buf[kvIdx[i]];
            keyLen++;
            int valueLen = block_data.buf[kvIdx[i] + keyLen];
            valueLen++;
            kvLen += keyLen;
            kvLen += valueLen;
            if (kvLen > halfKVLen) {
                newnode->block_data.hdr.filledSize = filledSize() - i;
                break;
            } else {
                memcpy(
                        newnode->block_data.buf
                                + newnode->block_data.hdr.kv_last_pos - kvLen,
                        block_data.buf + kvIdx[i], kvLen);
                block_data.buf[kvIdx[i]] = ~block_data.buf[kvIdx[i]];
                newnode->block_data.hdr.kv_last_pos -= kvLen;
            }
        }
        int newLen = 0;
        for (int i = block_data.hdr.kv_last_pos; i < BLK_SIZE;) {
            int keyLen = block_data.buf[i];
            int kvLen = 0;
            if (keyLen < 0) {
                keyLen = ~keyLen;
                kvLen = keyLen;
                kvLen++;
                kvLen += block_data.buf[i];
                kvLen++;
                memmove(block_data.buf + i, block_data.buf + i + kvLen, kvLen);
            } else {
                kvLen = keyLen;
                kvLen++;
                kvLen += block_data.buf[i];
                kvLen++;
                newLen += kvLen;
            }
            i += kvLen;
        }
        byte *kv_last_idx = block_data.buf + block_data.hdr.kv_last_pos;
        memmove(kv_last_idx + newLen, kv_last_idx, newLen);
        block_data.hdr.kv_last_pos += newLen;
        if (node->getParent() == null) {
            root = new linex_block();
            node->setParent(root);
            root = node->parent;
            root.first = node->first;
            root.children = new Node[mSizeBy2];
            root.children[0] = node;
            root.children[1] = newNode;
            root.isLeaf = false;
            root.filledSize() = 1;
            root.lastSearchPos = 0;
            newnode->parent = root;
        } else {
            Node parent = node->parent;
            if (parent.lastSearchPos == parent.filledSize())
                parent.lastSearchPos = ~(parent.filledSize() + 1);
            else
                parent.lastSearchPos = ~(parent.lastSearchPos + 1);
            recursiveUpdate(parent, newNode, null);
        }
        // System.out.println("Grow");
        if (idx > mSizeBy4Minus1) {
            node = newNode;
            arr = (node->isLeaf ? newnode->data : newnode->children);
            idx -= mSizeBy4;
            if (!node->isLeaf)
                ((Node) key).parent = newNode;
        }
        if (idx == 0) {
            if (node->isLeaf)
                node->first = (Comparable) arr[0];
            else
                node->first = (Comparable)((Node) arr[0]).first;
            updateParentsRecursively(node, node->first);
        }
    }
    int idxPlus1 = *pIdx + 1;
    if (idx <= node->filledSize())
        System.arraycopy(arr, idx, arr, idxPlus1, node->filledSize() - idx + 1);
    arr[idx] = key;
    if (node->isLeaf) {
        if (idx <= node->filledSize())
            System.arraycopy(arr, idxPlusSizeBy2, arr, idxPlusSizeBy2 + 1,
                    node->filledSize() - idx + 1);
        arr[idxPlusSizeBy2] = value;
        size++;
    }
    node->filledSize()++;}

void linex_block::updateParentsRecursively(Node node, Comparable first) {
    if (node == null)
        return;
    Node parent = node->parent;
    if (parent != null && parent.children[0].equals(node)) {
        parent.first = first;
        updateParentsRecursively(parent, first);
    }
}

void linex::recursiveUpdate(linex_block *node, int pos, char *key, int key_len,
        char *value, int value_len, int lastSearchPos[], int level) {
    int idx = lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        node = node->addData(&idx, key, key_len, value, value_len);
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

int linex_block::getData(int pos, char *data) {
}
