#ifndef db_h
#define db_h

class db {

private:
    char path[100];
    char name[40];
public:
    db(const char p[], const char n[]);
    const char * getPath();

};

#endif
