#ifndef BASIX_H
#define BASIX_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

#if BPT_9_BIT_PTR == 1
#define BLK_HDR_SIZE 14
#define BITMAP_POS 6
#else
#define BLK_HDR_SIZE 6
#endif

class basix : public bplus_tree_handler {
private:
    inline int16_t binarySearch(const char *key, int16_t key_len);
public:
    int16_t pos;
    bool isFull();
    byte *split(byte *first_key, int16_t *first_len_ptr);
    inline int16_t searchCurrentBlock();
    void addData(int16_t idx);
    void addFirstData();
    void insertCurrent();
    inline byte *getChildPtrPos(int16_t idx);
    inline byte *getPtrPos();
    inline int getHeaderSize();
};

#endif
