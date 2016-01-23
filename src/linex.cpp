#include "linex.h"

void linex::put(char *key, int key_len, char *value, int value_len) {
    // isPut = true;
    if (root->filledSize() == 0) {
        root->addData(key, key_len, value, value_len);
        total_size++;
    } else {
        linex_block *foundNode;
        int lastSearchPos[numLevels];
        int pos = recursiveSearch(key, key_len, root, foundNode, lastSearchPos);
        recursiveUpdate(foundNode, pos, key, key_len, value, value_len,
                lastSearchPos, numLevels - 1);
    }
    // isPut = false;
}

int linex::recursiveSearch(char *key, int key_len, linex_block *node,
        linex_block *foundNode, int lastSearchPos[]) {
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
                foundNode = node;
                return lastSearchPos[level];
            }
        }
        node = node->getChild(lastSearchPos[level]);
        level++;
    }
    foundNode = node;
    return node->binarySearch(key, key_len);
}

int linex::get(char *key, int key_len, char *value) {
    linex_block *foundNode;
    int lastSearchPos[numLevels];
    int pos = recursiveSearch(key, key_len, root, foundNode, lastSearchPos);
    if (pos < 0)
        return 0;
    return foundNode->getData(pos, value);
}

void linex::recursiveUpdate(linex_block *node, int pos, char *key, int key_len,
        char *value, int value_len, int lastSearchPos[], int level) {
    int idx = lastSearchPos[level];
    if (idx < 0) {
        idx = ~idx;
        if (node->isFull(key_len + value_len)) {
            linex_block *newNode = new linex_block();
            Object[] newArray;
            if (node->isLeaf) {
                newnode->data = new Comparable[mSize];
                newnode->first = (Comparable) arr[mSizeBy4];
                newArray = newnode->data;
            } else {
                newnode->children = new Node[mSizeBy2];
                newnode->first = (Comparable)((Node) arr[mSizeBy4]).first;
                newnode->isLeaf = false;
                newArray = newnode->children;
            }
            newnode->parent = node->parent;
            System.arraycopy(arr, mSizeBy4, newArray, 0, mSizeBy4);
            if (node->isLeaf)
                System.arraycopy(arr, mSizeBy2Plus4, newArray, mSizeBy2,
                        mSizeBy4);
            else {
                for (int i = 0; i < mSizeBy4; i++) {
                    Node child = (Node) newArray[i];
                    child.parent = newNode;
                }
            }
            newnode->filledSize() = node->filledSize() = mSizeBy4Minus1;
            if (node->parent == null) {
                node->parent = new linex_block();
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
        }
        int idxPlus1 = idx + 1;
        int idxPlusSizeBy2 = idx + mSizeBy2;
        if (idx <= node->filledSize())
            System.arraycopy(arr, idx, arr, idxPlus1,
                    node->filledSize() - idx + 1);
        arr[idx] = key;
        if (node->isLeaf) {
            if (idx <= node->filledSize())
                System.arraycopy(arr, idxPlusSizeBy2, arr, idxPlusSizeBy2 + 1,
                        node->filledSize() - idx + 1);
            arr[idxPlusSizeBy2] = value;
            size++;
        }
        if (idx == 0) {
            if (node->isLeaf)
                node->first = (Comparable) arr[0];
            else
                node->first = (Comparable)((Node) arr[0]).first;
            updateParentsRecursively(node, node->first);
        }
        node->filledSize()++;}
else    {
        if (node->isLeaf) {
            int vIdx = idx + mSizeBy2;
            returnValue = (V) arr[vIdx];
            arr[vIdx] = value;
        }
    }
    return returnValue;
}

private void updateParentsRecursively(Node node, Comparable first) {
    if (node == null)
    return;
    Node parent = node->parent;
    if (parent != null && parent.children[0].equals(node)) {
        parent.first = first;
        updateParentsRecursively(parent, first);
    }
}

static int[] roots;
static int[] left;
static int[] ryte;
static int[] parent;
static byte[] bit_count = new byte[256];
static {
    if (left == null) {
        GenTree.TREE_SIZE = mSizeBy2;
        GenTree.generateLists();
        left = new int[GenTree.TREE_SIZE];
        ryte = new int[GenTree.TREE_SIZE];
        roots = new int[GenTree.TREE_SIZE];
        for (int i = 0; i < GenTree.TREE_SIZE; i++) {
            left[i] = GenTree.lstLeft.get(i);
            ryte[i] = GenTree.lstRyte.get(i);
            roots[i] = GenTree.lstRoots.get(i);
        }
    }
    for (int i = 0; i < 256; i++) {
        bit_count[i] = countSetBits(i);
    }
}

int linex_block::binarySearchLeaf(Object key, int key_len) {
    int middle, cmp, filledSize();
    filledSize() = node->filledSize();
    middle = roots[filledSize()];
    do {
        cmp = node->data[middle].compareTo(key);
        if (cmp < 0) {
            middle = ryte[middle];
            while (middle > filledSize())
                middle = left[middle];
        } else if (cmp > 0)
            middle = left[middle];
        else
            return middle;
    } while (middle >= 0);
    return middle;
}

int linex_block::binarySearchNode(Object key, int key_len) {
    int middle, cmp, filledSize();
    filledSize() = node->filledSize();
    middle = roots[filledSize()];
    do {
        Node child = node->children[middle];
        cmp = child.first.compareTo(key);
        if (cmp > 0)
            middle = left[middle];
        else if (cmp < 0) {
            Node nextChild = null;
            if (node->filledSize() > middle) {
                nextChild = node->children[middle + 1];
                cmp = nextChild.first.compareTo(key);
                if (cmp > 0)
                    return middle;
                else if (cmp < 0) {
                    middle = ryte[middle];
                    while (middle > filledSize())
                        middle = left[middle];
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
    root = new linex_block();
    total_size = 0;
    numLevels = 1;
}

linex_block::linex_block() {
    memset(block_data.buf, '\0', sizeof(block_data.buf));
    block_data.hdr.isLeaf = 1;
    block_data.hdr.filledSize = 0;
    block_data.hdr.kv_last_pos = sizeof(block_data.buf);
    block_data.hdr.parent = null;
}

bool linex_block::isLeaf() {
    return (block_data.hdr.isLeaf == 1);
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

void linex_block::addData(char *key, int key_len, char *value, int value_len) {
}

int linex_block::getData(int pos, char *data) {
}
