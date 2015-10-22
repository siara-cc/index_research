#include <iostream>
#include "db.h"
#include "rs.h"
using namespace std;

int main() {

    db d("/tmp", "test");
    rs r(&d, "table1");
    r.insert("record1", 7);
    return 0;

}
