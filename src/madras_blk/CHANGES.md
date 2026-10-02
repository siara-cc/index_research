# Files copied from madras-trie (include/madras/dv1)

Copied for madras_blk so it builds standalone. To be refactored back later.

| File | Source |
|---|---|
| in_mem_trie.hpp | builder/trie/memtrie/in_mem_trie.hpp |
| bv.hpp | ds_common/bv.hpp |
| byte_blocks.hpp | ds_common/byte_blocks.hpp |
| gen.hpp | ds_common/gen.hpp |
| compat.hpp | ds_common/compat.hpp |
| vint.hpp | ds_common/vint.hpp |

## Changes

1. in_mem_trie.hpp: include paths changed from `madras/dv1/ds_common/*.hpp` to local.
2. in_mem_trie.hpp: added `#include <memory>` (needed for std::unique_ptr).
3. in_mem_trie.hpp: constructor body moved into `init()`; added `reset()` which
   frees node sets and calls `init()`, so one instance can be reused per block.
4. byte_blocks.hpp: added `#include "vint.hpp"` (copy_fvint64 was otherwise
   undeclared when byte_blocks.hpp is included on its own).
