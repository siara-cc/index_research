#ifndef GENTREE_H
#define GENTREE_H
#include "univix_util.h"

#define TREE_SIZE 64

class GenTree {
public:
    static int16_t *roots;
    static int16_t *left;
    static int16_t *ryte;
    static int16_t *parent;
    static int16_t ixRoots;
    static int16_t ixLeft;
    static int16_t ixRyte;
    static int16_t ixPrnt;

    static int16_t simulateBinarySearch(int16_t idx) {
        int16_t first, last;
        int16_t middle, prevMiddle;
        last = TREE_SIZE - 1;
        first = 0;
        middle = (last - first) / 2;
        prevMiddle = middle;
        while (first <= last) {
            if (idx == middle) {
                int16_t oldLast = last;
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
        roots = (int16_t *) util::alignedAlloc(TREE_SIZE * 2);
        left = (int16_t *) util::alignedAlloc(TREE_SIZE * 2);
        ryte = (int16_t *) util::alignedAlloc(TREE_SIZE * 2);
        parent = (int16_t *) util::alignedAlloc(TREE_SIZE * 2);
//        roots = new int16_t[TREE_SIZE];
//        left = new int16_t[TREE_SIZE];
//        ryte = new int16_t[TREE_SIZE];
//        parent = new int16_t[TREE_SIZE];
        int16_t j = 0;
        int16_t nxt = 1;
        for (int16_t i = 0; i < TREE_SIZE; i++) {
            if (i == 0)
                roots[ixRoots++] = 0;
            else {
                int16_t root = (1 << nxt);
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

};

#endif
