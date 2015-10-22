#ifndef csv_h
#define csv_h

    // Parsing states
#define CSV_ST_NOT_STARTED 0
#define CSV_ST_DATA_STARTED_WITHOUT_QUOTE 1
#define CSV_ST_DATA_STARTED_WITH_QUOTE 2
#define CSV_ST_QUOTE_WITHIN_QUOTE 3
#define CSV_ST_DATA_ENDED_WITH_QUOTE 4
#define CSV_ST_FIELD_ENDED 5

class csv {

private:
    char *str;
    unsigned long len;
    unsigned long last_pos;
    short state;
public:
    csv(char s[], int l);
    long parse(char token[], int max_len);

};

#endif
