#include "GenTree.h"

byte GenTree::bit_count[256];
byte GenTree::last_bit_mask[256];
int16_t *GenTree::roots;
int16_t *GenTree::left;
int16_t *GenTree::ryte;
int16_t *GenTree::parent;
int16_t GenTree::ixRoots;
int16_t GenTree::ixLeft;
int16_t GenTree::ixRyte;
int16_t GenTree::ixPrnt;
uint64_t GenTree::left_mask64[64];
uint64_t GenTree::ryte_mask64[64];
uint64_t GenTree::mask64[64];
