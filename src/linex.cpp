#include "linex.h"

char *linex::get(const char *key, int16_t key_len, int16_t *pValueLen) {
    linex_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    if (!node.isLeaf())
        node.traverseToLeaf();
    if (node.locate() < 0)
        return null;
    char * ret = node.getValueAt(pValueLen);
    return ret;
}

void linex_node_handler::traverseToLeaf(byte *node_paths[]) {
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
            key_at = prev_key_at;
        }
        setBuf(getChildPtr(key_at));
        if (isPut)
            node_paths[level++] = buf;
    }
}

void linex::put(const char *key, int16_t key_len, const char *value,
        int16_t value_len) {
    byte *node_paths[7];
    linex_node_handler node(root_data);
    node.key = key;
    node.key_len = key_len;
    node.value = value;
    node.value_len = value_len;
    node.isPut = true;
    if (node.filledSize() == 0) {
        node.addData();
        total_size++;
    } else {
        if (!node.isLeaf())
            node.traverseToLeaf(node_paths);
        node.locate();
        recursiveUpdate(&node, -1, node_paths, numLevels - 1);
    }
}

void linex::recursiveUpdate(linex_node_handler *node, int16_t pos,
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
            linex_node_handler new_block(b);
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
                root_data = (byte *) util::alignedAlloc(LINEX_NODE_SIZE);
                linex_node_handler root(root_data);
                root.initBuf();
                root.isPut = true;
                root.setLeaf(0);
                byte addr[9];
                root.initVars();
                root.key = "";
                root.key_len = 1;
                root.value = (char *) addr;
                root.value_len = util::ptrToBytes((unsigned long) node->buf, addr);
                root.addData();
                root.initVars();
                root.key = (char *) first_key;
                root.key_len = first_len;
                root.value = (char *) addr;
                root.value_len = util::ptrToBytes((unsigned long) new_block.buf, addr);
                root.pos = ~root.locate();
                root.addData();
                numLevels++;
            } else {
                int16_t prev_level = level - 1;
                byte *parent_data = node_paths[prev_level];
                linex_node_handler parent(parent_data);
                byte addr[9];
                parent.initVars();
                parent.isPut = true;
                parent.key = (char *) first_key;
                parent.key_len = first_len;
                parent.value = (char *) addr;
                parent.value_len = util::ptrToBytes((unsigned long) new_block.buf, addr);
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

void linex_node_handler::addData() {
    int16_t prev_plen = 0;
    int16_t filled_size = filledSize();
    setFilledSize(filled_size + 1);
#if LX_PREFIX_CODING == 1
    if (filled_size && key_at != prev_key_at) {
        while (prev_plen < key_len) {
            if (prev_plen < prev_prefix_len) {
                if (prefix[prev_plen] != key[prev_plen])
                    break;
            } else {
                if (prev_key_at[prev_plen - prev_prefix_len + 2]
                        != key[prev_plen])
                    break;
            }
            prev_plen++;
        }
    }
#endif
    int16_t kv_last_pos = getKVLastPos();
    int16_t key_left = key_len - prev_plen;
    int16_t tot_len = key_left + value_len + 3;
    int16_t len_to_move = kv_last_pos - (key_at - buf);
    if (len_to_move) {
#if LX_PREFIX_CODING == 1
        int16_t next_plen = prefix_len;
        while (key_at[next_plen - prefix_len + 2] == key[next_plen]
                && next_plen < key_len)
            next_plen++;
        int16_t diff = next_plen - prefix_len;
        if (diff) {
            int16_t next_key_len = key_at[1];
            int16_t next_value_len = key_at[next_key_len + 2];
            *key_at = next_plen;
            key_at[1] -= diff;
            memmove(key_at + 2, key_at + diff + 2,
                    next_key_len + next_value_len + 1 - diff);
            memmove(key_at + diff, key_at,
                    next_key_len + next_value_len + 3 - diff);
            tot_len -= diff;
        }
#endif
        memmove(key_at + tot_len, key_at, len_to_move);
    }
    *key_at++ = prev_plen;
    *key_at++ = key_left;
    memcpy(key_at, key + prev_plen, key_left);
    key_at += key_left;
    *key_at++ = value_len;
    memcpy(key_at, value, value_len);
    kv_last_pos += tot_len;
    setKVLastPos(kv_last_pos);
}

byte *linex_node_handler::split(byte *first_key, int16_t *first_len_ptr) {
    int16_t filled_size = filledSize();
    byte *b = (byte *) util::alignedAlloc(LINEX_NODE_SIZE);
    linex_node_handler new_block(b);
    new_block.initBuf();
    new_block.isPut = true;
    if (!isLeaf())
        new_block.setLeaf(false);
    int16_t kv_last_pos = getKVLastPos();
    int16_t halfKVLen = kv_last_pos / 2;

    int16_t brk_idx = -1;
    int16_t brk_kv_pos;
    int16_t tot_len;
    brk_kv_pos = tot_len = 0;
    // Copy all data to new block in ascending order
    int16_t new_idx;
    int16_t src_idx = LX_BLK_HDR_SIZE;
    int16_t dst_idx = src_idx;
    prefix_len = 0;
    prev_key_at = buf + src_idx;
    for (new_idx = 0; new_idx < filled_size; new_idx++) {
        for (int16_t pctr = 0; prefix_len < buf[src_idx]; pctr++)
            prefix[prefix_len++] = prev_key_at[pctr + 2];
        if (prefix_len > buf[src_idx])
            prefix_len = buf[src_idx];
        src_idx++;
        int16_t kv_len = buf[src_idx];
        kv_len++;
        kv_len += buf[src_idx + kv_len];
        kv_len++;
        tot_len += kv_len;
        if (brk_idx == new_idx) {
            new_block.buf[dst_idx++] = 0;
            new_block.buf[dst_idx++] = buf[src_idx] + prefix_len;
            memcpy(new_block.buf + dst_idx, prefix, prefix_len);
            dst_idx += prefix_len;
            memcpy(new_block.buf + dst_idx, buf + src_idx + 1, kv_len - 1);
            dst_idx += kv_len;
            dst_idx--;
#if LX_PREFIX_CODING == 0
            if (isLeaf()) {
                prefix_len = 0;
                for (int i = 0; buf[src_idx + i + 1] == prev_key_at[i + 2]; i++) {
                    prefix[i] = prev_key_at[i + 2];
                    prefix_len++;
                }
            }
#endif
            memcpy(first_key, prefix, prefix_len);
            if (isLeaf()) {
#if LX_PREFIX_CODING == 0
                first_key[prefix_len] = buf[src_idx + prefix_len + 1];
#else
                first_key[prefix_len] = buf[src_idx + 1];
#endif
                *first_len_ptr = prefix_len + 1;
            } else {
                *first_len_ptr = buf[src_idx];
                memcpy(first_key + prefix_len, buf + src_idx + 1,
                        *first_len_ptr);
                *first_len_ptr += prefix_len;
            }
            prefix_len = 0;
        } else {
            new_block.buf[dst_idx++] = prefix_len;
            memcpy(new_block.buf + dst_idx, buf + src_idx, kv_len);
            dst_idx += kv_len;
        }
        if (brk_idx == -1) {
            if (tot_len > halfKVLen || new_idx == (filled_size / 2)) {
                brk_idx = new_idx + 1;
                brk_kv_pos = dst_idx;
            }
        }
        prev_key_at = buf + src_idx - 1;
        src_idx += kv_len;
        kv_last_pos = dst_idx;
    }

    int16_t old_blk_new_len = brk_kv_pos - LX_BLK_HDR_SIZE;
    memcpy(buf + LX_BLK_HDR_SIZE, new_block.buf + LX_BLK_HDR_SIZE,
            old_blk_new_len); // Copy back first half to old block
    setKVLastPos(LX_BLK_HDR_SIZE + old_blk_new_len);
    setFilledSize(brk_idx); // Set filled upto for old block

    int16_t new_size = filled_size - brk_idx;
    // Move index of second half to first half in new block
    byte *new_kv_idx = new_block.buf + brk_kv_pos;
    memmove(new_block.buf + LX_BLK_HDR_SIZE, new_kv_idx,
            kv_last_pos - brk_kv_pos); // Move first block to front
    // Set KV Last pos for new block
    new_block.setKVLastPos(LX_BLK_HDR_SIZE + kv_last_pos - brk_kv_pos);
    new_block.setFilledSize(new_size); // Set filled upto for new block

    return b;

}

int16_t linex_node_handler::linearSearch() {
    int16_t idx = 0;
    int16_t filled_size = filledSize();
    key_at = buf + LX_BLK_HDR_SIZE;
    prev_key_at = key_at;
    prefix_len = prev_prefix_len = 0;
    while (idx < filled_size) {
        int16_t cmp;
        key_at_len = *(key_at + 1);
#if LX_PREFIX_CODING == 1
        byte *p_cur_prefix_len = key_at;
        for (int16_t pctr = 0; prefix_len < (*p_cur_prefix_len & 0x7F); pctr++)
            prefix[prefix_len++] = prev_key_at[pctr + 2];
        if (prefix_len > *key_at)
            prefix_len = *key_at;
        cmp = memcmp(prefix, key, prefix_len);
        if (cmp == 0) {
            cmp = util::compare((char *) key_at + 2, key_at_len,
                    key + prefix_len, key_len - prefix_len);
        }
#if LX_DATA_AREA == 1
        if (cmp == 0) {
            int16_t partial_key_len = prefix_len + key_at_len;
            byte *data_key_at = buf + key_at_len + *key_at + 1;
            if (*p_cur_prefix_len & 0x80)
                data_key_at += 256;
            cmp = util::compare((char *) data_key_at + 1, *data_key_at,
                    key + partial_key_len, key_len - partial_key_len);
        }
#endif
#else
        cmp = util::compare((char *) key_at + 2, key_at_len, key, key_len);
#endif
        if (cmp > 0)
            return ~idx;
        else if (cmp == 0)
            return idx;
        prev_key_at = key_at;
        prev_prefix_len = prefix_len;
        key_at += key_at_len;
        key_at += 2;
        key_at += *key_at;
        key_at++;
#if LX_DATA_AREA == 1
        key_at++;
#endif
        idx++;
    }
    return ~idx;
}

int16_t linex_node_handler::locate() {
    pos = linearSearch();
    return pos;
}

linex::~linex() {
    delete root_data;
}

linex::linex() {
    GenTree::generateLists();
    root_data = (byte *) util::alignedAlloc(LINEX_NODE_SIZE);
    linex_node_handler root(root_data);
    root.initBuf();
    total_size = maxKeyCountLeaf = maxKeyCountNode = 0;
    numLevels = blockCountLeaf = blockCountNode = 1;
}

linex_node_handler::linex_node_handler(byte *b) {
    setBuf(b);
    isPut = false;
}

void linex_node_handler::setBuf(byte *b) {
    buf = b;
    key_at = buf + LX_BLK_HDR_SIZE;
    prefix_len = 0;
}

void linex_node_handler::initBuf() {
    //memset(buf, '\0', LINEX_NODE_SIZE);
    setLeaf(1);
    setFilledSize(0);
    setKVLastPos(LX_BLK_HDR_SIZE);
    prefix_len = 0;
}

bool linex_node_handler::isFull(int16_t kv_len) {
    if ((getKVLastPos() + kv_len + 3) >= LINEX_NODE_SIZE)
        return true;
    return false;
}

byte *linex_node_handler::getChildPtr(byte *ptr) {
    ptr++;
    ptr += (*ptr + 1);
    return (byte *) util::bytesToPtr(ptr);
}

char *linex_node_handler::getValueAt(int16_t *vlen) {
    key_at++;
    key_at += *key_at;
    key_at++;
    *vlen = *key_at;
    key_at++;
    return (char *) key_at;
}

void linex_node_handler::initVars() {
}
