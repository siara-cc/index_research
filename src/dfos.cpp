#include <math.h>
#include <stdint.h>
#include "dfos.h"

#if DS_SIBLING_PTR_SIZE == 1
const byte dfos::need_counts[10] = {0, 2, 2, 2, 2, 0, 6, 0, 0, 0};
#else
const byte dfos::need_counts[10] = {0, 2, 2, 2, 2, 0, 7, 0, 0, 0};
#endif
