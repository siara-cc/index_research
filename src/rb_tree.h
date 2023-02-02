#ifndef RB_TREE_H
#define RB_TREE_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "GenTree.h"
#include "bplus_tree_handler.h"

using namespace std;

#if RB_TREE_NODE_SIZE == 512
#define RB_TREE_HDR_SIZE 7
#define DATA_END_POS 2
#define ROOT_NODE_POS 4
#else
#define RB_TREE_HDR_SIZE 8
#define DATA_END_POS 3
#define ROOT_NODE_POS 5
#endif

#define RB_RED 0
#define RB_BLACK 1

#if RB_TREE_NODE_SIZE == 512
#define RBT_BITMAP_POS 0
#define LEFT_PTR_POS 1
#define RYTE_PTR_POS 2
#define PARENT_PTR_POS 3
#define KEY_LEN_POS 4
#else
#define COLOR_POS 0
#define LEFT_PTR_POS 1
#define RYTE_PTR_POS 3
#define PARENT_PTR_POS 5
#define KEY_LEN_POS 7
#endif

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class rb_tree : public bplus_tree_handler<rb_tree> {
private:
    int16_t binarySearch(const uint8_t *key, int16_t key_len);
    inline int16_t getLeft(int16_t n);
    inline int16_t getRight(int16_t n);
    inline int16_t getParent(int16_t n);
    int16_t getSibling(int16_t n);
    inline int16_t getUncle(int16_t n);
    inline int16_t getGrandParent(int16_t n);
    inline int16_t getRoot();
    inline int16_t getColor(int16_t n);
    inline void setLeft(int16_t n, int16_t l);
    inline void setRight(int16_t n, int16_t r);
    inline void setParent(int16_t n, int16_t p);
    inline void setRoot(int16_t n);
    inline void setColor(int16_t n, uint8_t c);
    void rotateLeft(int16_t n);
    void rotateRight(int16_t n);
    void replaceNode(int16_t oldn, int16_t newn);
    void insertCase1(int16_t n);
    void insertCase2(int16_t n);
    void insertCase3(int16_t n);
    void insertCase4(int16_t n);
    void insertCase5(int16_t n);
public:
    int16_t pos;
    uint8_t last_direction;

    rb_tree(uint32_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
            uint32_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
            const char *fname = NULL) :
       bplus_tree_handler<rb_tree>(leaf_block_sz, parent_block_sz, cache_sz, fname) {
        GenTree::generateLists();
        initBuf();
    }

    rb_tree(uint32_t block_sz, uint8_t *block) :
      bplus_tree_handler<rb_tree>(block_sz, block) {
        init_stats();
    }

    int16_t getDataEndPos();
    void setDataEndPos(int16_t pos);
    int16_t filledUpto();
    void setFilledUpto(int16_t filledUpto);
    int16_t getPtr(int16_t pos);
    void setPtr(int16_t pos, int16_t ptr);
    bool isFull(int16_t search_result);
    void setCurrentBlockRoot();
    void setCurrentBlock(uint8_t *m);
    int16_t searchCurrentBlock();
    void addData(int16_t search_result);
    void addFirstData();
    uint8_t *getKey(int16_t pos, int16_t *plen);
    uint8_t *getFirstKey(int16_t *plen);
    int16_t getFirst();
    int16_t getNext(int16_t n);
    int16_t getPrevious(int16_t n);
    uint8_t *split(uint8_t *first_key, int16_t *first_len_ptr);
    void initVars();
    uint8_t *getChildPtrPos(int16_t search_result);
    uint8_t *getValueAt(int16_t *vlen);
    using bplus_tree_handler::getChildPtr;
    uint8_t *getChildPtr(int16_t pos);
    uint8_t *getPtrPos();
    int getHeaderSize();
    void initBuf();
};

#endif
