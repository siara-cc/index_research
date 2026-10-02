#ifndef MADRAS_BLK_H
#define MADRAS_BLK_H

// madras_blk: static block-level index.
//   - Parent blocks: B+tree blocks (basix style, key lengths derived from offsets)
//   - Leaf blocks: self-contained succinct tries (loctets style: tail/term/child/leaf bits)
//   - Tails: one contiguous area in madras format (text + 15 terminator, or bin15),
//            ordered by descending frequency, pointed to by fvint byte offsets
//
// Phase 1: max_inner_tries = 0 only (plain tails), lookup only.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

#include "madras_blk/in_mem_trie.hpp"
#ifdef __BMI2__
#include <immintrin.h>
#endif

namespace madras_blk {

namespace mtrie = madras::dv1::memtrie;

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

// node_byte_mode for tail nodes:
//   MBX_NODE_BYTE_FIRST: node byte = first byte of tail, tail area has the rest,
//                        pointer = fvint(ptr)
//   MBX_NODE_BYTE_PTR:   node byte = low 8 bits of ptr (madras convention),
//                        tail area has the whole tail, pointer = fvint(ptr >> 8)
#define MBX_NODE_BYTE_FIRST 0
#define MBX_NODE_BYTE_PTR 1

struct bldr_options {
  uint8_t max_inner_tries;   // same meaning as madras bldr_options (only 0 supported now)
  uint32_t leaf_blk_size;    // up to 65536
  uint32_t parent_blk_size;  // up to 65536
  uint8_t node_byte_mode;    // MBX_NODE_BYTE_*
  uint8_t tail_sfx;          // 1 = text tails share bytes with tails they are suffixes of
};

static const bldr_options dflt_opts = { 0, 4096, 4096, MBX_NODE_BYTE_FIRST, 0 };

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------

// Meta block (block 0): madras header (280 bytes) + 1 opts record (24 bytes)
// followed by madras_blk section.
#define MBX_MDX_HEADER_SIZE 280
#define MBX_OPTS_REC_SIZE 24
#define MBX_SECTION_LOC (MBX_MDX_HEADER_SIZE + MBX_OPTS_REC_SIZE)
#define MBX_MAGIC_BYTE 0xA5

// Leaf block:
//   [0]     block type 0x80 (leaf, same as basix leaf type byte)
//   [1]     level (0)
//   [2..3]  node count n
//   [4..5]  key count
//   [6..7]  tail pointer bytes used
//   [8..]   child_rank[G], term_rank[G], leaf_rank[G], ptr_off[G]  (2 bytes each)
//           flags: per group 4 x uint64 (tail, term, child, leaf)
//           node bytes[n]
//           tail pointers (fvint), ptr_off[g] = offset of group g's first pointer
#define MBX_LEAF_HDR_SIZE 8
#define MBX_BLK_TYPE_LEAF 0x80
#define MBX_FLAG_TAIL 0
#define MBX_FLAG_TERM 1
#define MBX_FLAG_CHILD 2
#define MBX_FLAG_LEAF 3

// Parent block:
//   [0]     level (1 = just above leaves)
//   [1]     child number width w (bytes)
//   [2..3]  entry count c
//   [4..5]  end of record area
//   [6..7]  reserved
//   [8..]   record offsets (2 bytes each, ascending)
//           records: key bytes + w-byte child number (little endian)
#define MBX_PARENT_HDR_SIZE 8

// madras_blk section in meta block (all little endian)
struct mbx_section {
  uint64_t parent_start;
  uint64_t leaf_start;
  uint64_t tail_area_loc;
  uint64_t tail_area_size;
  uint64_t total_size;
  uint32_t leaf_blk_size;
  uint32_t parent_blk_size;
  uint32_t leaf_count;
  uint32_t parent_count;
  uint8_t parent_levels;
  uint8_t child_width;
  uint8_t node_byte_mode;
  uint8_t tail_sfx;
  uint8_t reserved[4];
};

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

static inline void put_u16(uint8_t *p, uint32_t v) {
  uint16_t v16 = (uint16_t) v;
  memcpy(p, &v16, 2);
}

static inline uint32_t get_u16(const uint8_t *p) {
  uint16_t v16;
  memcpy(&v16, p, 2);
  return v16;
}

static inline void put_u64(uint8_t *p, uint64_t v) {
  memcpy(p, &v, 8);
}

static inline uint64_t get_u64(const uint8_t *p) {
  uint64_t v;
  memcpy(&v, p, 8);
  return v;
}

static inline uint64_t get_uint_n(const uint8_t *p, int n) {
  uint64_t v = 0;
  for (int i = n - 1; i >= 0; i--)
    v = (v << 8) | p[i];
  return v;
}

static inline void put_uint_n(uint8_t *p, uint64_t v, int n) {
  for (int i = 0; i < n; i++) {
    p[i] = (uint8_t) (v & 0xFF);
    v >>= 8;
  }
}

static inline int bytes_needed(uint64_t v) {
  int n = 1;
  while (v > 0xFF) {
    v >>= 8;
    n++;
  }
  return n;
}

static inline int fvint_size(uint64_t v) {
  int n = 1;
  while (v >= 128) {
    v >>= 7;
    n++;
  }
  return n;
}

static inline size_t write_fvint(uint8_t *p, uint64_t v) {
  size_t len = 0;
  do {
    uint8_t b = v & 0x7F;
    v >>= 7;
    if (v > 0)
      b |= 0x80;
    p[len++] = b;
  } while (v > 0);
  return len;
}

static inline uint64_t read_fvint(const uint8_t *p, size_t& len) {
  uint64_t ret = 0;
  int shift = 0;
  len = 0;
  uint8_t b;
  do {
    b = p[len++];
    ret |= ((uint64_t) (b & 0x7F) << shift);
    shift += 7;
  } while (b & 0x80);
  return ret;
}

static inline const uint8_t *skip_fvint(const uint8_t *p) {
  while (*p++ & 0x80);
  return p;
}

// position (0 based) of the n-th (1 based) set bit of w; w must have it
static inline int select_in_word(uint64_t w, uint32_t n) {
#ifdef __BMI2__
  return __builtin_ctzll(_pdep_u64((uint64_t) 1 << (n - 1), w));
#else
  while (--n > 0)
    w &= (w - 1);
  return __builtin_ctzll(w);
#endif
}

// Skips n fvints 8 bytes at a time. May read up to 7 bytes beyond the
// last fvint, so the index buffer is padded at the end.
static inline const uint8_t *skip_fvints(const uint8_t *p, int n) {
  while (n > 0) {
    uint64_t w;
    memcpy(&w, p, 8);
    uint64_t ends = ~w & 0x8080808080808080ULL;
    int c = __builtin_popcountll(ends);
    if (c < n) {
      n -= c;
      p += 8;
      continue;
    }
    return p + (select_in_word(ends, (uint32_t) n) >> 3) + 1;
  }
  return p;
}

static inline int compare_keys(const uint8_t *k1, size_t len1, const uint8_t *k2, size_t len2) {
  size_t lim = len1 < len2 ? len1 : len2;
  int cmp = memcmp(k1, k2, lim);
  if (cmp != 0)
    return cmp;
  return len1 < len2 ? -1 : (len1 > len2 ? 1 : 0);
}

static inline uint32_t lcp_len(const uint8_t *k1, size_t len1, const uint8_t *k2, size_t len2) {
  size_t lim = len1 < len2 ? len1 : len2;
  size_t i = 0;
  while (i < lim && k1[i] == k2[i])
    i++;
  return (uint32_t) i;
}

static inline bool is_text_byte(uint8_t b) {
  return (uint8_t) (b - 15) > 16; // b < 15 || b > 31
}

// madras: empty key is stored as "!" (memtrie empty_value)
static const uint8_t empty_key_value[] = { '!' };
static const size_t empty_key_value_len = 1;

struct key_ref {
  const uint8_t *key;
  uint32_t len;
};

// ---------------------------------------------------------------------------
// Tail store: unique tail remainders (tail minus first byte, which lives in
// the node byte), ordered by descending frequency, laid out in madras format
// ---------------------------------------------------------------------------

class tail_store {
  public:
    struct tail_info {
      std::string str;
      uint32_t freq;
      uint64_t ptr;
      bool is_member;  // shares bytes of another (container) tail
    };
    std::vector<tail_info> tails;
    std::unordered_map<std::string, uint32_t> idx_map;
    uint64_t area_size;
    uint32_t max_tail_len;
    int ptr_shift;   // bits of ptr held in node byte (0 or 8)
    int tail_skip;   // leading tail bytes held in node byte (1 or 0)
    bool sfx;

    tail_store() : area_size (0), max_tail_len (0), ptr_shift (0), tail_skip (1), sfx (false) {
    }

    void set_mode(uint8_t node_byte_mode, uint8_t tail_sfx) {
      ptr_shift = (node_byte_mode == MBX_NODE_BYTE_PTR ? 8 : 0);
      tail_skip = (node_byte_mode == MBX_NODE_BYTE_PTR ? 0 : 1);
      sfx = (tail_sfx != 0);
    }

    int ptr_width(uint64_t ptr) const {
      return fvint_size(ptr >> ptr_shift);
    }

    static bool is_bin(const uint8_t *s, size_t len) {
      for (size_t i = 0; i < len; i++) {
        if (!is_text_byte(s[i]))
          return true;
      }
      return false;
    }

    static size_t len_len(size_t len) {
      size_t n = 0;
      do {
        n++;
        len >>= 3;
      } while (len > 0);
      return n;
    }

    static size_t entry_size(const uint8_t *s, size_t len) {
      if (is_bin(s, len))
        return len_len(len) + len;
      return len + 1;
    }

    // text: ptr points to first byte; bin: ptr points to last length byte
    static uint64_t entry_ptr(uint64_t start, const uint8_t *s, size_t len) {
      if (is_bin(s, len))
        return start + len_len(len) - 1;
      return start;
    }

    void add_freq(const uint8_t *s, size_t len) {
      std::string str((const char *) s, len);
      std::unordered_map<std::string, uint32_t>::iterator it = idx_map.find(str);
      if (it == idx_map.end()) {
        tail_info ti;
        ti.str = str;
        ti.freq = 1;
        ti.ptr = 0;
        ti.is_member = false;
        idx_map[str] = (uint32_t) tails.size();
        tails.push_back(ti);
      } else {
        tails[it->second].freq++;
      }
    }

    static bool freq_cmp(const tail_info& lhs, const tail_info& rhs) {
      if (lhs.freq != rhs.freq)
        return lhs.freq > rhs.freq;
      return lhs.str < rhs.str;
    }

    static bool rev_str_less(const tail_info *lhs, const tail_info *rhs) {
      return std::lexicographical_compare(lhs->str.rbegin(), lhs->str.rend(),
          rhs->str.rbegin(), rhs->str.rend());
    }

    static bool is_suffix_of(const std::string& sfx_str, const std::string& str) {
      return str.length() > sfx_str.length()
          && str.compare(str.length() - sfx_str.length(), sfx_str.length(), sfx_str) == 0;
    }

    struct sfx_group {
      uint32_t container;           // index in tails
      std::vector<uint32_t> members;
      uint64_t freq;                // sum of freq of container and members
    };

    static bool group_freq_cmp(const sfx_group *lhs, const sfx_group *rhs) {
      if (lhs->freq != rhs->freq)
        return lhs->freq > rhs->freq;
      return lhs->container < rhs->container;
    }

    // Groups text tails that are suffixes of a longer tail (container).
    // Groups are laid out by descending total frequency.
    void sfx_reorder() {
      std::vector<tail_info *> text_tails;
      for (size_t i = 0; i < tails.size(); i++) {
        if (!is_bin((const uint8_t *) tails[i].str.data(), tails[i].str.length()))
          text_tails.push_back(&tails[i]);
      }
      std::sort(text_tails.begin(), text_tails.end(), rev_str_less);
      std::vector<int64_t> container_of(tails.size(), -1);
      // If a tail is a suffix of any later tail in reversed order, it is
      // a suffix of the immediately next one
      for (size_t i = text_tails.size(); i-- > 0;) {
        if (i + 1 < text_tails.size() && is_suffix_of(text_tails[i]->str, text_tails[i + 1]->str)) {
          uint32_t cur = (uint32_t) (text_tails[i] - &tails[0]);
          uint32_t nxt = (uint32_t) (text_tails[i + 1] - &tails[0]);
          container_of[cur] = container_of[nxt] >= 0 ? container_of[nxt] : (int64_t) nxt;
        }
      }
      std::vector<sfx_group> groups;
      std::vector<int64_t> group_of(tails.size(), -1);
      for (size_t i = 0; i < tails.size(); i++) {
        if (container_of[i] < 0) {
          sfx_group sg;
          sg.container = (uint32_t) i;
          sg.freq = tails[i].freq;
          group_of[i] = (int64_t) groups.size();
          groups.push_back(sg);
        }
      }
      for (size_t i = 0; i < tails.size(); i++) {
        if (container_of[i] >= 0) {
          sfx_group& sg = groups[(size_t) group_of[(size_t) container_of[i]]];
          sg.members.push_back((uint32_t) i);
          sg.freq += tails[i].freq;
        }
      }
      std::vector<sfx_group *> order;
      for (size_t i = 0; i < groups.size(); i++)
        order.push_back(&groups[i]);
      std::sort(order.begin(), order.end(), group_freq_cmp);
      std::vector<tail_info> new_tails;
      new_tails.reserve(tails.size());
      area_size = 0;
      for (size_t i = 0; i < order.size(); i++) {
        sfx_group *sg = order[i];
        tail_info ti = tails[sg->container];
        const uint8_t *s = (const uint8_t *) ti.str.data();
        uint64_t start = area_size;
        ti.ptr = entry_ptr(start, s, ti.str.length());
        ti.is_member = false;
        area_size += entry_size(s, ti.str.length());
        size_t clen = ti.str.length();
        new_tails.push_back(ti);
        for (size_t j = 0; j < sg->members.size(); j++) {
          tail_info mi = tails[sg->members[j]];
          mi.ptr = start + (clen - mi.str.length());
          mi.is_member = true;
          new_tails.push_back(mi);
        }
      }
      tails.swap(new_tails);
    }

    // Called after pass 1: sort by frequency and assign pointers
    void finalize_order() {
      std::sort(tails.begin(), tails.end(), freq_cmp);
      if (sfx && tails.size() > 0) {
        sfx_reorder();
      } else {
        area_size = 0;
        for (size_t i = 0; i < tails.size(); i++) {
          tail_info& ti = tails[i];
          const uint8_t *s = (const uint8_t *) ti.str.data();
          ti.ptr = entry_ptr(area_size, s, ti.str.length());
          area_size += entry_size(s, ti.str.length());
        }
      }
      idx_map.clear();
      for (size_t i = 0; i < tails.size(); i++)
        idx_map[tails[i].str] = (uint32_t) i;
    }

    int64_t find(const uint8_t *s, size_t len) {
      std::unordered_map<std::string, uint32_t>::iterator it = idx_map.find(std::string((const char *) s, len));
      if (it == idx_map.end())
        return -1;
      return it->second;
    }

    // Width of pointer, assuming a not-found tail would be appended now
    int width(const uint8_t *s, size_t len) {
      int64_t idx = find(s, len);
      if (idx >= 0)
        return ptr_width(tails[(size_t) idx].ptr);
      return ptr_width(entry_ptr(area_size, s, len));
    }

    // Returns pointer, appending the tail (with lowest frequency) if new
    uint64_t get_or_add(const uint8_t *s, size_t len) {
      int64_t idx = find(s, len);
      if (idx >= 0)
        return tails[(size_t) idx].ptr;
      tail_info ti;
      ti.str = std::string((const char *) s, len);
      ti.freq = 0;
      ti.is_member = false;
      ti.ptr = entry_ptr(area_size, s, len);
      area_size += entry_size(s, len);
      idx_map[ti.str] = (uint32_t) tails.size();
      tails.push_back(ti);
      return ti.ptr;
    }

    void rollback(size_t tail_count, uint64_t old_area_size) {
      for (size_t i = tail_count; i < tails.size(); i++)
        idx_map.erase(tails[i].str);
      tails.resize(tail_count);
      area_size = old_area_size;
    }

    void write_area(std::vector<uint8_t>& out) {
      size_t start = out.size();
      for (size_t i = 0; i < tails.size(); i++) {
        tail_info& ti = tails[i];
        const uint8_t *s = (const uint8_t *) ti.str.data();
        size_t len = ti.str.length();
        if (max_tail_len < len + tail_skip)
          max_tail_len = (uint32_t) (len + tail_skip);
        if (ti.is_member)
          continue;
        if (entry_ptr(out.size() - start, s, len) != ti.ptr)
          fprintf(stderr, "madras_blk: tail ptr mismatch at %lu\n", (unsigned long) i);
        if (is_bin(s, len)) {
          // same as madras get_set_len_len(): least significant 3 bits first,
          // first byte marked with 0x08, pointer at last length byte
          size_t l = len;
          uint8_t first_byte = 0x08;
          do {
            out.push_back((uint8_t) ((l & 0x07) | 0x10 | first_byte));
            first_byte = 0;
            l >>= 3;
          } while (l > 0);
          out.insert(out.end(), s, s + len);
        } else {
          out.insert(out.end(), s, s + len);
          out.push_back(15);
        }
      }
      if (out.size() - start != area_size)
        fprintf(stderr, "madras_blk: tail area size mismatch %lu %lu\n",
            (unsigned long) (out.size() - start), (unsigned long) area_size);
    }
};

// Reads madras bin15 length backwards from t (same as madras read_len_bw)
static inline uint64_t read_len_bw(const uint8_t *t) {
  uint64_t out_len = 0;
  while (*t > 15 && *t < 32) {
    out_len <<= 3;
    out_len += (*t & 0x07);
    if (*t-- & 0x08)
      break;
  }
  return out_len;
}

// ---------------------------------------------------------------------------
// Block pricer: tracks exact leaf block size as sorted keys are added,
// mirroring how memtrie forms nodes and tails, without building the trie
// ---------------------------------------------------------------------------

class block_pricer {
  public:
    struct edge {
      uint32_t a;  // start depth (node byte at key[a])
      uint32_t b;  // end depth (exclusive)
      int w;       // tail pointer width (0 if no tail)
    };
    struct plan {
      int kind;
      size_t edge_idx;
      uint32_t l;
      uint32_t dn;
      int64_t dp;
      int w_new;
      int w_upper;
      int w_lower;
    };
    enum { PLAN_FIRST, PLAN_CHILD, PLAN_SIBLING, PLAN_SPLIT };

    std::vector<edge> path;
    std::string prev;
    uint32_t node_count;
    int64_t ptr_bytes;
    tail_store *ts;

    block_pricer(tail_store *_ts) : node_count (0), ptr_bytes (0), ts (_ts) {
    }

    void reset() {
      path.clear();
      prev.clear();
      node_count = 0;
      ptr_bytes = 0;
    }

    static size_t block_size_for(uint32_t n, int64_t p) {
      size_t grp_count = (n + 63) / 64;
      return MBX_LEAF_HDR_SIZE + grp_count * 40 + n + (size_t) p;
    }

    int edge_width(const uint8_t *k, uint32_t a, uint32_t b) {
      if (b - a > 1)
        return ts->width(k + a + ts->tail_skip, b - a - ts->tail_skip);
      return 0;
    }

    // Returns block size if key k were added
    size_t try_add(const uint8_t *k, uint32_t len, plan& pl) {
      if (path.size() == 0) {
        pl.kind = PLAN_FIRST;
        pl.dn = 1;
        pl.w_new = edge_width(k, 0, len);
        pl.dp = pl.w_new;
        return block_size_for(node_count + pl.dn, ptr_bytes + pl.dp);
      }
      const uint8_t *p = (const uint8_t *) prev.data();
      uint32_t plen = (uint32_t) prev.length();
      uint32_t l = lcp_len(p, plen, k, len);
      pl.l = l;
      if (l == plen) {
        // previous key is prefix of k: new child under last node
        pl.kind = PLAN_CHILD;
        pl.dn = 1;
        pl.w_new = edge_width(k, l, len);
        pl.dp = pl.w_new;
      } else {
        size_t i = path.size() - 1;
        while (i > 0 && path[i].a > l)
          i--;
        pl.edge_idx = i;
        edge& e = path[i];
        pl.w_new = edge_width(k, l, len);
        if (e.a == l) {
          pl.kind = PLAN_SIBLING;
          pl.dn = 1;
          pl.dp = pl.w_new;
        } else {
          pl.kind = PLAN_SPLIT;
          pl.dn = 2;
          pl.w_upper = edge_width(p, e.a, l);
          pl.w_lower = edge_width(p, l, e.b);
          pl.dp = (int64_t) pl.w_upper + pl.w_lower + pl.w_new - e.w;
        }
      }
      return block_size_for(node_count + pl.dn, ptr_bytes + pl.dp);
    }

    void commit(const uint8_t *k, uint32_t len, plan& pl) {
      edge ne;
      switch (pl.kind) {
        case PLAN_FIRST:
          ne.a = 0; ne.b = len; ne.w = pl.w_new;
          path.push_back(ne);
          break;
        case PLAN_CHILD:
          ne.a = pl.l; ne.b = len; ne.w = pl.w_new;
          path.push_back(ne);
          break;
        case PLAN_SIBLING:
          path.resize(pl.edge_idx);
          ne.a = pl.l; ne.b = len; ne.w = pl.w_new;
          path.push_back(ne);
          break;
        case PLAN_SPLIT:
          path.resize(pl.edge_idx + 1);
          path.back().b = pl.l;
          path.back().w = pl.w_upper;
          ne.a = pl.l; ne.b = len; ne.w = pl.w_new;
          path.push_back(ne);
          break;
      }
      node_count += pl.dn;
      ptr_bytes += pl.dp;
      prev.assign((const char *) k, len);
    }
};

// ---------------------------------------------------------------------------
// Builder
// ---------------------------------------------------------------------------

// Collects keys of an in-memory basix-like B+tree (cache_size == 0) in order.
// Key pointers point into the tree's blocks, so the tree must outlive the build.
template <class T>
void collect_basix_keys(T *bx, uint8_t *blk, std::vector<key_ref>& out) {
  bx->set_current_block(blk);
  int filled = bx->filled_size();
  if (bx->is_leaf()) {
    for (int i = 0; i < filled; i++) {
      int len;
      uint8_t *k = bx->get_key(i, &len);
      key_ref kr;
      kr.key = k;
      kr.len = (uint32_t) len;
      out.push_back(kr);
    }
    return;
  }
  for (int i = 0; i < filled; i++) {
    bx->set_current_block(blk);
    uint8_t *loc = bx->get_child_ptr_pos(i);
    uint8_t *child = bx->get_child_ptr(loc);
    collect_basix_keys(bx, child, out);
  }
}

class builder {
  public:
    struct leaf_node {
      uint8_t b;
      uint8_t flags; // bit per MBX_FLAG_*
      uint64_t ptr;
    };
    struct parent_entry {
      std::string key;
      uint32_t child;
    };

    bldr_options opts;
    tail_store ts;
    std::vector<uint8_t> leaf_blocks;
    std::vector<std::string> leaf_seps;
    std::vector< std::vector< std::vector<parent_entry> > > parent_levels; // [level-1][block][entry]
    uint64_t total_nodes;
    uint64_t total_node_sets;
    uint64_t key_count;
    uint32_t max_key_len;
    uint32_t leaf_count;
    uint32_t rebuild_count;
    uint32_t price_mismatch_count;
    int child_width;
    std::vector<uint8_t> out;

    builder(const bldr_options& _opts = dflt_opts) : opts (_opts) {
      ts.set_mode(opts.node_byte_mode, opts.tail_sfx);
      total_nodes = total_node_sets = key_count = 0;
      max_key_len = leaf_count = rebuild_count = price_mismatch_count = 0;
      child_width = 1;
    }

    template <class T>
    std::vector<uint8_t>& build_from_basix(T *bx) {
      std::vector<key_ref> keys;
      collect_basix_keys(bx, bx->root_block, keys);
      return build(keys);
    }

    // keys must be sorted and unique
    std::vector<uint8_t>& build(std::vector<key_ref>& in_keys) {
      if (opts.max_inner_tries > 0)
        fprintf(stderr, "madras_blk: inner tries not implemented yet, using plain tails\n");
      std::vector<key_ref> keys;
      normalize_keys(in_keys, keys);
      key_count = keys.size();
      pass1_tails(keys);
      pass2_leaves(keys);
      build_parents();
      serialize();
      return out;
    }

    size_t size() {
      return out.size();
    }

    bool write_file(const char *filename) {
      FILE *fp = fopen(filename, "wb");
      if (fp == NULL)
        return false;
      size_t written = fwrite(out.data(), 1, out.size(), fp);
      fclose(fp);
      return written == out.size();
    }

  private:
    // madras logic: empty key stored as empty_value
    void normalize_keys(std::vector<key_ref>& in_keys, std::vector<key_ref>& keys) {
      keys = in_keys;
      bool has_empty = false;
      for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].len == 0) {
          keys[i].key = empty_key_value;
          keys[i].len = (uint32_t) empty_key_value_len;
          has_empty = true;
        }
        if (max_key_len < keys[i].len)
          max_key_len = keys[i].len;
      }
      if (has_empty) {
        // empty_value may collide with a real key (same as madras)
        std::sort(keys.begin(), keys.end(), key_ref_less);
        keys.erase(std::unique(keys.begin(), keys.end(), key_ref_equal), keys.end());
      }
    }

    static bool key_ref_equal(const key_ref& lhs, const key_ref& rhs) {
      return compare_keys(lhs.key, lhs.len, rhs.key, rhs.len) == 0;
    }

    static bool key_ref_less(const key_ref& lhs, const key_ref& rhs) {
      return compare_keys(lhs.key, lhs.len, rhs.key, rhs.len) < 0;
    }

    template <class F>
    static void walk_memtrie(mtrie::in_mem_trie& trie, F& visit) {
      if (trie.all_node_sets.size() <= 1)
        return; // empty trie
      std::vector<uintxx_t> queue;
      queue.push_back(1);
      for (size_t qi = 0; qi < queue.size(); qi++) {
        mtrie::node_set_handler nsh(trie.all_node_sets, queue[qi]);
        int last = nsh.last_node_idx();
        for (int i = 0; i <= last; i++) {
          mtrie::node n = nsh[i];
          uint8_t mflags = n.get_flags();
          const uint8_t *tail = NULL;
          size_t tail_len = 0;
          if (mflags & NFLAG_TAIL) {
            size_t vlen;
            uint8_t *t = (*trie.all_tails)[n.get_tail()];
            tail_len = gen::read_fast_vuint64(t, vlen);
            tail = t + vlen;
          }
          visit(n.get_byte(), mflags, tail, tail_len);
          if (mflags & NFLAG_CHILD)
            queue.push_back(n.get_child());
        }
      }
    }

    struct freq_visitor {
      tail_store *ts;
      void operator()(uint8_t b, uint8_t mflags, const uint8_t *tail, size_t tail_len) {
        (void) b;
        if (mflags & NFLAG_TAIL)
          ts->add_freq(tail + ts->tail_skip, tail_len - ts->tail_skip);
      }
    };

    // Pass 1: global memtrie to discover tails and their frequencies
    void pass1_tails(std::vector<key_ref>& keys) {
      mtrie::in_mem_trie *trie = new mtrie::in_mem_trie();
      for (size_t i = 0; i < keys.size(); i++)
        trie->insert(keys[i].key, keys[i].len);
      freq_visitor fv;
      fv.ts = &ts;
      walk_memtrie(*trie, fv);
      delete trie;
      ts.finalize_order();
    }

    struct node_collector {
      std::vector<leaf_node> *nodes;
      tail_store *ts;
      int64_t ptr_bytes;
      uint32_t key_count;
      void operator()(uint8_t b, uint8_t mflags, const uint8_t *tail, size_t tail_len) {
        leaf_node ln;
        ln.b = b;
        ln.flags = 0;
        ln.ptr = 0;
        if (mflags & NFLAG_TAIL) {
          ln.flags |= (1 << MBX_FLAG_TAIL);
          ln.ptr = ts->get_or_add(tail + ts->tail_skip, tail_len - ts->tail_skip);
          ptr_bytes += ts->ptr_width(ln.ptr);
          if (ts->ptr_shift > 0)
            ln.b = (uint8_t) (ln.ptr & 0xFF);
        }
        if (mflags & NFLAG_TERM)
          ln.flags |= (1 << MBX_FLAG_TERM);
        if (mflags & NFLAG_CHILD)
          ln.flags |= (1 << MBX_FLAG_CHILD);
        if (mflags & NFLAG_LEAF) {
          ln.flags |= (1 << MBX_FLAG_LEAF);
          key_count++;
        }
        nodes->push_back(ln);
      }
    };

    // Builds leaf block for keys [s, e). Returns false (and rolls back new
    // tails) if it does not fit.
    bool build_leaf_block(mtrie::in_mem_trie& trie, std::vector<key_ref>& keys,
          size_t s, size_t e, uint32_t priced_n, int64_t priced_p) {
      trie.reset();
      for (size_t i = s; i < e; i++)
        trie.insert(keys[i].key, keys[i].len);
      size_t tail_count = ts.tails.size();
      uint64_t old_area_size = ts.area_size;
      std::vector<leaf_node> nodes;
      node_collector nc;
      nc.nodes = &nodes;
      nc.ts = &ts;
      nc.ptr_bytes = 0;
      nc.key_count = 0;
      walk_memtrie(trie, nc);
      uint32_t n = (uint32_t) nodes.size();
      if (priced_n != UINT32_MAX && (n != priced_n || nc.ptr_bytes != priced_p))
        price_mismatch_count++;
      size_t blk_size = block_pricer::block_size_for(n, nc.ptr_bytes);
      if (blk_size > opts.leaf_blk_size || n > 65535) {
        ts.rollback(tail_count, old_area_size);
        return false;
      }
      write_leaf_block(nodes, nc.key_count, (uint32_t) nc.ptr_bytes);
      total_nodes += n;
      return true;
    }

    void write_leaf_block(std::vector<leaf_node>& nodes, uint32_t blk_key_count, uint32_t ptr_bytes) {
      size_t blk_start = leaf_blocks.size();
      leaf_blocks.resize(blk_start + opts.leaf_blk_size, 0);
      uint8_t *blk = leaf_blocks.data() + blk_start;
      uint32_t n = (uint32_t) nodes.size();
      uint32_t grp_count = (n + 63) / 64;
      blk[0] = MBX_BLK_TYPE_LEAF;
      blk[1] = 0;
      put_u16(blk + 2, n);
      put_u16(blk + 4, blk_key_count);
      put_u16(blk + 6, ptr_bytes);
      uint8_t *child_rank = blk + MBX_LEAF_HDR_SIZE;
      uint8_t *term_rank = child_rank + grp_count * 2;
      uint8_t *leaf_rank = term_rank + grp_count * 2;
      uint8_t *ptr_off = leaf_rank + grp_count * 2;
      uint8_t *flags = ptr_off + grp_count * 2;
      uint8_t *node_bytes = flags + grp_count * 32;
      uint8_t *ptrs = node_bytes + n;
      uint32_t child_cnt = 0, term_cnt = 0, leaf_cnt = 0, ptr_pos = 0;
      for (uint32_t i = 0; i < n; i++) {
        uint32_t g = i / 64;
        if ((i % 64) == 0) {
          put_u16(child_rank + g * 2, child_cnt);
          put_u16(term_rank + g * 2, term_cnt);
          put_u16(leaf_rank + g * 2, leaf_cnt);
          put_u16(ptr_off + g * 2, ptr_pos);
        }
        leaf_node& ln = nodes[i];
        uint64_t bit = (uint64_t) 1 << (i % 64);
        for (int f = 0; f < 4; f++) {
          if (ln.flags & (1 << f)) {
            uint8_t *wp = flags + g * 32 + f * 8;
            put_u64(wp, get_u64(wp) | bit);
          }
        }
        if (ln.flags & (1 << MBX_FLAG_CHILD))
          child_cnt++;
        if (ln.flags & (1 << MBX_FLAG_TERM)) {
          term_cnt++;
          total_node_sets++;
        }
        if (ln.flags & (1 << MBX_FLAG_LEAF))
          leaf_cnt++;
        node_bytes[i] = ln.b;
        if (ln.flags & (1 << MBX_FLAG_TAIL))
          ptr_pos += (uint32_t) write_fvint(ptrs + ptr_pos, ln.ptr >> ts.ptr_shift);
      }
    }

    // Pass 2: pack sorted keys into leaf blocks
    void pass2_leaves(std::vector<key_ref>& keys) {
      mtrie::in_mem_trie *trie = new mtrie::in_mem_trie();
      block_pricer pricer(&ts);
      size_t s = 0;
      size_t key_cnt = keys.size();
      while (s < key_cnt) {
        pricer.reset();
        size_t e = s;
        while (e < key_cnt) {
          block_pricer::plan pl;
          size_t sz = pricer.try_add(keys[e].key, keys[e].len, pl);
          if (e > s && (sz > opts.leaf_blk_size || pricer.node_count + pl.dn > 65535))
            break;
          pricer.commit(keys[e].key, keys[e].len, pl);
          e++;
        }
        uint32_t priced_n = pricer.node_count;
        int64_t priced_p = pricer.ptr_bytes;
        while (!build_leaf_block(*trie, keys, s, e, priced_n, priced_p)) {
          rebuild_count++;
          if (e - s == 1) {
            fprintf(stderr, "madras_blk: key too large for leaf block\n");
            delete trie;
            return;
          }
          e--;
          priced_n = UINT32_MAX; // not tracked for rebuilds
        }
        // separator for this leaf block
        if (leaf_count == 0) {
          leaf_seps.push_back(std::string());
        } else {
          key_ref& last = keys[s - 1];
          key_ref& first = keys[s];
          uint32_t l = lcp_len(last.key, last.len, first.key, first.len);
          leaf_seps.push_back(std::string((const char *) first.key, l + 1));
        }
        leaf_count++;
        s = e;
      }
      delete trie;
    }

    size_t parent_blk_used(std::vector<parent_entry>& blk) {
      size_t sz = MBX_PARENT_HDR_SIZE;
      for (size_t i = 0; i < blk.size(); i++)
        sz += 2 + blk[i].key.length() + child_width;
      return sz;
    }

    void pack_parent_level(std::vector<parent_entry>& entries, std::vector< std::vector<parent_entry> >& blocks) {
      std::vector<parent_entry> cur;
      size_t cur_size = MBX_PARENT_HDR_SIZE;
      for (size_t i = 0; i < entries.size(); i++) {
        size_t esz = 2 + entries[i].key.length() + child_width;
        if (cur.size() > 0 && cur_size + esz > opts.parent_blk_size) {
          blocks.push_back(cur);
          cur.clear();
          cur_size = MBX_PARENT_HDR_SIZE;
        }
        if (MBX_PARENT_HDR_SIZE + esz > opts.parent_blk_size)
          fprintf(stderr, "madras_blk: separator too large for parent block\n");
        cur.push_back(entries[i]);
        cur_size += esz;
      }
      if (cur.size() > 0)
        blocks.push_back(cur);
    }

    void build_parents() {
      child_width = bytes_needed(leaf_count);
      std::vector<parent_entry> entries;
      for (uint32_t i = 0; i < leaf_count; i++) {
        parent_entry pe;
        pe.key = leaf_seps[i];
        pe.child = i;
        entries.push_back(pe);
      }
      do {
        std::vector< std::vector<parent_entry> > blocks;
        pack_parent_level(entries, blocks);
        parent_levels.push_back(blocks);
        entries.clear();
        if (blocks.size() > 1) {
          for (size_t i = 0; i < blocks.size(); i++) {
            parent_entry pe;
            pe.key = blocks[i][0].key;
            pe.child = (uint32_t) i;
            entries.push_back(pe);
          }
        }
      } while (entries.size() > 0);
    }

    void write_parent_block(uint8_t *blk, int level, std::vector<parent_entry>& entries, uint32_t child_base) {
      blk[0] = (uint8_t) level;
      blk[1] = (uint8_t) child_width;
      put_u16(blk + 2, (uint32_t) entries.size());
      uint32_t rec_pos = MBX_PARENT_HDR_SIZE + (uint32_t) entries.size() * 2;
      for (size_t i = 0; i < entries.size(); i++) {
        put_u16(blk + MBX_PARENT_HDR_SIZE + i * 2, rec_pos);
        parent_entry& pe = entries[i];
        memcpy(blk + rec_pos, pe.key.data(), pe.key.length());
        rec_pos += (uint32_t) pe.key.length();
        put_uint_n(blk + rec_pos, child_base + pe.child, child_width);
        rec_pos += child_width;
      }
      put_u16(blk + 4, rec_pos);
    }

    void write_meta(uint8_t *meta, mbx_section& sec) {
      const char *magic = "Madras Sorcery Blocks DB Format 1.0";
      memcpy(meta, magic, strlen(magic));
      meta[36] = MBX_MAGIC_BYTE;
      meta[37] = 1; // version
      meta[38] = 1; // pk_col_count
      meta[39] = 0; // trie_level
      put_u16(meta + 40, ts.max_tail_len > 0 ? ts.max_tail_len - 1 : 0);
      put_u16(meta + 42, 0); // max_level
      put_u64(meta + 72, key_count);   // row_count
      put_u64(meta + 80, total_nodes); // node_count
      put_u64(meta + 88, MBX_OPTS_REC_SIZE); // opts_size
      put_u64(meta + 96, total_node_sets);
      put_u64(meta + 104, key_count);
      put_u64(meta + 112, max_key_len);
      uint8_t *o = meta + MBX_MDX_HEADER_SIZE;
      o[0] = opts.max_inner_tries > 0 ? 1 : 0; // inner_tries
      o[12] = opts.max_inner_tries;
      o[20] = 1; // opts_count
      memcpy(meta + MBX_SECTION_LOC, &sec, sizeof(sec));
    }

    void serialize() {
      out.clear();
      mbx_section sec;
      memset(&sec, 0, sizeof(sec));
      sec.leaf_blk_size = opts.leaf_blk_size;
      sec.parent_blk_size = opts.parent_blk_size;
      sec.leaf_count = leaf_count;
      sec.parent_levels = (uint8_t) parent_levels.size();
      sec.child_width = (uint8_t) child_width;
      sec.node_byte_mode = opts.node_byte_mode;
      sec.tail_sfx = opts.tail_sfx;
      size_t meta_size = opts.leaf_blk_size;
      if (meta_size < MBX_SECTION_LOC + sizeof(sec))
        meta_size = MBX_SECTION_LOC + sizeof(sec);
      // parent blocks: top level first
      std::vector<uint32_t> level_start(parent_levels.size());
      uint32_t parent_count = 0;
      for (size_t lvl = parent_levels.size(); lvl-- > 0;) {
        level_start[lvl] = parent_count;
        parent_count += (uint32_t) parent_levels[lvl].size();
      }
      sec.parent_count = parent_count;
      sec.parent_start = meta_size;
      sec.leaf_start = sec.parent_start + (uint64_t) parent_count * opts.parent_blk_size;
      sec.tail_area_loc = sec.leaf_start + (uint64_t) leaf_count * opts.leaf_blk_size;
      out.resize(sec.tail_area_loc, 0);
      for (size_t lvl = parent_levels.size(); lvl-- > 0;) {
        uint32_t child_base = (lvl == 0 ? 0 : level_start[lvl - 1]);
        for (size_t b = 0; b < parent_levels[lvl].size(); b++) {
          uint8_t *blk = out.data() + sec.parent_start + (uint64_t) (level_start[lvl] + b) * opts.parent_blk_size;
          write_parent_block(blk, (int) lvl + 1, parent_levels[lvl][b], child_base);
        }
      }
      memcpy(out.data() + sec.leaf_start, leaf_blocks.data(), leaf_blocks.size());
      ts.write_area(out);
      sec.tail_area_size = out.size() - sec.tail_area_loc;
      out.resize(out.size() + 8, 0); // padding for word-at-a-time reads
      sec.total_size = out.size();
      write_meta(out.data(), sec);
      std::vector<uint8_t>().swap(leaf_blocks);
    }
};

// ---------------------------------------------------------------------------
// Reader
// ---------------------------------------------------------------------------

class blk_trie {
  private:
    const uint8_t *data;
    std::vector<uint8_t> owned;
    mbx_section sec;
    const uint8_t *tail_area;

    static inline uint64_t get_word(const uint8_t *flags, uint32_t g, int f) {
      return get_u64(flags + g * 32 + f * 8);
    }

    // position of k-th (1 based) set bit, using cumulative 2-byte rank array
    static inline uint32_t select1(const uint8_t *rank_arr, const uint8_t *flags,
          int f, uint32_t grp_count, uint32_t k) {
      uint32_t lo = 0;
      uint32_t hi = grp_count - 1;
      while (lo < hi) {
        uint32_t mid = (lo + hi + 1) >> 1;
        if (get_u16(rank_arr + mid * 2) < k)
          lo = mid;
        else
          hi = mid - 1;
      }
      uint32_t r = k - get_u16(rank_arr + lo * 2);
      return lo * 64 + (uint32_t) select_in_word(get_word(flags, lo, f), r);
    }

    inline bool match_tail(uint64_t ptr, const uint8_t *key, uint32_t key_len, uint32_t& key_pos) const {
      const uint8_t *t = tail_area + ptr;
      if (is_text_byte(*t)) {
        do {
          if (key_pos >= key_len || *t != key[key_pos])
            return false;
          t++;
          key_pos++;
        } while (is_text_byte(*t));
        return true;
      }
      uint64_t bin_len = read_len_bw(t);
      if (key_pos + bin_len > key_len)
        return false;
      if (memcmp(key + key_pos, t + 1, bin_len) != 0)
        return false;
      key_pos += (uint32_t) bin_len;
      return true;
    }

  public:
    blk_trie() : data (NULL), tail_area (NULL) {
      memset(&sec, 0, sizeof(sec));
    }

    bool load(const uint8_t *buf, size_t buf_size) {
      if (buf_size < MBX_SECTION_LOC + sizeof(sec) || buf[36] != MBX_MAGIC_BYTE)
        return false;
      data = buf;
      memcpy(&sec, data + MBX_SECTION_LOC, sizeof(sec));
      if (sec.total_size != buf_size)
        return false;
      tail_area = data + sec.tail_area_loc;
      return true;
    }

    bool load_file(const char *filename) {
      FILE *fp = fopen(filename, "rb");
      if (fp == NULL)
        return false;
      fseek(fp, 0, SEEK_END);
      long sz = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      owned.resize((size_t) sz);
      size_t rd = fread(owned.data(), 1, owned.size(), fp);
      fclose(fp);
      if (rd != owned.size())
        return false;
      return load(owned.data(), owned.size());
    }

    uint32_t get_leaf_count() const {
      return sec.leaf_count;
    }

    uint32_t get_parent_count() const {
      return sec.parent_count;
    }

    // Returns entry index of last separator <= key
    static int search_parent(const uint8_t *blk, const uint8_t *key, uint32_t key_len) {
      int cnt = (int) get_u16(blk + 2);
      int w = blk[1];
      uint32_t rec_end = get_u16(blk + 4);
      int lo = 0;
      int hi = cnt;  // first entry with sep > key
      while (lo < hi) {
        int mid = (lo + hi) >> 1;
        uint32_t off = get_u16(blk + MBX_PARENT_HDR_SIZE + mid * 2);
        uint32_t next = (mid + 1 < cnt ? get_u16(blk + MBX_PARENT_HDR_SIZE + (mid + 1) * 2) : rec_end);
        uint32_t sep_len = next - off - w;
        if (compare_keys(blk + off, sep_len, key, key_len) <= 0)
          lo = mid + 1;
        else
          hi = mid;
      }
      return lo > 0 ? lo - 1 : 0;
    }

    uint32_t find_leaf_block(const uint8_t *key, uint32_t key_len) const {
      uint32_t blk_no = 0; // root is first parent block
      for (int lvl = sec.parent_levels; lvl > 0; lvl--) {
        const uint8_t *blk = data + sec.parent_start + (uint64_t) blk_no * sec.parent_blk_size;
        int idx = search_parent(blk, key, key_len);
        int w = blk[1];
        uint32_t next = (idx + 1 < (int) get_u16(blk + 2) ? get_u16(blk + MBX_PARENT_HDR_SIZE + (idx + 1) * 2) : get_u16(blk + 4));
        blk_no = (uint32_t) get_uint_n(blk + next - w, w);
      }
      return blk_no;
    }

    const uint8_t *get_leaf_block(uint32_t leaf_no) const {
      return data + sec.leaf_start + (uint64_t) leaf_no * sec.leaf_blk_size;
    }

    inline uint8_t first_tail_byte(uint64_t ptr) const {
      const uint8_t *t = tail_area + ptr;
      return is_text_byte(*t) ? *t : t[1];
    }

    // node byte = first tail byte (same as v1)
    bool lookup_in_leaf_nb_first(const uint8_t *blk, const uint8_t *key, uint32_t key_len) const {
      uint32_t n = get_u16(blk + 2);
      uint32_t grp_count = (n + 63) / 64;
      const uint8_t *child_rank = blk + MBX_LEAF_HDR_SIZE;
      const uint8_t *term_rank = child_rank + grp_count * 2;
      const uint8_t *ptr_off = term_rank + grp_count * 4;
      const uint8_t *flags = ptr_off + grp_count * 2;
      const uint8_t *node_bytes = flags + grp_count * 32;
      const uint8_t *ptrs = node_bytes + n;
      uint32_t node_id = 0;
      uint32_t key_pos = 0;
      while (true) {
        uint8_t kb = key[key_pos];
        uint32_t g, bit;
        while (true) {
          uint8_t nb = node_bytes[node_id];
          g = node_id / 64;
          bit = node_id % 64;
          if (nb == kb)
            break;
          if (nb > kb)
            return false;
          if (get_word(flags, g, MBX_FLAG_TERM) & ((uint64_t) 1 << bit))
            return false;
          node_id++;
        }
        uint64_t mask = ((uint64_t) 1 << bit) - 1;
        uint64_t tail_w = get_word(flags, g, MBX_FLAG_TAIL);
        if (tail_w & ((uint64_t) 1 << bit)) {
          int skip = __builtin_popcountll(tail_w & mask);
          const uint8_t *p = skip_fvints(ptrs + get_u16(ptr_off + g * 2), skip);
          size_t vlen;
          uint64_t tail_ptr = read_fvint(p, vlen);
          key_pos++;
          if (!match_tail(tail_ptr, key, key_len, key_pos))
            return false;
        } else {
          key_pos++;
        }
        if (key_pos == key_len)
          return (get_word(flags, g, MBX_FLAG_LEAF) >> bit) & 1;
        uint64_t child_w = get_word(flags, g, MBX_FLAG_CHILD);
        if ((child_w & ((uint64_t) 1 << bit)) == 0)
          return false;
        uint32_t child_cnt = get_u16(child_rank + g * 2) + (uint32_t) __builtin_popcountll(child_w & mask);
        node_id = select1(term_rank, flags, MBX_FLAG_TERM, grp_count, child_cnt + 1) + 1;
      }
    }

    // node byte = low 8 bits of tail ptr: tail pointers decoded while
    // scanning siblings to get first byte of each tail
    bool lookup_in_leaf_nb_ptr(const uint8_t *blk, const uint8_t *key, uint32_t key_len) const {
      uint32_t n = get_u16(blk + 2);
      uint32_t grp_count = (n + 63) / 64;
      const uint8_t *child_rank = blk + MBX_LEAF_HDR_SIZE;
      const uint8_t *term_rank = child_rank + grp_count * 2;
      const uint8_t *ptr_off = term_rank + grp_count * 4;
      const uint8_t *flags = ptr_off + grp_count * 2;
      const uint8_t *node_bytes = flags + grp_count * 32;
      const uint8_t *ptrs = node_bytes + n;
      uint32_t node_id = 0;
      uint32_t key_pos = 0;
      while (true) {
        uint8_t kb = key[key_pos];
        uint32_t g = node_id / 64;
        uint32_t bit = node_id % 64;
        uint64_t tail_w = get_word(flags, g, MBX_FLAG_TAIL);
        uint64_t term_w = get_word(flags, g, MBX_FLAG_TERM);
        // cursor to this node's tail pointer, advanced while scanning siblings
        int skip = __builtin_popcountll(tail_w & (((uint64_t) 1 << bit) - 1));
        const uint8_t *p = skip_fvints(ptrs + get_u16(ptr_off + g * 2), skip);
        uint64_t tail_ptr = 0;
        bool has_tail;
        while (true) {
          has_tail = (tail_w >> bit) & 1;
          uint8_t nb = node_bytes[node_id];
          if (has_tail) {
            size_t vlen;
            tail_ptr = (read_fvint(p, vlen) << 8) | nb;
            p += vlen;
            nb = first_tail_byte(tail_ptr);
          }
          if (nb == kb)
            break;
          if (nb > kb)
            return false;
          if ((term_w >> bit) & 1)
            return false;
          node_id++;
          bit++;
          if (bit == 64) {
            g++;
            bit = 0;
            tail_w = get_word(flags, g, MBX_FLAG_TAIL);
            term_w = get_word(flags, g, MBX_FLAG_TERM);
            p = ptrs + get_u16(ptr_off + g * 2);
          }
        }
        if (has_tail) {
          // whole tail including first byte is in tail area
          if (!match_tail(tail_ptr, key, key_len, key_pos))
            return false;
        } else {
          key_pos++;
        }
        if (key_pos == key_len)
          return (get_word(flags, g, MBX_FLAG_LEAF) >> bit) & 1;
        uint64_t child_w = get_word(flags, g, MBX_FLAG_CHILD);
        if (((child_w >> bit) & 1) == 0)
          return false;
        uint32_t child_cnt = get_u16(child_rank + g * 2)
            + (uint32_t) __builtin_popcountll(child_w & (((uint64_t) 1 << bit) - 1));
        node_id = select1(term_rank, flags, MBX_FLAG_TERM, grp_count, child_cnt + 1) + 1;
      }
    }

    bool lookup_in_leaf(const uint8_t *blk, const uint8_t *key, uint32_t key_len) const {
      if (sec.node_byte_mode == MBX_NODE_BYTE_PTR)
        return lookup_in_leaf_nb_ptr(blk, key, key_len);
      return lookup_in_leaf_nb_first(blk, key, key_len);
    }

    bool lookup(const uint8_t *key, size_t key_len) const {
      if (key_len == 0) {
        key = empty_key_value;
        key_len = empty_key_value_len;
      }
      if (sec.leaf_count == 0)
        return false;
      uint32_t leaf_no = find_leaf_block(key, (uint32_t) key_len);
      return lookup_in_leaf(get_leaf_block(leaf_no), key, (uint32_t) key_len);
    }

    bool lookup(const char *key, size_t key_len) const {
      return lookup((const uint8_t *) key, key_len);
    }
};

} // namespace madras_blk

#endif
