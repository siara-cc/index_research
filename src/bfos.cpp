#include <math.h>
#include <stdint.h>
#include "bfos.h"

#if BS_CHILD_PTR_SIZE == 1
const uint8_t bfos::need_counts[10] = {0, 4, 4, 2, 4, 0, 7, 0, 0, 0};
#else
const uint8_t bfos::need_counts[10] = {0, 4, 4, 2, 4, 0, 8, 0, 0, 0};
#endif
