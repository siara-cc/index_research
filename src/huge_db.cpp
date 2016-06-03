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
#include "dfos.h"
#include "dfox.h"
#include "rb_tree.h"
#ifdef _MSC_VER
#include <windows.h>
#include <unordered_map>
#else
#include <tr1/unordered_map>
#include <sys/time.h>
#endif

using namespace std::tr1;
using namespace std;

#define CS_PRINTABLE 1
#define CS_NUMBER_ONLY 2
#define CS_ONE_PER_OCTET 3
#define CS_255_RANDOM 4
#define CS_255_DENSE 5

long NUM_ENTRIES = 24;
int CHAR_SET = 1;
int KEY_LEN = 8;

void insert(unordered_map<string, string>& m) {
    char k[100];
    char v[100];
    srand(time(NULL));
    for (long l = 0; l < NUM_ENTRIES; l++) {

        if (CHAR_SET == CS_PRINTABLE) {
            k[0] = 32 + (rand() % 95);
            k[1] = 32 + (rand() % 95);
            k[2] = 32 + (rand() % 95);
            k[3] = 32 + (rand() % 95);
            k[4] = 32 + (rand() % 95);
            k[5] = 32 + (rand() % 95);
            k[6] = 32 + (rand() % 95);
            k[7] = 32 + (rand() % 95);
            k[KEY_LEN] = 0;
        } else if (CHAR_SET == CS_NUMBER_ONLY) {
            k[0] = 48 + (rand() % 10);
            k[1] = 48 + (rand() % 10);
            k[2] = 48 + (rand() % 10);
            k[3] = 48 + (rand() % 10);
            k[4] = 48 + (rand() % 10);
            k[5] = 48 + (rand() % 10);
            k[6] = 48 + (rand() % 10);
            k[7] = 48 + (rand() % 10);
            k[7] = 48 + (rand() % 10);
            k[KEY_LEN] = 0;
        } else if (CHAR_SET == CS_ONE_PER_OCTET) {
            k[0] = ((rand() % 32) << 3) | 0x07;
            k[1] = ((rand() % 32) << 3) | 0x07;
            k[2] = ((rand() % 32) << 3) | 0x07;
            k[3] = ((rand() % 32) << 3) | 0x07;
            k[4] = ((rand() % 32) << 3) | 0x07;
            k[5] = ((rand() % 32) << 3) | 0x07;
            k[6] = ((rand() % 32) << 3) | 0x07;
            k[7] = ((rand() % 32) << 3) | 0x07;
            k[KEY_LEN] = 0;
        } else if (CHAR_SET == CS_255_RANDOM) {
            k[0] = (rand() % 255);
            k[1] = (rand() % 255);
            k[2] = (rand() % 255);
            k[3] = (rand() % 255);
            k[4] = (rand() % 255);
            k[5] = (rand() % 255);
            k[6] = (rand() % 255);
            k[7] = (rand() % 255);
            k[KEY_LEN] = 0;
            for (int i = 0; i < 8; i++) {
                if (k[i] == 0)
                    k[i] = i + 1;
            }
        } else if (CHAR_SET == CS_255_DENSE) {
            k[0] = (l >> 24) & 0xFF;
            k[1] = (l >> 16) & 0xFF;
            k[2] = (l >> 8) & 0xFF;
            k[3] = (l & 0xFF);
            if (k[0] == 0)
                k[0]++;
            if (k[1] == 0)
                k[1]++;
            if (k[2] == 0)
                k[2]++;
            if (k[3] == 0)
                k[3]++;
            k[4] = 0;
        }

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
    NUM_ENTRIES = m.size();
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
/*
void print(rb_tree *dx, const char *key, int16_t key_len) {
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
*/

void print(dfos *dx, const char *key, int16_t key_len) {
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

int main() {
    GenTree::generateBitCounts();
    //rb_tree *dx = new rb_tree();
    dfos *dx = new dfos();
    //dfox *dx = new dfox();
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
    dx->put("rid", 3, "5.5", 3);
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
    print(dx, "rid", 3);
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
    print(dx, "youth", 5);
    print(dx, "yousuf", 6);

    dx->printMaxKeyCount(24);
    dx->printNumLevels();
    cout << "Trie size: " << (int) dx->root_data[1] << endl;
    cout << "Filled size: " << (int) dx->root_data[2] << endl;
    return 0;
}

int main3() {
    GenTree::generateBitCounts();
    //linex *dx = new linex();
    dfos *dx = new dfos();
    //dfox *dx = new dfox();
    dx->put("\001\001\001\001", 4, "one", 3);
    dx->put("\001\001\001\x80", 4, "two", 3);
    print(dx, "\001\001\001\001", 4);
    print(dx, "\001\001\001\x80", 4);
    return 0;
}

int main1() {
    GenTree::generateBitCounts();
    //linex *dx = new linex();
    //dfox *dx = new dfox();
    //rb_tree *dx = new rb_tree();
    dfos *dx = new dfos();
    dx->put("95092354", 8, "9059", 4);
    dx->put("81389267", 8, "8318", 4);
    dx->put("10586447", 8, "8501", 4);
    dx->put("24033543", 8, "3042", 4);
    dx->put("22390302", 8, "9322", 4);
    dx->put("07481457", 8, "8470", 4);
    dx->put("38019107", 8, "1083", 4);
    dx->put("14851776", 8, "5841", 4);
    dx->put("93241121", 8, "4239", 4);
    dx->put("67233682", 8, "3276", 4);
    dx->put("62246422", 8, "4226", 4);
    dx->put("96101902", 8, "0169", 4);
    dx->put("59723753", 8, "2795", 4);
    dx->put("12455151", 8, "5421", 4);
    dx->put("07981817", 8, "8970", 4);
    dx->put("28363048", 8, "6382", 4);
    dx->put("79621176", 8, "2697", 4);
    dx->put("78140292", 8, "4187", 4);
    dx->put("14346258", 8, "4341", 4);
    dx->put("01210415", 8, "1210", 4);
    print(dx, "95092354", 8);
    print(dx, "81389267", 8);
    print(dx, "10586447", 8);
    print(dx, "24033543", 8);
    print(dx, "22390302", 8);
    print(dx, "07481457", 8);
    print(dx, "38019107", 8);
    print(dx, "14851776", 8);
    print(dx, "93241121", 8);
    print(dx, "67233682", 8);
    print(dx, "62246422", 8);
    print(dx, "96101902", 8);
    print(dx, "59723753", 8);
    print(dx, "12455151", 8);
    print(dx, "07981817", 8);
    print(dx, "28363048", 8);
    print(dx, "79621176", 8);
    print(dx, "78140292", 8);
    print(dx, "14346258", 8);
    print(dx, "01210415", 8);
//    dx->put("io-+yu4F", 8, "+-oi", 4);
//    dx->put("a6FC-wX ", 8, "CF6a", 4);
//    dx->put("[X<?zzky", 8, "?<X[", 4);
//    dx->put("%%\\#9IV#", 8, "#\\%%", 4);
//    dx->put("6n|+gTJo", 8, "+|n6", 4);
//    dx->put("1GoTOaDK", 8, "ToG1", 4);
//    dx->put("rJKD~`  ", 8, "DKJr", 4);
//    dx->put("*LCzBNpI", 8, "zCL*", 4);
//    dx->put("{EJj,)e7", 8, "jJE{", 4);
//    dx->put("Sxx6TW4^", 8, "6xxS", 4);
//    dx->put("g^1PtTZO", 8, "P1^g", 4);
//    dx->put("ov6%~0O&", 8, "%6vo", 4);
//    dx->put("H\\GkDVN,", 8, "kG\\H", 4);
//    dx->put("(M/5Cg2b", 8, "5/M(", 4);
//    dx->put("N\\57zEFz", 8, "75\\N", 4);
//    dx->put("A$WcZ&oa", 8, "cW$A", 4);
//    dx->put("\"`8EJZL2", 8, "E8`\"", 4);
//    dx->put("! fxE+po", 8, "xf !", 4);
//    dx->put("q:mVk6C*", 8, "Vm:q", 4);
//    dx->put("2SD3w1cv", 8, "3DS2", 4);
//    dx->put("QET75+yI", 8, "7TEQ", 4);
//    dx->put("Gdv<9Oh?", 8, "<vdG", 4);
//    dx->put("QLeR(A'i", 8, "ReLQ", 4);
//    dx->put("$TVC!DlO", 8, "CVT$", 4);
//    dx->put("f}24KR6;", 8, "42}f", 4);
//    dx->put("UqqBdZ^}", 8, "BqqU", 4);
//    dx->put("Q7.$Tn{7", 8, "$.7Q", 4);
//    dx->put("z#qzdxQ5", 8, "zq#z", 4);
//    dx->put("gBDCG9@Q", 8, "CDBg", 4);
//    dx->put("Mo1!#d*U", 8, "!1oM", 4);
//    dx->put("?%A>b>?t", 8, ">A%?", 4);
//    dx->put("[Aa3TAm;", 8, "3aA[", 4);
//    dx->put("(7vtajRF", 8, "tv7(", 4);
//    dx->put("P\"0^O%<e", 8, "^0\"P", 4);
//    dx->put("*dE,6pU/", 8, ",Ed*", 4);
//    dx->put("l(ud#^[T", 8, "du(l", 4);
//    dx->put("exH,]%w|", 8, ",Hxe", 4);
//    dx->put("*#mkj>3/", 8, "km#*", 4);
//    dx->put("L,LsClB'", 8, "sL,L", 4);
//    dx->put("&Dg+T=`w", 8, "+gD&", 4);
//    dx->put("iS~In>?`", 8, "I~Si", 4);
//    dx->put("zlkUW5_Y", 8, "Uklz", 4);
//    dx->put("}U891Mw`", 8, "98U}", 4);
//    dx->put("`i00nMs]", 8, "00i`", 4);
//    dx->put("q`)ini`}", 8, "i)`q", 4);
//    dx->put("(2k`eEaL", 8, "`k2(", 4);
//    dx->put(">C0BYLl^", 8, "B0C>", 4);
//    dx->put(".NR^-s*n", 8, "^RN.", 4);
//    dx->put("FK7PFd_n", 8, "P7KF", 4);
//    dx->put("FLAFZmI,", 8, "FALF", 4);
//    dx->put("\"W0SC-j)", 8, "S0W\"", 4);
//    dx->put(" 'L9V2U)", 8, "9L' ", 4);
//    dx->put("4}J@$0UX", 8, "@J}4", 4);
//    dx->put("YSM0|7kV", 8, "0MSY", 4);
//    dx->put("'MUuhqVa", 8, "uUM'", 4);
//    dx->put(")L`cPMB\"", 8, "c`L)", 4);
//    dx->put("jR;3%QNH", 8, "3;Rj", 4);
//    dx->put("<QpDpt+x", 8, "DpQ<", 4);
//    dx->put("QF?klrk>", 8, "k?FQ", 4);
//    dx->put("T5ua*|M2", 8, "au5T", 4);
//    print(dx, "io-+yu4F", 8);
//    print(dx, "a6FC-wX ", 8);
//    print(dx, "[X<?zzky", 8);
//    print(dx, "%%\\#9IV#", 8);
//    print(dx, "6n|+gTJo", 8);
//    print(dx, "1GoTOaDK", 8);
//    print(dx, "rJKD~`  ", 8);
//    print(dx, "*LCzBNpI", 8);
//    print(dx, "{EJj,)e7", 8);
//    print(dx, "Sxx6TW4^", 8);
//    print(dx, "g^1PtTZO", 8);
//    print(dx, "ov6%~0O&", 8);
//    print(dx, "H\\GkDVN,", 8);
//    print(dx, "(M/5Cg2b", 8);
//    print(dx, "N\\57zEFz", 8);
//    print(dx, "A$WcZ&oa", 8);
//    print(dx, "\"`8EJZL2", 8);
//    print(dx, "! fxE+po", 8);
//    print(dx, "q:mVk6C*", 8);
//    print(dx, "2SD3w1cv", 8);
//    print(dx, "QET75+yI", 8);
//    print(dx, "Gdv<9Oh?", 8);
//    print(dx, "QLeR(A'i", 8);
//    print(dx, "$TVC!DlO", 8);
//    print(dx, "f}24KR6;", 8);
//    print(dx, "UqqBdZ^}", 8);
//    print(dx, "Q7.$Tn{7", 8);
//    print(dx, "z#qzdxQ5", 8);
//    print(dx, "gBDCG9@Q", 8);
//    print(dx, "Mo1!#d*U", 8);
//    print(dx, "?%A>b>?t", 8);
//    print(dx, "[Aa3TAm;", 8);
//    print(dx, "(7vtajRF", 8);
//    print(dx, "P\"0^O%<e", 8);
//    print(dx, "*dE,6pU/", 8);
//    print(dx, "l(ud#^[T", 8);
//    print(dx, "exH,]%w|", 8);
//    print(dx, "*#mkj>3/", 8);
//    print(dx, "L,LsClB'", 8);
//    print(dx, "&Dg+T=`w", 8);
//    print(dx, "iS~In>?`", 8);
//    print(dx, "zlkUW5_Y", 8);
//    print(dx, "}U891Mw`", 8);
//    print(dx, "`i00nMs]", 8);
//    print(dx, "q`)ini`}", 8);
//    print(dx, "(2k`eEaL", 8);
//    print(dx, ">C0BYLl^", 8);
//    print(dx, ".NR^-s*n", 8);
//    print(dx, "FK7PFd_n", 8);
//    print(dx, "FLAFZmI,", 8);
//    print(dx, "\"W0SC-j)", 8);
//    print(dx, " 'L9V2U)", 8);
//    print(dx, "4}J@$0UX", 8);
//    print(dx, "YSM0|7kV", 8);
//    print(dx, "'MUuhqVa", 8);
//    print(dx, ")L`cPMB\"", 8);
//    print(dx, "jR;3%QNH", 8);
//    print(dx, "<QpDpt+x", 8);
//    print(dx, "QF?klrk>", 8);
//    print(dx, "T5ua*|M2", 8);
    dx->printMaxKeyCount(NUM_ENTRIES);
    dx->printNumLevels();
    std::cout << "Size:" << dx->size() << endl;
    std::cout << "Trie Size:" << (int) dx->root_data[1] << endl;
    return 0;
}

int main2(int argc, char *argv[]) {

    if (argc > 1) {
        NUM_ENTRIES = atol(argv[1]);
    }
    if (argc > 2) {
        CHAR_SET = atoi(argv[2]);
    }
    if (argc > 3) {
        KEY_LEN = atoi(argv[3]);
    }

    GenTree::generateBitCounts();

    unordered_map<string, string> m;
    uint32_t start, stop;
    start = getTimeVal();
    insert(m);
    stop = getTimeVal();
    cout << "HashMap insert time:" << timedifference(start, stop) << endl;
    cout << "HashMap size:" << m.size() << endl;
    //getchar();

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
    start = getTimeVal();
    for (; it != m.end(); ++it) {
        m1[it->first];
    }
    stop = getTimeVal();
    cout << "RB Tree get time:" << timedifference(start, stop) << endl;
    cout << "RB Tree size:" << m1.size() << endl;

    unordered_map<string, string>::iterator it1;

    int null_ctr = 0;
    int ctr = 0;
    int cmp = 0;

    art_tree at;
    art_tree_init(&at);
    start = getTimeVal();
    it1 = m.begin();
    for (; it1 != m.end(); ++it1) {
        art_insert(&at, (unsigned char*) it1->first.c_str(),
                it1->first.length(), (void *) it1->second.c_str(),
                it1->second.length());
    }
    stop = getTimeVal();
    cout << "ART Insert Time:" << timedifference(start, stop) << endl;
    //getchar();
    it1 = m.begin();
    start = getTimeVal();
    for (; it1 != m.end(); ++it1) {
        int len;
        char *value = (char *) art_search(&at,
                (unsigned char*) it1->first.c_str(), it1->first.length(), &len);
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
    cout << "ART Get Time:" << timedifference(start, stop) << endl;
    cout << "Null:" << null_ctr << endl;
    cout << "Cmp:" << cmp << endl;
    cout << "ART Size:" << art_size(&at) << endl;
    //getchar();

//    ctr = 0;
//            it = m.begin();
//            for (; it != m.end(); ++it) {
//                cout << "\"" << it->first.c_str() << "\", \"" << it->second.c_str()
//                        << "\"," << endl;
//                if (ctr++ > 90)
//                    break;
//            }

    null_ctr = 0;
    ctr = 0;
    cmp = 0;
    //dfox *dx = new dfox();
    dfos *dx = new dfos();
    //rb_tree *dx = new rb_tree();
    it1 = m.begin();
    start = getTimeVal();
    for (; it1 != m.end(); ++it1) {
        dx->put(it1->first.c_str(), it1->first.length(), it1->second.c_str(),
                it1->second.length());
        ctr++;
    }
    stop = getTimeVal();
    cout << "DFox+Tree insert time:" << timedifference(start, stop) << endl;
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
    std::cout << "Trie Size:" << (int) dx->root_data[1] << endl;
    dx->printMaxKeyCount(NUM_ENTRIES);
    dx->printNumLevels();
    cout << "Root filled size:" << (int) dx->root_data[2] << endl;
    //getchar();

    null_ctr = 0;
    ctr = 0;
    cmp = 0;
    //linex *lx = new linex();
    rb_tree *lx = new rb_tree();
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
