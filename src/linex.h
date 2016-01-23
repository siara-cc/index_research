#ifndef LINEX_H
#define LINEX_H

typedef unsigned char byte;
#define null 0
#define BLK_SIZE 512
class linex_block {
private:
    union {
        byte buf[BLK_SIZE];
        struct {
            byte isLeaf;
            byte filledSize;
            int kv_last_pos;
            linex_block *parent;
        } hdr;
    } block_data;
    int binarySearchLeaf(char *key, int key_len);
    int binarySearchNode(char *key, int key_len);
public:
    linex_block();
    bool isLeaf();
    bool isFull(int kv_len);
    int filledSize();
    linex_block *getChild(int pos);
    int binarySearch(char *key, int key_len);
    void addData(char *key, int key_len, char *value, int value_len);
    int getData(int pos, char *data);
};

class linex {
private:
    linex_block *root;
    long total_size;
    int numLevels;
    int recursiveSearch(char *key, int key_len, linex_block *node,
            linex_block *foundNode, int lastSearchPos[]);
    void recursiveUpdate(linex_block *foundNode, int pos, char *key,
            int key_len, char *value, int value_len, int lastSearchPos[],
            int level);
public:
    linex();
    long size();
    int get(char *key, int key_len, char *value);
    void put(char *key, int key_len, char *value, int value_len);
};

#endif
