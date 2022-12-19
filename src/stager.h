#ifndef STAGER_H
#define STAGER_H

#include <iostream>
#include "basix.h"
#include "bfos.h"

class stager {
    protected:
      basix *idx0;
      basix *idx1;
      bool is_cache0_full;

    public:
        stager(const char *fname, size_t cache_size) {
            char fname0[strlen(fname) + 5];
            char fname1[strlen(fname) + 5];
            strcpy(fname0, fname);
            strcpy(fname1, fname);
            strcat(fname0, ".ix0");
            strcat(fname1, ".ix1");
            int cache0_size = cache_size;
            int cache1_size = cache_size * 4;
            idx0 = new basix(32768, 32768, cache0_size, fname0);
            idx1 = new basix(4096, 4096, cache1_size, fname1);
            is_cache0_full = false;
        }

        ~stager() {
            delete idx0;
            delete idx1;
        }

        uint8_t *put(const char *key, uint8_t key_len, const char *value,
                int16_t value_len, int16_t *pValueLen = NULL) {
            return put((const uint8_t *) key, key_len, (const uint8_t *) value, value_len, pValueLen);
        }

        uint8_t *put(const uint8_t *key, uint8_t key_len, const uint8_t *value,
                int16_t value_len, int16_t *pValueLen = NULL) {
            uint8_t *val = idx0->get(key, key_len, pValueLen);
            if (val == NULL) {
                val = idx1->get(key, key_len, pValueLen);
                int new_val_len = (val == NULL ? value_len : *pValueLen) + 1;
                uint8_t new_val[new_val_len];
                if (val == NULL)
                    memcpy(new_val, value, value_len);
                else
                    memcpy(new_val, val, new_val_len);
                new_val[new_val_len - 1] = 1;
                if (idx0->isFull(-1) && is_cache0_full) {
                    int target_size = idx0->filledSize() / 3;
                    int cur_count = 1;
                    int next_min = 255;
                    while (idx0->filledSize() > target_size) {
                        for (int i = 0; i < idx0->filledSize(); i++) {
                            uint16_t src_idx = idx0->getPtr(i);
                            int16_t k_len = idx0->current_block[src_idx];
                            uint8_t *k = idx0->current_block + src_idx + 1;
                            int16_t v_len = idx0->current_block[src_idx + k_len + 1];
                            uint8_t *v = idx0->current_block + src_idx + k_len + 2;
                            if (v_len != 5)
                                cout << "src_idx: " << src_idx << ", vlen: " << v_len << " ";
                            int entry_count = v[v_len - 1];
                            if (v_len != 5) {
                                cout << entry_count << endl;
                                printf("k: %.*s, len: %d\n", k_len, k, k_len);
                            }
                            if (entry_count <= cur_count) {
                                idx1->put(k, k_len, v, v_len - 1, NULL);
                                //cout << "put success" << endl;
                                idx0->remove_entry(i);
                                i--;
                            } else {
                                if (entry_count < next_min)
                                    next_min = entry_count;
                            }
                        }
                        cur_count = (cur_count == next_min ? 255 : next_min);
                        //cout << "next_min: " << next_min << endl;
                    }
                }
                val = idx0->put(key, key_len, new_val, new_val_len);
                if (idx0->cache->cache_size_in_pages <= idx0->cache->file_page_count) {
                    is_cache0_full = true;
                }
                return val;
            }
            uint8_t entry_count = val[*pValueLen - 1];
            if (entry_count < 255)
                val[*pValueLen - 1]++;
            return val;
        }

        uint8_t *get(const char *key, uint8_t key_len, int16_t *pValueLen) {
            return get((const uint8_t *) key, key_len, pValueLen);
        }

        uint8_t *get(const uint8_t *key, uint8_t key_len, int16_t *pValueLen) {
            uint8_t *val = idx0->get(key, key_len, pValueLen);
            if (val == NULL)
                return idx1->get(key, key_len, pValueLen);
            return val;
        }

        cache_stats get_cache_stats() {
            return idx1->get_cache_stats();
        }
        int get_max_key_len() {
            return max(idx0->get_max_key_len(), idx1->get_max_key_len());
        }
        int getNumLevels() {
            return idx1->getNumLevels();
        }
        void printStats(long sz) {
            idx0->printStats(idx0->size());
            idx1->printStats(idx1->size());
        }
        void printNumLevels() {
            idx0->printNumLevels();
            idx1->printNumLevels();
        }
        long size() {
            return idx0->size() + idx1->size();
        }

};
#endif // STAGER_H
