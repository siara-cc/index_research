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
//#include "dfos.h"
#include "dfox.h"
#include "rb_tree.h"
#ifdef _MSC_VER
#include <windows.h>
#include <unordered_map>
#else
#include <tr1/unordered_map>
#include <sys/time.h>
#include <stx/btree_map.h>
#endif

using namespace std::tr1;
using namespace std;

#define CS_PRINTABLE 1
#define CS_ALPHA_ONLY 2
#define CS_NUMBER_ONLY 3
#define CS_ONE_PER_OCTET 4
#define CS_255_RANDOM 5
#define CS_255_DENSE 6

long NUM_ENTRIES = 2000;
int CHAR_SET = 2;
int KEY_LEN = 8;

void insert(unordered_map<string, string>& m) {
    char k[100];
    char v[100];
    srand(time(NULL));
    for (long l = 0; l < NUM_ENTRIES; l++) {

        if (CHAR_SET == CS_PRINTABLE) {
            for (int i=0; i<KEY_LEN; i++)
                k[i] = 32 + (rand() % 95);
            k[KEY_LEN] = 0;
        } else if (CHAR_SET == CS_ALPHA_ONLY) {
            for (int i=0; i<KEY_LEN; i++)
                k[i] = 97 + (rand() % 26);
            k[KEY_LEN] = 0;
        } else if (CHAR_SET == CS_NUMBER_ONLY) {
            for (int i=0; i<KEY_LEN; i++)
                k[i] = 48 + (rand() % 10);
            k[KEY_LEN] = 0;
        } else if (CHAR_SET == CS_ONE_PER_OCTET) {
            for (int i=0; i<KEY_LEN; i++)
                k[i] = ((rand() % 32) << 3) | 0x07;
            k[KEY_LEN] = 0;
        } else if (CHAR_SET == CS_255_RANDOM) {
            for (int i=0; i<KEY_LEN; i++)
                k[i] = (rand() % 255);
            k[KEY_LEN] = 0;
            for (int i = 0; i < KEY_LEN; i++) {
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

//void print(dfos *dx, const char *key, int16_t key_len) {
//    int16_t len;
//    char *value = dx->get(key, key_len, &len);
//    if (value == null || len == 0) {
//        std::cout << "Value for " << key << " is null" << endl;
//        return;
//    }
//    char s[100];
//    strncpy(s, value, len);
//    s[len] = 0;
//    std::cout << "Key: " << key << ", Value:" << s << endl;
//}

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
    //rb_tree *dx = new rb_tree();
    //dfos *dx = new dfos();
    dfox *dx = new dfox();
    //linex *dx = new linex();

    dx->put("Hello", 5, "World", 5);
    dx->put("Nice", 4, "Place", 5);
    dx->put("Arun", 4, "Hello", 5);
    dx->put("arun", 4, "dale", 4);
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

    dx->put("additional", 10, "check", 5);
    dx->put("absorb", 6, "carelessly", 10);
    dx->put("add", 3, "chairman", 8);
    dx->put("accuse", 6, "cat", 3);
    dx->put("act", 3, "easy", 4);
    dx->put("adopt", 5, "charity", 7);
    dx->put("agreement", 9, "camping", 7);
    dx->put("actively", 8, "called", 6);
    dx->put("aircraft", 8, "car", 3);
    dx->put("agree", 5, "calmly", 6);
    dx->put("adequate", 8, "else", 4);
    dx->put("afraid", 6, "easily", 6);
    dx->put("accident", 8, "cheaply", 7);
    dx->put("activity", 8, "card", 4);
    dx->put("abroad", 6, "element", 7);
    dx->put("acquire", 7, "calm", 4);
    dx->put("aim", 3, "chair", 5);
    dx->put("absence", 7, "centre", 6);
    dx->put("afterwards", 10, "careful", 7);
    dx->put("advice", 6, "cap", 3);
    dx->put("advertise", 9, "captain", 7);
    dx->put("absolute", 8, "calculation", 11);
    dx->put("acid", 4, "century", 7);
    dx->put("address", 7, "chance", 6);
    dx->put("advertisement", 13, "chamber", 7);
    dx->put("advanced", 8, "carrot", 6);
    dx->put("absolutely", 10, "capital", 7);
    dx->put("admire", 6, "careless", 8);
    dx->put("agency", 6, "certificate", 11);
    dx->put("aggressive", 10, "elevator", 8);
    dx->put("affair", 6, "eastern", 7);

    dx->put("access", 6, "cease", 5);
    dx->put("afternoon", 9, "east", 4);
    dx->put("afford", 6, "cannot", 6);
    dx->put("acceptable", 10, "cake", 4);
    dx->put("ability", 7, "calculate", 9);
    dx->put("again", 5, "cardboard", 9);
    dx->put("advertising", 11, "ceremony", 8);
    dx->put("accidental", 10, "eleven", 6);
    dx->put("action", 6, "ease", 4);
    dx->put("academic", 8, "carefully", 9);
    dx->put("advert", 6, "elegant", 7);
    dx->put("account", 7, "earth", 5);
    dx->put("above", 5, "certainly", 9);
    dx->put("accompany", 9, "case", 4);
    dx->put("adapt", 5, "castle", 6);
    dx->put("ago", 3, "earn", 4);
    dx->put("actually", 8, "cancel", 6);
    dx->put("alarmed", 7, "celebrate", 9);
    dx->put("abuse", 5, "cast", 4);
    dx->put("accurately", 10, "chain", 5);
    dx->put("accurate", 8, "charge", 6);
    dx->put("a", 1, "carpet", 6);
    dx->put("ahead", 5, "cause", 5);
    dx->put("advantage", 9, "capacity", 8);
    dx->put("accommodation", 13, "camp", 4);
    dx->put("actual", 6, "ceiling", 7);
    dx->put("acknowledge", 11, "cancer", 6);
    dx->put("actress", 7, "chart", 5);
    dx->put("abandoned", 9, "character", 9);
    dx->put("able", 4, "cash", 4);
    dx->put("absent", 6, "channel", 7);
    dx->put("airport", 7, "carry", 5);

    dx->put("against", 7, "catch", 5);
    dx->put("across", 6, "cell", 4);
    dx->put("aid", 3, "central", 7);
    dx->put("advance", 7, "eleventh", 8);
    dx->put("accept", 6, "career", 6);
    dx->put("adult", 5, "capable", 7);
    dx->put("ad", 2, "care", 4);
    dx->put("aged", 4, "candidate", 9);
    dx->put("alarm", 5, "CD", 2);
    dx->put("admiration", 10, "campaign", 8);
    dx->put("affect", 6, "cent", 4);
    dx->put("adjust", 6, "capture", 7);
    dx->put("accidentally", 12, "change", 6);
    dx->put("about", 5, "cheat", 5);
    dx->put("actor", 5, "celebration", 11);
    dx->put("after", 5, "electronic", 10);
    dx->put("air", 3, "chase", 5);
    dx->put("accent", 6, "cable", 5);
    dx->put("agent", 5, "camera", 6);
    dx->put("admit", 5, "certain", 7);
    dx->put("adequately", 10, "cheap", 5);
    dx->put("abandon", 7, "category", 8);
    dx->put("addition", 8, "chapter", 7);
    dx->put("affection", 9, "electricity", 11);
    dx->put("achieve", 7, "early", 5);
    dx->put("adventure", 9, "candy", 5);
    dx->put("active", 6, "centimetre", 10);
    dx->put("advise", 6, "challenge", 9);
    dx->put("age", 3, "call", 4);
    dx->put("achievement", 11, "chat", 4);

    print(dx, "additional", 10);
    print(dx, "absorb", 6);
    print(dx, "add", 3);
    print(dx, "accuse", 6);
    print(dx, "act", 3);
    print(dx, "adopt", 5);
    print(dx, "agreement", 9);
    print(dx, "actively", 8);
    print(dx, "aircraft", 8);
    print(dx, "agree", 5);
    print(dx, "adequate", 8);
    print(dx, "afraid", 6);
    print(dx, "accident", 8);
    print(dx, "activity", 8);
    print(dx, "abroad", 6);
    print(dx, "acquire", 7);
    print(dx, "aim", 3);
    print(dx, "absence", 7);
    print(dx, "afterwards", 10);
    print(dx, "advice", 6);
    print(dx, "advertise", 9);
    print(dx, "absolute", 8);
    print(dx, "acid", 4);
    print(dx, "address", 7);
    print(dx, "advertisement", 13);
    print(dx, "advanced", 8);
    print(dx, "absolutely", 10);
    print(dx, "admire", 6);
    print(dx, "agency", 6);
    print(dx, "aggressive", 10);
    print(dx, "affair", 6);

    print(dx, "access", 6);
    print(dx, "afternoon", 9);
    print(dx, "afford", 6);
    print(dx, "acceptable", 10);
    print(dx, "ability", 7);
    print(dx, "again", 5);
    print(dx, "advertising", 11);
    print(dx, "accidental", 10);
    print(dx, "action", 6);
    print(dx, "academic", 8);
    print(dx, "advert", 6);
    print(dx, "account", 7);
    print(dx, "above", 5);
    print(dx, "accompany", 9);
    print(dx, "adapt", 5);
    print(dx, "ago", 3);
    print(dx, "actually", 8);
    print(dx, "alarmed", 7);
    print(dx, "abuse", 5);
    print(dx, "accurately", 10);
    print(dx, "accurate", 8);
    print(dx, "a", 1);
    print(dx, "ahead", 5);
    print(dx, "advantage", 9);
    print(dx, "accommodation", 13);
    print(dx, "actual", 6);
    print(dx, "acknowledge", 11);
    print(dx, "actress", 7);
    print(dx, "abandoned", 9);
    print(dx, "able", 4);
    print(dx, "absent", 6);
    print(dx, "airport", 7);

    print(dx, "against", 7);
    print(dx, "across", 6);
    print(dx, "aid", 3);
    print(dx, "advance", 7);
    print(dx, "accept", 6);
    print(dx, "adult", 5);
    print(dx, "ad", 2);
    print(dx, "aged", 4);
    print(dx, "alarm", 5);
    print(dx, "admiration", 10);
    print(dx, "affect", 6);
    print(dx, "adjust", 6);
    print(dx, "accidentally", 12);
    print(dx, "about", 5);
    print(dx, "actor", 5);
    print(dx, "after", 5);
    print(dx, "air", 3);
    print(dx, "accent", 6);
    print(dx, "agent", 5);
    print(dx, "admit", 5);
    print(dx, "adequately", 10);
    print(dx, "abandon", 7);
    print(dx, "addition", 8);
    print(dx, "affection", 9);
    print(dx, "achieve", 7);
    print(dx, "adventure", 9);
    print(dx, "active", 6);
    print(dx, "advise", 6);
    print(dx, "age", 3);
    print(dx, "achievement", 11);

    dx->printMaxKeyCount(24);
    dx->printNumLevels();
    cout << "Trie size: " << (int) dx->root_data[1] << endl;
    cout << "Filled size: " << (int) dx->root_data[2] << endl;
    return 0;
}

int main3() {
    int numKeys = 10;
    GenTree::generateBitCounts();
    //linex *dx = new linex();
    //dfos *dx = new dfos();
    dfox *dx = new dfox();
    char k[5] = "\001\001\001\000";
    for (int i = 1; i<numKeys; i++) {
        char v[5];
        k[3] = i;
        sprintf(v, "%d", i);
        dx->put(k, 4, v, strlen(v));
    }
    for (int i = 1; i<numKeys; i++) {
        char v[5];
        k[3] = i;
        sprintf(v, "%d", i);
        print(dx, (char *) k, 4);
    }
    return 0;
}

int main1() {
    GenTree::generateBitCounts();
    //linex *dx = new linex();
    dfox *dx = new dfox();
    //rb_tree *dx = new rb_tree();
    //dfos *dx = new dfos();

    dx->put("fqofzfvv", 8, "foqf", 4);
    dx->put("bqvqjhrx", 8, "qvqb", 4);
    dx->put("ywuowscl", 8, "ouwy", 4);
    dx->put("iobvvzvo", 8, "vboi", 4);
    dx->put("ituaqsrm", 8, "auti", 4);
    dx->put("qrhyxpdc", 8, "yhrq", 4);
    dx->put("oocfzpvx", 8, "fcoo", 4);
    dx->put("cxgvhsns", 8, "vgxc", 4);
    dx->put("saixfyep", 8, "xias", 4);
    dx->put("fphhhjce", 8, "hhpf", 4);
    dx->put("cpiyzscc", 8, "yipc", 4);
    dx->put("vnhzbiys", 8, "zhnv", 4);
    dx->put("fubombkf", 8, "obuf", 4);
    dx->put("lsaubwra", 8, "uasl", 4);
    dx->put("zehwjrmp", 8, "whez", 4);
    dx->put("qimmqtrr", 8, "mmiq", 4);
    dx->put("ptnkefll", 8, "kntp", 4);
    dx->put("pzjijtpo", 8, "ijzp", 4);
    dx->put("piuubwas", 8, "uuip", 4);
    dx->put("mypevmqx", 8, "epym", 4);
    dx->put("mztxyzqs", 8, "xtzm", 4);
    dx->put("aiksulmf", 8, "skia", 4);
    dx->put("xazjiyzz", 8, "jzax", 4);
    dx->put("wgcevaja", 8, "ecgw", 4);
    dx->put("fttoxdre", 8, "ottf", 4);
    dx->put("fmmrsbha", 8, "rmmf", 4);
    dx->put("hsiyxdok", 8, "yish", 4);
    dx->put("pdwnjlkd", 8, "nwdp", 4);
    dx->put("zowzhhmp", 8, "zwoz", 4);
    dx->put("rzielafw", 8, "eizr", 4);
    dx->put("duyucjmb", 8, "uyud", 4);
    dx->put("gdninjib", 8, "indg", 4);
    dx->put("cesmxsig", 8, "msec", 4);
    dx->put("nvcigkcd", 8, "icvn", 4);
    dx->put("qkktpdhx", 8, "tkkq", 4);
    dx->put("mgggaadz", 8, "gggm", 4);
    dx->put("mflzmzwg", 8, "zlfm", 4);
    dx->put("chulxyvw", 8, "luhc", 4);
    dx->put("xrhjcfzg", 8, "jhrx", 4);
    dx->put("iykkcwuq", 8, "kkyi", 4);
    dx->put("ljrwtddu", 8, "wrjl", 4);
    dx->put("yxkvkkog", 8, "vkxy", 4);
    dx->put("yfaqlwne", 8, "qafy", 4);
    dx->put("kylcwzwz", 8, "clyk", 4);
    dx->put("zqspcvuc", 8, "psqz", 4);
    dx->put("fhqvvfie", 8, "vqhf", 4);
    dx->put("qggeubrp", 8, "eggq", 4);
    dx->put("lqvgxzrs", 8, "gvql", 4);
    dx->put("hutjgvdz", 8, "jtuh", 4);
    dx->put("ljwhzsbo", 8, "hwjl", 4);
    dx->put("ahechctg", 8, "ceha", 4);
    dx->put("wihjwrjh", 8, "jhiw", 4);
    dx->put("qlfviaaf", 8, "vflq", 4);
    dx->put("nyzfqozb", 8, "fzyn", 4);
    dx->put("eyyjmhgw", 8, "jyye", 4);
    dx->put("uunnfahw", 8, "nnuu", 4);
    dx->put("gauvbcwp", 8, "vuag", 4);
    dx->put("lfkovvzf", 8, "okfl", 4);
    dx->put("lvuqgclo", 8, "quvl", 4);
    dx->put("ktisrkdu", 8, "sitk", 4);
    dx->put("utafptfb", 8, "fatu", 4);
    dx->put("jppcklbj", 8, "cppj", 4);
    dx->put("vdqwkoex", 8, "wqdv", 4);
    dx->put("wxzrqdih", 8, "rzxw", 4);
    dx->put("phkbcogd", 8, "bkhp", 4);
    dx->put("gqeignuv", 8, "ieqg", 4);
    dx->put("zzotsjnt", 8, "tozz", 4);
    dx->put("lhelmwus", 8, "lehl", 4);
    dx->put("tkdjwobq", 8, "jdkt", 4);
    dx->put("qxpjsvvt", 8, "jpxq", 4);
    dx->put("jwqhrxku", 8, "hqwj", 4);
    dx->put("xpqehtmv", 8, "eqpx", 4);
    dx->put("zbhfrxuy", 8, "fhbz", 4);
    dx->put("ulvhzezy", 8, "hvlu", 4);
    dx->put("flmqoilv", 8, "qmlf", 4);
    dx->put("olehccwh", 8, "helo", 4);
    dx->put("brdclhsz", 8, "cdrb", 4);
    dx->put("xlmhcbmt", 8, "hmlx", 4);
    dx->put("bkldesoe", 8, "dlkb", 4);
    dx->put("qvutmtxr", 8, "tuvq", 4);
    dx->put("loalanrq", 8, "laol", 4);
    dx->put("xxtgcftx", 8, "gtxx", 4);
    dx->put("dlkvpbij", 8, "vkld", 4);
    dx->put("tmejvjcw", 8, "jemt", 4);
    dx->put("yflqgvuv", 8, "qlfy", 4);
    dx->put("yqctalxj", 8, "tcqy", 4);
    dx->put("wmanhizg", 8, "namw", 4);
    dx->put("tjqmqnok", 8, "mqjt", 4);
    dx->put("qmjbrmqf", 8, "bjmq", 4);
    dx->put("ifmfmzoh", 8, "fmfi", 4);
    dx->put("wjdazhdg", 8, "adjw", 4);
    dx->put("psxeficy", 8, "exsp", 4);
    dx->put("jtemuteu", 8, "metj", 4);
    dx->put("diwhegps", 8, "hwid", 4);
    dx->put("elnjefht", 8, "jnle", 4);
    dx->put("urhrvjhf", 8, "rhru", 4);
    dx->put("eooecchf", 8, "eooe", 4);
    dx->put("ynumssbv", 8, "muny", 4);
    dx->put("pndhkmaw", 8, "hdnp", 4);
    dx->put("wucnblkx", 8, "ncuw", 4);
    dx->put("slcppeur", 8, "pcls", 4);
    dx->put("vqjalesw", 8, "ajqv", 4);
    dx->put("uaxudeai", 8, "uxau", 4);
    dx->put("xzvxtjfa", 8, "xvzx", 4);
    dx->put("oraletck", 8, "laro", 4);
    dx->put("tkazqocx", 8, "zakt", 4);
    dx->put("tdtwqkec", 8, "wtdt", 4);
    dx->put("ksjphgwz", 8, "pjsk", 4);
    dx->put("kyegvwsn", 8, "geyk", 4);
    dx->put("rvyxjiyi", 8, "xyvr", 4);
    dx->put("aynbipff", 8, "bnya", 4);
    dx->put("xiuelqos", 8, "euix", 4);
    dx->put("fthlgkev", 8, "lhtf", 4);
    dx->put("diglremo", 8, "lgid", 4);
    dx->put("lppjjcgu", 8, "jppl", 4);
    dx->put("iphoourq", 8, "ohpi", 4);
    dx->put("ririipjb", 8, "irir", 4);
    dx->put("ascpkert", 8, "pcsa", 4);
    dx->put("tmvlxuqj", 8, "lvmt", 4);
    dx->put("otzvdllc", 8, "vzto", 4);
    dx->put("ebdnsjzh", 8, "ndbe", 4);
    dx->put("emtaizti", 8, "atme", 4);
    dx->put("flntucdz", 8, "tnlf", 4);
    dx->put("oeldskky", 8, "dleo", 4);
    dx->put("wsorafgb", 8, "rosw", 4);
    dx->put("pbexxeyr", 8, "xebp", 4);
    dx->put("gyiomytz", 8, "oiyg", 4);
    dx->put("chxbynvm", 8, "bxhc", 4);
    dx->put("esawdgmc", 8, "wase", 4);
    dx->put("gtwyfnfv", 8, "ywtg", 4);
    dx->put("vreizhhh", 8, "ierv", 4);
    dx->put("gunqltii", 8, "qnug", 4);
    dx->put("ntqlxxaz", 8, "lqtn", 4);
    dx->put("waiqoijh", 8, "qiaw", 4);
    dx->put("izgrxqnp", 8, "rgzi", 4);
    dx->put("qdbermxo", 8, "ebdq", 4);
    dx->put("bwbddebq", 8, "dbwb", 4);
    dx->put("unmfmvci", 8, "fmnu", 4);
    dx->put("afpbxaqa", 8, "bpfa", 4);
    dx->put("crfhnzos", 8, "hfrc", 4);
    dx->put("sdjwkxfr", 8, "wjds", 4);
    dx->put("flnwnpav", 8, "wnlf", 4);
    dx->put("ylarqotl", 8, "raly", 4);
    dx->put("folqjneu", 8, "qlof", 4);
    dx->put("pdathrhz", 8, "tadp", 4);
    dx->put("selayibk", 8, "ales", 4);
    dx->put("gjyszmio", 8, "syjg", 4);
    dx->put("tisduryh", 8, "dsit", 4);
    dx->put("jqoqiqqw", 8, "qoqj", 4);
    dx->put("ynyfouge", 8, "fyny", 4);
    dx->put("hfodrqfz", 8, "dofh", 4);
    dx->put("ejswunvf", 8, "wsje", 4);
    dx->put("mvkqwfri", 8, "qkvm", 4);
    dx->put("ntsvlizy", 8, "vstn", 4);
    dx->put("ydjhxlxa", 8, "hjdy", 4);
    dx->put("syxziped", 8, "zxys", 4);
    dx->put("kkfcrvcw", 8, "cfkk", 4);
    dx->put("djadhylc", 8, "dajd", 4);
    dx->put("ancgxjhf", 8, "gcna", 4);
    dx->put("meplxhtq", 8, "lpem", 4);
    dx->put("sacnccge", 8, "ncas", 4);
    dx->put("npebssuw", 8, "bepn", 4);
    dx->put("yvbzycpf", 8, "zbvy", 4);
    dx->put("xcrkwpeg", 8, "krcx", 4);
    dx->put("afpyidab", 8, "ypfa", 4);
    dx->put("yaiyugmu", 8, "yiay", 4);
    dx->put("vnogbxgj", 8, "gonv", 4);
    dx->put("hwtkyvos", 8, "ktwh", 4);
    dx->put("gdngtxqc", 8, "gndg", 4);
    dx->put("qazowrlm", 8, "ozaq", 4);
    dx->put("gpngpdum", 8, "gnpg", 4);
    dx->put("ksniygcl", 8, "insk", 4);
    dx->put("nsmoqtgl", 8, "omsn", 4);
    dx->put("dibaogqu", 8, "abid", 4);
    dx->put("ojvuhssc", 8, "uvjo", 4);
    dx->put("qwvpzngw", 8, "pvwq", 4);
    dx->put("mfjnwyda", 8, "njfm", 4);
    dx->put("jyfohqzi", 8, "ofyj", 4);
    dx->put("vraaynbu", 8, "aarv", 4);
    dx->put("eksainee", 8, "aske", 4);
    dx->put("nywrqybt", 8, "rwyn", 4);
    dx->put("hayelbeu", 8, "eyah", 4);
    dx->put("ilhkjysg", 8, "khli", 4);
    dx->put("ukubisil", 8, "buku", 4);
    dx->put("xgqgxdzc", 8, "gqgx", 4);
    dx->put("apiwfpcd", 8, "wipa", 4);
    dx->put("ysqgcvtj", 8, "gqsy", 4);
    dx->put("finljuov", 8, "lnif", 4);
    dx->put("saxxbphr", 8, "xxas", 4);
    dx->put("ivgoiemf", 8, "ogvi", 4);
    dx->put("apcoqpta", 8, "ocpa", 4);
    dx->put("wuhlqwyt", 8, "lhuw", 4);
    dx->put("lnixuhjx", 8, "xinl", 4);
    dx->put("zxyzqogc", 8, "zyxz", 4);
    dx->put("ttcgjwkb", 8, "gctt", 4);
    dx->put("lqmgkdbq", 8, "gmql", 4);
    dx->put("vheqhcze", 8, "qehv", 4);
    dx->put("bifwihgc", 8, "wfib", 4);
    dx->put("jdsopgrc", 8, "osdj", 4);
    dx->put("kcfefuzt", 8, "efck", 4);
    dx->put("gnkexqow", 8, "ekng", 4);
    dx->put("wqfocgqd", 8, "ofqw", 4);
    dx->put("lbgkgebv", 8, "kgbl", 4);
    dx->put("hfnsbdsv", 8, "snfh", 4);
    dx->put("bqewlmnv", 8, "weqb", 4);
    dx->put("dcmqxfui", 8, "qmcd", 4);
    dx->put("isumbzmy", 8, "musi", 4);
    dx->put("tcosncvl", 8, "soct", 4);
    dx->put("ccwxvspa", 8, "xwcc", 4);
    dx->put("cngwquod", 8, "wgnc", 4);
    dx->put("rlvshczu", 8, "svlr", 4);
    dx->put("jjicldqo", 8, "cijj", 4);
    dx->put("kznvvnfw", 8, "vnzk", 4);
    dx->put("nvgagzqn", 8, "agvn", 4);
    dx->put("gvmpyott", 8, "pmvg", 4);
    dx->put("uejiphnw", 8, "ijeu", 4);
    dx->put("uohxnlmr", 8, "xhou", 4);
    dx->put("windxuol", 8, "dniw", 4);
    dx->put("bdlcshjb", 8, "cldb", 4);
    dx->put("msxvvlkh", 8, "vxsm", 4);
    dx->put("jvqsqocx", 8, "sqvj", 4);
    dx->put("icgyizvu", 8, "ygci", 4);
    dx->put("qezjxcac", 8, "jzeq", 4);
    dx->put("pixjvzjj", 8, "jxip", 4);
    dx->put("nuaszsck", 8, "saun", 4);
    dx->put("ghrcxmio", 8, "crhg", 4);
    dx->put("ovtbazgy", 8, "btvo", 4);
    dx->put("abmopltf", 8, "omba", 4);
    dx->put("kddvfcjs", 8, "vddk", 4);
    dx->put("gsovsbwo", 8, "vosg", 4);
    dx->put("mzcracag", 8, "rczm", 4);
    dx->put("iqchmzty", 8, "hcqi", 4);
    dx->put("xabmzqiu", 8, "mbax", 4);
    dx->put("gudhujdv", 8, "hdug", 4);
    dx->put("kjwuamqn", 8, "uwjk", 4);
    dx->put("hlonayae", 8, "nolh", 4);
    dx->put("xzwasajg", 8, "awzx", 4);
    dx->put("slpgdqkl", 8, "gpls", 4);
    dx->put("nbxxbfhz", 8, "xxbn", 4);
    dx->put("buzzlyrr", 8, "zzub", 4);
    dx->put("xxbpzhnk", 8, "pbxx", 4);
    dx->put("kiemmzom", 8, "meik", 4);
    dx->put("mrbmsksu", 8, "mbrm", 4);
    dx->put("apyydfzf", 8, "yypa", 4);
    dx->put("mxxluhia", 8, "lxxm", 4);
    dx->put("uspzjjkx", 8, "zpsu", 4);
    dx->put("iykpnamr", 8, "pkyi", 4);
    dx->put("vmxwtegw", 8, "wxmv", 4);
    dx->put("mekiicsa", 8, "ikem", 4);
    dx->put("qsnjeohs", 8, "jnsq", 4);
    dx->put("mlzhzymc", 8, "hzlm", 4);
    dx->put("pqlkdiwl", 8, "klqp", 4);
    dx->put("zxkhpfbc", 8, "hkxz", 4);
    dx->put("abatvmkp", 8, "taba", 4);
    dx->put("weiahthe", 8, "aiew", 4);
    dx->put("znjawttn", 8, "ajnz", 4);
    dx->put("dpvrsjew", 8, "rvpd", 4);
    dx->put("tzlthhmi", 8, "tlzt", 4);
    dx->put("fxsysves", 8, "ysxf", 4);
    dx->put("eyyuprqy", 8, "uyye", 4);
    dx->put("prgdvugy", 8, "dgrp", 4);
    dx->put("gmluspto", 8, "ulmg", 4);
    dx->put("fsticqdz", 8, "itsf", 4);
    dx->put("pwbnutzi", 8, "nbwp", 4);
    dx->put("ffpomyrw", 8, "opff", 4);
    dx->put("ftlqnnrv", 8, "qltf", 4);
    dx->put("joegklrq", 8, "geoj", 4);
    dx->put("cyvbhvta", 8, "bvyc", 4);
    dx->put("vvcoowix", 8, "ocvv", 4);
    dx->put("jnjtkvfd", 8, "tjnj", 4);
    dx->put("lvaxykrm", 8, "xavl", 4);
    dx->put("bjqmzagu", 8, "mqjb", 4);
    dx->put("nikbxikj", 8, "bkin", 4);
    dx->put("poftfeaw", 8, "tfop", 4);
    dx->put("haxmqrqc", 8, "mxah", 4);
    dx->put("kntbchyq", 8, "btnk", 4);
    dx->put("tnxlmxbb", 8, "lxnt", 4);
    dx->put("ypeuaoys", 8, "uepy", 4);
    dx->put("hwxqcsra", 8, "qxwh", 4);
    dx->put("cjujykin", 8, "jujc", 4);
    dx->put("qjfpheuh", 8, "pfjq", 4);
    dx->put("uxsdbmuw", 8, "dsxu", 4);
    dx->put("nxwklpdp", 8, "kwxn", 4);
    dx->put("tstothem", 8, "otst", 4);
    dx->put("vxqrfzzy", 8, "rqxv", 4);
    dx->put("ntentxvw", 8, "netn", 4);
    dx->put("gkoounyg", 8, "ookg", 4);
    dx->put("srxruxck", 8, "rxrs", 4);
    dx->put("fjvkmkzy", 8, "kvjf", 4);
    dx->put("ujdjsdha", 8, "jdju", 4);
    dx->put("otbukfna", 8, "ubto", 4);
    dx->put("bdwobndq", 8, "owdb", 4);
    dx->put("fqxgedit", 8, "gxqf", 4);
    dx->put("imseeede", 8, "esmi", 4);
    dx->put("frrsifey", 8, "srrf", 4);
    dx->put("garjnxgl", 8, "jrag", 4);
    dx->put("ixyudwou", 8, "uyxi", 4);
    dx->put("rtkvaitn", 8, "vktr", 4);
    dx->put("dglwdcjl", 8, "wlgd", 4);
    dx->put("tettytyk", 8, "ttet", 4);
    dx->put("jgrbactf", 8, "brgj", 4);
    dx->put("lrkvwuub", 8, "vkrl", 4);
    dx->put("tokewdxa", 8, "ekot", 4);
    dx->put("kvfrzmvz", 8, "rfvk", 4);
    dx->put("ahesruzy", 8, "seha", 4);
    dx->put("mkygwgax", 8, "gykm", 4);
    dx->put("cotrgece", 8, "rtoc", 4);
    dx->put("apiclmka", 8, "cipa", 4);
    dx->put("fkzkvdon", 8, "kzkf", 4);
    dx->put("fnwubwwo", 8, "uwnf", 4);
    dx->put("uflyvkvz", 8, "ylfu", 4);
    dx->put("qwbfotvu", 8, "fbwq", 4);
    dx->put("albxcrwr", 8, "xbla", 4);
    dx->put("mhesddok", 8, "sehm", 4);
    dx->put("wwwylexx", 8, "ywww", 4);
    dx->put("chytaxin", 8, "tyhc", 4);
    dx->put("hmuuefps", 8, "uumh", 4);
    dx->put("bcdhsqrn", 8, "hdcb", 4);
    dx->put("mkjukkrx", 8, "ujkm", 4);
    dx->put("xijkaqdx", 8, "kjix", 4);
    dx->put("nejotxmv", 8, "ojen", 4);
    dx->put("dekfojbp", 8, "fked", 4);
    dx->put("cpqzwdhu", 8, "zqpc", 4);
    dx->put("dsmoegfj", 8, "omsd", 4);
    dx->put("wbkkhixg", 8, "kkbw", 4);
    dx->put("ydqcaous", 8, "cqdy", 4);
    dx->put("nskfzmul", 8, "fksn", 4);
    dx->put("ncltcufy", 8, "tlcn", 4);
    dx->put("cidexsjp", 8, "edic", 4);
    dx->put("otnjhfts", 8, "jnto", 4);
    dx->put("zrbwtcwt", 8, "wbrz", 4);
    dx->put("smgqnyqw", 8, "qgms", 4);
    dx->put("figjidza", 8, "jgif", 4);
    dx->put("rdibjxog", 8, "bidr", 4);
    dx->put("nucjxgdn", 8, "jcun", 4);
    dx->put("syvtsfmm", 8, "tvys", 4);
    dx->put("ddqbaees", 8, "bqdd", 4);
    dx->put("pdavyqno", 8, "vadp", 4);
    dx->put("xcwnokjx", 8, "nwcx", 4);
    dx->put("izgdvtjk", 8, "dgzi", 4);
    dx->put("oapmtuta", 8, "mpao", 4);
    dx->put("qmglgoau", 8, "lgmq", 4);
    dx->put("ffnvxyzc", 8, "vnff", 4);
    dx->put("vzjkagyp", 8, "kjzv", 4);
    dx->put("mmkkxukj", 8, "kkmm", 4);
    dx->put("gnvateuh", 8, "avng", 4);
    dx->put("owctrnsu", 8, "tcwo", 4);
    dx->put("fbemkfue", 8, "mebf", 4);
    dx->put("zvutqbsc", 8, "tuvz", 4);
    dx->put("yzvktize", 8, "kvzy", 4);
    dx->put("yyxqattk", 8, "qxyy", 4);
    dx->put("cdwfkdfa", 8, "fwdc", 4);
    dx->put("tcbmepyf", 8, "mbct", 4);
    dx->put("syztntwa", 8, "tzys", 4);
    dx->put("qikvxdjp", 8, "vkiq", 4);
    dx->put("nrdquuoq", 8, "qdrn", 4);
    dx->put("fqiilygj", 8, "iiqf", 4);
    dx->put("jabhejwd", 8, "hbaj", 4);
    dx->put("xfrayaxz", 8, "arfx", 4);
    dx->put("imvbksfp", 8, "bvmi", 4);
    dx->put("xnwzewgo", 8, "zwnx", 4);
    dx->put("twoitaic", 8, "iowt", 4);
    dx->put("nwndynwf", 8, "dnwn", 4);
    dx->put("snxqtoew", 8, "qxns", 4);
    dx->put("xelftpjr", 8, "flex", 4);
    dx->put("lmqjbodz", 8, "jqml", 4);
    dx->put("xadzysgg", 8, "zdax", 4);
    dx->put("xsumoebx", 8, "musx", 4);
    dx->put("uulldjst", 8, "lluu", 4);
    dx->put("qbajtwri", 8, "jabq", 4);
    dx->put("olnujwns", 8, "unlo", 4);
    dx->put("ejkjkkqj", 8, "jkje", 4);
    dx->put("pujvxrih", 8, "vjup", 4);
    dx->put("rsvereza", 8, "evsr", 4);
    dx->put("alspxafg", 8, "psla", 4);
    dx->put("svnrqwmv", 8, "rnvs", 4);
    dx->put("nsmowsnb", 8, "omsn", 4);
    dx->put("xmnjfxts", 8, "jnmx", 4);
    dx->put("vrjbcaqy", 8, "bjrv", 4);
    dx->put("lptgfdvy", 8, "gtpl", 4);
    dx->put("pezjlhay", 8, "jzep", 4);
    dx->put("aklgzymc", 8, "glka", 4);
    dx->put("qhctcvjw", 8, "tchq", 4);
    dx->put("yirlpidc", 8, "lriy", 4);
    dx->put("qotyvhsd", 8, "ytoq", 4);
    dx->put("pharnxfp", 8, "rahp", 4);
    dx->put("khooyrxw", 8, "oohk", 4);
    dx->put("rjuyvmpu", 8, "yujr", 4);
    dx->put("kjdrqtpn", 8, "rdjk", 4);
    dx->put("zcvsbtxm", 8, "svcz", 4);
    dx->put("ikffgszd", 8, "ffki", 4);
    dx->put("azbdbmvd", 8, "dbza", 4);
    dx->put("zggonylk", 8, "oggz", 4);
    dx->put("cwqssmov", 8, "sqwc", 4);
    dx->put("dintmaws", 8, "tnid", 4);
    dx->put("gabevnbm", 8, "ebag", 4);
    dx->put("nmmmnbri", 8, "mmmn", 4);

    print(dx, "fqofzfvv", 8);
    print(dx, "bqvqjhrx", 8);
    print(dx, "ywuowscl", 8);
    print(dx, "iobvvzvo", 8);
    print(dx, "ituaqsrm", 8);
    print(dx, "qrhyxpdc", 8);
    print(dx, "oocfzpvx", 8);
    print(dx, "cxgvhsns", 8);
    print(dx, "saixfyep", 8);
    print(dx, "fphhhjce", 8);
    print(dx, "cpiyzscc", 8);
    print(dx, "vnhzbiys", 8);
    print(dx, "fubombkf", 8);
    print(dx, "lsaubwra", 8);
    print(dx, "zehwjrmp", 8);
    print(dx, "qimmqtrr", 8);
    print(dx, "ptnkefll", 8);
    print(dx, "pzjijtpo", 8);
    print(dx, "piuubwas", 8);
    print(dx, "mypevmqx", 8);
    print(dx, "mztxyzqs", 8);
    print(dx, "aiksulmf", 8);
    print(dx, "xazjiyzz", 8);
    print(dx, "wgcevaja", 8);
    print(dx, "fttoxdre", 8);
    print(dx, "fmmrsbha", 8);
    print(dx, "hsiyxdok", 8);
    print(dx, "pdwnjlkd", 8);
    print(dx, "zowzhhmp", 8);
    print(dx, "rzielafw", 8);
    print(dx, "duyucjmb", 8);
    print(dx, "gdninjib", 8);
    print(dx, "cesmxsig", 8);
    print(dx, "nvcigkcd", 8);
    print(dx, "qkktpdhx", 8);
    print(dx, "mgggaadz", 8);
    print(dx, "mflzmzwg", 8);
    print(dx, "chulxyvw", 8);
    print(dx, "xrhjcfzg", 8);
    print(dx, "iykkcwuq", 8);
    print(dx, "ljrwtddu", 8);
    print(dx, "yxkvkkog", 8);
    print(dx, "yfaqlwne", 8);
    print(dx, "kylcwzwz", 8);
    print(dx, "zqspcvuc", 8);
    print(dx, "fhqvvfie", 8);
    print(dx, "qggeubrp", 8);
    print(dx, "lqvgxzrs", 8);
    print(dx, "hutjgvdz", 8);
    print(dx, "ljwhzsbo", 8);
    print(dx, "ahechctg", 8);
    print(dx, "wihjwrjh", 8);
    print(dx, "qlfviaaf", 8);
    print(dx, "nyzfqozb", 8);
    print(dx, "eyyjmhgw", 8);
    print(dx, "uunnfahw", 8);
    print(dx, "gauvbcwp", 8);
    print(dx, "lfkovvzf", 8);
    print(dx, "lvuqgclo", 8);
    print(dx, "ktisrkdu", 8);
    print(dx, "utafptfb", 8);
    print(dx, "jppcklbj", 8);
    print(dx, "vdqwkoex", 8);
    print(dx, "wxzrqdih", 8);
    print(dx, "phkbcogd", 8);
    print(dx, "gqeignuv", 8);
    print(dx, "zzotsjnt", 8);
    print(dx, "lhelmwus", 8);
    print(dx, "tkdjwobq", 8);
    print(dx, "qxpjsvvt", 8);
    print(dx, "jwqhrxku", 8);
    print(dx, "xpqehtmv", 8);
    print(dx, "zbhfrxuy", 8);
    print(dx, "ulvhzezy", 8);
    print(dx, "flmqoilv", 8);
    print(dx, "olehccwh", 8);
    print(dx, "brdclhsz", 8);
    print(dx, "xlmhcbmt", 8);
    print(dx, "bkldesoe", 8);
    print(dx, "qvutmtxr", 8);
    print(dx, "loalanrq", 8);
    print(dx, "xxtgcftx", 8);
    print(dx, "dlkvpbij", 8);
    print(dx, "tmejvjcw", 8);
    print(dx, "yflqgvuv", 8);
    print(dx, "yqctalxj", 8);
    print(dx, "wmanhizg", 8);
    print(dx, "tjqmqnok", 8);
    print(dx, "qmjbrmqf", 8);
    print(dx, "ifmfmzoh", 8);
    print(dx, "wjdazhdg", 8);
    print(dx, "psxeficy", 8);
    print(dx, "jtemuteu", 8);
    print(dx, "diwhegps", 8);
    print(dx, "elnjefht", 8);
    print(dx, "urhrvjhf", 8);
    print(dx, "eooecchf", 8);
    print(dx, "ynumssbv", 8);
    print(dx, "pndhkmaw", 8);
    print(dx, "wucnblkx", 8);
    print(dx, "slcppeur", 8);
    print(dx, "vqjalesw", 8);
    print(dx, "uaxudeai", 8);
    print(dx, "xzvxtjfa", 8);
    print(dx, "oraletck", 8);
    print(dx, "tkazqocx", 8);
    print(dx, "tdtwqkec", 8);
    print(dx, "ksjphgwz", 8);
    print(dx, "kyegvwsn", 8);
    print(dx, "rvyxjiyi", 8);
    print(dx, "aynbipff", 8);
    print(dx, "xiuelqos", 8);
    print(dx, "fthlgkev", 8);
    print(dx, "diglremo", 8);
    print(dx, "lppjjcgu", 8);
    print(dx, "iphoourq", 8);
    print(dx, "ririipjb", 8);
    print(dx, "ascpkert", 8);
    print(dx, "tmvlxuqj", 8);
    print(dx, "otzvdllc", 8);
    print(dx, "ebdnsjzh", 8);
    print(dx, "emtaizti", 8);
    print(dx, "flntucdz", 8);
    print(dx, "oeldskky", 8);
    print(dx, "wsorafgb", 8);
    print(dx, "pbexxeyr", 8);
    print(dx, "gyiomytz", 8);
    print(dx, "chxbynvm", 8);
    print(dx, "esawdgmc", 8);
    print(dx, "gtwyfnfv", 8);
    print(dx, "vreizhhh", 8);
    print(dx, "gunqltii", 8);
    print(dx, "ntqlxxaz", 8);
    print(dx, "waiqoijh", 8);
    print(dx, "izgrxqnp", 8);
    print(dx, "qdbermxo", 8);
    print(dx, "bwbddebq", 8);
    print(dx, "unmfmvci", 8);
    print(dx, "afpbxaqa", 8);
    print(dx, "crfhnzos", 8);
    print(dx, "sdjwkxfr", 8);
    print(dx, "flnwnpav", 8);
    print(dx, "ylarqotl", 8);
    print(dx, "folqjneu", 8);
    print(dx, "pdathrhz", 8);
    print(dx, "selayibk", 8);
    print(dx, "gjyszmio", 8);
    print(dx, "tisduryh", 8);
    print(dx, "jqoqiqqw", 8);
    print(dx, "ynyfouge", 8);
    print(dx, "hfodrqfz", 8);
    print(dx, "ejswunvf", 8);
    print(dx, "mvkqwfri", 8);
    print(dx, "ntsvlizy", 8);
    print(dx, "ydjhxlxa", 8);
    print(dx, "syxziped", 8);
    print(dx, "kkfcrvcw", 8);
    print(dx, "djadhylc", 8);
    print(dx, "ancgxjhf", 8);
    print(dx, "meplxhtq", 8);
    print(dx, "sacnccge", 8);
    print(dx, "npebssuw", 8);
    print(dx, "yvbzycpf", 8);
    print(dx, "xcrkwpeg", 8);
    print(dx, "afpyidab", 8);
    print(dx, "yaiyugmu", 8);
    print(dx, "vnogbxgj", 8);
    print(dx, "hwtkyvos", 8);
    print(dx, "gdngtxqc", 8);
    print(dx, "qazowrlm", 8);
    print(dx, "gpngpdum", 8);
    print(dx, "ksniygcl", 8);
    print(dx, "nsmoqtgl", 8);
    print(dx, "dibaogqu", 8);
    print(dx, "ojvuhssc", 8);
    print(dx, "qwvpzngw", 8);
    print(dx, "mfjnwyda", 8);
    print(dx, "jyfohqzi", 8);
    print(dx, "vraaynbu", 8);
    print(dx, "eksainee", 8);
    print(dx, "nywrqybt", 8);
    print(dx, "hayelbeu", 8);
    print(dx, "ilhkjysg", 8);
    print(dx, "ukubisil", 8);
    print(dx, "xgqgxdzc", 8);
    print(dx, "apiwfpcd", 8);
    print(dx, "ysqgcvtj", 8);
    print(dx, "finljuov", 8);
    print(dx, "saxxbphr", 8);
    print(dx, "ivgoiemf", 8);
    print(dx, "apcoqpta", 8);
    print(dx, "wuhlqwyt", 8);
    print(dx, "lnixuhjx", 8);
    print(dx, "zxyzqogc", 8);
    print(dx, "ttcgjwkb", 8);
    print(dx, "lqmgkdbq", 8);
    print(dx, "vheqhcze", 8);
    print(dx, "bifwihgc", 8);
    print(dx, "jdsopgrc", 8);
    print(dx, "kcfefuzt", 8);
    print(dx, "gnkexqow", 8);
    print(dx, "wqfocgqd", 8);
    print(dx, "lbgkgebv", 8);
    print(dx, "hfnsbdsv", 8);
    print(dx, "bqewlmnv", 8);
    print(dx, "dcmqxfui", 8);
    print(dx, "isumbzmy", 8);
    print(dx, "tcosncvl", 8);
    print(dx, "ccwxvspa", 8);
    print(dx, "cngwquod", 8);
    print(dx, "rlvshczu", 8);
    print(dx, "jjicldqo", 8);
    print(dx, "kznvvnfw", 8);
    print(dx, "nvgagzqn", 8);
    print(dx, "gvmpyott", 8);
    print(dx, "uejiphnw", 8);
    print(dx, "uohxnlmr", 8);
    print(dx, "windxuol", 8);
    print(dx, "bdlcshjb", 8);
    print(dx, "msxvvlkh", 8);
    print(dx, "jvqsqocx", 8);
    print(dx, "icgyizvu", 8);
    print(dx, "qezjxcac", 8);
    print(dx, "pixjvzjj", 8);
    print(dx, "nuaszsck", 8);
    print(dx, "ghrcxmio", 8);
    print(dx, "ovtbazgy", 8);
    print(dx, "abmopltf", 8);
    print(dx, "kddvfcjs", 8);
    print(dx, "gsovsbwo", 8);
    print(dx, "mzcracag", 8);
    print(dx, "iqchmzty", 8);
    print(dx, "xabmzqiu", 8);
    print(dx, "gudhujdv", 8);
    print(dx, "kjwuamqn", 8);
    print(dx, "hlonayae", 8);
    print(dx, "xzwasajg", 8);
    print(dx, "slpgdqkl", 8);
    print(dx, "nbxxbfhz", 8);
    print(dx, "buzzlyrr", 8);
    print(dx, "xxbpzhnk", 8);
    print(dx, "kiemmzom", 8);
    print(dx, "mrbmsksu", 8);
    print(dx, "apyydfzf", 8);
    print(dx, "mxxluhia", 8);
    print(dx, "uspzjjkx", 8);
    print(dx, "iykpnamr", 8);
    print(dx, "vmxwtegw", 8);
    print(dx, "mekiicsa", 8);
    print(dx, "qsnjeohs", 8);
    print(dx, "mlzhzymc", 8);
    print(dx, "pqlkdiwl", 8);
    print(dx, "zxkhpfbc", 8);
    print(dx, "abatvmkp", 8);
    print(dx, "weiahthe", 8);
    print(dx, "znjawttn", 8);
    print(dx, "dpvrsjew", 8);
    print(dx, "tzlthhmi", 8);
    print(dx, "fxsysves", 8);
    print(dx, "eyyuprqy", 8);
    print(dx, "prgdvugy", 8);
    print(dx, "gmluspto", 8);
    print(dx, "fsticqdz", 8);
    print(dx, "pwbnutzi", 8);
    print(dx, "ffpomyrw", 8);
    print(dx, "ftlqnnrv", 8);
    print(dx, "joegklrq", 8);
    print(dx, "cyvbhvta", 8);
    print(dx, "vvcoowix", 8);
    print(dx, "jnjtkvfd", 8);
    print(dx, "lvaxykrm", 8);
    print(dx, "bjqmzagu", 8);
    print(dx, "nikbxikj", 8);
    print(dx, "poftfeaw", 8);
    print(dx, "haxmqrqc", 8);
    print(dx, "kntbchyq", 8);
    print(dx, "tnxlmxbb", 8);
    print(dx, "ypeuaoys", 8);
    print(dx, "hwxqcsra", 8);
    print(dx, "cjujykin", 8);
    print(dx, "qjfpheuh", 8);
    print(dx, "uxsdbmuw", 8);
    print(dx, "nxwklpdp", 8);
    print(dx, "tstothem", 8);
    print(dx, "vxqrfzzy", 8);
    print(dx, "ntentxvw", 8);
    print(dx, "gkoounyg", 8);
    print(dx, "srxruxck", 8);
    print(dx, "fjvkmkzy", 8);
    print(dx, "ujdjsdha", 8);
    print(dx, "otbukfna", 8);
    print(dx, "bdwobndq", 8);
    print(dx, "fqxgedit", 8);
    print(dx, "imseeede", 8);
    print(dx, "frrsifey", 8);
    print(dx, "garjnxgl", 8);
    print(dx, "ixyudwou", 8);
    print(dx, "rtkvaitn", 8);
    print(dx, "dglwdcjl", 8);
    print(dx, "tettytyk", 8);
    print(dx, "jgrbactf", 8);
    print(dx, "lrkvwuub", 8);
    print(dx, "tokewdxa", 8);
    print(dx, "kvfrzmvz", 8);
    print(dx, "ahesruzy", 8);
    print(dx, "mkygwgax", 8);
    print(dx, "cotrgece", 8);
    print(dx, "apiclmka", 8);
    print(dx, "fkzkvdon", 8);
    print(dx, "fnwubwwo", 8);
    print(dx, "uflyvkvz", 8);
    print(dx, "qwbfotvu", 8);
    print(dx, "albxcrwr", 8);
    print(dx, "mhesddok", 8);
    print(dx, "wwwylexx", 8);
    print(dx, "chytaxin", 8);
    print(dx, "hmuuefps", 8);
    print(dx, "bcdhsqrn", 8);
    print(dx, "mkjukkrx", 8);
    print(dx, "xijkaqdx", 8);
    print(dx, "nejotxmv", 8);
    print(dx, "dekfojbp", 8);
    print(dx, "cpqzwdhu", 8);
    print(dx, "dsmoegfj", 8);
    print(dx, "wbkkhixg", 8);
    print(dx, "ydqcaous", 8);
    print(dx, "nskfzmul", 8);
    print(dx, "ncltcufy", 8);
    print(dx, "cidexsjp", 8);
    print(dx, "otnjhfts", 8);
    print(dx, "zrbwtcwt", 8);
    print(dx, "smgqnyqw", 8);
    print(dx, "figjidza", 8);
    print(dx, "rdibjxog", 8);
    print(dx, "nucjxgdn", 8);
    print(dx, "syvtsfmm", 8);
    print(dx, "ddqbaees", 8);
    print(dx, "pdavyqno", 8);
    print(dx, "xcwnokjx", 8);
    print(dx, "izgdvtjk", 8);
    print(dx, "oapmtuta", 8);
    print(dx, "qmglgoau", 8);
    print(dx, "ffnvxyzc", 8);
    print(dx, "vzjkagyp", 8);
    print(dx, "mmkkxukj", 8);
    print(dx, "gnvateuh", 8);
    print(dx, "owctrnsu", 8);
    print(dx, "fbemkfue", 8);
    print(dx, "zvutqbsc", 8);
    print(dx, "yzvktize", 8);
    print(dx, "yyxqattk", 8);
    print(dx, "cdwfkdfa", 8);
    print(dx, "tcbmepyf", 8);
    print(dx, "syztntwa", 8);
    print(dx, "qikvxdjp", 8);
    print(dx, "nrdquuoq", 8);
    print(dx, "fqiilygj", 8);
    print(dx, "jabhejwd", 8);
    print(dx, "xfrayaxz", 8);
    print(dx, "imvbksfp", 8);
    print(dx, "xnwzewgo", 8);
    print(dx, "twoitaic", 8);
    print(dx, "nwndynwf", 8);
    print(dx, "snxqtoew", 8);
    print(dx, "xelftpjr", 8);
    print(dx, "lmqjbodz", 8);
    print(dx, "xadzysgg", 8);
    print(dx, "xsumoebx", 8);
    print(dx, "uulldjst", 8);
    print(dx, "qbajtwri", 8);
    print(dx, "olnujwns", 8);
    print(dx, "ejkjkkqj", 8);
    print(dx, "pujvxrih", 8);
    print(dx, "rsvereza", 8);
    print(dx, "alspxafg", 8);
    print(dx, "svnrqwmv", 8);
    print(dx, "nsmowsnb", 8);
    print(dx, "xmnjfxts", 8);
    print(dx, "vrjbcaqy", 8);
    print(dx, "lptgfdvy", 8);
    print(dx, "pezjlhay", 8);
    print(dx, "aklgzymc", 8);
    print(dx, "qhctcvjw", 8);
    print(dx, "yirlpidc", 8);
    print(dx, "qotyvhsd", 8);
    print(dx, "pharnxfp", 8);
    print(dx, "khooyrxw", 8);
    print(dx, "rjuyvmpu", 8);
    print(dx, "kjdrqtpn", 8);
    print(dx, "zcvsbtxm", 8);
    print(dx, "ikffgszd", 8);
    print(dx, "azbdbmvd", 8);
    print(dx, "zggonylk", 8);
    print(dx, "cwqssmov", 8);
    print(dx, "dintmaws", 8);
    print(dx, "gabevnbm", 8);
    print(dx, "nmmmnbri", 8);

    dx->printMaxKeyCount(NUM_ENTRIES);
    dx->printNumLevels();
    std::cout << "Size:" << dx->size() << endl;
    std::cout << "Trie Size:" << (int) dx->root_data[9] << endl;
    return 0;
}

int main4() {
    stx::btree_map<string, string> bm;
    bm.insert(pair<string, string>("hello", "world"));
    cout << bm["hello"] << endl;
    return 1;
}

int main(int argc, char *argv[]) {

    if (argc > 1)
        NUM_ENTRIES = atol(argv[1]);
    if (argc > 2)
        CHAR_SET = atoi(argv[2]);
    if (argc > 3)
        KEY_LEN = atoi(argv[3]);

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
    int ctr = 0;
    int null_ctr = 0;
    int cmp = 0;

//    stx::btree_map<string, string> m1;
//    //map<string, string> m1;
//    m.begin();
//    start = getTimeVal();
//    it = m.begin();
//    for (; it != m.end(); ++it)
//        m1.insert(pair<string, string>(it->first, it->second));
//    stop = getTimeVal();
//    cout << "RB Tree insert time:" << timedifference(start, stop) << endl;
//    it = m.begin();
//    start = getTimeVal();
//    for (; it != m.end(); ++it) {
//        string value = m1[it->first.c_str()];
//        char v[100];
//        if (value.length() == 0) {
//            null_ctr++;
//        } else {
//            int16_t d = util::compare(it->second.c_str(), it->second.length(),
//                    value.c_str(), value.length());
//            if (d != 0) {
//                cmp++;
//                strncpy(v, value.c_str(), value.length());
//                v[it->first.length()] = 0;
//                cout << cmp << ":" << it->first.c_str() << "=========="
//                        << it->second.c_str() << "----------->" << v << endl;
//            }
//        }
//        ctr++;
//    }
//    stop = getTimeVal();
//    cout << "RB Tree get time:" << timedifference(start, stop) << endl;
//    cout << "Null:" << null_ctr << endl;
//    cout << "Cmp:" << cmp << endl;
//    cout << "RB Tree size:" << m1.size() << endl;

    unordered_map<string, string>::iterator it1;

//    ctr = 0;
//    it = m.begin();
//    for (; it != m.end(); ++it) {
//        cout << "\"" << it->first.c_str() << "\", \"" << it->second.c_str()
//                << "\"," << endl;
//        if (ctr++ > 90)
//            break;
//    }

    ctr = 0;
    art_tree at;
    art_tree_init(&at);
    start = getTimeVal();
    it1 = m.begin();
    for (; it1 != m.end(); ++it1) {
        art_insert(&at, (unsigned char*) it1->first.c_str(),
                it1->first.length(), (void *) it1->second.c_str(),
                it1->second.length());
        ctr++;
    }
    stop = getTimeVal();
    cout << "ART Insert Time:" << timedifference(start, stop) << endl;
    //getchar();

    ctr = 0;
    linex *lx = new linex();
    //rb_tree *lx = new rb_tree();
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
    dfox *dx = new dfox();
    //dfos *dx = new dfos();
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

    if (null_ctr > 0 || cmp > 0) {
        it = m.begin();
        for (; it != m.end(); ++it) {
            cout << "\"" << it->first.c_str() << "\", \"" << it->second.c_str()
                    << "\"," << endl;
        }
    }

    null_ctr = 0;
    cmp = 0;
    ctr = 0;
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

    null_ctr = 0;
    cmp = 0;
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

    null_ctr = 0;
    cmp = 0;
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
    std::cout << "Trie Size:" << (int) dx->root_data[MAX_PTR_BITMAP_BYTES+1] << endl;
    dx->printMaxKeyCount(NUM_ENTRIES);
    dx->printNumLevels();
    cout << "Root filled size:" << (int) dx->root_data[MAX_PTR_BITMAP_BYTES+2] << endl;
    //getchar();

    return 0;

}
