#ifndef STAGER_H
#define STAGER_H

#include <sys/stat.h>
#include <iostream>

#include "bfos.h"
#include "basix.h"
#include "basix3.h"

//#define STAGING_BLOCK_SIZE 524288
#define STAGING_BLOCK_SIZE 262144
//#define STAGING_BLOCK_SIZE 32768
#define BUCKET_BLOCK_SIZE 4096

#define BUCKET_COUNT 2

#define IDX1_SIZE_LIMIT_MB 1024

typedef vector<basix *> cache_more;

class stager {
    protected:
      basix3 *idx0;
      basix *idx1;
      cache_more idx1_more;
#if BUCKET_COUNT == 2
      basix *idx2;
#endif
      bool is_cache0_full;
      int cache0_size;
      int cache0_page_count;
      int cache1_size;
      int cache2_size;
      int cache_more_size;
      string idx1_name;

      int *flush_counts;
      long no_of_inserts;
      int zero_count;

    public:
        stager(const char *fname, size_t cache_size_mb) {
            char fname0[strlen(fname) + 5];
            char fname1[strlen(fname) + 5];
            strcpy(fname0, fname);
            strcpy(fname1, fname);
            strcat(fname0, ".ix0");
            strcat(fname1, ".ix1");
            idx1_name = fname1;
            cache0_size = cache_size_mb > 0xFFFF ? cache_size_mb & 0xFFFF : cache_size_mb;
            cache1_size = cache_size_mb > 0xFFFF ? cache_size_mb >> 16 : cache_size_mb;
            cache_more_size = (cache_size_mb > 0xFFFF ? cache_size_mb >> 16 : cache_size_mb) / 4;
            idx0 = new basix3(STAGING_BLOCK_SIZE, STAGING_BLOCK_SIZE, cache0_size, fname0);
            idx1 = new basix(BUCKET_BLOCK_SIZE, BUCKET_BLOCK_SIZE, cache1_size, fname1);
            bool more_files = true;
            size_t more_count = 0;
            while (more_files) {
                char new_name[idx1_name.length() + 10];
                sprintf(new_name, "%s.%lu", idx1_name.c_str(), more_count + 1);
                if (file_exists(new_name))
                    idx1_more.push_back(new basix(BUCKET_BLOCK_SIZE, BUCKET_BLOCK_SIZE, cache_more_size, new_name));
                else
                    break;
                more_files = false;
                more_count++;
            }
#if BUCKET_COUNT == 2
            char fname2[strlen(fname) + 5];
            strcpy(fname2, fname);
            strcat(fname2, ".ix2");
            cache2_size = (cache_size_mb > 0xFFFF ? cache_size_mb >> 16 : cache_size_mb) / 4;
            idx2 = new basix(BUCKET_BLOCK_SIZE, BUCKET_BLOCK_SIZE, cache2_size, fname2);
#endif
            is_cache0_full = false;
            cache0_page_count = cache0_size * 1024 * 1024 / STAGING_BLOCK_SIZE;
            flush_counts = new int[cache0_page_count];
            memset(flush_counts, '\0', sizeof(int) * cache0_page_count);
            no_of_inserts = 0;
            zero_count = cache0_page_count;
        }

        ~stager() {
            delete idx0;
            delete idx1;
#if BUCKET_COUNT == 2
            delete idx2;
#endif
            for (cache_more::iterator it = idx1_more.begin(); it != idx1_more.end(); it++)
                delete *it;
            delete flush_counts;
        }

        bool file_exists (char *filename) {
          struct stat   buffer;   
          return (stat (filename, &buffer) == 0);
        }

        void spawn_more_idx1_if_full() {
            if (idx1->cache->file_page_count * BUCKET_BLOCK_SIZE == IDX1_SIZE_LIMIT_MB * 1024 * 1024) {
                delete idx1;
                char new_name[idx1_name.length() + 10];
                sprintf(new_name, "%s.%lu", idx1_name.c_str(), idx1_more.size() + 1);
                if (rename(idx1_name.c_str(), new_name))
                    cout << "Error renaming file from: " << idx1_name << " to: " << new_name << endl;
                else {
                    idx1_more.push_back(new basix(BUCKET_BLOCK_SIZE, BUCKET_BLOCK_SIZE, cache_more_size, new_name));
                    idx1 = new basix(BUCKET_BLOCK_SIZE, BUCKET_BLOCK_SIZE, cache1_size, idx1_name.c_str());
                }
            }
        }

        char *put(const char *key, uint8_t key_len, const char *value,
                int16_t value_len, int16_t *pValueLen = NULL) {
            return (char *) put((const uint8_t *) key, key_len, (const uint8_t *) value, value_len, pValueLen);
        }

        uint8_t *put(const uint8_t *key, uint8_t key_len, const uint8_t *value,
                int16_t value_len, int16_t *pValueLen = NULL) {
            // no_of_inserts++;
            // if (no_of_inserts % 10000000 == 0) {
            //     for (int i = 0; i < cache0_page_count; i++)
            //         printf("%2x", flush_counts[i]);
            //     cout << endl;
            //     memset(flush_counts, '\0', sizeof(int) * cache0_page_count);
            //     zero_count = cache0_page_count;
            // }
            if (idx0->cache->cache_size_in_pages <= idx0->cache->file_page_count) {
                is_cache0_full = true;
            }
            uint8_t *val = idx0->get(key, key_len, pValueLen);
            if (val == NULL) {
#if BUCKET_COUNT == 2
                val = idx2->get(key, key_len, pValueLen);
                if (val != NULL && idx2->isChanged())
                    return val;
                uint8_t start_count = (val == NULL ? 1 : 2);
#else
                uint8_t start_count = 1;
#endif
                if (val == NULL) {
                    val = idx1->get(key, key_len, pValueLen);
                    if (val != NULL && idx1->isChanged()) {
                        //cout << "Found in idx1 " << endl;
                        return val;
                    }
                    for (cache_more::iterator it = idx1_more.begin(); it != idx1_more.end(); it++) {
                        val = (*it)->get(key, key_len, pValueLen);
                        if (val != NULL)
                            break;
                    }
                    if (val == NULL && idx1->isChanged()) {
                        idx1->put(key, key_len, value, value_len);
                        spawn_more_idx1_if_full();
                        return NULL;
                    }
                }
                int new_val_len = (val == NULL ? value_len : *pValueLen);
                uint8_t new_val[new_val_len + 1];
                memcpy(new_val, val == NULL ? value : val, new_val_len);
                new_val[new_val_len] = start_count;
                new_val_len++;
                bool is_full = idx0->isFull(0);
                if (is_full && is_cache0_full) {
                    int target_size = idx0->filledSize() / 3;
                    int cur_count = 1;
                    int next_min = 255;
                    while (idx0->filledSize() > target_size) {
                        for (int i = 0; i < idx0->filledSize(); i++) {
                            uint32_t src_idx = idx0->getPtr(i);
                            int16_t k_len = idx0->current_block[src_idx];
                            uint8_t *k = idx0->current_block + src_idx + 1;
                            int16_t v_len = idx0->current_block[src_idx + k_len + 1];
                            uint8_t *v = idx0->current_block + src_idx + k_len + 2;
                            if (v_len != value_len + 1)
                                cout << "src_idx: " << src_idx << ", vlen: " << v_len << " ";
                            int entry_count = v[v_len - 1];
                            if (v_len != value_len + 1) {
                                cout << entry_count << endl;
                                printf("k: %.*s, len: %d\n", k_len, k, k_len);
                            }
                            if (entry_count <= cur_count) {
                                int16_t v1_len = 0;
                                uint8_t *v1;
#if BUCKET_COUNT == 2
                                if (entry_count <= 1) {
                                    v1 = idx1->put(k, k_len, v, v_len - 1, &v1_len);
                                    spawn_more_idx1_if_full();
                                } else
                                    v1 = idx2->put(k, k_len, v, v_len - 1, &v1_len);
#else
                                v1 = idx1->put(k, k_len, v, v_len - 1, &v1_len);
                                spawn_more_idx1_if_full();
#endif
                                if (v1 != NULL)
                                    memcpy(v1, v, v_len - 1);
                                idx0->remove_entry(i);
                                i--;
                            } else {
                                if (entry_count < next_min)
                                    next_min = entry_count;
                                if (entry_count > 2)
                                    v[v_len - 1]--;
                            }
                            //if (idx0->filledSize() <= target_size)
                            //    break;
                        }
                        cur_count = (cur_count == next_min ? 255 : next_min);
                        //cout << "next_min: " << next_min << endl;
                    }
                    //if (idx0->filledSize() > 0)
                    //    cout << "Not emptied" << endl;
                    idx0->makeSpace();
                    // if (idx0->current_block != idx0->root_block) {
                    //     int idx = (idx0->current_block - idx0->cache->page_cache)/STAGING_BLOCK_SIZE;
                    //     if (idx < cache0_page_count && flush_counts[idx] == 0 && zero_count)
                    //         zero_count--;
                    //     if (flush_counts[idx] < 255)
                    //         flush_counts[idx]++;
                    // }
                }
                val = idx0->put(key, key_len, new_val, new_val_len);
                return val;
            }
            uint8_t entry_count = val[*pValueLen - 1];
            if (entry_count < 255)
                val[*pValueLen - 1]++;
            //cout << "Found in idx0: " << entry_count << endl;
            return val;
        }

        char *get(const char *key, uint8_t key_len, int16_t *pValueLen) {
            return (char *) get((const uint8_t *) key, key_len, pValueLen);
        }

        uint8_t *get(const uint8_t *key, uint8_t key_len, int16_t *pValueLen) {
            uint8_t *val = idx0->get(key, key_len, pValueLen);
            if (val != NULL)
                (*pValueLen)--;
#if BUCKET_COUNT == 2
            if (val == NULL)
                val = idx2->get(key, key_len, pValueLen);
#endif
            if (val == NULL)
                val = idx1->get(key, key_len, pValueLen);
            if (val == NULL) {
                for (cache_more::iterator it = idx1_more.begin(); it != idx1_more.end(); it++) {
                    val = (*it)->get(key, key_len, pValueLen);
                    if (val != NULL)
                        break;
                }
            }
            return val;
        }

        cache_stats get_cache_stats() {
            return idx1->get_cache_stats();
        }
        int get_max_key_len() {
#if BUCKET_COUNT == 2
            return max(max(idx0->get_max_key_len(), idx1->get_max_key_len()), idx2->get_max_key_len());
#else
            return max(idx0->get_max_key_len(), idx1->get_max_key_len());
#endif
        }
        int getNumLevels() {
            return idx1->getNumLevels();
        }
        void printStats(long sz) {
            idx0->printStats(idx0->size());
            idx1->printStats(idx1->size());
#if BUCKET_COUNT == 2
            idx2->printStats(idx2->size());
#endif
        }
        void printNumLevels() {
            idx0->printNumLevels();
            idx1->printNumLevels();
#if BUCKET_COUNT == 2
            idx2->printNumLevels();
#endif
        }
        long size() {
#if BUCKET_COUNT == 2
            return idx0->size() + idx1->size() + idx2->size();
#else
            return idx0->size() + idx1->size();
#endif
        }
        long filledSize() {
#if BUCKET_COUNT == 2
            return idx0->filledSize() + idx1->filledSize() + idx2->size();
#else
            return idx0->filledSize() + idx1->filledSize();
#endif
        }

};
#endif // STAGER_H
