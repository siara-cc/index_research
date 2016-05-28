#ifndef dfos_H
#define dfos_H
#include <cstdio>
#include <cstring>
#include <iostream>
#include "util.h"

using namespace std;

typedef unsigned char byte;
#define DFOS_NODE_SIZE 4096
#define IDX_HDR_SIZE 5

#define INSERT_MIDDLE1 1
#define INSERT_MIDDLE2 2
#define INSERT_THREAD 3
#define INSERT_LEAF 4

#define IS_LEAF_BYTE buf[0]
#define TRIE_LEN buf[1]
#define FILLED_SIZE buf[2]
#define LAST_DATA_PTR buf+3

class dfos_node_handler {
private:
    byte *trie;
    static byte left_mask[8];
    static byte ryte_mask[8];
    static byte ryte_incl_mask[8];
    static byte pos_mask[48];
    inline void insAt(byte pos, byte b);
    inline void insAt(byte pos, int16_t i);
    inline void insAt(byte pos, byte b1, byte b2);
    inline void insAt(byte pos, byte b1, byte b2, byte b3);
    inline byte insAt(byte pos, byte tc, byte leaf, int16_t c1_kv_pos);
    inline byte insAt(byte pos, byte tc, byte child, byte leaf,
            int16_t c1_kv_pos);
    inline byte insAt(byte pos, byte tc, byte child, byte leaf,
            int16_t c1_kv_pos, int16_t c2_kv_pos, byte c2_mask);
    inline void setAt(byte pos, byte b);
    inline byte getAt(byte pos);
    inline void delAt(byte pos);
    inline void delAt(byte pos, int16_t count);
    static byte *alignedAlloc();
public:
    byte *buf;
    byte tc;
    byte mask;
    byte msb5;
    byte keyPos;
    byte children;
    byte leaves;
    byte triePos;
    byte origPos;
    byte need_count;
    byte insertState;
    byte isPut;
    const char *key;
    int16_t key_len;
    const char *key_at;
    int16_t key_at_len;
    const char *value;
    int16_t value_len;
    dfos_node_handler(byte *m);
    void initBuf();
    void initVars();
    void setBuf(byte *m);
    bool isFull(int16_t kv_lens);
    inline bool isLeaf();
    inline void setLeaf(char isLeaf);
    int16_t filledSize();
    inline void setFilledSize(int16_t filledSize);
    inline int16_t getKVLastPos();
    inline void setKVLastPos(int16_t val);
    void addData(int16_t ptr);
    byte *getChild(int16_t ptr);
    byte *getKey(int16_t ptr, int16_t *plen);
    byte *getData(int16_t ptr, int16_t *plen);
    byte *split(int16_t *pbrk_idx);
    int16_t locate(int16_t level);
    void insertCurrent(int16_t kv_pos);
};

class dfos {
private:
    long total_size;
    int numLevels;
    int maxKeyCount;
    int blockCount;
    void recursiveSearch(dfos_node_handler *node, int16_t lastSearchPos[],
            byte *node_paths[], int16_t *pIdx);
    void recursiveSearchForGet(dfos_node_handler *node, int16_t *pIdx);
    void recursiveUpdate(dfos_node_handler *node, int16_t pos,
            int16_t lastSearchPos[], byte *node_paths[], int16_t level);
public:
    byte *root_data;
    int maxThread;
    dfos();
    ~dfos();
    char *get(const char *key, int16_t key_len, int16_t *pValueLen);
    void put(const char *key, int16_t key_len, const char *value,
            int16_t value_len);
    void printMaxKeyCount(long num_entries) {
        std::cout << "Block Count:" << blockCount << std::endl;
        std::cout << "Avg Block Count:" << (num_entries / blockCount)
                << std::endl;
        std::cout << "Avg Max Count:" << (maxKeyCount / blockCount)
                << std::endl;
    }
    void printNumLevels() {
        std::cout << "Level Count:" << numLevels << std::endl;
    }
    long size() {
        return total_size;
    }

};

#endif
