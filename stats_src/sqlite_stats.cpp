#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <stdint.h>
#include "sqlite.h"

using namespace std;

#define BPT_LEAF0_LVL 14
#define PAGE_RESV_BYTES 5

typedef struct {
  long leaf_count;
  long leaf_filled_size;
  long leaf_data_size;
  long leaf_write_counts[256];
  long parent_count;
  long parent_filled_size;
  long parent_data_size;
  long parent_write_counts[256];
} stats;

typedef struct {
  stats block_stats;
  stats total_block_stats;
  long level_counts[9];
} all_stats;

int page_referred = 0;

void print_block_stats(stats& stat, stats& ptr_stat, bool is_leaf, const char *prefix) {
    if (is_leaf) {
      cout << endl << prefix << "Leaf Data size: " << stat.leaf_data_size << ", " <<
                      prefix << "Leaf Filled size: " << stat.leaf_filled_size << ", " <<
                      prefix << "Leaf count: " << stat.leaf_count << endl;
      cout << prefix << "Leaf write counts: ";
      for (int i = 0; i < 256; i++) {
        cout << stat.leaf_write_counts[i] << " ";
      }
      cout << endl;
    } else {
      cout << endl << prefix << "Parent Data size: " << stat.parent_data_size << ", " <<
                      prefix << "Parent Filled size: " << stat.parent_filled_size << ", " <<
                      prefix << "Parent count: " << stat.parent_count << endl;
      cout << prefix << "Parent write counts: ";
      for (int i = 0; i < 256; i++) {
        cout << stat.parent_write_counts[i] << " ";
      }
      cout << endl;
    }
}

uint16_t getInt(uint8_t *pos) {
    uint16_t ret = ((*pos << 8) | *(pos + 1));
    return ret;
}

uint32_t read_uint32(const uint8_t *ptr) {
    uint32_t ret;
    ret = ((uint32_t)*ptr++) << 24;
    ret += ((uint32_t)*ptr++) << 16;
    ret += ((uint32_t)*ptr++) << 8;
    ret += *ptr;
    return ret;
}

void process_block(uint8_t *buf, int page_size, all_stats& stats, uint32_t page_no) {
    memset(&stats.block_stats, '\0', sizeof(stats.block_stats));
    if (*buf == 10) {
      stats.block_stats.leaf_filled_size = getInt(buf+3);
      stats.block_stats.leaf_data_size = (page_size - PAGE_RESV_BYTES - getInt(buf+5));
      stats.total_block_stats.leaf_count++;
      stats.total_block_stats.leaf_filled_size += stats.block_stats.leaf_filled_size;
      stats.total_block_stats.leaf_data_size += stats.block_stats.leaf_data_size;
      //if (buf[5] > 18) {
      //  stats.block_stats.leaf_write_counts[19]++;
      //  stats.total_block_stats.leaf_write_counts[19]++;
      //} else {
                            //stats.block_stats.leaf_write_counts[buf[5]]++;
                            //stats.total_block_stats.leaf_write_counts[buf[5]]++;
      //}
    } else if (*buf == 2) {
      stats.block_stats.parent_filled_size = getInt(buf+3);
      stats.block_stats.parent_data_size = (page_size - PAGE_RESV_BYTES - getInt(buf+5));
      stats.total_block_stats.parent_count++;
      stats.total_block_stats.parent_filled_size += stats.block_stats.parent_filled_size;
      stats.total_block_stats.parent_data_size += stats.block_stats.parent_data_size;
      //if (buf[5] > 18) {
      //  stats.block_stats.parent_write_counts[19]++;
      //  stats.total_block_stats.parent_write_counts[19]++;
      //} else {
                            //stats.block_stats.parent_write_counts[buf[5]]++;
                            //stats.total_block_stats.parent_write_counts[buf[5]]++;
      //}
      uint32_t right_page = read_uint32(buf + 8);
      if (right_page == page_referred)
        cout << "Page referred at: " << page_no << endl;
      int filled_size = getInt(buf + 3);
      for (int i = 0; i < filled_size; i++) {
        int ptr_pos = getInt(buf + 12 + i * 2);
        //cout << "Ptr Pos: " << ptr_pos << endl;
        if (ptr_pos < page_size) {
          uint32_t child_page = read_uint32(buf + ptr_pos);
          if (child_page == page_referred)
              cout << "Page referred at: " << page_no << ", idx: " << i << endl;
        }
      }
    }
    //stats.level_counts[(*buf & 0x1F) - BPT_LEAF0_LVL]++;
}

int main(int argc, char *argv[]) {

  if (argc < 3) {
    cout << "Pass arguments <filename> <page_size>" << endl;
    return 1;
  }
  FILE *fp = fopen(argv[1], "rb");
  if (fp == NULL) {
    perror("Error: ");
  }
  int page_size = atoi(argv[2]);
  int referred_page = 0;
  if (argc > 3) {
    referred_page = atol(argv[3]);
    page_referred = referred_page;
  }
  uint8_t buf[page_size];
  all_stats stats;
  all_stats ptr_stats;
  memset(&stats, '\0', sizeof(stats));
  memset(&ptr_stats, '\0', sizeof(ptr_stats));
  long page_no = 0;
  while (!feof(fp)) {
    int rcount = fread(buf, 1, page_size, fp);
    if (rcount == 0) {
      cout << "Read 0: " << page_no << endl;
      break;
    }
    if (rcount != page_size) {
      cout << "Premature eof" << endl;
      break;
    }
    page_no++;
    process_block(buf, page_size, stats, page_no);

    //print_block_stats(stats.block_stats, ptr_stats.block_stats, (*buf & 0x80) == 0x80, "");
//if (page_no > 2) break;
  }
  print_block_stats(stats.total_block_stats, ptr_stats.total_block_stats, true, "Total ");
  print_block_stats(stats.total_block_stats, ptr_stats.total_block_stats, false, "Total ");

  cout << endl << "Grand Total Level counts: ";
  for (int i = 0; i < 9; i++) {
    cout << stats.level_counts[i] << " ";
  }
  cout << endl;

  cout << endl << "Grand Total write counts: ";
  for (int i = 0; i < 256; i++) {
    cout << stats.total_block_stats.leaf_write_counts[i] 
        + stats.total_block_stats.parent_write_counts[i] << " ";
  }
  cout << endl;

  fclose(fp);

  return 1;

}
