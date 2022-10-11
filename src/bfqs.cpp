#include <math.h>
#include <stdint.h>
#include "bfqs.h"

const uint8_t bfqs::need_counts[10] = {0, 4, 4, 2, 4, 0, 6, 0, 0, 0};

const uint8_t bfqs::switch_map[8] = {0, 1, 2, 3, 0, 1, 0, 1};
const uint8_t bfqs::shift_mask[8] = {0, 0, 3, 3, 15, 15, x3F, x3F};
