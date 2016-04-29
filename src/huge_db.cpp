#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <tr1/unordered_map>
#include "art.h"
#include "linex.h"
#include "dfox.h"
#include <sys/time.h>

#define NUM_ENTRIES 4000000

using namespace std::tr1;
using namespace std;

void insert(unordered_map<string, string>& m) {
    char k[100];
    char v[100];
    srand(time(NULL));
    for (long l = 0; l < NUM_ENTRIES; l++) {
        long r = rand() * rand();
        for (int b = 0; b < 4; b++) {
            char c = (r >> (24 - (3 - b) * 8));
            k[b * 2] = 48 + (c >> 4);
            k[b * 2 + 1] = 48 + (c & 0x0F);
        }
        r = rand() * rand();
        for (int b = 4; b < 8; b++) {
            char c = (r >> (24 - (b - 4) * 8));
            k[b * 2] = 48 + (c >> 4);
            k[b * 2 + 1] = 48 + (c & 0x0F);
        }
        k[8] = 0;
        for (int i = 0; i < 8; i++)
            v[7 - i] = k[i];
        v[4] = 0;
        //itoa(rand(), v, 10);
        //itoa(rand(), v + strlen(v), 10);
        //itoa(rand(), v + strlen(v), 10);
        if (l == 0)
            cout << "key:" << k << endl;
        m.insert(pair<string, string>(k, v));
    }
}
float timedifference_msec(struct timeval t0, struct timeval t1) {
    return (t1.tv_sec - t0.tv_sec) * 1000.0f
            + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}

void print(dfox *dx, const char *key, int key_len) {
    int len;
    char *value = dx->get(key, key_len, &len);
    if (value == null || len == 0) {
        std::cout << "Value for " << key << " is null" << endl;
        return;
    }
    char s[100];
    strncpy(s, value, len);
    s[len] = 0;
    std::cout << "Key: " << key << ", Value:" << s << endl;
}

void print(linex *dx, const char *key, int key_len) {
    int len;
    char *value = dx->get(key, key_len, &len);
    if (value == null || len == 0) {
        std::cout << "Value for " << key << " is null" << endl;
        return;
    }
    char s[100];
    strncpy(s, value, len);
    s[len] = 0;
    std::cout << "Key: " << key << ", Value:" << s << endl;
}

int main2() {
    GenTree::generateBitCounts();
    dfox *dx = new dfox();
    //linex *dx = new linex();
    dx->put("Hello", 5, "World", 5);
    dx->put("Nice", 4, "Place", 5);
    dx->put("Arun", 4, "Hello", 5);
    dx->put("arun", 4, "0", 1);
    dx->put("resin", 5, "34623", 5);
    dx->put("rinse", 5, "2", 1);
    dx->put("rickshaw", 8, "4", 1);
    dx->put("ride", 4, "5", 1);
    dx->put("rider", 5, "6", 1);
    dx->put("rice", 4, "7", 1);
    dx->put("rick", 4, "8", 1);
    dx->put("aruna", 5, "9", 1);
    dx->put("hello", 5, "10", 2);
    dx->put("world", 5, "11", 2);
    dx->put("how", 3, "12", 2);
    dx->put("are", 3, "13", 2);
    dx->put("you", 3, "14", 2);
    dx->put("boy", 3, "15", 2);
    dx->put("boat", 4, "16", 2);
    dx->put("buoy", 4, "17", 2);
    dx->put("boast", 5, "18", 2);
    dx->put("young", 5, "19", 2);
    dx->put("youth", 5, "20", 2);
    dx->put("yousuf", 6, "21", 2);
    print(dx, "Hello", 5);
    print(dx, "Nice", 4);
    print(dx, "Arun", 4);
    print(dx, "arun", 4);
    print(dx, "resin", 5);
    print(dx, "rinse", 5);
    print(dx, "rickshaw", 8);
    print(dx, "ride", 4);
    print(dx, "rider", 5);
    print(dx, "rice", 4);
    print(dx, "rick", 4);
    print(dx, "aruna", 5);
    print(dx, "hello", 5);
    print(dx, "world", 5);
    print(dx, "how", 3);
    print(dx, "are", 3);
    print(dx, "you", 3);
    print(dx, "boy", 3);
    print(dx, "boat", 4);
    print(dx, "buoy", 4);
    print(dx, "boast", 5);
    print(dx, "young", 5);
    print(dx, "youth", 5);
    print(dx, "yousuf", 6);
    dx->printMaxKeyCount(24);
    dx->printNumLevels();
    return 0;
}

int main1() {
    GenTree::generateBitCounts();
    dfox *dx = new dfox();
    dx->put("02,;0:*0025:51/:", 16, "0*:0;,20", 8);
    dx->put("056=*6(0003>*8*0", 16, "0(6*=650", 8);
    dx->put("06+0*978031:01+:", 16, "879*0+60", 8);
    dx->put("114:(5/406)>*60:", 16, "4/5(:411", 8);
    dx->put("2:(4(8);0223-80>", 16, ";)8(4(:2", 8);
    dx->put("01(70:-<137=53(=", 16, "<-:07(10", 8);
    dx->put("122;*22010.049.=", 16, "022*;221", 8);
    dx->put("1=3840-726.25736", 16, "7-0483=1", 8);
    dx->put("003:-202052077+4", 16, "202-:300", 8);
    dx->put("00-2.5(;1211.740", 16, ";(5.2-00", 8);
    dx->put("000<+3382218)92<", 16, "833+<000", 8);
    dx->put("0944+66:2:16(82<", 16, ":66+4490", 8);
    dx->put("07023:(0001>*578", 16, "0(:32070", 8);
    dx->put("2863.=381237)6.0", 16, "83=.3682", 8);
    dx->put("00*2+5700233,?*0", 16, "075+2*00", 8);
    dx->put("20*36>-20733*070", 16, "2->63*02", 8);
    dx->put("1?*<301624*0*<40", 16, "6103<*?1", 8);
    dx->put("10*8,7+61236*465", 16, "6+7,8*01", 8);
    dx->put("05+>+36>0=.421/8", 16, ">63+>+50", 8);
    dx->put("00+5/33801->+?)<", 16, "833/5+00", 8);
    char *value = null;
    int len;
    //value = dx->get("2:(4(8);0223-80>", 16, &len);
    value = dx->get("1=3840-726.25736", 16, &len);
    if (value != null) {
        char v[100];
        memset(v, 0, sizeof(v));
        strncpy(v, value, len);
        cout << v << endl;
    } else
        cout << "Value null" << endl;
    std::cout << "Trie Size:" << dx->size() << endl;
    return 0;
}

int main() {

    GenTree::generateBitCounts();

    struct timeval stop, start;
    unordered_map<string, string> m;
    gettimeofday(&start, NULL);
    insert(m);
    gettimeofday(&stop, NULL);
    cout << "HashMap insert time:" << timedifference_msec(start, stop) << endl;
    cout << "HashMap size:" << m.size() << endl;

    unordered_map<string, string>::iterator it;

    /*
     map<string, string> m1;
     m.begin();
     gettimeofday(&start, NULL);
     it = m.begin();
     for (; it != m.end(); ++it) {
     m1.insert(pair<string, string>(it->first, it->second));
     }
     gettimeofday(&stop, NULL);
     cout << "RB Tree insert time:" << timedifference_msec(start, stop) << endl;
     it = m.begin();
     gettimeofday(&start, NULL);
     for (; it != m.end(); ++it) {
     m1[it->first];
     }
     gettimeofday(&stop, NULL);
     cout << "RB Tree get time:" << timedifference_msec(start, stop) << endl;
     cout << "RB Tree size:" << m1.size() << endl;

    art_tree at;
    art_tree_init(&at);
    gettimeofday(&start, NULL);
    it = m.begin();
    for (; it != m.end(); ++it) {

        art_insert(&at, (unsigned char*) it->first.c_str(), it->first.length(),
                (void *) it->second.c_str());
    }
    gettimeofday(&stop, NULL);
    cout << "ART Insert Time:" << timedifference_msec(start, stop) << endl;
    it = m.begin();
    gettimeofday(&start, NULL);
    for (; it != m.end(); ++it) {
        art_search(&at, (unsigned char*) it->first.c_str(), it->first.length());
    }
    gettimeofday(&stop, NULL);
    cout << "ART Get Time:" << timedifference_msec(start, stop) << endl;
    cout << "ART Size:" << art_size(&at) << endl;
     */

    int ctr = 0;
    int cmp = 0;

    linex *lx = new linex();
    it = m.begin();
    gettimeofday(&start, NULL);
    for (; it != m.end(); ++it) {
        lx->put(it->first.c_str(), it->first.length(), it->second.c_str(),
                it->second.length());
    }
    gettimeofday(&stop, NULL);
    cout << "B+Tree insert time:" << timedifference_msec(start, stop) << endl;

    ctr = 0;
    cmp = 0;
    it = m.begin();
    gettimeofday(&start, NULL);
    for (; it != m.end(); ++it) {
        int len;
        char *value = lx->get(it->first.c_str(), it->first.length(), &len);
//        char v[100];
//        if (value == null) {
//            ctr++;
//        } else {
//            int d = util::compare(it->second.c_str(), it->second.length(),
//                    value, len);
//            if (d != 0) {
//                cmp++;
//                strncpy(v, value, len);
//                v[it->first.length()] = 0;
//                cout << cmp << ":" << it->first.c_str() << "=========="
//                        << it->second.c_str() << "----------->" << v << endl;
//            }
//        }
    }
    gettimeofday(&stop, NULL);
    cout << "B+Tree Get Time:" << timedifference_msec(start, stop) << endl;
    cout << "Null:" << ctr << endl;
    cout << "Cmp:" << cmp << endl;
    lx->printMaxKeyCount(NUM_ENTRIES);
    lx->printNumLevels();
    cout << "Root filled size:" << lx->root->filledSize() << endl;

    dfox *dx = new dfox();
    it = m.begin();
    gettimeofday(&start, NULL);
    for (; it != m.end(); ++it) {
        dx->put(it->first.c_str(), it->first.length(), it->second.c_str(),
                it->second.length());
    }
    gettimeofday(&stop, NULL);
    cout << "DFox+Tree insert time:" << timedifference_msec(start, stop)
            << endl;

    it = m.begin();
    gettimeofday(&start, NULL);
    for (; it != m.end(); ++it) {
        int len;
        char *value = dx->get(it->first.c_str(), it->first.length(), &len);
//        char v[100];
//        if (value == null) {
//            ctr++;
//        } else {
//            int d = util::compare(it->second.c_str(), it->second.length(),
//                    value, len);
//            if (d != 0) {
//                cmp++;
//                strncpy(v, value, len);
//                v[it->first.length()] = 0;
//                cout << cmp << ":" << it->first.c_str() << "=========="
//                        << it->second.c_str() << "----------->" << v << endl;
//            }
//        }
    }
    gettimeofday(&stop, NULL);
    cout << "Null:" << ctr << endl;
    cout << "Cmp:" << cmp << endl;
    cout << "DFox+Tree get time:" << timedifference_msec(start, stop) << endl;
    std::cout << "Trie Size:" << dx->size() << endl;
    dx->printMaxKeyCount(NUM_ENTRIES);
    dx->printNumLevels();
    cout << "Root filled size:" << dx->root->filledSize() << endl;

//    if (ctr > 0 || cmp > 0) {
//        it = m.begin();
//        for (; it != m.end(); ++it) {
//            cout << "\"" << it->first.c_str() << "\", \"" << it->second.c_str()
//                    << "\"," << endl;
//        }
//    }

    return 0;

}
