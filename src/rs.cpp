#include "rs.h"
#include "stdio.h"
#include "string.h"

rs::rs(class db *d, const char fname[]) {
    db = d;
    strcpy(file_name, fname);
    char full_path[200];
    strcpy(full_path, db->getPath());
    strcat(full_path, "/");
    strcat(full_path, fname);
    fh = fopen(full_path, "r+");
    // if file could not be found
    if (!fh) {
        // create it
        fh = fopen(full_path, "w+");
        // create the header, indicating rs static characterestics
        // static characterestics don't change for the lifetime
    }
}


int rs::insert(const char data[], int len) {

}
