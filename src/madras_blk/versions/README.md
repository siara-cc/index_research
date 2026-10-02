# madras_blk versions

Each file is a complete drop-in replacement for `src/madras_blk.h`.
To go back, copy the version over `src/madras_blk.h`.

| Version | Changes |
|---|---|
| v1 | Phase 1: plain tails, node byte = first tail byte, lookup only |
| v2 | Options `node_byte_mode` (MBX_NODE_BYTE_FIRST / MBX_NODE_BYTE_PTR) and `tail_sfx` (suffix sharing in tail area). Both recorded in meta. Separate leaf lookup per node byte mode. |
| v3 | Faster leaf lookup: select via `_pdep_u64` (BMI2, loop fallback), skipping tail pointer fvints 8 bytes at a time. Output buffer padded by 8 bytes. Same format otherwise. |
| v4 | Inner tries (`max_inner_tries`, requires node_byte_mode PTR). Builder plans all levels top down, then emits all levels. Level descriptors and per-level lookup tables (cumulative leaf ids per block + direct-mapped sample) stored after meta. Inner blocks reuse the leaf block layout (leaf bit = a reversed tail ends here). Reader reads v3 files (no descriptors = one plain level). `builder::ts` replaced by `plain_store()`. |
| v5 | Reverse lookup, same API as madras: `lookup(input_ctx&)` sets `node_id`; `reverse_lookup(leaf_id, ...)`, `reverse_lookup_from_node_id(node_id, ...)` (std::string and madras buffer variants), `leaf_rank1`, `leaf_select1`, `is_leaf`. Main trie id tables (cumulative node and key counts per leaf block, each with direct-mapped sample) stored with the other lookup tables after meta (`has_main_lt` flag). Reads v3/v4 files (forward lookup only). |
| v6 | Select lookup tables per block for term, child and leaf bits (`sel_shift`, default 6 = one 2-byte group number per 64 set bits). `select1` starts at the sampled group and walks the rank array forward; `sel_shift` 0 keeps binary search. Block header 12 bytes with select lookup ([8..9] node set count). Pricer tracks node sets and keys so block sizes stay exact. Reads v5 files. |
| v7 | Select entries 1 byte: entry = group >> sel_grp_shift, where the shift is derived from the leaf block size so all groups fit (exact group up to 16 KB, 2 groups at 32 KB, 4 groups at 64 KB); select walks the rank array forward from entry << shift. Format version byte (meta[37]) = 2; reader still reads version 1 (v6) files with 2-byte entries. |
| v8 | Select entries always 1 byte: entry width check removed from `select1` and `init_view`. Version 1 (v6) files with select lookup are rejected by `load()`. Same format as v7. |

Benchmark: word_freq_sorted.txt (1,096,763 keys, 14.5 MB), 4 KB blocks, random order lookup.

| Config (node_byte_mode, tail_sfx) | Size | Random lookup | Sorted lookup |
|---|---|---|---|
| v3 (0, 0) | 4,174,810 | 762 ms | 358 ms |
| v3 (1, 0) | 3,808,477 | 902 ms | 421 ms |
| v3 (0, 1) | 4,145,046 | 808 ms | 373 ms |
| v3 (1, 1) | 3,622,432 | 874 ms | 372 ms |
| marisa num_tries 1 | 3,770,152 | 760 ms | 289 ms |
| marisa num_tries 3 | 3,434,848 | 851 ms | 360 ms |

v4 with node_byte_mode 1, tail_sfx 1, same dataset:

| max_inner_tries | Size | Main trie | Tail levels | Random lookup | Sorted lookup |
|---|---|---|---|---|---|
| 0 | 3,622,432 | 832 blocks | plain 194,072 | 873 ms | 388 ms |
| 1 | 3,514,103 | 819 blocks | inner 26 blocks + plain 28,399 | 1,449 ms | 647 ms |
| 2 | 3,508,688 | 819 blocks | inner 26 + 4 blocks + plain 6,600 | 1,478 ms | 670 ms |
| 3 | 3,512,886 | 819 blocks | | 1,477 ms | 665 ms |

v5 reverse lookup (1,096,763 random leaf ids, node_byte_mode 1, tail_sfx 1):

| | Size | Reverse lookup | Forward lookup |
|---|---|---|---|
| v5, 0 inner tries | 3,642,912 | 494 ms | 886 ms |
| v5, 1 inner trie | 3,530,487 | 719 ms | 1,437 ms |
| v5, 2 inner tries | 3,525,072 | 725 ms | 1,511 ms |
| marisa num_tries 1 | 3,770,152 | 620 ms | 740 ms |
| marisa num_tries 3 | 3,434,848 | 771 ms | 905 ms |

v6 select lookup (node_byte_mode 1, tail_sfx 1). Benchmark: `MBX_SEL=<shift>` environment variable.

| Block | Inner | sel_shift | Size | Forward rand | Forward sorted | Reverse rand |
|---|---|---|---|---|---|---|
| 4 KB | 0 | 0 (binary search) | 3,642,912 | 913 ms | 389 ms | 506 ms |
| 4 KB | 0 | 5 | 3,798,927 | 715 ms | 361 ms | 439 ms |
| 4 KB | 0 | 6 | 3,724,847 | 751 ms | 378 ms | 457 ms |
| 4 KB | 0 | 7 | 3,692,071 | 763 ms | 378 ms | 451 ms |
| 4 KB | 1 | 0 | 3,530,487 | 1,384 ms | 685 ms | 692 ms |
| 4 KB | 1 | 6 | 3,616,635 | 1,177 ms | 614 ms | 632 ms |
| 64 KB | 0 | 0 | 3,796,354 | 1,203 ms | 433 ms | 684 ms |
| 64 KB | 0 | 6 | 3,861,944 | 882 ms | 366 ms | 497 ms |
| 64 KB | 1 | 0 | 3,698,207 | 2,084 ms | 834 ms | 968 ms |
| 64 KB | 1 | 6 | 3,829,298 | 1,254 ms | 577 ms | 619 ms |

v7 vs v6 select entries (node_byte_mode 1, tail_sfx 1, 3 alternating runs each, medians):

| Block | Inner | sel_shift | v6 size (2-byte) | v7 size (1-byte) | v6 fwd / rev | v7 fwd / rev |
|---|---|---|---|---|---|---|
| 4 KB | 0 | 5 | 3,798,927 | 3,725,109 | 817 / 486 ms | 826 / 488 ms |
| 4 KB | 0 | 6 | 3,724,847 | 3,692,273 | 833 / 491 ms | 841 / 493 ms |
| 4 KB | 0 | 7 | 3,692,071 | 3,671,798 | 858 / 507 ms | 863 / 499 ms |
| 4 KB | 1 | 6 | 3,616,635 | 3,575,627 | 1,248 / 672 ms | 1,250 / 674 ms |
| 64 KB | 0 | 6 | 3,861,944 | 3,861,898 | 957 / 499 ms | 977 / 539 ms |
| 64 KB | 1 | 6 | 3,829,298 | 3,763,747 | 1,330 / 631 ms | 1,415 / 687 ms |

On 64 KB blocks file size changes in whole blocks, so the per-entry saving shows up only in steps.

v8 vs v7 (4 KB blocks, node_byte_mode 1, tail_sfx 1): no measurable difference (within run-to-run noise).
Without BMI2 (`-mno-bmi2`, as on ARM / Apple Silicon), select and fvint skip fall back to loops:

| Inner tries | Forward BMI2 / no BMI2 | Reverse BMI2 / no BMI2 |
|---|---|---|
| 0 | ~855 / ~920 ms | ~495 / ~620 ms |
| 2 | ~1,300 / ~1,770 ms | ~670 / ~935 ms |
