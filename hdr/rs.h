#ifndef rs_h
#define rs_h
#include <stdio.h>

#include "db.h"

class rs {

private:
    class db* db;
    //class rs_sch* schema;
    char file_name[40];
    FILE *fh;
    char *data_buf;
    //idx *indices;
public:
    rs(class db *d, const char fname[]);
    int insert(const char data[], int len);

};

#endif
