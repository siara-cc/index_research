#ifndef LINEX_H
#define LINEX_H

typedef unsigned char byte;
#define null 0
#define BLK_SIZE 512
#define TREE_SIZE 128
#define MAX_DATA_LEN 127

class linex_block {
private:
    union {
        byte buf[BLK_SIZE];
        struct {
            byte isLeaf;
            byte filledSize;
            int kv_last_pos;
        } hdr;
    } block_data;
    int binarySearchLeaf(char *key, int key_len);
    int binarySearchNode(char *key, int key_len);
    void updateParentsRecursively(Node node, Comparable first);
public:
    linex_block();
    bool isLeaf();
    void setLeaf(byte isLeaf);
    bool isFull(int kv_len);
    linex_block *getParent();
    void setParent(linex_block *p);
    int filledSize();
    linex_block *getChild(int pos);
    int binarySearch(char *key, int key_len);
    linex_block *addData(int *pIdx, char *key, int key_len, char *value,
            int value_len, int lastSearchPos[], int level);
    int getData(int pos, char *data);
};

class linex {
private:
    linex_block *root;
    long total_size;
    int numLevels;
    linex_block *recursiveSearch(char *key, int key_len, linex_block *node,
            int lastSearchPos[], int *pIdx);
    void recursiveUpdate(linex_block *foundNode, int pos, char *key,
            int key_len, char *value, int value_len, int lastSearchPos[],
            int level);
public:
    linex();
    long size();
    int get(char *key, int key_len, char *value);
    void put(char *key, int key_len, char *value, int value_len);
};

class GenTree {
public:
    static byte bit_count[256];
    static int *roots;
    static int *left;
    static int *ryte;
    static int *parent;
    static int ixRoots;
    static int ixLeft;
    static int ixRyte;
    static int ixPrnt;

    static int binarySearch(int idx) {
        int first, last;
        int middle, prevMiddle;
        last = TREE_SIZE - 1;
        first = 0;
        middle = (last - first) / 2;
        prevMiddle = middle;
        while (first <= last) {
            if (idx == middle) {
                int oldLast = last;
                parent[ixPrnt++] = prevMiddle;
                last = middle - 1;
                if (first <= last)
                    left[ixLeft++] = (first + (last - first) / 2);
                else
                    left[ixLeft++] = ~idx;
                last = oldLast;
                first = middle + 1;
                if (first <= last)
                    ryte[ixRyte++] = (first + (last - first) / 2);
                else
                    ryte[ixRyte++] = ~(idx + 1);
                return middle;
            }
            if (idx > middle)
                first = middle + 1;
            else
                last = middle - 1;
            prevMiddle = middle;
            middle = first + (last - first) / 2;
        }
        return ~middle;
    }

    static void generateLists() {
        ixRoots = 0;
        ixLeft = 0;
        ixRyte = 0;
        ixPrnt = 0;
        roots = new int[TREE_SIZE];
        left = new int[TREE_SIZE];
        ryte = new int[TREE_SIZE];
        parent = new int[TREE_SIZE];
        int j = 0;
        int nxt = 1;
        for (int i = 0; i < TREE_SIZE; i++) {
            if (i == 0)
                roots[ixRoots++] = 0;
            else {
                int root = 2 ^ nxt;
                root--;
                if (i + 1 == TREE_SIZE) {
                    roots[ixRoots] = roots[ixRoots - 1];
                    ixRoots++;
                } else
                    roots[ixRoots++] = root;
                j++;
                if (j > root) {
                    j = 0;
                    nxt++;
                }
            }
            binarySearch(i);
        }
        for (int i = 0; i < 256; i++) {
            bit_count[i] = countSetBits(i);
        }
    }

    // Function to get no of set bits in binary
    // representation of passed binary no.
    static byte countSetBits(int n) {
        int count = 0;
        while (n > 0) {
            n &= (n - 1);
            count++;
        }
        return (byte) count;
    }

};

#endif
