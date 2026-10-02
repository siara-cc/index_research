#ifndef _BYTE_BLOCKS_HPP_
#define _BYTE_BLOCKS_HPP_

#include "gen.hpp"
#include "vint.hpp"

namespace gen {

class block_store {
  protected:
    block_store& operator=(const block_store&) = delete;
    const size_t block_size;
    size_t count;
    std::vector<uint8_t *> blocks;
    bit_vector<uint8_t> is_allocated;
    size_t block_remaining;
  public:
    block_store(size_t _block_size)
      : block_size(_block_size), count(0),
        block_remaining(0) {
    }
    virtual ~block_store() {
      release_blocks();
    }
    void release_blocks() {
      for (size_t i = 0; i < blocks.size(); i++) {
        if (is_allocated[i])
          delete [] blocks[i];
      }
      blocks.clear();
      is_allocated.reset();
      block_remaining = 0;
      count = 0;
    }
    size_t size_in_bytes() {
      return block_size * blocks.size() - block_remaining;
    }
    size_t entry_count() {
      return count;
    }
    size_t get_block_size() {
      return block_size;
    }
};

template <class T>
class fixed_block_store : public block_store {
  protected:
    fixed_block_store& operator=(const fixed_block_store&) = delete;
    fixed_block_store& operator=(fixed_block_store&&) = delete;
    T *reserve(size_t val_len, size_t& pos) {
      if (!blocks.empty() && val_len <= block_remaining) {
        uint8_t* block = blocks.back();
        size_t used = block_size - block_remaining;
        pos = (blocks.size()-1) * block_size + used;
        block_remaining -= val_len;
        return reinterpret_cast<T*>(block + used);
      }
      size_t needed_blocks = val_len / block_size;
      if (val_len % block_size)
        needed_blocks++;

      uint8_t *new_block = reinterpret_cast<uint8_t *>(
          new uint64_t[needed_blocks * block_size / sizeof(uint64_t)]);
      pos = blocks.size() * block_size;
      size_t base = blocks.size();
      for (size_t i = 0; i < needed_blocks; i++) {
        is_allocated.set(base + i, i == 0);
        blocks.push_back(new_block + i * block_size);
      }
      block_remaining = needed_blocks * block_size - val_len;
      return reinterpret_cast<T*>(new_block);
    }

  public:
    fixed_block_store(size_t _block_size = 4096) : block_store (_block_size) {
    }
    T* operator[](size_t pos) {
      pos *= sizeof(T);
      return reinterpret_cast<T*>(blocks[pos / block_size] + (pos % block_size));
    }
    T *push_back(T val) {
      size_t pos;
      T *buf = reserve(sizeof(T), pos);
      *buf = val;
      count++;
      return buf;
    }

    void push_back_n(const T *vals, size_t n) {
      if (n == 0) return;

      size_t total_bytes = n * sizeof(T);
      const uint8_t *src = reinterpret_cast<const uint8_t *>(vals);

      // 1. Fill whatever space remains in the current (last) block first.
      if (!blocks.empty() && block_remaining > 0) {
        size_t fill_bytes = std::min(block_remaining, total_bytes);
        fill_bytes -= fill_bytes % sizeof(T);  // keep T-aligned

        if (fill_bytes > 0) {
          uint8_t *block = blocks.back();
          size_t used = block_size - block_remaining;
          std::memcpy(block + used, src, fill_bytes);

          block_remaining -= fill_bytes;
          count += fill_bytes / sizeof(T);
          src += fill_bytes;
          total_bytes -= fill_bytes;
        }
      }

      if (total_bytes == 0) return;

      // 2. Allocate one contiguous run of blocks for everything left over.
      size_t needed_blocks = total_bytes / block_size;
      if (total_bytes % block_size)
        needed_blocks++;

      uint8_t *new_chunk = reinterpret_cast<uint8_t *>(
          new uint64_t[needed_blocks * block_size / sizeof(uint64_t)]);
      std::memcpy(new_chunk, src, total_bytes);

      size_t base = blocks.size();
      for (size_t i = 0; i < needed_blocks; i++) {
        is_allocated.set(base + i, i == 0);
        blocks.push_back(new_chunk + i * block_size);
      }

      block_remaining = needed_blocks * block_size - total_bytes;
      count += total_bytes / sizeof(T);
    }
};

class byte_blocks : public block_store {
  private:
    std::vector<uint16_t> block_lens;

  protected:
    uint8_t *reserve_dont_deduct(size_t val_len, size_t& pos, size_t extra = 0) {
      if (val_len == 0)
        val_len = 1;

      if (val_len + extra <= block_remaining) {
        uint8_t *ret = blocks.back();
        pos = blocks.size() * block_size;
        pos -= block_size;
        size_t block_pos = (block_size - block_remaining);
        pos += block_pos;
        block_lens.back() += (uint16_t) val_len;
        return ret + block_pos;
      }

      size_t needed_bytes = val_len;
      size_t needed_blocks = needed_bytes / block_size;
      if (needed_bytes % block_size)
        needed_blocks++;

      block_remaining = needed_blocks * block_size;

      // Align to 8 bytes
      uint8_t *new_block = reinterpret_cast<uint8_t *>(new uint64_t[needed_blocks * block_size / sizeof(uint64_t)]);
      //memset(new_block, '\0', needed_blocks * block_size);

      pos = blocks.size() * block_size ;

      size_t base = blocks.size();
      for (size_t i = 0; i < needed_blocks; i++) {
        is_allocated.set(base + i, i == 0);
        blocks.push_back(new_block + i * block_size);
        block_lens.push_back((uint16_t) val_len);
      }

      return new_block;
    }
    uint8_t *reserve(size_t val_len, size_t& pos, size_t extra = 0) {
      uint8_t *ret = reserve_dont_deduct(val_len, pos, extra);
      block_remaining -= val_len;
      return ret;
    }

  public:
    byte_blocks& operator=(const byte_blocks&) = delete;
    byte_blocks(size_t _block_size = 4096) : block_store (_block_size) {
      count = 0;
      block_remaining = 0;
    }

    virtual ~byte_blocks() {
      release_blocks();
    }
    void reset() {
      release_blocks();
    }
    size_t get_size_in_bytes() {
      return block_size * blocks.size() + is_allocated.size_bytes();
    }
    size_t push_back(const void *val, size_t val_len) {
      size_t pos;
      uint8_t *buf = reserve(val_len, pos);
      memcpy(buf, val, val_len);
      count++;
      return pos;
    }
    size_t push_back_rev(const void *val, size_t val_len) {
      size_t pos;
      uint8_t *buf = reserve(val_len, pos);
      for (int i = (int) val_len - 1; i >= 0; i--)
        buf[val_len - i - 1] = ((uint8_t *) val)[i];
      count++;
      return pos;
    }
    size_t push_back_fvint64(uint64_t u64, size_t &vlen, size_t extra = 0) {
      uint8_t fvint[16];
      vlen = gen::copy_fvint64(fvint, u64);
      size_t pos;
      uint8_t *buf = reserve(vlen, pos, extra);
      memcpy(buf, fvint, vlen);
      count++;
      return pos;
    }
    size_t push_back_fvint64_diff_from_new_pos(uint64_t u64, size_t &vlen, size_t min_left) {
      size_t pos;
      uint8_t *buf = reserve_dont_deduct(min_left, pos);
      uint8_t fvint[16];
      vlen = gen::copy_fvint64(fvint, u64 == UINT64_MAX ? 0 : pos - u64);
      memcpy(buf, fvint, vlen);
      block_remaining -= vlen;
      block_lens.back() -= (uint16_t) min_left;
      block_lens.back() += (uint16_t) vlen;
      count++;
      return pos;
    }
    size_t push_back_with_vlen(const void *val, size_t val_len, size_t extra = 0) {
      size_t pos;
      uint8_t vlen = gen::fast_vuint64_size(val_len);
      uint8_t *buf = reserve(val_len + vlen, pos, extra);
      uint64_t packed = ((uint64_t) val_len << 4) | vlen;
      memcpy(buf, &packed, vlen);
      memcpy(buf + vlen, val, val_len);
      count++;
      return pos;
    }
    uint8_t *operator[](size_t pos) {
      return blocks[pos / block_size] + (pos % block_size);
    }
    uint8_t *get(size_t pos) const {
      return blocks[pos / block_size] + (pos % block_size);
    }
    uint8_t *advance_by(size_t &pos, size_t delta) {
      size_t block_id = pos / block_size;
      size_t block_pos = pos % block_size;
      if (block_id >= block_lens.size()) return nullptr;
      uint16_t block_len = block_lens[block_id];
      if (block_pos + delta < block_len) {
        pos += delta;
        return blocks[block_id] + block_pos + delta;
      }
      block_id += (delta / block_size);
      if ((delta % block_size) != 0) block_id++;
      if (block_id < blocks.size()) {
        pos = block_id * block_size;
        return blocks[block_id];
      }
      return nullptr;
    }
};

}

#endif
