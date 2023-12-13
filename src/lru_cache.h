#ifndef LRUCACHE_H
#define LRUCACHE_H
#include <set>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <cstring>
#include <time.h>
#include <unordered_map>

#include "common.h"

typedef struct dbl_lnklst_st {
    size_t disk_page;
    int cache_loc;
    struct dbl_lnklst_st *prev;
    struct dbl_lnklst_st *next;
} dbl_lnklst;

typedef struct {
    long total_cache_req;
    long total_cache_misses;
    long cache_flush_count;
    long pages_written;
    long pages_read;
    int last_pages_to_flush;
} cache_stats;

class chg_iface {
  public:
    virtual void set_block_changed(uint8_t *block, int block_sz, bool is_changed) = 0;
    virtual bool is_block_changed(uint8_t *block, int block_sz) = 0;
    virtual ~chg_iface() {}
};

class lru_cache {
protected:
    int page_size;
    uint8_t *root_block;
    int cache_occupied_size;
    int skip_page_count;
    dbl_lnklst *lnklst_first_entry;
    dbl_lnklst *lnklst_last_entry;
    dbl_lnklst *lnklst_last_free;
    std::unordered_map<size_t, dbl_lnklst*> disk_to_cache_map;
    size_t disk_to_cache_map_size;
    dbl_lnklst *llarr;
    std::set<size_t> new_pages;
    std::string filename;
    int fd;
    uint8_t empty;
    cache_stats stats;
    long max_pages_to_flush;
    void *(*malloc_fn)(size_t);
    void write_pages(std::set<size_t>& pages_to_write) {
        if (options != 0)
            return;
        for (std::set<size_t>::iterator it = pages_to_write.begin(); it != pages_to_write.end(); it++) {
            uint8_t *block = &page_cache[page_size * disk_to_cache_map[*it]->cache_loc];
            iface->set_block_changed(block, page_size, false);
            //if (page_size < 65537 && block[5] < 255)
            //    block[5]++;
            off_t file_pos = page_size;
            file_pos *= *it;
            common::write_bytes(fd, block, file_pos, page_size);
            stats.pages_written++;
        }
    }
    void calc_flush_count() {
        if (stats.total_cache_req == 0) {
          stats.last_pages_to_flush = max_pages_to_flush < 20 ? max_pages_to_flush : 20;
          return;
        }
        stats.last_pages_to_flush = max_pages_to_flush; //cache_size_in_pages * stats.total_cache_misses / stats.total_cache_req;
        if (stats.last_pages_to_flush > 500)
           stats.last_pages_to_flush = 500;
        if (stats.last_pages_to_flush < 2)
           stats.last_pages_to_flush = 2;
    }
    void flush_pages_in_seq(uint8_t *block_to_keep) {
        if (options != 0)
            return;
        if (lnklst_last_entry == NULL)
            return;
        stats.cache_flush_count++;
        std::set<size_t> pages_to_write(new_pages);
        calc_flush_count();
        int pages_to_check = stats.last_pages_to_flush * 3;
        dbl_lnklst *cur_entry = lnklst_last_entry;
        do {
            uint8_t *block = &page_cache[cur_entry->cache_loc * page_size];
            if (block_to_keep != block) {
              if (iface->is_block_changed(block, page_size)) {
                pages_to_write.insert(cur_entry->disk_page);
                if (cur_entry->disk_page == 0 || !disk_to_cache_map[cur_entry->disk_page])
                    std::cout << "Disk cache map entry missing" << std::endl;
              }
              if (pages_to_write.size() > (stats.last_pages_to_flush + new_pages.size()))
                break;
            }
            cur_entry = cur_entry->prev;
        } while (--pages_to_check && cur_entry);
        write_pages(pages_to_write);
        new_pages.clear();
        lnklst_last_free = lnklst_last_entry;
    }
    void move_to_front(dbl_lnklst *entry_to_move) {
        //if (entry_to_move == lnklst_last_free)
        //  lnklst_last_free = lnklst_last_free->prev;
        if (entry_to_move != lnklst_first_entry) {
            if (entry_to_move == lnklst_last_entry)
                lnklst_last_entry = lnklst_last_entry->prev;
            entry_to_move->prev->next = entry_to_move->next;
            if (entry_to_move->next != NULL)
                entry_to_move->next->prev = entry_to_move->prev;
            entry_to_move->next = lnklst_first_entry;
            entry_to_move->prev = NULL;
            lnklst_first_entry->prev = entry_to_move;
            lnklst_first_entry = entry_to_move;
        }
    }
    void check_map_size() {
        // if (disk_to_cache_map_size <= file_page_count) {
        //     //cout << "Expanding cache at: " << file_page_count << endl;
        //     dbl_lnklst **temp = disk_to_cache_map;
        //     size_t old_size = disk_to_cache_map_size;
        //     disk_to_cache_map_size = file_page_count + 1000;
        //     disk_to_cache_map = (dbl_lnklst **) malloc_fn(disk_to_cache_map_size * sizeof(dbl_lnklst *));
        //     memset(disk_to_cache_map, '\0', disk_to_cache_map_size * sizeof(dbl_lnklst *));
        //     memcpy(disk_to_cache_map, temp, old_size * sizeof(dbl_lnklst*));
        //     free(temp);
        // }
    }

public:
    size_t file_page_count;
    int cache_size_in_pages;
    uint8_t *page_cache;
    chg_iface *iface;
    uint8_t options;
    lru_cache(int pg_size, int cache_size_kb, const char *fname,
            chg_iface *_iface, int init_page_count = 0,
            const uint8_t opts = 0, void *(*alloc_fn)(size_t) = NULL, int given_fd = -1) {
        iface = _iface;
        options = opts;
        if (alloc_fn == NULL)
            alloc_fn = malloc;
        malloc_fn = alloc_fn;
        page_size = pg_size;
        cache_size_in_pages = cache_size_kb * 1024 / page_size;
        cache_occupied_size = 0;
        lnklst_first_entry = lnklst_last_entry = NULL;
        filename = fname;
        page_cache = (uint8_t *) alloc_fn(pg_size * cache_size_in_pages);
        root_block = (uint8_t *) alloc_fn(pg_size);
        llarr = (dbl_lnklst *) alloc_fn(cache_size_in_pages * sizeof(dbl_lnklst));
        memset(llarr, '\0', cache_size_in_pages * sizeof(dbl_lnklst));
        skip_page_count = init_page_count;
        file_page_count = init_page_count;
        max_pages_to_flush = cache_size_in_pages / 2;
        struct stat file_stat;
        memset(&file_stat, '\0', sizeof(file_stat));
        fd = (given_fd == -1 ? common::open_fd(fname) : given_fd);
        if (fd == -1)
          throw errno;
        fstat(fd, &file_stat);
        common::set_fadvise(fd, POSIX_FADV_RANDOM);
        file_page_count = file_stat.st_size;
        if (file_page_count > 0)
           file_page_count /= page_size;
        //cout << "File page count: " << file_page_count << endl;
        //disk_to_cache_map_size = std::max(file_page_count + 1000, (size_t) cache_size_in_pages);
        // disk_to_cache_map = (dbl_lnklst **) alloc_fn(disk_to_cache_map_size * sizeof(dbl_lnklst *));
        // memset(disk_to_cache_map, '\0', disk_to_cache_map_size * sizeof(dbl_lnklst *));
        empty = 0;
        //lseek(fd, skip_page_count * page_size, SEEK_SET);
        if (common::read_bytes(fd, root_block, skip_page_count * page_size, page_size) != page_size) {
            file_page_count = skip_page_count + 1;
            if (common::write_bytes(fd, root_block, 0, page_size) != page_size)
              throw EIO;
            empty = 1;
        }
        stats.pages_read++;
        lnklst_last_free = NULL;
        memset(&stats, '\0', sizeof(stats));
        calc_flush_count();
    }
    virtual ~lru_cache() {
        flush_pages_in_seq(0);
        std::set<size_t> pages_to_write;
        for (std::unordered_map<size_t, dbl_lnklst*>::iterator it = disk_to_cache_map.begin(); it != disk_to_cache_map.end(); it++) {
            uint8_t *block = &page_cache[page_size * it->second->cache_loc];
            if (block[0] & 0x02) // is it changed
                pages_to_write.insert(it->first);
        }
        // for (size_t ll = 0; ll < cache_size_in_pages; ll++) {
        //     if (llarr[ll].disk_page == 0)
        //       continue;
        //     uint8_t *block = &page_cache[page_size * llarr[ll].cache_loc];
        //     if (iface->is_block_changed(block, page_size)) // is it changed
        //         pages_to_write.insert(llarr[ll].disk_page);
        // }
        write_pages(pages_to_write);
        free(page_cache);
        common::write_bytes(fd, root_block, skip_page_count * page_size, page_size);
        close(fd);
        free(root_block);
        free(llarr);
        //free(disk_to_cache_map);
        // cout << "cache requests: " << " " << stats.total_cache_req 
        //      << ", Misses: " << stats.total_cache_misses
        //      << ", Flush#: " << stats.cache_flush_count << endl;
    }
    int read_page(uint8_t *block, off_t file_pos, size_t bytes) {
        size_t pg_sz = bytes;
        uint8_t *read_buf;
        if (options == 0)
            file_pos *= bytes;
        else {
            bytes = file_pos & 0xFFFF;
            file_pos = (file_pos >> 16);
            read_buf = new uint8_t[65536];
            //std::cout << "File pos: " << file_pos << ", len: " << bytes << std::endl;
        }
        size_t read_count = read_count = common::read_bytes(fd, options == 0 ? block : read_buf, file_pos, bytes);
        if (read_count != bytes) {
            perror("read");
            printf("read_count: %lu, %lu\n", read_count, bytes);
        }
        if (options != 0) {
            size_t dsize = common::decompress_block(options, read_buf, read_count, block, pg_sz);
            if (dsize != pg_sz) {
                std::cout << "Uncompressed length not matching page_size: " << dsize << std::endl;
            }
            //printf("%2x,%2x,%2x,%2x,%2x\n", block[0], block[1], block[2], block[3], block[4]);
            read_count = dsize;
            delete [] read_buf;
        }
        return read_count;
    }
    void write_page(uint8_t *block, off_t file_pos, size_t bytes, bool is_new = true) {
        if (options != 0)
            return;
        //if (is_new)
        //  fseek(fp, 0, SEEK_END);
        //else
        common::write_bytes(fd, block, file_pos, bytes);
    }
    uint8_t *get_disk_page_in_cache(unsigned long disk_page, uint8_t *block_to_keep = NULL, bool is_new = false) {
        if (disk_page < skip_page_count)
            std::cout << "WARNING: asking disk_page: " << disk_page << std::endl;
        if (disk_page == skip_page_count)
            return root_block;
        int cache_pos = 0;
        size_t removed_disk_page = 0;
        //if (disk_to_cache_map[disk_page] == NULL) {
        if (disk_to_cache_map.find(disk_page) == disk_to_cache_map.end()) {
            if (cache_occupied_size < cache_size_in_pages) {
                dbl_lnklst *new_entry = &llarr[cache_occupied_size]; // new dbl_lnklst();
                new_entry->disk_page = disk_page;
                new_entry->cache_loc = cache_occupied_size;
                new_entry->prev = lnklst_last_entry;
                new_entry->next = NULL;
                if (lnklst_last_entry != NULL)
                    lnklst_last_entry->next = new_entry;
                lnklst_last_entry = new_entry;
                if (lnklst_first_entry == NULL)
                    lnklst_first_entry = new_entry;
                disk_to_cache_map[disk_page] = new_entry;
                cache_pos = cache_occupied_size++;
            } else {
                    //uint8_t *block;
                    //while (1) {
                    //  block = &page_cache[entry_to_move->cache_loc * page_size];
                    //  if (block != block_to_keep) {
                    //     break;
                    //  }
                    //  entry_to_move = entry_to_move->prev;
                    //}
                calc_flush_count();
                uint8_t *block;
                dbl_lnklst *entry_to_move;
                do {
                  entry_to_move = lnklst_last_free;
                  if (entry_to_move == NULL)
                    entry_to_move = lnklst_last_entry;
                  int check_count = 40; // last_pages_to_flush * 2;
                  while (check_count--) { // find block which is not changed
                    block = &page_cache[entry_to_move->cache_loc * page_size];
                    if (!iface->is_block_changed(block, page_size) && block_to_keep != block)
                      break;
                    if (entry_to_move->prev == NULL) {  // TODO: Review lru logic
                      lnklst_last_free = NULL;
                      break;
                    }
                    entry_to_move = entry_to_move->prev;
                  }
                  if (iface->is_block_changed(block, page_size) || new_pages.size() > stats.last_pages_to_flush
                             || new_pages.find(disk_page) != new_pages.end() || entry_to_move->prev == NULL) {
                      flush_pages_in_seq(block_to_keep);
                      entry_to_move = lnklst_last_entry;
                      break;
                  }
                } while (iface->is_block_changed(block, page_size));
                if (!iface->is_block_changed(block, page_size))
                    lnklst_last_free = entry_to_move->prev;
                /*entry_to_move = lnklst_last_entry;
                  int check_count = 40;
                  while (check_count-- && entry_to_move != NULL) { // find block which is not changed
                    block = &page_cache[entry_to_move->cache_loc * page_size];
                    if (block_to_keep != block) {
                      if (iface->is_block_changed(block, page_size)) {
                        set_changed_fn(block, page_size, false);
                        //if (page_size < 65537 && block[5] < 255)
                        //    block[5]++;
                        write_page(block, entry_to_move->disk_page * page_size, page_size);
                        //fflush(fp);
                      }
                      if (new_pages.size() > 0) {
                        write_pages(new_pages);
                        new_pages.clear();
                      }
                      break;
                    }
                    entry_to_move = entry_to_move->prev;
                  }
                  if (entry_to_move == NULL) {
                    cout << "Could not satisfy cache miss" << endl;
                    exit(1);
                  }*/
                removed_disk_page = entry_to_move->disk_page;
                cache_pos = entry_to_move->cache_loc;
                //if (!is_new)
                  move_to_front(entry_to_move);
                entry_to_move->disk_page = disk_page;
                disk_to_cache_map.erase(removed_disk_page);
                disk_to_cache_map[disk_page] = entry_to_move;
                stats.total_cache_misses++;
                stats.total_cache_req++;
            }
            if (!is_new && new_pages.find(disk_page) == new_pages.end()) {
                size_t read_ret = read_page(&page_cache[page_size * cache_pos], disk_page, page_size);
                if (read_ret != page_size)
                    std::cout << "Unable to read: " << disk_page << ", " << read_ret << std::endl;
                stats.pages_read++;
            }
        } else {
            if (is_new)
                std::cout << "WARNING: How was new page found in cache?" << std::endl;
            dbl_lnklst *current_entry = disk_to_cache_map[disk_page];
            if (lnklst_last_free == current_entry && lnklst_last_free != NULL
                    && lnklst_last_free->prev != NULL)
                lnklst_last_free = lnklst_last_free->prev;
            move_to_front(current_entry);
            cache_pos = current_entry->cache_loc;
            if (cache_occupied_size >= cache_size_in_pages)
              stats.total_cache_req++;
        }
        return &page_cache[page_size * cache_pos];
    }
    uint8_t *get_new_page(uint8_t *block_to_keep) {
        if (new_pages.size() > stats.last_pages_to_flush)
            flush_pages_in_seq(block_to_keep);
        check_map_size();
        uint8_t *new_page = get_disk_page_in_cache(file_page_count, block_to_keep, true);
        new_pages.insert(file_page_count);
            //set_changed_fn(new_page, page_size, false);
            //write_page(new_page, file_page_count * page_size, page_size, true);
            //fflush(fp);
            //set_changed(new_page, page_size, true); // change it
            //printf("file_page_count:%ld\n", file_page_count);
        file_page_count++;
        return new_page;
    }
    int get_page_count() {
        return file_page_count;
    }
    uint8_t is_empty() {
        return empty;
    }
    cache_stats get_cache_stats() {
        return stats;
    }
};
#endif
