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
#include "rb_tree.h"
#ifdef _MSC_VER
#include <windows.h>
#include <unordered_map>
#else
#include <tr1/unordered_map>
#include <sys/time.h>
#endif

#define NUM_ENTRIES 600000

using namespace std::tr1;
using namespace std;

void insert(unordered_map<string, string>& m) {
    char k[100];
    char v[100];
    srand(time(NULL));
    for (long l = 0; l < NUM_ENTRIES; l++) {

        k[0] = 32 + (rand() % 95);
        k[1] = 32 + (rand() % 95);
        k[2] = 32 + (rand() % 95);
        k[3] = 32 + (rand() % 95);
        k[4] = 32 + (rand() % 95);
        k[5] = 32 + (rand() % 95);
        k[6] = 32 + (rand() % 95);
        k[7] = 32 + (rand() % 95);
        k[8] = 0;

//        k[0] = (rand() % 255);
//        k[1] = (rand() % 255);
//        k[2] = (rand() % 255);
//        k[3] = (rand() % 255);
//        k[4] = (rand() % 255);
//        k[5] = (rand() % 255);
//        k[6] = (rand() % 255);
//        k[7] = (rand() % 255);
//        k[8] = 0;
//        for (int i=0; i<8; i++) {
//            if (k[i] == 0)
//                k[i] = i+1;
//        }

//        long r = rand() * rand();
//        for (int16_t b = 0; b < 4; b++) {
//            char c = (r >> (24 - (3 - b) * 8));
//            k[b * 2] = 48 + (c >> 4);
//            k[b * 2 + 1] = 48 + (c & 0x0F);
//        }
//        r = rand() * rand();
//        for (int16_t b = 4; b < 8; b++) {
//            char c = (r >> (24 - (b - 4) * 8));
//            k[b * 2] = 48 + (c >> 4);
//            k[b * 2 + 1] = 48 + (c & 0x0F);
//        }
//        k[8] = 0;

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
    rb_tree *dx = new rb_tree();
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
    //dfox *dx = new dfox();
    rb_tree *dx = new rb_tree();
    dx->put("] hQzoXO", 8, "Qh ]", 4);
    dx->put("mL5lbn/q", 8, "l5Lm", 4);
    dx->put("G,-K&7LX", 8, "K-,G", 4);
    dx->put("_;kRYmmb", 8, "Rk;_", 4);
    dx->put("O-)c8F3j", 8, "c)-O", 4);
    dx->put("Z-o`\"k8n", 8, "`o-Z", 4);
    dx->put("`@;B+\\{>", 8, "B;@`", 4);
    dx->put("1~%\\]}zn", 8, "\\%~1", 4);
    dx->put("*]qj\" 7U", 8, "jq]*", 4);
    dx->put("!(iNa un", 8, "Ni(!", 4);
    dx->put("}:k=}2|i", 8, "=k:}", 4);
    dx->put("^)sOj\"D&", 8, "Os)^", 4);
    dx->put("1e7NAW6O", 8, "N7e1", 4);
    dx->put("4<&F?#9|", 8, "F&<4", 4);
    dx->put("U1kyp1?5", 8, "yk1U", 4);
    dx->put("d=r/EBPN", 8, "/r=d", 4);
    dx->put("=Mk~ }K1", 8, "~kM=", 4);
    dx->put(">.jjCXwm", 8, "jj.>", 4);
    dx->put("7Rhw+Pa^", 8, "whR7", 4);
    dx->put("|uvNS; 8", 8, "Nvu|", 4);
    dx->put("$HiIY|]0", 8, "IiH$", 4);
    dx->put("]@9I]9Ov", 8, "I9@]", 4);
    dx->put("v/&mBtcE", 8, "m&/v", 4);
    dx->put("+BAP<2ao", 8, "PAB+", 4);
    dx->put("WdcZY0kF", 8, "ZcdW", 4);
    dx->put("@GmVAGV'", 8, "VmG@", 4);
    dx->put("$E8X?LED", 8, "X8E$", 4);
    dx->put("jYP8bg(,", 8, "8PYj", 4);
    dx->put("*@?cpFLT", 8, "c?@*", 4);
    dx->put("+N^B5Er9", 8, "B^N+", 4);
    dx->put("G.sh-1q:", 8, "hs.G", 4);
    dx->put("1=X~!^.T", 8, "~X=1", 4);
    dx->put("GzH#[+v9", 8, "#HzG", 4);
    dx->put("Nod*Z6jL", 8, "*doN", 4);
    dx->put("N&&L OpB", 8, "L&&N", 4);
    dx->put("Rf\"kXW`e", 8, "k\"fR", 4);
    dx->put("cqZ{U<F4", 8, "{Zqc", 4);
    dx->put("!3 f'];/", 8, "f 3!", 4);
    dx->put("&aK[sc0t", 8, "[Ka&", 4);
    dx->put("t<bRh J5", 8, "Rb<t", 4);
    dx->put("bYBFk= &", 8, "FBYb", 4);
    dx->put("\"<f(SD.b", 8, "(f<\"", 4);
    dx->put("nbmy#i0Y", 8, "ymbn", 4);
    dx->put("lxb##)Q`", 8, "#bxl", 4);
    dx->put(">5-1BnfP", 8, "1-5>", 4);
    dx->put("U?R4`C$R", 8, "4R?U", 4);
    dx->put("[;do:jj3", 8, "od;[", 4);
    dx->put("/BKs<eu<", 8, "sKB/", 4);
    dx->put("ld&*%cPW", 8, "*&dl", 4);
    dx->put("E82c#PL@", 8, "c28E", 4);
    dx->put("b$$d=[Cj", 8, "d$$b", 4);
    dx->put("CwbY8`y\"", 8, "YbwC", 4);
    dx->put("c'$~j,o_", 8, "~$'c", 4);
    dx->put("GI;K09kD", 8, "K;IG", 4);
    dx->put("P:~s}xIF", 8, "s~:P", 4);
    dx->put(")W6=H=l?", 8, "=6W)", 4);
    dx->put("lW}q:B5]", 8, "q}Wl", 4);
    dx->put("XSG<Zep7", 8, "<GSX", 4);
    dx->put("m+>)LGxe", 8, ")>+m", 4);
    dx->put("`h39q-e#", 8, "93h`", 4);
    dx->put("?0[\\7nX;", 8, "\\[0?", 4);
    dx->put("$|M{i#K:", 8, "{M|$", 4);
    dx->put("`@21a=pq", 8, "12@`", 4);
    dx->put("?H5NoW7A", 8, "N5H?", 4);
    dx->put("Fta@cy:%", 8, "@atF", 4);
    dx->put("=P8E-1`-", 8, "E8P=", 4);
    dx->put("])l~M(RA", 8, "~l)]", 4);
    dx->put("e%_F@tE=", 8, "F_%e", 4);
    dx->put("m/?HB't'", 8, "H?/m", 4);
    dx->put("! kaCo4)", 8, "ak !", 4);
    dx->put("UYU^;b`I", 8, "^UYU", 4);

    dx->put("frK#U&|*", 8, "#Krf", 4);
    dx->put("Eg\\hA8=o", 8, "h\\gE", 4);
    dx->put("8yj9r>\">", 8, "9jy8", 4);
    dx->put("xcPo>ido", 8, "oPcx", 4);
    dx->put("b:C>E'qt", 8, ">C:b", 4);
    dx->put("HZo^2h^T", 8, "^oZH", 4);
    dx->put("HTXgUobu", 8, "gXTH", 4);
    dx->put("2t y)',X", 8, "y t2", 4);
    dx->put("1`]Kt[K)", 8, "K]`1", 4);
    dx->put("cIRDS21&", 8, "DRIc", 4);
    dx->put("sbD[pV`U", 8, "[Dbs", 4);
    dx->put("5\"bJRrZ#", 8, "Jb\"5", 4);
    dx->put("q.'y&xR8", 8, "y'.q", 4);
    dx->put("9}N=<v>4", 8, "=N}9", 4);
    dx->put("nk3.P}&Q", 8, ".3kn", 4);
    dx->put("9_HCp8]`", 8, "CH_9", 4);
    dx->put("h:>JqBrL", 8, "J>:h", 4);
    dx->put("sf{^CY^W", 8, "^{fs", 4);
    dx->put("p^GlAdd&", 8, "lG^p", 4);
    dx->put("LXQ]W~f?", 8, "]QXL", 4);
    dx->put("e8FaJ@Kx", 8, "aF8e", 4);
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
//                if (ctr++ > 90)
//                    break;
//            }

    null_ctr = 0;
    ctr = 0;
    cmp = 0;
    rb_tree *dx = new rb_tree();
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
    dx->root->depth = 0;
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
    cout << "Root filled size:" << (int) dx->root_data[MAX_PTR_BITMAP_BYTES+1] << endl;
    cout << "Max depth:" << (int) dx->root->depth<< endl;
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
