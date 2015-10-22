#include "db.h"
#include "string.h"
#include "stdio.h"
#include <sys/stat.h>

db::db(const char p[], const char n[]) {
    strcpy(path, p);
    strcat(path, "/");
    strcat(path, n);
    strcpy(name, n);
    mkdir(path, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
}

const char * db::getPath() {
    return path;
}
