#include "basix.h"

char *basix::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    basix_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    if (!node.isLeaf())
        node.traverseToLeaf();
    if (node.locate() < 0)
        return null;
    char * ret = node.getValueAt(pValueLen);
    return ret;
}

void basix_node_handler::traverseToLeaf(byte *node_paths[]) {
    byte level;
    level = 1;
    if (isPut)
        *node_paths = buf;
    while (!isLeaf()) {
        int16_t idx = locate();
        if (idx < 0) {
            idx = ~idx;
            if (idx)
                idx--;
        }
        key_at = buf + getPtr(idx);
        setBuf(getChildPtr(key_at));
        if (isPut)
            node_paths[level++] = buf;
    }
}

void basix::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    byte *node_paths[7];
    basix_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    node.value = value;
    node.value_len = value_len;
    node.isPut = true;
    if (node.filledUpto() == -1) {
        node.pos = 0;
        node.addData();
        total_size++;
    } else {
        if (!node.isLeaf())
            node.traverseToLeaf(node_paths);
        node.locate();
        recursiveUpdate(&node, node.pos, node_paths, numLevels - 1);
    }
}

void basix::recursiveUpdate(basix_node_handler *node, int16_t pos,
        byte *node_paths[], int16_t level) {
    int16_t idx = pos;
    if (idx < 0) {
        idx = ~idx;
        if (node->isFull(node->key_len + node->value_len)) {
            //std::cout << "Full\n" << std::endl;
            //if (maxKeyCount < block->filledSize())
            //    maxKeyCount = block->filledSize();
            //printf("%d\t%d\t%d\n", block->isLeaf(), block->filledSize(), block->TRIE_LEN);
            //cout << (int) node->TRIE_LEN << endl;
            if (node->isLeaf()) {
                maxKeyCountLeaf += node->filledSize();
                blockCountLeaf++;
            } else {
                maxKeyCountNode += node->filledSize();
                blockCountNode++;
            }
            //    maxKeyCount += node->TRIE_LEN;
            //maxKeyCount += node->PREFIX_LEN;
            byte first_key[64];
            int16_t first_len;
            byte *b = node->split(first_key, &first_len);
            basix_node_handler new_block(b);
            new_block.isPut = true;
            int16_t cmp = util::compare((char *) first_key, first_len,
                    node->key, node->key_len);
            if (cmp <= 0) {
                new_block.initVars();
                new_block.key = node->key;
                new_block.key_len = node->key_len;
                new_block.value = node->value;
                new_block.value_len = node->value_len;
                new_block.pos = ~new_block.locate();
                new_block.addData();
            } else {
                node->initVars();
                node->pos = ~node->locate();
                node->addData();
            }
            if (root_data == node->buf) {
                blockCountNode++;
                root_data = (byte *) util::alignedAlloc(node_size);
                basix_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                root.setLeaf(0);
                byte addr[9];
                util::ptrToBytes((unsigned long) node->buf, addr);
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = (char *) addr;
#if defined(ENV64BIT)
                root.value_len = 5;
#else
                root.value_len = 4;
#endif
                root.pos = 0;
                root.addData();
                util::ptrToBytes((unsigned long) new_block.buf, addr);
                root.initVars();
                root.key = (char *) first_key;
                root.key_len = first_len;
                root.value = (char *) addr;
                //root.value_len = sizeof(char *);
                root.pos = ~root.locate();
                root.addData();
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                basix_node_handler parent(parent_data);
                byte addr[9];
                util::ptrToBytes((unsigned long) new_block.buf, addr);
                parent.initVars();
                parent.isPut = true;
                parent.key = (char *) first_key;
                parent.key_len = first_len;
                parent.value = (char *) addr;
#if defined(ENV64BIT)
                parent.value_len = 5;
#else
                parent.value_len = 4;
#endif
                parent.locate();
                recursiveUpdate(&parent, parent.pos, node_paths, prev_level);
            }
        } else {
            node->pos = idx;
            node->addData();
        }
    } else {
        //if (node->isLeaf) {
        //    int16_t vIdx = idx + mSizeBy2;
        //    returnValue = (V) arr[vIdx];
        //    arr[vIdx] = value;
        //}
    }
}

void basix_node_handler::insBit(uint32_t *ui32, int pos, int16_t kv_pos) {
    uint32_t ryte_part = (*ui32) & GenTree::ryte_mask32[pos];
    ryte_part >>= 1;
    if (kv_pos >= 256)
        ryte_part |= GenTree::mask32[pos];
    (*ui32) = (ryte_part | ((*ui32) & GenTree::left_mask32[pos]));

}

void basix_node_handler::insBit(uint64_t *ui64, int pos, int16_t kv_pos) {
    uint64_t ryte_part = (*ui64) & GenTree::ryte_mask64[pos];
    ryte_part >>= 1;
    if (kv_pos >= 256)
        ryte_part |= GenTree::mask64[pos];
    (*ui64) = (ryte_part | ((*ui64) & GenTree::left_mask64[pos]));

}

void basix_node_handler::insPtr(int16_t pos, int16_t kv_last_pos) {
#if BX_9_BIT_PTR == 1
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    int16_t filled_upto = filledUpto();
    filled_upto++;
    if (pos < filled_upto)
    memmove(kvIdx + 1, kvIdx, filled_upto - pos);
    kvIdx[0] = kv_last_pos;
#if BX_INT64MAP == 1
    insBit(bitmap, pos, kv_last_pos);
#else
    if (pos & 0xFFE0) {
        insBit(bitmap2, pos - 32, kv_last_pos);
    } else {
        byte last_bit = (*bitmap1 & 0x01);
        insBit(bitmap1, pos, kv_last_pos);
        *bitmap2 >>= 1;
        if (last_bit)
        *bitmap2 |= *GenTree::mask32;
    }
#endif
#else
    byte *kvIdx = buf + BLK_HDR_SIZE;
    kvIdx += pos;
    kvIdx += pos;
    int16_t filled_upto = filledUpto();
    filled_upto++;
    if (pos < filled_upto)
        memmove(kvIdx + 2, kvIdx, (filled_upto - pos) * 2);
    util::setInt(kvIdx, kv_last_pos);
#endif
    util::setInt(buf + 1, filled_upto);

}
void basix_node_handler::addData() {

    int16_t kv_last_pos = getKVLastPos() - (key_len + value_len + 2);
    setKVLastPos(kv_last_pos);
    buf[kv_last_pos] = key_len & 0xFF;
    memcpy(buf + kv_last_pos + 1, key, key_len);
    buf[kv_last_pos + key_len + 1] = value_len & 0xFF;
    memcpy(buf + kv_last_pos + key_len + 2, value, value_len);
    insPtr(pos, kv_last_pos);
}

byte *basix_node_handler::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t filled_upto = filledUpto();
    byte *b = (byte *) util::alignedAlloc(BASIX_NODE_SIZE);
    basix_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    if (!isLeaf())
        new_block.setLeaf(false);
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = BASIX_NODE_SIZE - kv_last_pos + 1;
    halfKVLen /= 2;

    int16_t brk_idx = -1;
    int16_t brk_kv_pos;
    int16_t tot_len;
    brk_kv_pos = tot_len = 0;
    // Copy all data to new block in ascending order
    int16_t new_idx;
    for (new_idx = 0; new_idx <= filled_upto; new_idx++) {
        int16_t src_idx = getPtr(new_idx);
        int16_t kv_len = buf[src_idx];
        kv_len++;
        kv_len += buf[src_idx + kv_len];
        kv_len++;
        tot_len += kv_len;
        memcpy(new_block.buf + kv_last_pos, buf + src_idx, kv_len);
        new_block.insPtr(new_idx, kv_last_pos);
        kv_last_pos += kv_len;
        if (brk_idx == -1) {
            if (tot_len > halfKVLen || new_idx == (filled_upto / 2)) {
                brk_idx = new_idx;
                brk_kv_pos = kv_last_pos;
                int16_t first_idx = getPtr(new_idx + 1);
                if (isLeaf()) {
                    int len = 0;
                    while (buf[first_idx + len + 1] == buf[src_idx + len + 1])
                        len++;
                    *first_len_ptr = len + 1;
                    memcpy(first_key, buf + first_idx + 1, *first_len_ptr);
                } else {
                    *first_len_ptr = buf[first_idx];
                    memcpy(first_key, buf + first_idx + 1, *first_len_ptr);
                }
            }
        }
    }
    //memset(buf + BLK_HDR_SIZE, '\0', BASIX_NODE_SIZE - BLK_HDR_SIZE);
    kv_last_pos = getKVLastPos();
    int16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
    memcpy(buf + BASIX_NODE_SIZE - old_blk_new_len, new_block.buf + kv_last_pos,
            old_blk_new_len); // Copy back first half to old block
    //memset(new_block.buf + kv_last_pos, '\0', old_blk_new_len);
    int diff = (BASIX_NODE_SIZE - brk_kv_pos);
    for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
        setPtr(new_idx, new_block.getPtr(new_idx) + diff);
    } // Set index of copied first half in old block
    setKVLastPos(getPtr(0));
    int16_t new_size = filled_upto - brk_idx;
    // Move index of second half to first half in new block
    byte *new_kv_idx = new_block.buf + BLK_HDR_SIZE;
#if BX_9_BIT_PTR == 1
    memcpy(new_kv_idx, new_kv_idx + new_idx, new_size);
#if BX_INT64MAP == 1
    (*new_block.bitmap) <<= brk_idx;
#else
    if (brk_idx & 0xFFE0)
    *new_block.bitmap1 = *new_block.bitmap2 << (brk_idx - 32);
    else {
        *new_block.bitmap1 <<= brk_idx;
        *new_block.bitmap1 |= (*new_block.bitmap2 >> (32 - brk_idx));
    }
#endif
#else
    memcpy(new_kv_idx, new_kv_idx + new_idx * 2, new_size * 2);
#endif
    //memset(new_kv_idx + new_size * 2, '\0', new_pos);
    // Set KV Last pos for new block
    new_block.setKVLastPos(new_block.getPtr(0));
    filled_upto = brk_idx;
    setFilledUpto(filled_upto); // Set filled upto for old block
    new_block.setFilledUpto(new_size - 1); // Set filled upto for new block
    return b;
}

//int16_t basix_node_handler::binarySearch(const char *key, int16_t key_len) {
//    int middle, filled_upto;
//    filled_upto = filledUpto();
//    middle = GenTree::roots[filled_upto];
//    do {
//        int16_t middle_key_len;
//        char *middle_key = (char *) getKey(middle, &middle_key_len);
//        int16_t cmp = util::compare(middle_key, middle_key_len, key, key_len);
//        if (cmp < 0) {
//            middle = GenTree::ryte[middle];
//            while (middle > filled_upto)
//                middle = GenTree::left[middle];
//        } else if (cmp > 0)
//            middle = GenTree::left[middle];
//        else
//            return middle;
//    } while (middle >= 0);
//    return middle;
//}

int16_t basix_node_handler::binarySearch(const char *key, int16_t key_len) {
    int middle, first, filled_upto;
    int16_t cmp;
    first = 0;
    filled_upto = filledUpto() + 1;
    while (first < filled_upto) {
        middle = (first + filled_upto) >> 1;
        int16_t middle_key_len;
        char *middle_key = (char *) getKey(middle, &middle_key_len);
        cmp = util::compare(middle_key, middle_key_len, key, key_len);
        if (cmp < 0)
            first = middle + 1;
        else if (cmp > 0)
            filled_upto = middle;
        else
            return middle;
    }
    return ~filled_upto;
}

// branch ffree
//int16_t basix_node_handler::binarySearchLeaf(const char *key, int16_t key_len) {
//    int middle, first, filled_upto;
//    int16_t cmp = 1;
//    first = 0;
//    filled_upto = filledUpto() + 1;
//    while (cmp && first < filled_upto) {
//        middle = (first + filled_upto) / 2;
//        int16_t middle_key_len;
//        char *middle_key = (char *) getKey(middle, &middle_key_len);
//        cmp = util::compare(middle_key, middle_key_len, key, key_len);
//        first = ((cmp < 0) ? middle + 1 : first);
//        filled_upto = ((cmp > 0) ? middle : filled_upto);
//    }
//    return (first < filled_upto) ? middle : ~filled_upto;
//}
//int16_t basix_node_handler::binarySearchLeaf(const char *key, int16_t key_len) {
//    int16_t n;
//    char cmp;
//    int16_t *base = (int16_t *) (buf + BLK_HDR_SIZE);
//    int16_t *new_base = base;
//    n = filledUpto() + 1;
//    int16_t half;
//    while (n > 1) {
//        half = n / 2;
//        char *middle_key = (char *) (buf + new_base[half]);
//        int16_t middle_key_len = *middle_key;
//        middle_key++;
//        cmp = util::compare(middle_key, middle_key_len, key, key_len);
//        new_base = ((cmp <= 0) ? &new_base[half] : new_base);
//        n -= half;
//    }
//    n = (new_base - base);
//    return (cmp == 0) ? n : ~(n+1);
//}

int16_t basix_node_handler::locate() {
    pos = binarySearch(key, key_len);
    return pos;
}

basix::~basix() {
    delete root_data;
}

basix::basix() {
    GenTree::generateLists();
    root_data = (byte *) util::alignedAlloc(BASIX_NODE_SIZE);
    basix_node_handler root(root_data);
    root.initBuf();
    total_size = maxKeyCountLeaf = maxKeyCountNode = 0;
    numLevels = blockCountLeaf = blockCountNode = 1;
}

basix_node_handler::basix_node_handler(byte *b) {
    setBuf(b);
    isPut = false;
}

void basix_node_handler::setBuf(byte *b) {
    buf = b;
#if BX_9_BIT_PTR == 1
#if BX_INT64MAP == 1
    bitmap = (uint64_t *) (buf + BITMAP_POS);
#else
    bitmap1 = (uint32_t *) (buf + BITMAP_POS);
    bitmap2 = bitmap1 + 1;
#endif
#endif
}

void basix_node_handler::initBuf() {
    //memset(buf, '\0', BASIX_NODE_SIZE);
    setLeaf(1);
    setFilledUpto(-1);
    setKVLastPos(BASIX_NODE_SIZE);
#if BX_9_BIT_PTR == 1
#if BX_INT64MAP == 1
    bitmap = (uint64_t *) (buf + BITMAP_POS);
#else
    bitmap1 = (uint32_t *) (buf + BITMAP_POS);
    bitmap2 = bitmap1 + 1;
#endif
#endif
}

void basix_node_handler::setFilledUpto(int16_t filledUpto) {
    util::setInt(buf + 1, filledUpto);
}

bool basix_node_handler::isFull(int16_t kv_len) {
    int16_t ptr_size = filledUpto() + 2;
#if BX_9_BIT_PTR == 0
    ptr_size <<= 1;
#endif
    if ((getKVLastPos() - kv_len - 2) <= (BLK_HDR_SIZE + ptr_size))
        return true;
#if BX_9_BIT_PTR == 1
    if (filledUpto() > 62)
    return true;
#endif
    return false;
}

int16_t basix_node_handler::filledUpto() {
    return util::getInt(buf + 1);
}

int16_t basix_node_handler::getPtr(int16_t pos) {
#if BX_9_BIT_PTR == 1
    int16_t ptr = buf[BLK_HDR_SIZE + pos];
#if BX_INT64MAP == 1
    if (*bitmap & GenTree::mask64[pos])
    ptr |= 256;
#else
    if (pos & 0xFFE0) {
        if (*bitmap2 & GenTree::mask32[pos - 32])
        ptr |= 256;
    } else {
        if (*bitmap1 & GenTree::mask32[pos])
        ptr |= 256;
    }
#endif
    return ptr;
#else
    byte *kvIdx = buf + BLK_HDR_SIZE + (pos << 1);
    return util::getInt(kvIdx);
#endif
}

void basix_node_handler::setPtr(int16_t pos, int16_t ptr) {
#if BX_9_BIT_PTR == 1
    buf[BLK_HDR_SIZE + pos] = ptr;
#if BX_INT64MAP == 1
    if (ptr >= 256)
    *bitmap |= GenTree::mask64[pos];
    else
    *bitmap &= ~GenTree::mask64[pos];
#else
    if (pos & 0xFFE0) {
        pos -= 32;
        if (ptr >= 256)
        *bitmap2 |= GenTree::mask32[pos];
        else
        *bitmap2 &= ~GenTree::mask32[pos];
    } else {
        if (ptr >= 256)
        *bitmap1 |= GenTree::mask32[pos];
        else
        *bitmap1 &= ~GenTree::mask32[pos];
    }
#endif
#else
    byte *kvIdx = buf + BLK_HDR_SIZE + (pos << 1);
    return util::setInt(kvIdx, ptr);
#endif
}

byte *basix_node_handler::getKey(int16_t pos, int16_t *plen) {
    byte *kvIdx = buf + getPtr(pos);
    *plen = kvIdx[0];
    kvIdx++;
    return kvIdx;
}

byte *basix_node_handler::getChildPtr(byte *ptr) {
    ptr += (*ptr + 2);
    return (byte *) util::bytesToPtr(ptr);
}

char *basix_node_handler::getValueAt(int16_t *vlen) {
    key_at = buf + getPtr(pos);
    key_at += *key_at;
    key_at++;
    *vlen = *key_at;
    key_at++;
    return (char *) key_at;
}

void basix_node_handler::initVars() {
}
