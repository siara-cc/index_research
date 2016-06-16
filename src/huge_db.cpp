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
#define CS_ALPHA_ONLY 2
#define CS_NUMBER_ONLY 3
#define CS_ONE_PER_OCTET 4
#define CS_255_RANDOM 5
#define CS_255_DENSE 6

long NUM_ENTRIES = 600;
int CHAR_SET = 6;
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
        } else if (CHAR_SET == CS_ALPHA_ONLY) {
            k[0] = 97 + (rand() % 26);
            k[1] = 97 + (rand() % 26);
            k[2] = 97 + (rand() % 26);
            k[3] = 97 + (rand() % 26);
            k[4] = 97 + (rand() % 26);
            k[5] = 97 + (rand() % 26);
            k[6] = 97 + (rand() % 26);
            k[7] = 97 + (rand() % 26);
            k[7] = 97 + (rand() % 26);
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
    //dfos *dx = new dfos();
    dfox *dx = new dfox();
    //linex *dx = new linex();

//    dx->put("Hello", 5, "World", 5);
//    dx->put("Nice", 4, "Place", 5);
//    dx->put("Arun", 4, "Hello", 5);
//    dx->put("arun", 4, "dale", 4);
//    dx->put("resin", 5, "34623", 5);
//    dx->put("rinse", 5, "2", 1);
//    dx->put("rickshaw", 8, "4", 1);
//    dx->put("ride", 4, "5", 1);
//    dx->put("rider", 5, "6", 1);
//    dx->put("rid", 3, "5.5", 3);
//    dx->put("rice", 4, "7", 1);
//    dx->put("rick", 4, "8", 1);
//    dx->put("aruna", 5, "9", 1);
//    dx->put("hello", 5, "10", 2);
//    dx->put("world", 5, "11", 2);
//    dx->put("how", 3, "12", 2);
//    dx->put("are", 3, "13", 2);
//    dx->put("you", 3, "14", 2);
//    dx->put("hundred", 7, "100", 3);
//    dx->put("boy", 3, "15", 2);
//    dx->put("boat", 4, "16", 2);
//    dx->put("thousand", 8, "1000", 4);
//    dx->put("buoy", 4, "17", 2);
//    dx->put("boast", 5, "18", 2);
//    dx->put("January", 7, "first", 5);
//    dx->put("February", 8, "second", 6);
//    dx->put("March", 5, "third", 5);
//    dx->put("April", 5, "forth", 5);
//    dx->put("May", 3, "fifth", 5);
//    dx->put("June", 4, "sixth", 5);
//    dx->put("July", 4, "seventh", 7);
//    dx->put("August", 6, "eighth", 6);
//    dx->put("September", 9, "ninth", 5);
//    dx->put("October", 7, "tenth", 5);
//    dx->put("November", 8, "eleventh", 8);
//    dx->put("December", 8, "twelfth", 7);
//    dx->put("Sunday", 6, "one", 3);
//    dx->put("Monday", 6, "two", 3);
//    dx->put("Tuesday", 7, "three", 5);
//    dx->put("Wednesday", 9, "four", 4);
//    dx->put("Thursday", 8, "five", 4);
//    dx->put("Friday", 6, "six", 3);
//    dx->put("Saturday", 8, "seven", 7);
//    dx->put("casa", 4, "nova", 4);
//    dx->put("young", 5, "19", 2);
//    dx->put("youth", 5, "20", 2);
//    dx->put("yousuf", 6, "21", 2);

//    print(dx, "Hello", 5);
//    print(dx, "Nice", 4);
//    print(dx, "Arun", 4);
//    print(dx, "arun", 4);
//    print(dx, "resin", 5);
//    print(dx, "rinse", 5);
//    print(dx, "rickshaw", 8);
//    print(dx, "ride", 4);
//    print(dx, "rider", 5);
//    print(dx, "rid", 3);
//    print(dx, "rice", 4);
//    print(dx, "rick", 4);
//    print(dx, "aruna", 5);
//    print(dx, "hello", 5);
//    print(dx, "world", 5);
//    print(dx, "how", 3);
//    print(dx, "are", 3);
//    print(dx, "you", 3);
//    print(dx, "hundred", 7);
//    print(dx, "boy", 3);
//    print(dx, "boat", 4);
//    print(dx, "thousand", 8);
//    print(dx, "buoy", 4);
//    print(dx, "boast", 5);
//    print(dx, "January", 7);
//    print(dx, "February", 8);
//    print(dx, "March", 5);
//    print(dx, "April", 5);
//    print(dx, "May", 3);
//    print(dx, "June", 4);
//    print(dx, "July", 4);
//    print(dx, "August", 6);
//    print(dx, "September", 9);
//    print(dx, "October", 7);
//    print(dx, "November", 8);
//    print(dx, "December", 8);
//    print(dx, "Sunday", 6);
//    print(dx, "Monday", 6);
//    print(dx, "Tuesday", 7);
//    print(dx, "Wednesday", 9);
//    print(dx, "Thursday", 8);
//    print(dx, "Friday", 6);
//    print(dx, "Saturday", 8);
//    print(dx, "casa", 4);
//    print(dx, "young", 5);
//    print(dx, "youth", 5);
//    print(dx, "yousuf", 6);

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
//    dx->put("afternoon", 9, "east", 4);
//    dx->put("afford", 6, "cannot", 6);
//    dx->put("acceptable", 10, "cake", 4);
//    dx->put("ability", 7, "calculate", 9);
//    dx->put("again", 5, "cardboard", 9);
//    dx->put("advertising", 11, "ceremony", 8);
//    dx->put("accidental", 10, "eleven", 6);
//    dx->put("action", 6, "ease", 4);
//    dx->put("academic", 8, "carefully", 9);
//    dx->put("advert", 6, "elegant", 7);
//    dx->put("account", 7, "earth", 5);
//    dx->put("above", 5, "certainly", 9);
//    dx->put("accompany", 9, "case", 4);
//    dx->put("adapt", 5, "castle", 6);
//    dx->put("ago", 3, "earn", 4);
//    dx->put("actually", 8, "cancel", 6);
//    dx->put("alarmed", 7, "celebrate", 9);
//    dx->put("abuse", 5, "cast", 4);
//    dx->put("accurately", 10, "chain", 5);
//    dx->put("accurate", 8, "charge", 6);
//    dx->put("a", 1, "carpet", 6);
//    dx->put("ahead", 5, "cause", 5);
//    dx->put("advantage", 9, "capacity", 8);
//    dx->put("accommodation", 13, "camp", 4);
//    dx->put("actual", 6, "ceiling", 7);
//    dx->put("acknowledge", 11, "cancer", 6);
//    dx->put("actress", 7, "chart", 5);
//    dx->put("abandoned", 9, "character", 9);
//    dx->put("able", 4, "cash", 4);
//    dx->put("absent", 6, "channel", 7);
//    dx->put("airport", 7, "carry", 5);

//    dx->put("against", 7, "catch", 5);
//    dx->put("across", 6, "cell", 4);
//    dx->put("aid", 3, "central", 7);
//    dx->put("advance", 7, "eleventh", 8);
//    dx->put("accept", 6, "career", 6);
//    dx->put("adult", 5, "capable", 7);
//    dx->put("ad", 2, "care", 4);
//    dx->put("aged", 4, "candidate", 9);
//    dx->put("alarm", 5, "CD", 2);
//    dx->put("admiration", 10, "campaign", 8);
//    dx->put("affect", 6, "cent", 4);
//    dx->put("adjust", 6, "capture", 7);
//    dx->put("accidentally", 12, "change", 6);
//    dx->put("about", 5, "cheat", 5);
//    dx->put("actor", 5, "celebration", 11);
//    dx->put("after", 5, "electronic", 10);
//    dx->put("air", 3, "chase", 5);
//    dx->put("accent", 6, "cable", 5);
//    dx->put("agent", 5, "camera", 6);
//    dx->put("admit", 5, "certain", 7);
//    dx->put("adequately", 10, "cheap", 5);
//    dx->put("abandon", 7, "category", 8);
//    dx->put("addition", 8, "chapter", 7);
//    dx->put("affection", 9, "electricity", 11);
//    dx->put("achieve", 7, "early", 5);
//    dx->put("adventure", 9, "candy", 5);
//    dx->put("active", 6, "centimetre", 10);
//    dx->put("advise", 6, "challenge", 9);
//    dx->put("age", 3, "call", 4);
//    dx->put("achievement", 11, "chat", 4);

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

//    print(dx, "against", 7);
//    print(dx, "across", 6);
//    print(dx, "aid", 3);
//    print(dx, "advance", 7);
//    print(dx, "accept", 6);
//    print(dx, "adult", 5);
//    print(dx, "ad", 2);
//    print(dx, "aged", 4);
//    print(dx, "alarm", 5);
//    print(dx, "admiration", 10);
//    print(dx, "affect", 6);
//    print(dx, "adjust", 6);
//    print(dx, "accidentally", 12);
//    print(dx, "about", 5);
//    print(dx, "actor", 5);
//    print(dx, "after", 5);
//    print(dx, "air", 3);
//    print(dx, "accent", 6);
//    print(dx, "agent", 5);
//    print(dx, "admit", 5);
//    print(dx, "adequately", 10);
//    print(dx, "abandon", 7);
//    print(dx, "addition", 8);
//    print(dx, "affection", 9);
//    print(dx, "achieve", 7);
//    print(dx, "adventure", 9);
//    print(dx, "active", 6);
//    print(dx, "advise", 6);
//    print(dx, "age", 3);
//    print(dx, "achievement", 11);

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
    dfox *dx = new dfox();
    //rb_tree *dx = new rb_tree();
    //dfos *dx = new dfos();

    dx->put("l6@uFU;.",8,"u@6l",4);
    dx->put("^O9]A)9|",8,"]9O^",4);
    dx->put("*VCv@z[e",8,"vCV*",4);
    dx->put("vLl6dA7M",8,"6lLv",4);
    dx->put("\\.t\\' }B",8,"\\t.\\",4);
    dx->put("_34fB0`[",8,"f43_",4);
    dx->put(" )r3b7#a",8,"3r) ",4);
    dx->put("jY:td#+(",8,"t:Yj",4);
    dx->put(".u~S4tH;",8,"S~u.",4);
    dx->put("!)Q.IXlp",8,".Q)!",4);
    dx->put("m_,1V]No",8,"1,_m",4);
    dx->put("U^R7%Td?",8,"7R^U",4);
    dx->put("v/sb2$J]",8,"bs/v",4);
    dx->put("hzX`cR-H",8,"`Xzh",4);
    dx->put(",y~=Wj!+",8,"=~y,",4);
    dx->put("#c^+TO(Y",8,"+^c#",4);
    dx->put("~(SOLvKb",8,"OS(~",4);
    dx->put("+%.Vafmk",8,"V.%+",4);
    dx->put("8zKQ)Mi*",8,"QKz8",4);
    dx->put(")(rRMEz+",8,"Rr()",4);
    dx->put("b#g>T2dF",8,">g#b",4);
    dx->put("H4jnE(w3",8,"nj4H",4);
    dx->put("e:P|\\QEX",8,"|P:e",4);
    dx->put("\\6!Rb;b ",8,"R!6\\",4);
    dx->put("C:&h7!3a",8,"h&:C",4);
    dx->put("dLt|]a5H",8,"|tLd",4);
    dx->put("!|xGB<PL",8,"Gx|!",4);
    dx->put("${?y{${'",8,"y?{$",4);
    dx->put("[n\\Ryx8$",8,"R\\n[",4);
    dx->put("6='0.oLP",8,"0'=6",4);
    dx->put("D0-d,^B*",8,"d-0D",4);
    dx->put("=sZViK\\'",8,"VZs=",4);
//    dx->put("8Dj;#d1|",8,";jD8",4);
//    dx->put("y8)rp:<5",8,"r)8y",4);
//    dx->put("/RT)$~fz",8,")TR/",4);
//    dx->put("B,t >7W ",8," t,B",4);
//    dx->put("G3HAYE?\\",8,"AH3G",4);
//    dx->put("~#EF B ;",8,"FE#~",4);
//    dx->put("[KO0BMU*",8,"0OK[",4);
//    dx->put("Y=_?t~a&",8,"?_=Y",4);

    print(dx, "l6@uFU;.",8);
    print(dx, "^O9]A)9|",8);
    print(dx, "*VCv@z[e",8);
    print(dx, "vLl6dA7M",8);
    print(dx, "\\.t\\' }B",8);
    print(dx, "_34fB0`[",8);
    print(dx, " )r3b7#a",8);
    print(dx, "jY:td#+(",8);
    print(dx, ".u~S4tH;",8);
    print(dx, "!)Q.IXlp",8);
    print(dx, "m_,1V]No",8);
    print(dx, "U^R7%Td?",8);
    print(dx, "v/sb2$J]",8);
    print(dx, "hzX`cR-H",8);
    print(dx, ",y~=Wj!+",8);
    print(dx, "#c^+TO(Y",8);
    print(dx, "~(SOLvKb",8);
    print(dx, "+%.Vafmk",8);
    print(dx, "8zKQ)Mi*",8);
    print(dx, ")(rRMEz+",8);
    print(dx, "b#g>T2dF",8);
    print(dx, "H4jnE(w3",8);
    print(dx, "e:P|\\QEX",8);
    print(dx, "\\6!Rb;b ",8);
    print(dx, "C:&h7!3a",8);
    print(dx, "dLt|]a5H",8);
    print(dx, "!|xGB<PL",8);
    print(dx, "${?y{${'",8);
    print(dx, "[n\\Ryx8$",8);
    print(dx, "6='0.oLP",8);
    print(dx, "D0-d,^B*",8);
    print(dx, "=sZViK\\'",8);
    print(dx, "8Dj;#d1|",8);
    print(dx, "y8)rp:<5",8);
    print(dx, "/RT)$~fz",8);
    print(dx, "B,t >7W ",8);
    print(dx, "G3HAYE?\\",8);
    print(dx, "~#EF B ;",8);
    print(dx, "[KO0BMU*",8);
    print(dx, "Y=_?t~a&",8);

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

//    map<string, string> m1;
//    m.begin();
//    start = getTimeVal();
//    it = m.begin();
//    for (; it != m.end(); ++it) {
//        m1.insert(pair<string, string>(it->first, it->second));
//    }
//    stop = getTimeVal();
//    cout << "RB Tree insert time:" << timedifference(start, stop) << endl;
//    it = m.begin();
//    start = getTimeVal();
//    for (; it != m.end(); ++it) {
//        m1[it->first];
//    }
//    stop = getTimeVal();
//    cout << "RB Tree get time:" << timedifference(start, stop) << endl;
//    cout << "RB Tree size:" << m1.size() << endl;

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

//    ctr = 0;
//    it = m.begin();
//    for (; it != m.end(); ++it) {
//        cout << "\"" << it->first.c_str() << "\", \"" << it->second.c_str()
//                << "\"," << endl;
//        if (ctr++ > 90)
//            break;
//    }

    null_ctr = 0;
    ctr = 0;
    cmp = 0;
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

    null_ctr = 0;
    ctr = 0;
    cmp = 0;
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
