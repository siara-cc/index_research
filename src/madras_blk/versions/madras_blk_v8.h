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

#define MBX_HAS_LEVELS 1
#define MBX_HAS_REVERSE 1

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
  uint8_t sel_shift;         // select lookup: one entry per (1 << sel_shift) set bits, 0 = none
};

static const bldr_options dflt_opts = { 0, 4096, 4096, MBX_NODE_BYTE_FIRST, 0, 6 };

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
//   [8..9]  node set (term bit) count, only with select lookup (header 12 bytes)
//   [..]    child_rank[G], term_rank[G], leaf_rank[G], ptr_off[G]  (2 bytes each)
//           with select lookup: term_sel, child_sel, leaf_sel (1 byte each),
//             entry j = (group of set bit number (j << sel_shift) + 1) >> sel_grp_shift
//           flags: per group 4 x uint64 (tail, term, child, leaf)
//           node bytes[n]
//           tail pointers (fvint), ptr_off[g] = offset of group g's first pointer
#define MBX_LEAF_HDR_SIZE 8
#define MBX_LEAF_HDR_SIZE_SEL 12  // with select lookup: [8..9] node set (term) count
#define MBX_FORMAT_VER 2          // meta[37]. Select entries are 1 byte from version 2;
                                  // version 1 files with select lookup are not readable

// Select entries are 1 byte holding (group >> sel_grp_shift), so that all
// groups of a block fit: up to 16 KB blocks exact group, 32 KB 2 groups,
// 64 KB 4 groups.
static inline int sel_grp_shift(uint32_t blk_size) {
  uint32_t max_grp = blk_size / 64;
  int shift = 0;
  while (max_grp > 0 && ((max_grp - 1) >> shift) > 255)
    shift++;
  return shift;
}
#define MBX_BLK_TYPE_LEAF 0x80
#define MBX_BLK_TYPE_INNER 0x40  // inner trie block, same layout as leaf block
#define MBX_MAX_LEVELS 8
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
  uint8_t level_count;   // tail levels: inner tries + 1 plain tail level
  uint8_t has_main_lt;   // main node id and leaf id tables present (after level descriptors)
  uint8_t sel_shift;     // select lookup entry per (1 << sel_shift) set bits, 0 = none
  uint8_t leaf_hdr_size; // 0 means MBX_LEAF_HDR_SIZE (files before v6)
};

// Tail level k (1 based) descriptor, stored after mbx_section. Pointers to an
// inner trie level are leaf ids (ordinal of the node where a tail ends);
// pointers to the plain level are byte offsets into the tail area.
// Inner trie blocks have the same layout as main leaf blocks, the leaf bit
// marking nodes where a (reversed) tail ends.
struct mbx_level_desc {
  uint64_t blk_start;    // inner: offset of first block; plain: tail area offset
  uint64_t lt_loc;       // inner: cumulative leaf id count per block (uint32, blk_count + 1)
  uint64_t sample_loc;   // inner: uint32 block number for every (1 << sample_shift) ids
  uint32_t blk_count;
  uint32_t id_count;
  uint8_t is_trie;
  uint8_t sample_shift;
  uint8_t reserved[6];
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

static inline uint32_t get_u32(const uint8_t *p) {
  uint32_t v;
  memcpy(&v, p, 4);
  return v;
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

// Same as madras input_ctx
struct input_ctx {
  const uint8_t *key;
  uint32_t key_len;
  uint32_t key_pos;
  uintxx_t node_id;
};

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
    bool is_trie;    // tails go to an inner trie: ptr = frequency rank until
                     // the inner trie assigns leaf ids (same width class)

    tail_store() : area_size (0), max_tail_len (0), ptr_shift (0), tail_skip (1),
          sfx (false), is_trie (false) {
    }

    // Start of width class c: ids with the same pointer width
    uint64_t class_start(int c) const {
      if (c == 0)
        return 0;
      return (uint64_t) 1 << (7 * c + ptr_shift);
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
      if (is_trie) {
        for (size_t i = 0; i < tails.size(); i++)
          tails[i].ptr = i;
      } else if (sfx && tails.size() > 0) {
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
      if (is_trie)
        return ptr_width(tails.size());
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
      if (is_trie) {
        ti.ptr = tails.size();
      } else {
        ti.ptr = entry_ptr(area_size, s, len);
        area_size += entry_size(s, len);
      }
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
      uint32_t dt;  // new node sets
    };
    enum { PLAN_FIRST, PLAN_CHILD, PLAN_SIBLING, PLAN_SPLIT };

    std::vector<edge> path;
    std::string prev;
    uint32_t node_count;
    int64_t ptr_bytes;
    tail_store *ts;
    bool rev;  // inner trie keys are reversed tails; their tails are stored reversed back
    std::string rev_buf;

    block_pricer(tail_store *_ts, bool _rev, int _sel_shift) : node_count (0), ptr_bytes (0),
          ts (_ts), rev (_rev), set_count (0), key_count (0), sel_shift (_sel_shift) {
    }

    void reset() {
      path.clear();
      prev.clear();
      node_count = 0;
      ptr_bytes = 0;
      set_count = 0;
      key_count = 0;
    }

    uint32_t set_count;   // node sets = term bits
    uint32_t key_count;   // leaf bits
    int sel_shift;

    static size_t sel_entries(uint32_t bit_count, int shift) {
      return (bit_count + ((size_t) 1 << shift) - 1) >> shift;
    }

    // n nodes, p pointer bytes, t node sets (term bits, child bits = t - 1),
    // l leaves
    static size_t block_size_for(uint32_t n, int64_t p, uint32_t t, uint32_t l, int shift) {
      size_t grp_count = (n + 63) / 64;
      size_t sz = grp_count * 40 + n + (size_t) p;
      if (shift == 0)
        return MBX_LEAF_HDR_SIZE + sz;
      size_t sel = sel_entries(t, shift) + sel_entries(t > 0 ? t - 1 : 0, shift) + sel_entries(l, shift);
      return MBX_LEAF_HDR_SIZE_SEL + sz + sel; // 1 byte per entry
    }

    size_t size_after(plan& pl) {
      return block_size_for(node_count + pl.dn, ptr_bytes + pl.dp, set_count + pl.dt, key_count + 1, sel_shift);
    }

    int edge_width(const uint8_t *k, uint32_t a, uint32_t b) {
      if (b - a <= 1)
        return 0;
      if (rev) {
        rev_buf.assign((const char *) k + a, b - a);
        std::reverse(rev_buf.begin(), rev_buf.end());
        return ts->width((const uint8_t *) rev_buf.data(), rev_buf.length());
      }
      return ts->width(k + a + ts->tail_skip, b - a - ts->tail_skip);
    }

    // Returns block size if key k were added
    size_t try_add(const uint8_t *k, uint32_t len, plan& pl) {
      if (path.size() == 0) {
        pl.kind = PLAN_FIRST;
        pl.dn = 1;
        pl.dt = 1; // root node set
        pl.w_new = edge_width(k, 0, len);
        pl.dp = pl.w_new;
        return size_after(pl);
      }
      const uint8_t *p = (const uint8_t *) prev.data();
      uint32_t plen = (uint32_t) prev.length();
      uint32_t l = lcp_len(p, plen, k, len);
      pl.l = l;
      if (l == plen) {
        // previous key is prefix of k: new child under last node
        pl.kind = PLAN_CHILD;
        pl.dn = 1;
        pl.dt = 1; // child node set under last node
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
          pl.dt = 0;
          pl.dp = pl.w_new;
        } else {
          pl.kind = PLAN_SPLIT;
          pl.dn = 2;
          pl.dt = 1; // child node set of the split node
          pl.w_upper = edge_width(p, e.a, l);
          pl.w_lower = edge_width(p, l, e.b);
          pl.dp = (int64_t) pl.w_upper + pl.w_lower + pl.w_new - e.w;
        }
      }
      return size_after(pl);
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
      set_count += pl.dt;
      key_count++;
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
    // Level 0 is the main trie. Level k >= 1 is the inner trie holding the
    // reversed tails of level k - 1 (stores[k - 1]).
    struct trie_level {
      std::vector<key_ref> keys;
      std::vector<std::string> key_strs;      // inner: reversed tails
      std::vector<uint32_t> key_rank;         // inner: tail index in stores[k - 1]
      std::vector<size_t> class_start;        // key index where each width class starts (+ end)
      std::vector<size_t> blk_start;          // key index where each block starts (+ end)
      std::vector<size_t> node_start;         // main: node id where each block starts (+ end)
      std::vector<uint8_t> blocks;
    };

    bldr_options opts;
    std::vector<tail_store *> stores;  // stores[k]: tails of level k nodes
    std::vector<trie_level> levels;
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
      total_nodes = total_node_sets = key_count = 0;
      max_key_len = leaf_count = rebuild_count = price_mismatch_count = 0;
      child_width = 1;
      if (opts.max_inner_tries + 1 > MBX_MAX_LEVELS)
        opts.max_inner_tries = MBX_MAX_LEVELS - 1;
      if (opts.max_inner_tries > 0 && opts.node_byte_mode != MBX_NODE_BYTE_PTR) {
        fprintf(stderr, "madras_blk: inner tries need node_byte_mode PTR, switching\n");
        opts.node_byte_mode = MBX_NODE_BYTE_PTR;
      }
      int level_count = opts.max_inner_tries + 1;
      for (int i = 0; i < level_count; i++) {
        tail_store *ts = new tail_store();
        ts->set_mode(opts.node_byte_mode, opts.tail_sfx);
        ts->is_trie = (i + 1 < level_count);
        if (ts->is_trie)
          ts->sfx = false;
        stores.push_back(ts);
      }
    }

    ~builder() {
      for (size_t i = 0; i < stores.size(); i++)
        delete stores[i];
    }

    // Last tail level, stored as plain tail area
    tail_store& plain_store() {
      return *stores.back();
    }

    template <class T>
    std::vector<uint8_t>& build_from_basix(T *bx) {
      std::vector<key_ref> keys;
      collect_basix_keys(bx, bx->root_block, keys);
      return build(keys);
    }

    // keys must be sorted and unique
    std::vector<uint8_t>& build(std::vector<key_ref>& in_keys) {
      int trie_level_count = (int) stores.size();
      levels.clear();
      levels.resize(trie_level_count);
      normalize_keys(in_keys, levels[0].keys);
      key_count = levels[0].keys.size();
      levels[0].class_start.push_back(0);
      levels[0].class_start.push_back(levels[0].keys.size());
      for (int k = 0; k < trie_level_count; k++) {
        if (k > 0)
          make_inner_keys(k);
        pass1_tails(k);
        plan_level(k);
      }
      for (int k = 0; k < trie_level_count; k++)
        emit_level(k);
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

    struct rev_idx_less {
      std::vector<std::string> *strs;
      bool operator()(uint32_t lhs, uint32_t rhs) const {
        return (*strs)[lhs] < (*strs)[rhs];
      }
    };

    // Keys of inner level k: tails of level k - 1, reversed, sorted within
    // each width class (classes in frequency order)
    void make_inner_keys(int k) {
      tail_store& ts = *stores[k - 1];
      trie_level& lvl = levels[k];
      size_t tail_count = ts.tails.size();
      std::vector<std::string> rev_strs(tail_count);
      for (size_t i = 0; i < tail_count; i++) {
        rev_strs[i] = ts.tails[i].str;
        std::reverse(rev_strs[i].begin(), rev_strs[i].end());
      }
      lvl.key_strs.reserve(tail_count);
      for (int c = 0; ts.class_start(c) < tail_count; c++) {
        size_t lo = (size_t) ts.class_start(c);
        size_t hi = (size_t) ts.class_start(c + 1);
        if (hi > tail_count)
          hi = tail_count;
        std::vector<uint32_t> idx;
        for (size_t i = lo; i < hi; i++)
          idx.push_back((uint32_t) i);
        rev_idx_less cmp;
        cmp.strs = &rev_strs;
        std::sort(idx.begin(), idx.end(), cmp);
        lvl.class_start.push_back(lvl.key_strs.size());
        for (size_t i = 0; i < idx.size(); i++) {
          lvl.key_strs.push_back(rev_strs[idx[i]]);
          lvl.key_rank.push_back(idx[i]);
        }
      }
      lvl.class_start.push_back(lvl.key_strs.size());
      for (size_t i = 0; i < lvl.key_strs.size(); i++) {
        key_ref kr;
        kr.key = (const uint8_t *) lvl.key_strs[i].data();
        kr.len = (uint32_t) lvl.key_strs[i].length();
        lvl.keys.push_back(kr);
      }
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
          visit(queue[qi], i, n.get_byte(), mflags, tail, tail_len);
          if (mflags & NFLAG_CHILD)
            queue.push_back(n.get_child());
        }
      }
    }

    // Tail string of a node as stored in the next level: main trie tails as
    // is (minus bytes kept in node byte), inner trie tails reversed back
    static const uint8_t *tail_str(const uint8_t *tail, size_t tail_len, bool rev,
          int tail_skip, std::string& buf, size_t& out_len) {
      if (rev) {
        buf.assign((const char *) tail, tail_len);
        std::reverse(buf.begin(), buf.end());
        out_len = buf.length();
        return (const uint8_t *) buf.data();
      }
      out_len = tail_len - tail_skip;
      return tail + tail_skip;
    }

    struct freq_visitor {
      tail_store *ts;
      bool rev;
      std::string buf;
      void operator()(uintxx_t ns, int idx, uint8_t b, uint8_t mflags, const uint8_t *tail, size_t tail_len) {
        (void) ns;
        (void) idx;
        (void) b;
        if (mflags & NFLAG_TAIL) {
          size_t len;
          const uint8_t *t = tail_str(tail, tail_len, rev, ts->tail_skip, buf, len);
          ts->add_freq(t, len);
        }
      }
    };

    // Pass 1 for level k: memtrie per width class to discover tails of
    // level k nodes and their frequencies
    void pass1_tails(int k) {
      trie_level& lvl = levels[k];
      freq_visitor fv;
      fv.ts = stores[k];
      fv.rev = (k > 0);
      mtrie::in_mem_trie *trie = new mtrie::in_mem_trie();
      for (size_t c = 0; c + 1 < lvl.class_start.size(); c++) {
        trie->reset();
        for (size_t i = lvl.class_start[c]; i < lvl.class_start[c + 1]; i++)
          trie->insert(lvl.keys[i].key, lvl.keys[i].len);
        walk_memtrie(*trie, fv);
      }
      delete trie;
      stores[k]->finalize_order();
    }

    struct node_collector {
      std::vector<leaf_node> *nodes;
      std::vector<uint32_t> *ns_base;
      tail_store *ts;
      bool rev;
      bool emit;
      bool missing;
      int64_t ptr_bytes;
      uint32_t key_count;
      std::string buf;
      void operator()(uintxx_t ns, int idx, uint8_t b, uint8_t mflags, const uint8_t *tail, size_t tail_len) {
        if (idx == 0)
          (*ns_base)[ns] = (uint32_t) nodes->size();
        leaf_node ln;
        ln.b = b;
        ln.flags = 0;
        ln.ptr = 0;
        if (mflags & NFLAG_TAIL) {
          ln.flags |= (1 << MBX_FLAG_TAIL);
          size_t len;
          const uint8_t *t = tail_str(tail, tail_len, rev, ts->tail_skip, buf, len);
          if (emit) {
            int64_t ti = ts->find(t, len);
            if (ti < 0)
              missing = true;
            else
              ln.ptr = ts->tails[(size_t) ti].ptr;
          } else {
            ln.ptr = ts->get_or_add(t, len);
          }
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

    // Builds the trie block of level k for keys [s, e).
    // Plan: checks size (rolling back new tails if it does not fit) and,
    // for inner levels, assigns leaf ids to the tails of level k - 1.
    // Emit: writes the block with final pointers.
    bool build_block(mtrie::in_mem_trie& trie, int k, size_t s, size_t e, bool emit,
          uint32_t priced_n, int64_t priced_p, uint32_t priced_t) {
      trie_level& lvl = levels[k];
      tail_store& ts = *stores[k];
      trie.reset();
      for (size_t i = s; i < e; i++)
        trie.insert(lvl.keys[i].key, lvl.keys[i].len);
      size_t tail_count = ts.tails.size();
      uint64_t old_area_size = ts.area_size;
      std::vector<leaf_node> nodes;
      std::vector<uint32_t> ns_base(trie.all_node_sets.size(), 0);
      node_collector nc;
      nc.nodes = &nodes;
      nc.ns_base = &ns_base;
      nc.ts = &ts;
      nc.rev = (k > 0);
      nc.emit = emit;
      nc.missing = false;
      nc.ptr_bytes = 0;
      nc.key_count = 0;
      walk_memtrie(trie, nc);
      uint32_t n = (uint32_t) nodes.size();
      uint32_t set_count = 0;
      for (size_t i = 0; i < nodes.size(); i++) {
        if (nodes[i].flags & (1 << MBX_FLAG_TERM))
          set_count++;
      }
      size_t blk_size = block_pricer::block_size_for(n, nc.ptr_bytes, set_count, nc.key_count, opts.sel_shift);
      if (emit) {
        if (nc.missing || blk_size > opts.leaf_blk_size)
          fprintf(stderr, "madras_blk: level %d block at key %lu differs from plan\n", k, (unsigned long) s);
        write_block(lvl.blocks, nodes, k, nc.key_count, (uint32_t) nc.ptr_bytes, ts.ptr_shift);
        if (k == 0)
          lvl.node_start.push_back(total_nodes);
        total_nodes += n;
        return true;
      }
      if (priced_n != UINT32_MAX && (n != priced_n || nc.ptr_bytes != priced_p || set_count != priced_t))
        price_mismatch_count++;
      if (blk_size > opts.leaf_blk_size || n > 65535) {
        ts.rollback(tail_count, old_area_size);
        return false;
      }
      if (k > 0)
        assign_leaf_ids(trie, k, s, e, nodes, ns_base);
      return true;
    }

    // Leaf id of a tail = its key index range start of the block + ordinal
    // of its end node among leaf nodes of the block (level order)
    void assign_leaf_ids(mtrie::in_mem_trie& trie, int k, size_t s, size_t e,
          std::vector<leaf_node>& nodes, std::vector<uint32_t>& ns_base) {
      trie_level& lvl = levels[k];
      tail_store& prev_ts = *stores[k - 1];
      std::vector<uint32_t> leaf_ord(nodes.size(), 0);
      uint32_t ord = 0;
      for (size_t i = 0; i < nodes.size(); i++) {
        leaf_ord[i] = ord;
        if (nodes[i].flags & (1 << MBX_FLAG_LEAF))
          ord++;
      }
      for (size_t i = s; i < e; i++) {
        mtrie::node_set_vars nsv;
        if (!trie.lookup(lvl.keys[i].key, lvl.keys[i].len, nsv)) {
          fprintf(stderr, "madras_blk: inner key not found in block\n");
          continue;
        }
        uint32_t node_id = ns_base[nsv.node_set_pos] + nsv.cur_node_idx;
        uint64_t id = s + leaf_ord[node_id];
        tail_store::tail_info& ti = prev_ts.tails[lvl.key_rank[i]];
        if (prev_ts.ptr_width(id) != prev_ts.ptr_width(ti.ptr))
          fprintf(stderr, "madras_blk: leaf id %lu out of width class\n", (unsigned long) id);
        ti.ptr = id;
      }
    }

    void write_block(std::vector<uint8_t>& blocks, std::vector<leaf_node>& nodes, int k,
          uint32_t blk_key_count, uint32_t ptr_bytes, int ptr_shift) {
      size_t blk_start = blocks.size();
      blocks.resize(blk_start + opts.leaf_blk_size, 0);
      uint8_t *blk = blocks.data() + blk_start;
      uint32_t n = (uint32_t) nodes.size();
      uint32_t grp_count = (n + 63) / 64;
      int shift = opts.sel_shift;
      uint32_t set_count = 0, child_total = 0;
      for (uint32_t i = 0; i < n; i++) {
        if (nodes[i].flags & (1 << MBX_FLAG_TERM))
          set_count++;
        if (nodes[i].flags & (1 << MBX_FLAG_CHILD))
          child_total++;
      }
      blk[0] = (k == 0 ? MBX_BLK_TYPE_LEAF : MBX_BLK_TYPE_INNER);
      blk[1] = (uint8_t) k;
      put_u16(blk + 2, n);
      put_u16(blk + 4, blk_key_count);
      put_u16(blk + 6, ptr_bytes);
      size_t hdr_size = MBX_LEAF_HDR_SIZE;
      if (shift > 0) {
        put_u16(blk + 8, set_count);
        hdr_size = MBX_LEAF_HDR_SIZE_SEL;
      }
      uint8_t *child_rank = blk + hdr_size;
      uint8_t *term_rank = child_rank + grp_count * 2;
      uint8_t *leaf_rank = term_rank + grp_count * 2;
      uint8_t *ptr_off = leaf_rank + grp_count * 2;
      uint8_t *term_sel = ptr_off + grp_count * 2;
      uint8_t *child_sel = term_sel;
      uint8_t *leaf_sel = term_sel;
      uint8_t *flags = term_sel;
      int grp_shift = sel_grp_shift(opts.leaf_blk_size);
      if (shift > 0) {
        child_sel = term_sel + block_pricer::sel_entries(set_count, shift);
        leaf_sel = child_sel + block_pricer::sel_entries(child_total, shift);
        flags = leaf_sel + block_pricer::sel_entries(blk_key_count, shift);
      }
      uint8_t *node_bytes = flags + grp_count * 32;
      uint8_t *ptrs = node_bytes + n;
      uint32_t child_cnt = 0, term_cnt = 0, leaf_cnt = 0, ptr_pos = 0;
      uint32_t sel_mask = shift > 0 ? (((uint32_t) 1 << shift) - 1) : 0;
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
        // select entry j: group of set bit number (j << shift) + 1
        if (ln.flags & (1 << MBX_FLAG_CHILD)) {
          if (shift > 0 && (child_cnt & sel_mask) == 0)
            child_sel[child_cnt >> shift] = (uint8_t) (g >> grp_shift);
          child_cnt++;
        }
        if (ln.flags & (1 << MBX_FLAG_TERM)) {
          if (shift > 0 && (term_cnt & sel_mask) == 0)
            term_sel[term_cnt >> shift] = (uint8_t) (g >> grp_shift);
          term_cnt++;
          total_node_sets++;
        }
        if (ln.flags & (1 << MBX_FLAG_LEAF)) {
          if (shift > 0 && (leaf_cnt & sel_mask) == 0)
            leaf_sel[leaf_cnt >> shift] = (uint8_t) (g >> grp_shift);
          leaf_cnt++;
        }
        node_bytes[i] = ln.b;
        if (ln.flags & (1 << MBX_FLAG_TAIL))
          ptr_pos += (uint32_t) write_fvint(ptrs + ptr_pos, ln.ptr >> ptr_shift);
      }
    }

    // Packs sorted keys of level k into blocks, one width class at a time
    void plan_level(int k) {
      trie_level& lvl = levels[k];
      mtrie::in_mem_trie *trie = new mtrie::in_mem_trie();
      block_pricer pricer(stores[k], k > 0, opts.sel_shift);
      for (size_t c = 0; c + 1 < lvl.class_start.size(); c++) {
        size_t s = lvl.class_start[c];
        size_t class_end = lvl.class_start[c + 1];
        while (s < class_end) {
          pricer.reset();
          size_t e = s;
          while (e < class_end) {
            block_pricer::plan pl;
            size_t sz = pricer.try_add(lvl.keys[e].key, lvl.keys[e].len, pl);
            if (e > s && (sz > opts.leaf_blk_size || pricer.node_count + pl.dn > 65535))
              break;
            pricer.commit(lvl.keys[e].key, lvl.keys[e].len, pl);
            e++;
          }
          uint32_t priced_n = pricer.node_count;
          int64_t priced_p = pricer.ptr_bytes;
          uint32_t priced_t = pricer.set_count;
          while (!build_block(*trie, k, s, e, false, priced_n, priced_p, priced_t)) {
            rebuild_count++;
            if (e - s == 1) {
              fprintf(stderr, "madras_blk: key too large for block\n");
              break;
            }
            e--;
            priced_n = UINT32_MAX; // not tracked for rebuilds
          }
          if (k == 0) {
            // separator for this leaf block
            if (s == 0) {
              leaf_seps.push_back(std::string());
            } else {
              key_ref& last = lvl.keys[s - 1];
              key_ref& first = lvl.keys[s];
              uint32_t l = lcp_len(last.key, last.len, first.key, first.len);
              leaf_seps.push_back(std::string((const char *) first.key, l + 1));
            }
          }
          lvl.blk_start.push_back(s);
          s = e;
        }
      }
      lvl.blk_start.push_back(lvl.keys.size());
      if (k == 0)
        leaf_count = (uint32_t) (lvl.blk_start.size() - 1);
      delete trie;
    }

    void emit_level(int k) {
      trie_level& lvl = levels[k];
      mtrie::in_mem_trie *trie = new mtrie::in_mem_trie();
      for (size_t b = 0; b + 1 < lvl.blk_start.size(); b++)
        build_block(*trie, k, lvl.blk_start[b], lvl.blk_start[b + 1], true, 0, 0, 0);
      if (k == 0)
        lvl.node_start.push_back(total_nodes);
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

    void write_meta(uint8_t *meta, mbx_section& sec, std::vector<mbx_level_desc>& descs) {
      const char *magic = "Madras Sorcery Blocks DB Format 1.0";
      memcpy(meta, magic, strlen(magic));
      meta[36] = MBX_MAGIC_BYTE;
      meta[37] = MBX_FORMAT_VER; // version
      meta[38] = 1; // pk_col_count
      meta[39] = 0; // trie_level
      uint32_t max_tail_len = 0;
      for (size_t i = 0; i < stores.size(); i++) {
        for (size_t j = 0; j < stores[i]->tails.size(); j++) {
          if (max_tail_len < stores[i]->tails[j].str.length())
            max_tail_len = (uint32_t) stores[i]->tails[j].str.length();
        }
      }
      max_tail_len += plain_store().tail_skip;
      put_u16(meta + 40, max_tail_len > 0 ? max_tail_len - 1 : 0);
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
      memcpy(meta + MBX_SECTION_LOC + sizeof(sec), descs.data(), descs.size() * sizeof(mbx_level_desc));
    }

    static void append_u32(std::vector<uint8_t>& v, uint32_t val) {
      size_t pos = v.size();
      v.resize(pos + 4);
      memcpy(v.data() + pos, &val, 4);
    }

    // Cumulative id counts per block (cum has blk_count + 1 entries) and
    // a direct map from id >> shift to block number
    void write_id_table(std::vector<size_t>& cum, mbx_level_desc& d) {
      uint32_t blk_count = (uint32_t) (cum.size() - 1);
      d.lt_loc = out.size();
      for (size_t b = 0; b <= blk_count; b++)
        append_u32(out, (uint32_t) cum[b]);
      uint64_t min_ids = UINT64_MAX;
      for (size_t b = 0; b + 1 < blk_count; b++) {
        uint64_t cnt = cum[b + 1] - cum[b];
        if (min_ids > cnt)
          min_ids = cnt;
      }
      int shift = 0;
      while (min_ids != UINT64_MAX && ((uint64_t) 2 << shift) <= min_ids && shift < 31)
        shift++;
      d.sample_shift = (uint8_t) shift;
      d.sample_loc = out.size();
      size_t id_count = cum[blk_count];
      uint32_t b = 0;
      for (size_t id = 0; id < id_count; id += ((size_t) 1 << shift)) {
        while (cum[b + 1] <= id)
          b++;
        append_u32(out, b);
      }
      d.blk_count = blk_count;
      d.id_count = (uint32_t) id_count;
    }

    void write_level_tables(int k, mbx_level_desc& d) {
      write_id_table(levels[k].blk_start, d);
      d.is_trie = 1;
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
      sec.level_count = (uint8_t) stores.size();
      sec.has_main_lt = 1;
      sec.sel_shift = opts.sel_shift;
      sec.leaf_hdr_size = (uint8_t) (opts.sel_shift > 0 ? MBX_LEAF_HDR_SIZE_SEL : MBX_LEAF_HDR_SIZE);
      // tail level descriptors, then main node id and leaf id tables
      std::vector<mbx_level_desc> descs(stores.size() + 2);
      memset(descs.data(), 0, descs.size() * sizeof(mbx_level_desc));
      size_t meta_size = opts.leaf_blk_size;
      size_t meta_needed = MBX_SECTION_LOC + sizeof(sec) + descs.size() * sizeof(mbx_level_desc);
      if (meta_size < meta_needed)
        meta_size = meta_needed;
      out.resize(meta_size, 0);
      // lookup tables, kept in front for caching
      write_id_table(levels[0].node_start, descs[stores.size()]);
      write_id_table(levels[0].blk_start, descs[stores.size() + 1]);
      for (size_t k = 1; k < levels.size(); k++)
        write_level_tables((int) k, descs[k - 1]);
      size_t parent_align = opts.parent_blk_size;
      out.resize((out.size() + parent_align - 1) / parent_align * parent_align, 0);
      // parent blocks: top level first
      std::vector<uint32_t> level_start(parent_levels.size());
      uint32_t parent_count = 0;
      for (size_t lvl = parent_levels.size(); lvl-- > 0;) {
        level_start[lvl] = parent_count;
        parent_count += (uint32_t) parent_levels[lvl].size();
      }
      sec.parent_count = parent_count;
      sec.parent_start = out.size();
      sec.leaf_start = sec.parent_start + (uint64_t) parent_count * opts.parent_blk_size;
      out.resize(sec.leaf_start, 0);
      for (size_t lvl = parent_levels.size(); lvl-- > 0;) {
        uint32_t child_base = (lvl == 0 ? 0 : level_start[lvl - 1]);
        for (size_t b = 0; b < parent_levels[lvl].size(); b++) {
          uint8_t *blk = out.data() + sec.parent_start + (uint64_t) (level_start[lvl] + b) * opts.parent_blk_size;
          write_parent_block(blk, (int) lvl + 1, parent_levels[lvl][b], child_base);
        }
      }
      // main leaf blocks, then inner trie blocks by level
      out.insert(out.end(), levels[0].blocks.begin(), levels[0].blocks.end());
      for (size_t k = 1; k < levels.size(); k++) {
        descs[k - 1].blk_start = out.size();
        out.insert(out.end(), levels[k].blocks.begin(), levels[k].blocks.end());
      }
      for (size_t k = 0; k < levels.size(); k++)
        std::vector<uint8_t>().swap(levels[k].blocks);
      // plain tail level
      sec.tail_area_loc = out.size();
      plain_store().write_area(out);
      sec.tail_area_size = out.size() - sec.tail_area_loc;
      mbx_level_desc& pd = descs[stores.size() - 1];
      pd.blk_start = sec.tail_area_loc;
      pd.is_trie = 0;
      pd.id_count = (uint32_t) plain_store().tails.size();
      out.resize(out.size() + 8, 0); // padding for word-at-a-time reads
      sec.total_size = out.size();
      write_meta(out.data(), sec, descs);
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
    mbx_level_desc descs[MBX_MAX_LEVELS];
    mbx_level_desc node_lt;  // main trie: cumulative node count per leaf block
    mbx_level_desc leaf_lt;  // main trie: cumulative leaf (key) count per leaf block
    const uint8_t *tail_area;
    uint32_t max_key_len;
    int blk_sel_grp_shift;

    // Parsed positions of a leaf or inner trie block
    struct blk_view {
      uint32_t n;
      uint32_t grp_count;
      int sel_shift;
      int sel_grp_shift;  // entry holds group >> sel_grp_shift
      const uint8_t *child_rank;
      const uint8_t *term_rank;
      const uint8_t *leaf_rank;
      const uint8_t *ptr_off;
      const uint8_t *rank_arr[4];  // by MBX_FLAG_*
      const uint8_t *sel_arr[4];   // by MBX_FLAG_*, NULL if no select lookup
      const uint8_t *flags;
      const uint8_t *node_bytes;
      const uint8_t *ptrs;
    };

    inline void init_view(blk_view& v, const uint8_t *blk) const {
      v.n = get_u16(blk + 2);
      v.grp_count = (v.n + 63) / 64;
      v.sel_shift = sec.sel_shift;
      v.child_rank = blk + (sec.leaf_hdr_size == 0 ? MBX_LEAF_HDR_SIZE : sec.leaf_hdr_size);
      v.term_rank = v.child_rank + v.grp_count * 2;
      v.leaf_rank = v.term_rank + v.grp_count * 2;
      v.ptr_off = v.leaf_rank + v.grp_count * 2;
      v.rank_arr[MBX_FLAG_TAIL] = NULL;
      v.rank_arr[MBX_FLAG_TERM] = v.term_rank;
      v.rank_arr[MBX_FLAG_CHILD] = v.child_rank;
      v.rank_arr[MBX_FLAG_LEAF] = v.leaf_rank;
      v.sel_arr[MBX_FLAG_TAIL] = NULL;
      v.sel_grp_shift = blk_sel_grp_shift;
      const uint8_t *p = v.ptr_off + v.grp_count * 2;
      if (v.sel_shift > 0) {
        uint32_t set_count = get_u16(blk + 8);
        uint32_t child_count = set_count > 0 ? set_count - 1 : 0;
        uint32_t leaf_count = get_u16(blk + 4);
        v.sel_arr[MBX_FLAG_TERM] = p;
        p += block_pricer::sel_entries(set_count, v.sel_shift);
        v.sel_arr[MBX_FLAG_CHILD] = p;
        p += block_pricer::sel_entries(child_count, v.sel_shift);
        v.sel_arr[MBX_FLAG_LEAF] = p;
        p += block_pricer::sel_entries(leaf_count, v.sel_shift);
      } else {
        v.sel_arr[MBX_FLAG_TERM] = v.sel_arr[MBX_FLAG_CHILD] = v.sel_arr[MBX_FLAG_LEAF] = NULL;
      }
      v.flags = p;
      v.node_bytes = v.flags + v.grp_count * 32;
      v.ptrs = v.node_bytes + v.n;
    }

    static inline uint64_t get_word(const uint8_t *flags, uint32_t g, int f) {
      return get_u64(flags + g * 32 + f * 8);
    }

    // Position of k-th (1 based) set bit of flag f. With select lookup, starts
    // at the group of the sampled set bit and walks the rank array forward,
    // otherwise binary searches the rank array.
    static inline uint32_t select1(const blk_view& v, int f, uint32_t k) {
      const uint8_t *rank_arr = v.rank_arr[f];
      uint32_t g;
      if (v.sel_arr[f] != NULL) {
        g = (uint32_t) v.sel_arr[f][(k - 1) >> v.sel_shift] << v.sel_grp_shift;
        while (g + 1 < v.grp_count && get_u16(rank_arr + (g + 1) * 2) < k)
          g++;
      } else {
        uint32_t lo = 0;
        uint32_t hi = v.grp_count - 1;
        while (lo < hi) {
          uint32_t mid = (lo + hi + 1) >> 1;
          if (get_u16(rank_arr + mid * 2) < k)
            lo = mid;
          else
            hi = mid - 1;
        }
        g = lo;
      }
      uint32_t r = k - get_u16(rank_arr + g * 2);
      return g * 64 + (uint32_t) select_in_word(get_word(v.flags, g, f), r);
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
    blk_trie() : data (NULL), tail_area (NULL), max_key_len (0), blk_sel_grp_shift (0) {
      memset(&sec, 0, sizeof(sec));
      memset(&node_lt, 0, sizeof(node_lt));
      memset(&leaf_lt, 0, sizeof(leaf_lt));
    }

    bool load(const uint8_t *buf, size_t buf_size) {
      if (buf_size < MBX_SECTION_LOC + sizeof(sec) || buf[36] != MBX_MAGIC_BYTE)
        return false;
      data = buf;
      memcpy(&sec, data + MBX_SECTION_LOC, sizeof(sec));
      if (sec.total_size != buf_size)
        return false;
      memset(descs, 0, sizeof(descs));
      if (sec.level_count > MBX_MAX_LEVELS)
        return false;
      const uint8_t *desc_loc = data + MBX_SECTION_LOC + sizeof(sec);
      if (sec.level_count > 0) // files before v4 have no descriptors (one plain level)
        memcpy(descs, desc_loc, sec.level_count * sizeof(mbx_level_desc));
      memset(&node_lt, 0, sizeof(node_lt));
      memset(&leaf_lt, 0, sizeof(leaf_lt));
      if (sec.has_main_lt) { // files before v5 have no main id tables
        memcpy(&node_lt, desc_loc + sec.level_count * sizeof(mbx_level_desc), sizeof(node_lt));
        memcpy(&leaf_lt, desc_loc + (sec.level_count + 1) * sizeof(mbx_level_desc), sizeof(leaf_lt));
      }
      max_key_len = (uint32_t) get_u64(data + 112);
      if (data[37] < 2 && sec.sel_shift > 0)
        return false; // v6 files: 2-byte select entries, no longer supported
      blk_sel_grp_shift = sel_grp_shift(sec.leaf_blk_size);
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

    // Block containing id, using cumulative counts and direct-mapped sample
    inline uint32_t locate_block(const mbx_level_desc& d, uint64_t id, uint32_t& local) const {
      const uint8_t *cum = data + d.lt_loc;
      uint32_t b = get_u32(data + d.sample_loc + (id >> d.sample_shift) * 4);
      while (get_u32(cum + (b + 1) * 4) <= id)
        b++;
      local = (uint32_t) (id - get_u32(cum + b * 4));
      return b;
    }

    uint32_t get_level_count() const {
      return sec.level_count == 0 ? 1 : sec.level_count;
    }

    const mbx_level_desc& get_level_desc(int k) const {
      return descs[k - 1];
    }

    // Block of inner trie level k holding leaf id, and the id's ordinal in it
    inline const uint8_t *locate_inner(int k, uint64_t id, uint32_t& local) const {
      const mbx_level_desc& d = descs[k - 1];
      uint32_t b = locate_block(d, id, local);
      return data + d.blk_start + (uint64_t) b * sec.leaf_blk_size;
    }

    // Tail pointer of a tail node (node byte holds low 8 bits)
    static inline uint64_t node_tail_ptr(const blk_view& v, uint32_t node) {
      uint32_t g = node / 64;
      uint64_t tail_w = get_word(v.flags, g, MBX_FLAG_TAIL);
      int skip = __builtin_popcountll(tail_w & (((uint64_t) 1 << (node % 64)) - 1));
      const uint8_t *p = skip_fvints(v.ptrs + get_u16(v.ptr_off + g * 2), skip);
      size_t vlen;
      return (read_fvint(p, vlen) << 8) | v.node_bytes[node];
    }

    // First byte of tail at tail level k
    uint8_t first_byte(int k, uint64_t ptr) const {
      if (!descs[k - 1].is_trie)
        return first_tail_byte(ptr);
      uint32_t local;
      blk_view v;
      init_view(v, locate_inner(k, ptr, local));
      uint32_t node = select1(v, MBX_FLAG_LEAF, local + 1);
      if ((get_word(v.flags, node / 64, MBX_FLAG_TAIL) >> (node % 64)) & 1)
        return first_byte(k + 1, node_tail_ptr(v, node));
      return v.node_bytes[node];
    }

    // Matches tail at tail level k against key from key_pos. Inner tries hold
    // reversed tails, so walking up from the end node gives the tail in order.
    bool match_tail_lvl(int k, uint64_t ptr, const uint8_t *key, uint32_t key_len, uint32_t& key_pos) const {
      if (!descs[k - 1].is_trie)
        return match_tail(ptr, key, key_len, key_pos);
      uint32_t local;
      blk_view v;
      init_view(v, locate_inner(k, ptr, local));
      uint32_t node = select1(v, MBX_FLAG_LEAF, local + 1);
      while (true) {
        uint32_t g = node / 64;
        uint32_t bit = node % 64;
        if ((get_word(v.flags, g, MBX_FLAG_TAIL) >> bit) & 1) {
          if (!match_tail_lvl(k + 1, node_tail_ptr(v, node), key, key_len, key_pos))
            return false;
        } else {
          if (key_pos >= key_len || key[key_pos] != v.node_bytes[node])
            return false;
          key_pos++;
        }
        uint64_t mask = ((uint64_t) 1 << bit) - 1;
        uint32_t ns = get_u16(v.term_rank + g * 2)
            + (uint32_t) __builtin_popcountll(get_word(v.flags, g, MBX_FLAG_TERM) & mask);
        if (ns == 0)
          return true; // root node set reached
        node = select1(v, MBX_FLAG_CHILD, ns);
      }
    }

    // Appends tail at tail level k to out (for debugging and reverse lookups)
    void get_tail(int k, uint64_t ptr, std::string& out_str) const {
      if (!descs[k - 1].is_trie) {
        const uint8_t *t = tail_area + ptr;
        if (is_text_byte(*t)) {
          while (is_text_byte(*t))
            out_str += (char) *t++;
        } else {
          uint64_t bin_len = read_len_bw(t);
          out_str.append((const char *) t + 1, bin_len);
        }
        return;
      }
      uint32_t local;
      blk_view v;
      init_view(v, locate_inner(k, ptr, local));
      uint32_t node = select1(v, MBX_FLAG_LEAF, local + 1);
      while (true) {
        uint32_t g = node / 64;
        uint32_t bit = node % 64;
        if ((get_word(v.flags, g, MBX_FLAG_TAIL) >> bit) & 1)
          get_tail(k + 1, node_tail_ptr(v, node), out_str);
        else
          out_str += (char) v.node_bytes[node];
        uint64_t mask = ((uint64_t) 1 << bit) - 1;
        uint32_t ns = get_u16(v.term_rank + g * 2)
            + (uint32_t) __builtin_popcountll(get_word(v.flags, g, MBX_FLAG_TERM) & mask);
        if (ns == 0)
          return;
        node = select1(v, MBX_FLAG_CHILD, ns);
      }
    }

    // node byte = first tail byte (same as v1)
    bool lookup_in_leaf_nb_first(const uint8_t *blk, const uint8_t *key, uint32_t key_len, uint32_t& out_node) const {
      blk_view v;
      init_view(v, blk);
      const uint8_t *child_rank = v.child_rank;
      const uint8_t *ptr_off = v.ptr_off;
      const uint8_t *flags = v.flags;
      const uint8_t *node_bytes = v.node_bytes;
      const uint8_t *ptrs = v.ptrs;
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
        if (key_pos == key_len) {
          out_node = node_id;
          return (get_word(flags, g, MBX_FLAG_LEAF) >> bit) & 1;
        }
        uint64_t child_w = get_word(flags, g, MBX_FLAG_CHILD);
        if ((child_w & ((uint64_t) 1 << bit)) == 0)
          return false;
        uint32_t child_cnt = get_u16(child_rank + g * 2) + (uint32_t) __builtin_popcountll(child_w & mask);
        node_id = select1(v, MBX_FLAG_TERM, child_cnt + 1) + 1;
      }
    }

    // node byte = low 8 bits of tail ptr: tail pointers decoded while
    // scanning siblings to get first byte of each tail
    bool lookup_in_leaf_nb_ptr(const uint8_t *blk, const uint8_t *key, uint32_t key_len, uint32_t& out_node) const {
      blk_view v;
      init_view(v, blk);
      const uint8_t *child_rank = v.child_rank;
      const uint8_t *ptr_off = v.ptr_off;
      const uint8_t *flags = v.flags;
      const uint8_t *node_bytes = v.node_bytes;
      const uint8_t *ptrs = v.ptrs;
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
            nb = first_byte(1, tail_ptr);
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
          if (!match_tail_lvl(1, tail_ptr, key, key_len, key_pos))
            return false;
        } else {
          key_pos++;
        }
        if (key_pos == key_len) {
          out_node = node_id;
          return (get_word(flags, g, MBX_FLAG_LEAF) >> bit) & 1;
        }
        uint64_t child_w = get_word(flags, g, MBX_FLAG_CHILD);
        if (((child_w >> bit) & 1) == 0)
          return false;
        uint32_t child_cnt = get_u16(child_rank + g * 2)
            + (uint32_t) __builtin_popcountll(child_w & (((uint64_t) 1 << bit) - 1));
        node_id = select1(v, MBX_FLAG_TERM, child_cnt + 1) + 1;
      }
    }

    bool lookup_in_leaf(const uint8_t *blk, const uint8_t *key, uint32_t key_len, uint32_t& out_node) const {
      if (sec.node_byte_mode == MBX_NODE_BYTE_PTR)
        return lookup_in_leaf_nb_ptr(blk, key, key_len, out_node);
      return lookup_in_leaf_nb_first(blk, key, key_len, out_node);
    }

    bool lookup(const uint8_t *key, size_t key_len) const {
      if (key_len == 0) {
        key = empty_key_value;
        key_len = empty_key_value_len;
      }
      if (sec.leaf_count == 0)
        return false;
      uint32_t leaf_no = find_leaf_block(key, (uint32_t) key_len);
      uint32_t local_node;
      return lookup_in_leaf(get_leaf_block(leaf_no), key, (uint32_t) key_len, local_node);
    }

    bool lookup(const char *key, size_t key_len) const {
      return lookup((const uint8_t *) key, key_len);
    }

    // Same as madras: on success in_ctx.node_id is the node where key ends
    bool lookup(input_ctx& in_ctx) const {
      const uint8_t *key = in_ctx.key;
      uint32_t key_len = in_ctx.key_len;
      if (key_len == 0) {
        key = empty_key_value;
        key_len = (uint32_t) empty_key_value_len;
      }
      in_ctx.key_pos = 0;
      if (sec.leaf_count == 0)
        return false;
      uint32_t leaf_no = find_leaf_block(key, key_len);
      uint32_t local_node = 0;
      bool found = lookup_in_leaf(get_leaf_block(leaf_no), key, key_len, local_node);
      if (found) {
        in_ctx.key_pos = in_ctx.key_len;
        if (sec.has_main_lt)
          in_ctx.node_id = get_u32(data + node_lt.lt_loc + (uint64_t) leaf_no * 4) + local_node;
      }
      return found;
    }

    uint32_t get_max_key_len() const {
      return max_key_len;
    }

    uint64_t get_node_count() const {
      return node_lt.id_count;
    }

    uint64_t get_key_count() const {
      return leaf_lt.id_count;
    }

    // Number of leaves (keys) before node_id. For a leaf node, this is its
    // leaf id. Leaf ids are in alphabetical order across blocks but in level
    // order within a block.
    uintxx_t leaf_rank1(uintxx_t node_id) const {
      uint32_t local;
      uint32_t b = locate_block(node_lt, node_id, local);
      blk_view v;
      init_view(v, get_leaf_block(b));
      uint32_t g = local / 64;
      uint64_t mask = ((uint64_t) 1 << (local % 64)) - 1;
      return get_u32(data + leaf_lt.lt_loc + (uint64_t) b * 4) + get_u16(v.leaf_rank + g * 2)
          + (uint32_t) __builtin_popcountll(get_word(v.flags, g, MBX_FLAG_LEAF) & mask);
    }

    // Node id of leaf with given leaf id (0 based)
    uintxx_t leaf_select1(uintxx_t leaf_id) const {
      uint32_t local;
      uint32_t b = locate_block(leaf_lt, leaf_id, local);
      blk_view v;
      init_view(v, get_leaf_block(b));
      uint32_t node = select1(v, MBX_FLAG_LEAF, local + 1);
      return get_u32(data + node_lt.lt_loc + (uint64_t) b * 4) + node;
    }

    bool is_leaf(uintxx_t node_id) const {
      uint32_t local;
      uint32_t b = locate_block(node_lt, node_id, local);
      blk_view v;
      init_view(v, get_leaf_block(b));
      return (get_word(v.flags, local / 64, MBX_FLAG_LEAF) >> (local % 64)) & 1;
    }

    // Key ending at node_id. Walks up within the leaf block, appending
    // node bytes and reversed tails, then reverses the whole key.
    bool reverse_lookup_from_node_id(uintxx_t node_id, std::string& out_key) const {
      out_key.clear();
      if (!sec.has_main_lt || node_id >= node_lt.id_count)
        return false;
      uint32_t local;
      uint32_t b = locate_block(node_lt, node_id, local);
      blk_view v;
      init_view(v, get_leaf_block(b));
      bool ptr_in_node_byte = (sec.node_byte_mode == MBX_NODE_BYTE_PTR);
      uint32_t node = local;
      std::string tail;
      while (true) {
        uint32_t g = node / 64;
        uint32_t bit = node % 64;
        uint64_t mask = ((uint64_t) 1 << bit) - 1;
        uint64_t tail_w = get_word(v.flags, g, MBX_FLAG_TAIL);
        if ((tail_w >> bit) & 1) {
          tail.clear();
          if (ptr_in_node_byte) {
            get_tail(1, node_tail_ptr(v, node), tail);
          } else {
            // node byte is first byte of tail, pointer is not shifted
            tail += (char) v.node_bytes[node];
            int skip = __builtin_popcountll(tail_w & mask);
            const uint8_t *p = skip_fvints(v.ptrs + get_u16(v.ptr_off + g * 2), skip);
            size_t vlen;
            get_tail(1, read_fvint(p, vlen), tail);
          }
          out_key.append(tail.rbegin(), tail.rend());
        } else {
          out_key += (char) v.node_bytes[node];
        }
        uint32_t ns = get_u16(v.term_rank + g * 2)
            + (uint32_t) __builtin_popcountll(get_word(v.flags, g, MBX_FLAG_TERM) & mask);
        if (ns == 0)
          break; // root node set of block
        node = select1(v, MBX_FLAG_CHILD, ns);
      }
      std::reverse(out_key.begin(), out_key.end());
      return true;
    }

    // Key with given leaf id
    bool reverse_lookup(uintxx_t leaf_id, std::string& out_key) const {
      if (!sec.has_main_lt || leaf_id >= leaf_lt.id_count) {
        out_key.clear();
        return false;
      }
      return reverse_lookup_from_node_id(leaf_select1(leaf_id), out_key);
    }

    // Same signatures as madras: ret_key must hold get_max_key_len() bytes
    bool reverse_lookup_from_node_id(uintxx_t node_id, uint32_t *in_size_out_key_len, uint8_t *ret_key) const {
      std::string k;
      if (!reverse_lookup_from_node_id(node_id, k))
        return false;
      if (k.length() > *in_size_out_key_len)
        return false;
      memcpy(ret_key, k.data(), k.length());
      *in_size_out_key_len = (uint32_t) k.length();
      return true;
    }

    bool reverse_lookup(uintxx_t leaf_id, uint32_t *in_size_out_key_len, uint8_t *ret_key) const {
      if (!sec.has_main_lt || leaf_id >= leaf_lt.id_count)
        return false;
      return reverse_lookup_from_node_id(leaf_select1(leaf_id), in_size_out_key_len, ret_key);
    }
};

} // namespace madras_blk

#endif
