#include "csv.h"
#include "stdio.h"
#include "string.h"

csv::csv(char s[], int l) {
    str = s;
    len = l;
    last_pos = 0;
    state = CSV_ST_NOT_STARTED;
}

#define CSV_ST_NOT_STARTED 0
#define CSV_ST_DATA_STARTED_WITHOUT_QUOTE 1
#define CSV_ST_DATA_STARTED_WITH_QUOTE 2
#define CSV_ST_QUOTE_WITHIN_QUOTE 3
#define CSV_ST_DATA_ENDED_WITH_QUOTE 4
#define CSV_ST_FIELD_ENDED 5

long csv::parse(char token[], int max) {
    return 0;
}
