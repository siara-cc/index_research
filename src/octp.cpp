#include <math.h>
#include <stdint.h>
#include "octp.h"

#if BS_CHILD_PTR_SIZE == 1
const byte octp::need_counts[10] = {0, 4, 4, 2, 4, 0, 7, 0, 0, 0};
#else
const byte octp::need_counts[10] = {0, 4, 4, 2, 4, 0, 8, 0, 0, 0};
#endif
