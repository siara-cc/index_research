#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <stdint.h>
#include "basix.h"

using namespace std;

#define BPT_LEAF0_LVL 14

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

void process_block(uint8_t *buf, int page_size, all_stats& stats) {
    memset(&stats.block_stats, '\0', sizeof(stats.block_stats));
    if (*buf & 0x80) {
      stats.block_stats.leaf_filled_size = getInt(buf+1);
      stats.block_stats.leaf_data_size = (page_size - getInt(buf+3));
      stats.total_block_stats.leaf_count++;
      stats.total_block_stats.leaf_filled_size += stats.block_stats.leaf_filled_size;
      stats.total_block_stats.leaf_data_size += stats.block_stats.leaf_data_size;
      //if (buf[5] > 18) {
      //  stats.block_stats.leaf_write_counts[19]++;
      //  stats.total_block_stats.leaf_write_counts[19]++;
      //} else {
        stats.block_stats.leaf_write_counts[buf[5]]++;
        stats.total_block_stats.leaf_write_counts[buf[5]]++;
      //}
    } else {
      stats.block_stats.parent_filled_size = getInt(buf+1);
      stats.block_stats.parent_data_size = (page_size - getInt(buf+3));
      stats.total_block_stats.parent_count++;
      stats.total_block_stats.parent_filled_size += stats.block_stats.parent_filled_size;
      stats.total_block_stats.parent_data_size += stats.block_stats.parent_data_size;
      //if (buf[5] > 18) {
      //  stats.block_stats.parent_write_counts[19]++;
      //  stats.total_block_stats.parent_write_counts[19]++;
      //} else {
        stats.block_stats.parent_write_counts[buf[5]]++;
        stats.total_block_stats.parent_write_counts[buf[5]]++;
      //}
    }
    stats.level_counts[(*buf & 0x1F) - BPT_LEAF0_LVL]++;
}

int main(int argc, char *argv[]) {

  if (argc < 3) {
    cout << "Pass arguments <filename> <page_size>" << endl;
    return 1;
  }
  FILE *fp = fopen(argv[1], "r");
  if (fp == NULL) {
    perror("Error: ");
  }
  int page_size = atoi(argv[2]);
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
    process_block(buf, page_size, stats);
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
