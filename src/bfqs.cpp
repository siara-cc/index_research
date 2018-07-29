#include <math.h>
#include <stdint.h>
#include "bfqs.h"

const byte bfqs::need_counts[10] = {0, 4, 4, 2, 4, 0, 6, 0, 0, 0};

const byte bfqs::switch_map[8] = {0, 1, 2, 3, 0, 1, 0, 1};
const byte bfqs::shift_mask[8] = {0, 0, 3, 3, 15, 15, x3F, x3F};
