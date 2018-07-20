#ifndef LINEX_H
#define LINEX_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "GenTree.h"
#include "bplus_tree_handler.h"

using namespace std;

#define LINEX_NODE_SIZE 768

#define LX_BLK_HDR_SIZE 6
#define LX_PREFIX_CODING 1
#define LX_DATA_AREA 0

class linex : public bplus_tree_handler {
private:
    int16_t linearSearch();
public:
    int16_t pos;
    byte *prev_key_at;
    byte prefix[60];
    int16_t prefix_len;
    int16_t prev_prefix_len;
    inline int16_t searchCurrentBlock();
    void addData(int16_t idx);
    bool isFull(int16_t kv_lens);
    byte *split(byte *first_key, int16_t *first_len_ptr);
    int16_t traverseToLeaf(byte *node_paths[] = null);
    void insertCurrent();
    inline char *getValueAt(int16_t *vlen);
    inline byte *getChildPtr(byte *ptr);
    inline byte *getChildPtrPos(int16_t idx);
    inline byte *getPtrPos();
    inline int getHeaderSize();
};

#endif
