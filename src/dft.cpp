#include <math.h>
#include <stdint.h>
#include "dft.h"

const byte dft::need_counts[10] = {0, DFT_UNIT_SIZE, DFT_UNIT_SIZE, 0, DFT_UNIT_SIZE, 0, 0, 0, 0, 0};
