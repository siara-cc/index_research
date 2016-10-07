#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include "util.h"
#include "art.h"
#include "linex.h"
//#include "dfos.h"
#include "dfox.h"
#include "bft.h"
#include "bfos.h"
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

char *IMPORT_FILE = NULL;
long NUM_ENTRIES = 1000;
int CHAR_SET = 2;
int KEY_LEN = 8;
int VALUE_LEN = 4;

void insert(unordered_map<string, string>& m) {
    char k[100];
    char v[100];
    srand(time(NULL));
    for (long l = 0; l < NUM_ENTRIES; l++) {

        if (CHAR_SET == CS_PRINTABLE) {
            for (int i = 0; i < KEY_LEN; i++)
                k[i] = 32 + (rand() % 95);
            k[KEY_LEN] = 0;
        } else if (CHAR_SET == CS_ALPHA_ONLY) {
            for (int i = 0; i < KEY_LEN; i++)
                k[i] = 97 + (rand() % 26);
            k[KEY_LEN] = 0;
        } else if (CHAR_SET == CS_NUMBER_ONLY) {
            for (int i = 0; i < KEY_LEN; i++)
                k[i] = 48 + (rand() % 10);
            k[KEY_LEN] = 0;
        } else if (CHAR_SET == CS_ONE_PER_OCTET) {
            for (int i = 0; i < KEY_LEN; i++)
                k[i] = ((rand() % 32) << 3) | 0x07;
            k[KEY_LEN] = 0;
        } else if (CHAR_SET == CS_255_RANDOM) {
            for (int i = 0; i < KEY_LEN; i++)
                k[i] = (rand() % 255);
            k[KEY_LEN] = 0;
            for (int i = 0; i < KEY_LEN; i++) {
                if (k[i] == 0)
                    k[i] = i + 1;
            }
        } else if (CHAR_SET == CS_255_DENSE) {
            KEY_LEN = 4;
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
        for (int16_t i = 0; i < VALUE_LEN; i++)
            v[VALUE_LEN - i - 1] = k[i];
        v[VALUE_LEN] = 0;
        //itoa(rand(), v, 10);
        //itoa(rand(), v + strlen(v), 10);
        //itoa(rand(), v + strlen(v), 10);
        if (l == 0)
            cout << "key:" << k << ", value: " << v << endl;
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

void print(bft *bx, const char *key, int16_t key_len) {
    int16_t len;
    char *value = bx->get(key, key_len, &len);
    if (value == null || len == 0) {
        std::cout << "Value for " << key << " is null" << endl;
        return;
    }
    char s[100];
    strncpy(s, value, len);
    s[len] = 0;
    std::cout << "Key: " << key << ", Value:" << s << endl;
}

void print(bfos *bx, const char *key, int16_t key_len) {
    int16_t len;
    char *value = bx->get(key, key_len, &len);
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
    //rb_tree *dx = new rb_tree();
    //dfos *dx = new dfos();
    //dfox *dx = new dfox();
    //bfos *dx = new bfos();
    bft *dx = new bft();
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
    cout << "Trie size: " << (int) dx->root_data[5] << endl;
    cout << "Filled size: " << (int) dx->root_data[2] << endl;
    return 0;
}

int main3() {
    int numKeys = 10;
    GenTree::generateBitCounts();
    //linex *dx = new linex();
    //dfos *dx = new dfos();
    //dfox *dx = new dfox();
    bfos *dx = new bfos();
    char k[5] = "\001\001\001\000";
    for (int i = 1; i < numKeys; i++) {
        char v[5];
        k[3] = i;
        sprintf(v, "%d", i);
        dx->put(k, 4, v, strlen(v));
    }
    for (int i = 1; i < numKeys; i++) {
        char v[5];
        k[3] = i;
        sprintf(v, "%d", i);
        print(dx, (char *) k, 4);
    }
    return 0;
}

int main4() {
    GenTree::generateBitCounts();
    //linex *dx = new linex();
    //bfos *dx = new bfos();
    //dfox *dx = new dfox();
    bft *dx = new bft();
    //rb_tree *dx = new rb_tree();
    //dfos *dx = new dfos();

    dx->put("abqvujtf", 8, "vqba", 4);
    dx->put("gqncjgvs", 8, "cnqg", 4);
    dx->put("izawtdcz", 8, "wazi", 4);

    print(dx, "abqvujtf", 8);
    print(dx, "gqncjgvs", 8);
    print(dx, "izawtdcz", 8);

    dx->printMaxKeyCount(NUM_ENTRIES);
    dx->printNumLevels();
    std::cout << "Size:" << dx->size() << endl;
    std::cout << "Trie Size:" << (int) dx->root_data[5] << endl;
    return 0;
}

int main5() {
    stx::btree_map<string, string> bm;
    bm.insert(pair<string, string>("hello", "world"));
    cout << bm["hello"] << endl;
    return 1;
}

int main6() {
    art_tree at;
    art_tree_init(&at);
    art_insert(&at, (const byte *) "arun", 5, (void *) "Hello", 5);
    art_insert(&at, (const byte *) "aruna", 6, (void *) "World", 5);
    //art_insert(&at, (const byte *) "absorb", 6, (void *) "ART", 5);
    int len;
    cout << (char*) art_search(&at, (byte *) "arun", 5, &len) << endl;
    cout << (char*) art_search(&at, (byte *) "aruna", 6, &len) << endl;
    //art_search(&at, (byte *) "absorb", 6, &len);
    return 1;
}

int main(int argc, char *argv[]) {

    if (argc > 1) {
        if (argv[1][0] >= '0' && argv[1][0] <= '9')
            NUM_ENTRIES = atol(argv[1]);
        else {
            IMPORT_FILE = argv[1];
            if (argc > 2)
                KEY_LEN = atoi(argv[2]);
        }
    }
    if (argc > 2)
        CHAR_SET = atoi(argv[2]);
    if (argc > 3)
        KEY_LEN = atoi(argv[3]);
    if (argc > 4)
        VALUE_LEN = atoi(argv[4]);

#if defined(ENV64BIT)
    if (sizeof(void*) != 8)
    {
        cout << "ENV64BIT: Error: pointer should be 8 bytes. Exiting." << endl;
        exit(0);
    }
    cout << "Diagnostics: we are running in 64-bit mode." << endl;
#elif defined (ENV32BIT)
    if (sizeof(void*) != 4) {
        cout << "ENV32BIT: Error: pointer should be 4 bytes. Exiting." << endl;
        exit(0);
    }
    cout << "Diagnostics: we are running in 32-bit mode." << endl;
#else
#error "Must define either ENV32BIT or ENV64BIT".
#endif

    GenTree::generateBitCounts();

    unordered_map<string, string> m;
    uint32_t start, stop;
    start = getTimeVal();
    if (IMPORT_FILE == NULL)
        insert(m);
    else {
        FILE *fp;
        char key[100];
        char value[50];
        char *buf;
        int ctr = 0;
        fp = fopen(IMPORT_FILE, "r");
        if (fp == NULL)
            perror("Error opening file");
        buf = key;
        NUM_ENTRIES = 0;
        for (int c = fgetc(fp); c > -1; c = fgetc(fp)) {
            if (c == '\t') {
                buf[ctr] = 0;
                ctr = 0;
                buf = value;
            } else if (c == '\n') {
                buf[ctr] = 0;
                ctr = 0;
                if (strlen(key) < KEY_LEN) {
                    if (buf == value)
                        m.insert(pair<string, string>(key, value));
                    else
                        m.insert(pair<string, string>(key, ""));
                }
                //cout << key << "\t" << value << endl;
                key[0] = 0;
                value[0] = 0;
                buf = key;
                NUM_ENTRIES++;
            } else {
                if (c != '\r')
                    buf[ctr++] = c;
            }
        }
        if (key[0] != 0) {
            m.insert(pair<string, string>(key, value));
            NUM_ENTRIES++;
        }
        fclose(fp);
    }
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
//        if (ctr++ > 300)
//            break;
//    }

    ctr = 0;
    art_tree at;
    art_tree_init(&at);
    start = getTimeVal();
    it1 = m.begin();
    for (; it1 != m.end(); ++it1) {
        art_insert(&at, (unsigned char*) it1->first.c_str(),
                it1->first.length() + 1, (void *) it1->second.c_str(),
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
    bft *dx = new bft();
    //dfox *dx = new dfox();
    //bfos *dx = new bfos();
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

    null_ctr = 0;
    cmp = 0;
    ctr = 0;
    it1 = m.begin();
    start = getTimeVal();
    for (; it1 != m.end(); ++it1) {
        int len;
        char *value = (char *) art_search(&at,
                (unsigned char*) it1->first.c_str(), it1->first.length() + 1,
                &len);
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
    //bfos::count = 0;
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
    std::cout << "Trie Size:" << (int) dx->root_data[MAX_PTR_BITMAP_BYTES + 5]
            << endl;
    std::cout << "Root filled size:"
            << (int) util::getInt(dx->root_data + MAX_PTR_BITMAP_BYTES + 1)
            << endl;
    std::cout << "Data Pos:"
            << (int) util::getInt(dx->root_data + MAX_PTR_BITMAP_BYTES + 3)
            << endl;
    dx->printMaxKeyCount(NUM_ENTRIES);
    dx->printNumLevels();
    //cout << "Root filled size:" << (int) dx->root_data[MAX_PTR_BITMAP_BYTES+2] << endl;
    //getchar();

    /*
     if (NUM_ENTRIES <= 1000 && (null_ctr > 0 || cmp > 0)) {
     it = m.begin();
     for (; it != m.end(); ++it) {
     cout << "\"" << it->first.c_str() << "\", \"" << it->second.c_str()
     << "\"," << endl;
     }
     }
     */

    return 0;

}
