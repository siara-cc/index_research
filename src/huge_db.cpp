#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include "art.h"
#include "linex.h"
#include "dfox.h"
#ifdef _MSC_VER
#include <windows.h>
#include <unordered_map>
#else
#include <tr1/unordered_map>
#include <sys/time.h>
#endif

#define NUM_ENTRIES 2000000

using namespace std::tr1;
using namespace std;

void insert(unordered_map<string, string>& m) {
    char k[100];
    char v[100];
    srand(time(NULL));
    for (long l = 0; l < NUM_ENTRIES; l++) {

//        k[0] = 32 + (rand() % 95);
//        k[1] = 32 + (rand() % 95);
//        k[2] = 32 + (rand() % 95);
//        k[3] = 32 + (rand() % 95);
//        k[4] = 32 + (rand() % 95);
//        k[5] = 32 + (rand() % 95);
//        k[6] = 32 + (rand() % 95);
//        k[7] = 32 + (rand() % 95);
//        k[8] = 0;

//        k[0] = (rand() % 255);
//        k[1] = (rand() % 255);
//        k[2] = (rand() % 255);
//        k[3] = (rand() % 255);
//        k[4] = (rand() % 255);
//        k[5] = (rand() % 255);
//        k[6] = (rand() % 255);
//        k[7] = (rand() % 255);
//        k[8] = 0;

        long r = rand() * rand();
        for (int16_t b = 0; b < 4; b++) {
            char c = (r >> (24 - (3 - b) * 8));
            k[b * 2] = 48 + (c >> 4);
            k[b * 2 + 1] = 48 + (c & 0x0F);
        }
        r = rand() * rand();
        for (int16_t b = 4; b < 8; b++) {
            char c = (r >> (24 - (b - 4) * 8));
            k[b * 2] = 48 + (c >> 4);
            k[b * 2 + 1] = 48 + (c & 0x0F);
        }
        k[8] = 0;

//        k[0] = (l >> 24) & 0xFF;
//        k[1] = (l >> 16) & 0xFF;
//        k[2] = (l >> 8) & 0xFF;
//        k[3] = (l & 0xFF);
//        if (k[0] == 0) k[0]++;
//        if (k[1] == 0) k[1]++;
//        if (k[2] == 0) k[2]++;
//        if (k[3] == 0) k[3]++;
//        k[4] = 0;

        for (int16_t i = 0; i < 4; i++)
            v[3 - i] = k[i];
        v[4] = 0;
        //itoa(rand(), v, 10);
        //itoa(rand(), v + strlen(v), 10);
        //itoa(rand(), v + strlen(v), 10);
        if (l == 0)
            cout << "key:" << k << endl;
        m.insert(pair<string, string>(k, v));
    }
}

uint32_t getTimeVal() {
#ifdef _MSC_VER
    return GetTickCount() * 1000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000) + tv.tv_usec;
#endif
}

double timedifference(uint32_t t0, uint32_t t1) {
    double ret = t1;
    ret -= t0;
    ret /= 1000;
    return ret;
}

void print(dfox *dx, const char *key, int16_t key_len) {
    int16_t len;
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

void print(linex *dx, const char *key, int16_t key_len) {
    int16_t len;
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
    dx->put("hundred", 7, "100", 3);
    dx->put("boy", 3, "15", 2);
    dx->put("boat", 4, "16", 2);
    dx->put("thousand", 8, "1000", 4);
    dx->put("buoy", 4, "17", 2);
    dx->put("boast", 5, "18", 2);
    dx->put("January", 7, "first", 5);
    dx->put("February", 8, "second", 6);
    dx->put("March", 5, "third", 5);
    dx->put("April", 5, "forth", 5);
    dx->put("May", 3, "fifth", 5);
    dx->put("June", 4, "sixth", 5);
    dx->put("July", 4, "seventh", 7);
    dx->put("August", 6, "eighth", 6);
    dx->put("September", 9, "ninth", 5);
    dx->put("October", 7, "tenth", 5);
    dx->put("November", 8, "eleventh", 8);
    dx->put("December", 8, "twelfth", 7);
    dx->put("Sunday", 6, "one", 3);
    dx->put("Monday", 6, "two", 3);
    dx->put("Tuesday", 7, "three", 5);
    dx->put("Wednesday", 9, "four", 4);
    dx->put("Thursday", 8, "five", 4);
    dx->put("Friday", 6, "six", 3);
    dx->put("Saturday", 8, "seven", 7);
    dx->put("casa", 4, "nova", 4);
    dx->put("young", 5, "19", 2);
    dx->put("methyl", 6, "alcohol", 7);
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
    print(dx, "hundred", 7);
    print(dx, "boy", 3);
    print(dx, "boat", 4);
    print(dx, "thousand", 8);
    print(dx, "buoy", 4);
    print(dx, "boast", 5);
    print(dx, "January", 7);
    print(dx, "February", 8);
    print(dx, "March", 5);
    print(dx, "April", 5);
    print(dx, "May", 3);
    print(dx, "June", 4);
    print(dx, "July", 4);
    print(dx, "August", 6);
    print(dx, "September", 9);
    print(dx, "October", 7);
    print(dx, "November", 8);
    print(dx, "December", 8);
    print(dx, "Sunday", 6);
    print(dx, "Monday", 6);
    print(dx, "Tuesday", 7);
    print(dx, "Wednesday", 9);
    print(dx, "Thursday", 8);
    print(dx, "Friday", 6);
    print(dx, "Saturday", 8);
    print(dx, "casa", 4);
    print(dx, "young", 5);
    print(dx, "methyl", 6);
    print(dx, "youth", 5);
    print(dx, "yousuf", 6);

    dx->printMaxKeyCount(24);
    dx->printNumLevels();
    cout << "Trie size: " << (int) dx->root_data[MAX_PTR_BITMAP_BYTES] << endl;
    return 0;
}

int main3() {
    GenTree::generateBitCounts();
    //linex *dx = new linex();
    dfox *dx = new dfox();
    dx->put("\001\001\001\001", 4, "one", 3);
    dx->put("\001\001\001\x80", 4, "two", 3);
    print(dx, "\001\001\001\001", 4);
    print(dx, "\001\001\001\x80", 4);
    return 0;
}

int main1() {
    GenTree::generateBitCounts();
    //linex *dx = new linex();
    dfox *dx = new dfox();
    dx->put("-86?1:0:", 8, "-86?1:0:", 6);
    dx->put("6=59*900", 8, "6=59*900", 6);
    dx->put("70726<28", 8, "70726<28", 6);
    dx->put("2?7>(202", 8, "2?7>(202", 6);
    dx->put("55796807", 8, "55796807", 6);
    dx->put(",44;2?2:", 8, ",44;2?2:", 6);
    dx->put("7=,5*:05", 8, "7=,5*:05", 6);
    dx->put("5<(1*219", 8, "5<(1*219", 6);
    dx->put("55+3(01<", 8, "55+3(01<", 6);
    dx->put("063<791=", 8, "063<791=", 6);
    dx->put("/6056:0:", 8, "/6056:0:", 6);
    dx->put("06->1100", 8, "06->1100", 6);
    dx->put("4<*1*31;", 8, "4<*1*31;", 6);
    dx->put("-4,5(:0>", 8, "-4,5(:0>", 6);
    dx->put("4254+:18", 8, "4254+:18", 6);
    dx->put(")03;460<", 8, ")03;460<", 6);
    dx->put("30*:+717", 8, "30*:+717", 6);
    dx->put("4:*26302", 8, "4:*26302", 6);
    dx->put("/051)?00", 8, "/051)?00", 6);
    dx->put("08,46:0?", 8, "08,46:0?", 6);
    dx->put("0?4;.803", 8, "0?4;.803", 6);
    dx->put("19*9421<", 8, "19*9421<", 6);
    dx->put("(46?)900", 8, "(46?)900", 6);
    dx->put(",3427=37", 8, ",3427=37", 6);
    dx->put("-8485501", 8, "-8485501", 6);
    dx->put(".01?+?0=", 8, ".01?+?0=", 6);
    dx->put(".=.10<15", 8, ".=.10<15", 6);
    dx->put("6:+41400", 8, "6:+41400", 6);
    dx->put(".0,0(;17", 8, ".0,0(;17", 6);
    dx->put("40,3):11", 8, "40,3):11", 6);
    dx->put(")00;,328", 8, ")00;,328", 6);
    dx->put(")?*7.500", 8, ")?*7.500", 6);
    dx->put(",74;+804", 8, ",74;+804", 6);
    dx->put("-2)25806", 8, "-2)25806", 6);
    dx->put("4=05631=", 8, "4=05631=", 6);
    dx->put("54.34=11", 8, "54.34=11", 6);
    dx->put(".?347615", 8, ".?347615", 6);
    dx->put("-0/55623", 8, "-0/55623", 6);
    dx->put(")<745002", 8, ")<745002", 6);
    dx->put("*>,>/318", 8, "*>,>/318", 6);
    dx->put("01.;1003", 8, "01.;1003", 6);
    dx->put("0>33/<13", 8, "31</33", 6);
    print(dx, "-86?1:0:", 8);
    print(dx, "6=59*900", 8);
    print(dx, "70726<28", 8);
    print(dx, "2?7>(202", 8);
    print(dx, "55796807", 8);
    print(dx, ",44;2?2:", 8);
    print(dx, "7=,5*:05", 8);
    print(dx, "5<(1*219", 8);
    print(dx, "55+3(01<", 8);
    print(dx, "063<791=", 8);
    print(dx, "/6056:0:", 8);
    print(dx, "06->1100", 8);
    print(dx, "4<*1*31;", 8);
    print(dx, "-4,5(:0>", 8);
    print(dx, "4254+:18", 8);
    print(dx, ")03;460<", 8);
    print(dx, "30*:+717", 8);
    print(dx, "4:*26302", 8);
    print(dx, "/051)?00", 8);
    print(dx, "08,46:0?", 8);
    print(dx, "0?4;.803", 8);
    print(dx, "19*9421<", 8);
    print(dx, "(46?)900", 8);
    print(dx, ",3427=37", 8);
    print(dx, "-8485501", 8);
    print(dx, ".01?+?0=", 8);
    print(dx, ".=.10<15", 8);
    print(dx, "6:+41400", 8);
    print(dx, ".0,0(;17", 8);
    print(dx, "40,3):11", 8);
    print(dx, ")00;,328", 8);
    print(dx, ")?*7.500", 8);
    print(dx, ",74;+804", 8);
    print(dx, "-2)25806", 8);
    print(dx, "4=05631=", 8);
    print(dx, "54.34=11", 8);
    print(dx, ".?347615", 8);
    print(dx, "-0/55623", 8);
    print(dx, ")<745002", 8);
    print(dx, "*>,>/318", 8);
    print(dx, "01.;1003", 8);
    print(dx, "0>33/<13", 8);
    dx->printMaxKeyCount(NUM_ENTRIES);
    dx->printNumLevels();
    std::cout << "Size:" << dx->size() << endl;
    return 0;
}

int main() {

    GenTree::generateBitCounts();

    unordered_map<string, string> m;
    uint32_t start, stop;
    start = getTimeVal();
    insert(m);
    stop = getTimeVal();
    cout << "HashMap insert time:" << timedifference(start, stop) << endl;
    cout << "HashMap size:" << m.size() << endl;
    //getchar();
/*
    unordered_map<string, string>::iterator it;

     map<string, string> m1;
     m.begin();
    start = getTimeVal();
     it = m.begin();
     for (; it != m.end(); ++it) {
     m1.insert(pair<string, string>(it->first, it->second));
     }
    stop = getTimeVal();
     cout << "RB Tree insert time:" << timedifference(start, stop) << endl;
     it = m.begin();
     gettimeofday(&start, NULL);
     for (; it != m.end(); ++it) {
     m1[it->first];
     }
     gettimeofday(&stop, NULL);
     cout << "RB Tree get time:" << timedifference_msec(start, stop) << endl;
     cout << "RB Tree size:" << m1.size() << endl;
*/

     unordered_map<string, string>::iterator it1;

    art_tree at;
    art_tree_init(&at);
    start = getTimeVal();
    it1 = m.begin();
    for (; it1 != m.end(); ++it1) {
        art_insert(&at, (unsigned char*) it1->first.c_str(), it1->first.length(),
                (void *) it1->second.c_str());
    }
    stop = getTimeVal();
    cout << "ART Insert Time:" << timedifference(start, stop) << endl;
    //getchar();
    it1 = m.begin();
    start = getTimeVal();
    for (; it1 != m.end(); ++it1) {
        art_search(&at, (unsigned char*) it1->first.c_str(), it1->first.length());
    }
    stop = getTimeVal();
    cout << "ART Get Time:" << timedifference(start, stop) << endl;
    cout << "ART Size:" << art_size(&at) << endl;
    //getchar();

    int null_ctr = 0;
    int ctr = 0;
    int cmp = 0;

//            it = m.begin();
//            for (; it != m.end(); ++it) {
//                cout << "\"" << it->first.c_str() << "\", \"" << it->second.c_str()
//                        << "\"," << endl;
//                if (ctr++ > 40)
//                    break;
//            }

    null_ctr = 0;
    ctr = 0;
    cmp = 0;
    dfox *dx = new dfox();
    it1 = m.begin();
    start = getTimeVal();
    for (; it1 != m.end(); ++it1) {
        dx->put(it1->first.c_str(), it1->first.length(), it1->second.c_str(),
                it1->second.length());
        ctr++;
    }
    stop = getTimeVal();
    cout << "DFox+Tree insert time:" << timedifference(start, stop)
            << endl;
    //getchar();

    ctr = 0;
    it1 = m.begin();
    start = getTimeVal();
    for (; it1 != m.end(); ++it1) {
        int16_t len;
        char *value = dx->get(it1->first.c_str(), it1->first.length(), &len);
        char v[100];
        if (value == null) {
            null_ctr++;
        } else {
            int16_t d = util::compare(it1->second.c_str(), it1->second.length(),
                    value, len);
            if (d != 0) {
                cmp++;
                strncpy(v, value, len);
                v[it1->first.length()] = 0;
                cout << cmp << ":" << it1->first.c_str() << "=========="
                        << it1->second.c_str() << "----------->" << v << endl;
            }
        }
        ctr++;
    }
    stop = getTimeVal();
    cout << "Null:" << null_ctr << endl;
    cout << "Cmp:" << cmp << endl;
    cout << "DFox+Tree get time:" << timedifference(start, stop) << endl;
    std::cout << "Trie Size:" << (int) dx->root_data[MAX_PTR_BITMAP_BYTES] << endl;
    dx->printMaxKeyCount(NUM_ENTRIES);
    dx->printNumLevels();
    cout << "Root filled size:" << dx->root_data[MAX_PTR_BITMAP_BYTES+1] << endl;
    //getchar();

    null_ctr = 0;
    ctr = 0;
    cmp = 0;
    linex *lx = new linex();
    it1 = m.begin();
    start = getTimeVal();
    for (; it1 != m.end(); ++it1) {
        lx->put(it1->first.c_str(), it1->first.length(), it1->second.c_str(),
                it1->second.length());
        ctr++;
    }
    stop = getTimeVal();
    cout << "B+Tree insert time:" << timedifference(start, stop) << endl;
    //getchar();

    ctr = 0;
    it1 = m.begin();
    start = getTimeVal();
    for (; it1 != m.end(); ++it1) {
        int16_t len;
        char *value = lx->get(it1->first.c_str(), it1->first.length(), &len);
        char v[100];
        if (value == null) {
            null_ctr++;
        } else {
            int16_t d = util::compare(it1->second.c_str(), it1->second.length(),
                    value, len);
            if (d != 0) {
                cmp++;
                strncpy(v, value, len);
                v[it1->first.length()] = 0;
                cout << cmp << ":" << it1->first.c_str() << "=========="
                        << it1->second.c_str() << "----------->" << v << endl;
            }
        }
        ctr++;
    }
    stop = getTimeVal();
    cout << "B+Tree Get Time:" << timedifference(start, stop) << endl;
    cout << "Null:" << null_ctr << endl;
    cout << "Cmp:" << cmp << endl;
    lx->printMaxKeyCount(NUM_ENTRIES);
    lx->printNumLevels();
    cout << "Root filled size:" << util::getInt(lx->root_data + 1) << endl;

    //    if (ctr > 0 || cmp > 0) {
    //        it = m.begin();
    //        for (; it != m.end(); ++it) {
    //            cout << "\"" << it->first.c_str() << "\", \"" << it->second.c_str()
    //                    << "\"," << endl;
    //        }
    //    }

    return 0;

}
