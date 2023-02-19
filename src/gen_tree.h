#ifndef GENTREE_H
#define GENTREE_H
#include "univix_util.h"

#define TREE_SIZE 64

class gen_tree {
public:
    static int16_t *roots;
    static int16_t *left;
    static int16_t *ryte;
    static int16_t *parent;
    static int16_t ix_roots;
    static int16_t ix_left;
    static int16_t ix_ryte;
    static int16_t ix_prnt;

    static int16_t simulate_binary_search(int16_t idx) {
        int16_t first, last;
        int16_t middle, prev_middle;
        last = TREE_SIZE - 1;
        first = 0;
        middle = (last - first) / 2;
        prev_middle = middle;
        while (first <= last) {
            if (idx == middle) {
                int16_t old_last = last;
                parent[ix_prnt++] = prev_middle;
                last = middle - 1;
                if (first <= last)
                    left[ix_left++] = (first + (last - first) / 2);
                else
                    left[ix_left++] = ~idx;
                last = old_last;
                first = middle + 1;
                if (first <= last)
                    ryte[ix_ryte++] = (first + (last - first) / 2);
                else
                    ryte[ix_ryte++] = ~(idx + 1);
                return middle;
            }
            if (idx > middle)
                first = middle + 1;
            else
                last = middle - 1;
            prev_middle = middle;
            middle = first + (last - first) / 2;
        }
        return ~middle;
    }

    static void generate_lists() {
        ix_roots = 0;
        ix_left = 0;
        ix_ryte = 0;
        ix_prnt = 0;
        roots = (int16_t *) util::aligned_alloc(TREE_SIZE * 2);
        left = (int16_t *) util::aligned_alloc(TREE_SIZE * 2);
        ryte = (int16_t *) util::aligned_alloc(TREE_SIZE * 2);
        parent = (int16_t *) util::aligned_alloc(TREE_SIZE * 2);
//        roots = new int16_t[TREE_SIZE];
//        left = new int16_t[TREE_SIZE];
//        ryte = new int16_t[TREE_SIZE];
//        parent = new int16_t[TREE_SIZE];
        int16_t j = 0;
        int16_t nxt = 1;
        for (int16_t i = 0; i < TREE_SIZE; i++) {
            if (i == 0)
                roots[ix_roots++] = 0;
            else {
                int16_t root = (1 << nxt);
                root--;
                if (i + 1 == TREE_SIZE) {
                    roots[ix_roots] = roots[ix_roots - 1];
                    ix_roots++;
                } else
                    roots[ix_roots++] = root;
                j++;
                if (j > root) {
                    j = 0;
                    nxt++;
                }
            }
            simulate_binary_search(i);
        }
    }

};

#endif
