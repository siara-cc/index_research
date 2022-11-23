#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <stdint.h>
#include "bfos.h"

using namespace std;

#define BFOS_HDR_SIZE 9
#define TRIE_LEN_PTR buf + 6
#define BPT_LEAF0_LVL 14

const uint8_t bit_count[256] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};

typedef struct {
  long leaf_leaf_counts[9];
  long leaf_child_counts[9];
  long leaf_prefix_count;
  long leaf_prefix_len;
  long parent_leaf_counts[9];
  long parent_child_counts[9];
  long parent_prefix_count;
  long parent_prefix_len;
  long leaf_trie_len;
  long leaf_count;
  long leaf_node_count;
  long leaf_both_count;
  long leaf_filled_size;
  long leaf_data_size;
  long parent_trie_len;
  long parent_count;
  long parent_node_count;
  long parent_both_count;
  long parent_filled_size;
  long parent_data_size;
} stats;

typedef struct {
  stats block_stats;
  stats total_block_stats;
  long level_counts[9];
} all_stats;

void print_block_stats(stats& stat, stats& ptr_stat, bool is_leaf, const char *prefix) {
    if (is_leaf) {
      cout << endl << prefix << "Leaf Trie len: " << stat.leaf_trie_len << ", " <<
                      prefix << "Leaf Filled size: " << stat.leaf_filled_size << ", " <<
                      prefix << "Leaf count: " << stat.leaf_count;
      cout << endl << prefix << "Leaf node count: " << stat.leaf_node_count << ", " <<
                      prefix << "Leaf both count: " << stat.leaf_both_count << ", " <<
                      prefix << "Leaf Data size: " << stat.leaf_data_size << endl;
      cout << prefix << "Leaf Leaf counts: ";
      for (int i = 0; i < 9; i++) {
        cout << stat.leaf_leaf_counts[i] << " ";
      }
      cout << endl << prefix << "Leaf Child counts: ";
      for (int i = 0; i < 9; i++) {
        cout << stat.leaf_child_counts[i] << " ";
      }
      cout << endl;
      cout << prefix << "Leaf Prefix count and len: " << stat.leaf_prefix_count << " " << stat.leaf_prefix_len << endl;
    } else {
      cout << endl << prefix << "Parent Trie len: " << stat.parent_trie_len << ", " <<
                      prefix << "Parent Filled size: " << stat.parent_filled_size << ", " <<
                      prefix << "Parent count: " << stat.parent_count;
      cout << endl << prefix << "Parent node count: " << stat.parent_node_count << ", " <<
                      prefix << "Parent both count: " << stat.parent_both_count << ", " <<
                      prefix << "Parent Data size: " << stat.parent_data_size << endl;
      cout << prefix << "Ptr Trie len: " << ptr_stat.leaf_trie_len << ", " <<
              prefix << "Ptr Filled size: " << ptr_stat.leaf_filled_size << ", " <<
              prefix << "Ptr Data size: " << ptr_stat.leaf_data_size << endl;
      cout << prefix << "Parent Leaf counts: ";
      for (int i = 0; i < 9; i++) {
        cout << stat.parent_leaf_counts[i] << " ";
      }
      cout << "    ";
      for (int i = 0; i < 9; i++) {
        cout << ptr_stat.leaf_leaf_counts[i] << " ";
      }
      cout << endl << prefix << "Parent Child counts: ";
      for (int i = 0; i < 9; i++) {
        cout << stat.parent_child_counts[i] << " ";
      }
      cout << "    ";
      for (int i = 0; i < 9; i++) {
        cout << ptr_stat.leaf_child_counts[i] << " ";
      }
      cout << endl;
      cout << prefix << "Parent Prefix count and len: " << stat.parent_prefix_count << " " << stat.parent_prefix_len 
            << "    " << ptr_stat.leaf_prefix_count << " " << ptr_stat.leaf_prefix_len << endl;
    }
}

uint16_t getInt(uint8_t *pos) {
    uint16_t ret = ((*pos << 8) | *(pos + 1));
    return ret;
}

void process_block(uint8_t *buf, int page_size, all_stats& stats, bfos *ix) {
    memset(&stats.block_stats, '\0', sizeof(stats.block_stats));
    if (*buf & 0x80) {
      stats.block_stats.leaf_trie_len = getInt(TRIE_LEN_PTR);
      stats.block_stats.leaf_filled_size = getInt(buf+1);
      stats.block_stats.leaf_data_size = (page_size - getInt(buf+3));
      stats.total_block_stats.leaf_count++;
      stats.total_block_stats.leaf_trie_len += stats.block_stats.leaf_trie_len;
      stats.total_block_stats.leaf_node_count += stats.block_stats.leaf_node_count;
      stats.total_block_stats.leaf_both_count += stats.block_stats.leaf_both_count;
      stats.total_block_stats.leaf_filled_size += stats.block_stats.leaf_filled_size;
      stats.total_block_stats.leaf_data_size += stats.block_stats.leaf_data_size;
    } else {
      stats.block_stats.parent_trie_len = getInt(TRIE_LEN_PTR);
      stats.block_stats.parent_filled_size = getInt(buf+1);
      stats.block_stats.parent_data_size = (page_size - getInt(buf+3));
      stats.total_block_stats.parent_count++;
      stats.total_block_stats.parent_trie_len += stats.block_stats.parent_trie_len;
      stats.total_block_stats.parent_node_count += stats.block_stats.parent_node_count;
      stats.total_block_stats.parent_both_count += stats.block_stats.parent_both_count;
      stats.total_block_stats.parent_filled_size += stats.block_stats.parent_filled_size;
      stats.total_block_stats.parent_data_size += stats.block_stats.parent_data_size;
    }
    stats.level_counts[(*buf & 0x1F) - BPT_LEAF0_LVL]++;
    uint8_t *t = buf + BFOS_HDR_SIZE;
    uint8_t *upto = t + getInt(TRIE_LEN_PTR);
    while (t < upto) {
        uint8_t tc = *t++;
        if (*buf & 0x80)
            stats.block_stats.leaf_node_count++;
        else
            stats.block_stats.parent_node_count++;
        if (tc & 0x01) {
            if (*buf & 0x80) {
              stats.block_stats.leaf_prefix_len += (tc >> 1);
              stats.total_block_stats.leaf_prefix_len += (tc >> 1);
              stats.block_stats.leaf_prefix_count++;
              stats.total_block_stats.leaf_prefix_count++;
            } else {
              stats.block_stats.parent_prefix_len += (tc >> 1);
              stats.total_block_stats.parent_prefix_len += (tc >> 1);
              stats.block_stats.parent_prefix_count++;
              stats.total_block_stats.parent_prefix_count++;
            }
            t += (tc >> 1);
            continue;
        }
        // if (tc != 0 && tc < 32) {
        //   cout << "Invalid value" << (int) tc << " " << (t - buf) << " " << (int) (*buf & 0x80) << " " << page_no << endl;
        //   fclose(fp);
        //   return 0;
        // }
        uint8_t child = 0;
        if (tc & 0x02) {
          child = *t++;
          if (*buf & 0x80) {
            stats.block_stats.leaf_child_counts[bit_count[child]]++;
            stats.total_block_stats.leaf_child_counts[bit_count[child]]++;
          } else {
            stats.block_stats.parent_child_counts[bit_count[child]]++;
            stats.total_block_stats.parent_child_counts[bit_count[child]]++;
          }
        }
        uint8_t leaves = *t++;
        if (*buf & 0x80) {
          if (child > 0 && leaves > 0)
            stats.block_stats.leaf_both_count++;
          stats.block_stats.leaf_leaf_counts[bit_count[leaves]]++;
          stats.total_block_stats.leaf_leaf_counts[bit_count[leaves]]++;
        } else {
          if (child > 0 && leaves > 0)
            stats.block_stats.parent_both_count++;
          stats.block_stats.parent_leaf_counts[bit_count[leaves]]++;
          stats.total_block_stats.parent_leaf_counts[bit_count[leaves]]++;
        }
        t += (bit_count[child] * 2);
        if (*buf & 0x80)
          t += (bit_count[leaves] * 2);
        else {
          int leaf_count = bit_count[leaves];
          while (leaf_count--) {
            int leaf_pos = getInt(t);
            uint8_t *child_ptr_pos = buf + leaf_pos;
            child_ptr_pos += (*child_ptr_pos);
            child_ptr_pos++;
            int key_len = (int) *child_ptr_pos++;
            uint8_t val[8];
            int zero_count = 4 - key_len;
            memcpy(val + zero_count, child_ptr_pos, key_len);
            while (zero_count--)
              val[zero_count] = 0;
            //unsigned long val = util::bytesToPtr(child_ptr_pos-1);
            //printf("%d %4lx\n", key_len, val);
            ix->put((const char *) val, 4, "", 0);
            t += 2;
          }
        }
    }
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
    bfos *ix;
    if ((*buf & 0x80) == 0)
      ix = new bfos(page_size, page_size);
    process_block(buf, page_size, stats, ix);
    if ((*buf & 0x80) == 0)
      process_block(ix->getCurrentBlock(), page_size, ptr_stats, NULL);
    print_block_stats(stats.block_stats, ptr_stats.block_stats, (*buf & 0x80) == 0x80, "");
    if ((*buf & 0x80) == 0)
      delete ix;
//if (page_no > 2) break;
  }
  print_block_stats(stats.total_block_stats, ptr_stats.total_block_stats, true, "Total ");
  print_block_stats(stats.total_block_stats, ptr_stats.total_block_stats, false, "Total ");

  cout << endl << "Grand Total Level counts: ";
  for (int i = 0; i < 9; i++) {
    cout << stats.level_counts[i] << " ";
  }
  cout << endl << "Grand Total Leaf counts: ";
  for (int i = 0; i < 9; i++) {
    cout << stats.total_block_stats.parent_leaf_counts[i] + stats.total_block_stats.leaf_leaf_counts[i] << " ";
  }
  cout << endl << "Grand Total Child counts: ";
  for (int i = 0; i < 9; i++) {
    cout << stats.total_block_stats.parent_child_counts[i] + stats.total_block_stats.leaf_child_counts[i] << " ";
  }
  cout << endl;
  cout << "Grand Total Prefix count and len: " << stats.total_block_stats.parent_prefix_count + stats.total_block_stats.leaf_prefix_count
        << " " << stats.total_block_stats.parent_prefix_len + stats.total_block_stats.leaf_prefix_len << endl;

  fclose(fp);

  return 1;

}
