#ifndef LRUCACHE_H
#define LRUCACHE_H
#include <unordered_map>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "univix_util.h"

typedef unsigned char byte;

using namespace std;

typedef struct dbl_lnklst_st {
    int disk_page;
    int cache_loc;
    struct dbl_lnklst_st *prev;
    struct dbl_lnklst_st *next;
} dbl_lnklst;

class lru_cache {
protected:
    int page_size;
    byte *page_cache;
    byte *root_block;
    int cache_size_in_pages;
    int cache_occupied_size;
    dbl_lnklst *lnklst_first_entry;
    dbl_lnklst *lnklst_last_entry;
    unordered_map<int, dbl_lnklst*> disk_to_cache_map;
    const char *filename;
    FILE *fp;
    int file_page_count;
    byte empty;
public:
    lru_cache(int pg_size, int page_count, const char *fname) {
        page_size = pg_size;
        cache_size_in_pages = page_count;
        cache_occupied_size = 0;
        lnklst_first_entry = lnklst_last_entry = NULL;
        filename = fname;
        page_cache = (byte *) util::alignedAlloc(pg_size * page_count);
        root_block = (byte *) util::alignedAlloc(pg_size);
        file_page_count = 0;
        fp = fopen(fname, "rb+");
        if (fp == NULL) {
            fp = fopen(fname, "wb");
            fclose(fp);
            fp = fopen(fname, "rb+");
            if (fp == NULL)
              cout << "Error opening file" << endl;
        }
        if (!fseek(fp, 0, SEEK_END)) {
            file_page_count = ftell(fp);
            if (file_page_count > 0)
                file_page_count /= page_size;
            fseek(fp, 0, SEEK_SET);
        }
        empty = 0;
        if (fread(root_block, 1, page_size, fp) != page_size) {
            file_page_count = 1;
            fseek(fp, 0, SEEK_SET);
            fwrite(root_block, 1, page_size, fp);
            empty = 1;
        }
    }
    ~lru_cache() {
        for (unordered_map<int, dbl_lnklst*>::iterator it = disk_to_cache_map.begin(); it != disk_to_cache_map.end(); it++) {
            fseek(fp, page_size * it->first, SEEK_SET);
            int write_count = fwrite(&page_cache[page_size * it->second->cache_loc], 1, page_size, fp);
            if (write_count != page_size)
                cout << "0:Only " << write_count << " bytes written at position: " << page_size * it->first << endl;
            delete disk_to_cache_map[it->first];
        }
        free(page_cache);
        fseek(fp, 0, SEEK_SET);
        int write_count = fwrite(root_block, 1, page_size, fp);
        if (write_count != page_size)
            cout << "1:Only " << write_count << " bytes written at position: 0" << endl;
        free(root_block);
    }
    byte *get_disk_page_in_cache(int disk_page) {
        if (disk_page == 0)
            return root_block;
        int cache_pos = 0;
        int removed_disk_page = 0;
        if (disk_to_cache_map.find(disk_page) == disk_to_cache_map.end()) {
            if (cache_occupied_size < cache_size_in_pages) {
                dbl_lnklst *new_entry = new dbl_lnklst();
                new_entry->disk_page = disk_page;
                new_entry->cache_loc = cache_occupied_size;
                new_entry->prev = lnklst_last_entry;
                new_entry->next = NULL;
                if (lnklst_last_entry == NULL)
                    lnklst_last_entry = new_entry;
                else
                    lnklst_last_entry->next = new_entry;
                if (lnklst_first_entry == NULL)
                    lnklst_first_entry = new_entry;
                disk_to_cache_map[disk_page] = new_entry;
                cache_pos = cache_occupied_size++;
                if (!fseek(fp, page_size * disk_page, SEEK_SET)) {
                    int read_count = fread(&page_cache[page_size * cache_pos], 1, page_size, fp);
                    if (read_count != page_size)
                        cout << "2:Only " << read_count << " bytes read at position: " << page_size * disk_page << endl;
                }
            } else {
                dbl_lnklst *entry_to_move = lnklst_last_entry;
                removed_disk_page = lnklst_last_entry->disk_page;
                cache_pos = entry_to_move->cache_loc;
                fseek(fp, page_size * removed_disk_page, SEEK_SET);
                int write_count = fwrite(&page_cache[page_size * cache_pos], 1, page_size, fp);
                if (write_count != page_size)
                    cout << "3:Only " << write_count << "bytes written at position: " << page_size * removed_disk_page << endl;
                entry_to_move->next = lnklst_first_entry;
                entry_to_move->prev = NULL;
                entry_to_move->disk_page = disk_page;
                lnklst_first_entry->prev = entry_to_move;
                lnklst_first_entry = entry_to_move;
                lnklst_last_entry = lnklst_last_entry->prev;
                lnklst_last_entry->next = NULL;
                disk_to_cache_map.erase(removed_disk_page);
                disk_to_cache_map[disk_page] = entry_to_move;
                if (!fseek(fp, page_size * disk_page, SEEK_SET)) {
                    int read_count = fread(&page_cache[page_size * cache_pos], 1, page_size, fp);
                    if (read_count != page_size)
                        cout << "4:Only " << read_count << " bytes read at position: " << page_size * disk_page << endl;
                }
            }
        } else {
            dbl_lnklst *current_entry = disk_to_cache_map[disk_page];
            cache_pos = current_entry->cache_loc;
            if (lnklst_first_entry != current_entry) {
                current_entry->prev->next = current_entry->next;
                if (current_entry->next != NULL)
                    current_entry->next->prev = current_entry->prev;
                current_entry->prev = NULL;
                current_entry->next = lnklst_first_entry;
                lnklst_first_entry->prev = current_entry;
                lnklst_first_entry = current_entry;
            }
        }
        return &page_cache[page_size * cache_pos];
    }
    byte *writeNewPage() {
        byte *new_page = get_disk_page_in_cache(file_page_count);
        if (fseek(fp, file_page_count * page_size, SEEK_SET))
            fseek(fp, 0, SEEK_END);
        int write_count = fwrite(new_page, 1, page_size, fp);
        if (write_count != page_size)
            cout << "5:Only " << write_count << "bytes written at position: " << file_page_count << endl;
        file_page_count++;
        return new_page;
    }
    int get_page_count() {
        return file_page_count;
    }
    byte is_empty() {
        return empty;
    }
};
#endif
