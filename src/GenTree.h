#ifndef GENTREE_H
#define GENTREE_H
#include "util.h"

#define TREE_SIZE 2048

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

    static int simulateBinarySearch(int idx) {
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
                int root = (1 << nxt);
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
            simulateBinarySearch(i);
        }
    }

    static void generateBitCounts() {
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
