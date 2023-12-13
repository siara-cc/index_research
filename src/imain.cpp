#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <set>
#include <map>
#include "univix_util.h"
#include "art.h"
#include "linex.h"
#include "sqlite.h"
#include "basix.h"
#include "basix3.h"
#include "dfox.h"
#include "dfost.h"
#include "octp.h"
#include "dfqx.h"
#include "bft_cl.h"
#include "dft.h"
#include "bfqs.h"
#include "bfos.h"
#include "bfox.h"
#include "rb_tree.h"
#include "stager.h"
#ifdef _MSC_VER
#include <windows.h>
#endif
#include <unordered_map>
#include <sys/time.h>
#include <sys/stat.h>

#include "../../squeezed-trie/src/squeezed.h"
#include "../../squeezed-trie/src/squeezed_builder.h"

using namespace std;

#define CS_PRINTABLE 1
#define CS_ALPHA_ONLY 2
#define CS_NUMBER_ONLY 3
#define CS_ONE_PER_OCTET 4
#define CS_255_RANDOM 5
#define CS_255_DENSE 6

//char *IMPORT_FILE = "/Users/arun/index_research/Release/w7.txt";
char *IMPORT_FILE = NULL; //"/Users/arun/index_research/Release/unordered_dbpedia_labels.txt";
//char *IMPORT_FILE = "/Users/arun/index_research/Release/domain_rank.csv";
long NUM_ENTRIES = 40;
int CHAR_SET = 2;
int KEY_LEN = 8;
int VALUE_LEN = 4;
int KEY_VALUE_VAR_LEN = 0;
int MIN_KEY_LEN = 12;
int LEAF_PAGE_SIZE = DEFAULT_LEAF_BLOCK_SIZE;
int PARENT_PAGE_SIZE = DEFAULT_PARENT_BLOCK_SIZE;
int CACHE_SIZE = 0;
char *OUT_FILE1 = NULL;
char *OUT_FILE2 = NULL;
int USE_HASHTABLE = 0;
int TEST_HASHTABLE = 0;
int TEST_ART = 1;
int TEST_SQZD = 1;
int TEST_IDX1 = 1;
int TEST_IDX2 = 1;

int ctr = 0;

// Returns how many bytes the given integer will
// occupy if stored as a variable integer
int8_t get_vlen_of_uint32(uint32_t vint) {
    return vint > ((1 << 28) - 1) ? 5
        : (vint > ((1 << 21) - 1) ? 4 
        : (vint > ((1 << 14) - 1) ? 3
        : (vint > ((1 << 7) - 1) ? 2 : 1)));
}

int write_vint32(uint8_t *ptr, uint32_t vint) {
    int len = get_vlen_of_uint32(vint);
    for (int i = len - 1; i > 0; i--)
        *ptr++ = 0x80 + ((vint >> (7 * i)) & 0x7F);
    *ptr = vint & 0x7F;
    return len;
}

uint32_t read_vint32(const uint8_t *ptr, int8_t *vlen) {
    uint32_t ret = 0;
    int8_t len = 5; // read max 5 bytes
    do {
        ret <<= 7;
        ret += *ptr & 0x7F;
        len--;
    } while ((*ptr++ & 0x80) == 0x80 && len);
    if (vlen)
        *vlen = 5 - len;
    return ret;
}

int64_t insert(unordered_map<string, string>& m, uint8_t *data_buf) {
    char k[KEY_LEN + 1];
    char v[VALUE_LEN + 1];
    int k_len = KEY_LEN;
    int v_len = VALUE_LEN;
    int64_t ret = 0;
    srand(time(NULL));
    for (unsigned long l = 0; l < NUM_ENTRIES; l++) {

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
        //cout << "Value: ";
        for (int i = 0; i < VALUE_LEN; i++) {
            v[VALUE_LEN - i - 1] = k[i % KEY_LEN];
            //cout << (char) k[i];
        }
        //cout << endl;
        v[VALUE_LEN] = 0;
        //itoa(rand(), v, 10);
        //itoa(rand(), v + strlen(v), 10);
        //itoa(rand(), v + strlen(v), 10);
        if (KEY_VALUE_VAR_LEN) {
            k_len = (rand() % KEY_LEN) + 1;
            v_len = (rand() % VALUE_LEN) + 1;
            if (k_len < MIN_KEY_LEN)
                k_len = MIN_KEY_LEN;
            k[k_len] = 0;
            v[v_len] = 0;
        }
        if (l == 0)
            printf("key: %.*s, value: %.*s\n", KEY_LEN, k, VALUE_LEN, v);
        if (USE_HASHTABLE)
            m.insert(pair<string, string>(k, v));
        else {
            ret += write_vint32(data_buf + ret, k_len);
            memcpy(data_buf + ret, k, k_len);
            ret += k_len;
            data_buf[ret++] = 0;
            ret += write_vint32(data_buf + ret, v_len);
            memcpy(data_buf + ret, v, v_len);
            ret += v_len;
            data_buf[ret++] = 0;
        }
    }
    if (USE_HASHTABLE)
       NUM_ENTRIES = m.size();
    return ret;
}

int64_t get_import_file_size() {
    struct stat st;
    stat(IMPORT_FILE, &st);
    return st.st_size;
}

int64_t load_file(unordered_map<string, string>& m, uint8_t *data_buf) {
    FILE *fp;
    char key[2000];
    char value[255];
    char *buf;
    int ctr = 0;
    int64_t ret = 0;
    set<string> keyset;
    fp = fopen(IMPORT_FILE, "r");
    if (fp == NULL)
        perror("Error opening file");
    buf = key;
    for (int c = fgetc(fp); c > -1; c = fgetc(fp)) {
        if (c == '\t') {
            buf[ctr] = 0;
            ctr = 0;
            buf = value;
        } else if (c == '\n') {
            buf[ctr] = 0;
            ctr = 0;
            int len = strlen(key);
            if (len > 0 && len <= KEY_LEN) {
                if (keyset.find(string(key)) == keyset.end()) {
                    keyset.insert(string(key));
                    //if (m[key].length() > 0)
                    //    cout << key << ":" << value << endl;
                    if (buf == value) {
                        if (USE_HASHTABLE)
                            m.insert(pair<string, string>(key, value));
                        else {
                            ret += write_vint32(data_buf + ret, len);
                            memcpy(data_buf + ret, key, len);
                            ret += len;
                            data_buf[ret++] = 0;
                            len = strlen(value);
                            ret += write_vint32(data_buf + ret, len);
                            memcpy(data_buf + ret, value, len);
                            ret += len;
                            data_buf[ret++] = 0;
                        }
                    } else {
                        sprintf(value, "%ld", NUM_ENTRIES);
                        //util::ptr_toBytes(NUM_ENTRIES, (uint8_t *) value);
                        //value[4] = 0;
                        if (USE_HASHTABLE)
                            m.insert(pair<string, string>(key, value));
                        else {
                            ret += write_vint32(data_buf + ret, len);
                            memcpy(data_buf + ret, key, len);
                            ret += len;
                            data_buf[ret++] = 0;
                            len = strlen(value);
                            ret += write_vint32(data_buf + ret, len);
                            memcpy(data_buf + ret, value, len);
                            ret += len;
                            data_buf[ret++] = 0;
                        }
                    }
                    if (NUM_ENTRIES % 100000 == 0)
                        cout << "Key:'" << key << "'" << "\t" << "Value:'" << value
                                << "'" << endl;
                    NUM_ENTRIES++;
                }
            }
            key[0] = 0;
            value[0] = 0;
            buf = key;
        } else {
            if (c != '\r')
                buf[ctr++] = c;
        }
    }
    if (key[0] != 0) {
        if (USE_HASHTABLE)
            m.insert(pair<string, string>(key, value));
        else {
            int len = strlen(key);
            ret += write_vint32(data_buf + ret, len);
            memcpy(data_buf + ret, key, len);
            ret += len;
            data_buf[ret++] = 0;
            len = strlen(value);
            ret += write_vint32(data_buf + ret, len);
            memcpy(data_buf + ret, value, len);
            ret += len;
            data_buf[ret++] = 0;
        }
        NUM_ENTRIES++;
    }
    fclose(fp);
    return ret;
}
uint32_t get_time_val() {
#ifdef _MSC_VER
    return Get_tick_count() * 1000;
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

template<class T>
void print(bplus_tree_handler<T> *dx, const char *key, int key_len) {
    int len = VALUE_LEN;
    char value[VALUE_LEN + 1];
    bool is_found = dx->get(key, key_len, &len, value);
    if (!is_found || len == 0) {
        std::cout << "Value for " << key << " is null" << endl;
        return;
    }
    value[len] = 0;
    std::cout << "Key: " << key << ", Value:" << value << endl;
}

int main2() {
    util::generate_bit_counts();
    //rb_tree *dx = new rb_tree();
    //dfox *dx = new dfox();
    bft *dx = new bft(32768, 32768);
    //dfqx *dx = new dfqx();
    //bfos *dx = new bfos();
    //bfqs *dx = new bfqs();
    //bft *dx = new bft();
    //dft *dx = new dft();
    //basix *dx = new basix();
    //linex *dx = new linex();

    dx->put("wwwl.co", 7, "89", 2);
    dx->put("wwzw.co", 7, "88", 2);
    dx->put("wx4.com", 7, "85", 2);
    dx->put("wxcn.cc", 7, "82", 2);
    dx->put("wxcs.cn", 7, "81", 2);
    dx->put("wwwf.co", 7, "99", 2);
    dx->put("wxjm.cc", 7, "77", 2);
    dx->put("wxpx.co", 7, "75", 2);
    dx->put("wxr.cc", 6, "74", 2);
    dx->put("wx.com", 6, "87", 2);
    dx->put("wxsj.co", 7, "70", 2);
    dx->put("wxsf.co", 7, "71", 2);
    dx->put("wxv.pl", 6, "69", 2);
    dx->put("wxxx.co", 7, "65", 2);
    dx->put("wxzp.co", 7, "64", 2);
    dx->put("wxzq.co", 7, "63", 2);
    dx->put("wy.edu", 6, "61", 2);
    dx->put("wy.gov", 6, "60", 2);
    dx->put("wy.us", 5, "59", 2);
    dx->put("wy13.cn", 7, "58", 2);
    dx->put("wycu.cc", 7, "57", 2);
    dx->put("wygk.cn", 7, "54", 2);
    dx->put("wyo.gov", 7, "49", 2);
    dx->put("wyrc.co", 7, "47", 2);
    dx->put("wyqc.co", 7, "48", 2);
    dx->put("wysf.co", 7, "46", 2);
    dx->put("wysp.ws", 7, "44", 2);
    dx->put("wz07.cn", 7, "30", 2);
    dx->put("wyww.co", 7, "39", 2);
    dx->put("wzwp.pw", 7, "8", 1);
    dx->put("wyw.it", 6, "41", 2);
    dx->put("wxmr.co", 7, "76", 2);
    dx->put("wxww.co", 7, "67", 2);
    dx->put("wyzf.co", 7, "36", 2);
    dx->put("wyw.cn", 6, "43", 2);
    dx->put("wxrc.co", 7, "73", 2);
    dx->put("wyzp.co", 7, "35", 2);
    dx->put("wxas.cn", 7, "84", 2);
    dx->put("wyzq.co", 7, "34", 2);
    dx->put("wy.cn", 5, "62", 2);
    dx->put("wyw.hu", 6, "42", 2);
    dx->put("wzxn.cc", 7, "6", 1);
    dx->put("wz.cz", 5, "33", 2);
    dx->put("wxxw.co", 7, "66", 2);
    dx->put("wz09.cn", 7, "28", 2);
    dx->put("wz3.cc", 6, "27", 2);
    dx->put("wxh.cc", 6, "79", 2);
    dx->put("wz7.cc", 6, "26", 2);
    dx->put("wyh.cc", 6, "53", 2);
    dx->put("wzhr.co", 7, "20", 2);
    dx->put("wzc.cn", 6, "24", 2);
    dx->put("wzzz.co", 7, "0", 1);
    dx->put("wylo.cn", 7, "50", 2);
    dx->put("wzdr.cn", 7, "21", 2);
    dx->put("wyyw.co", 7, "37", 2);
    dx->put("wzrd.ru", 7, "15", 2);
    dx->put("wzmr.co", 7, "18", 2);
    dx->put("wzyy.co", 7, "3", 1);
    dx->put("wzpy.cn", 7, "16", 2);
    dx->put("wzzq.co", 7, "1", 1);
    dx->put("wyy.cn", 6, "38", 2);
    dx->put("wxwm.co", 7, "68", 2);
    dx->put("wzb.eu", 6, "25", 2);
    dx->put("wzsf.co", 7, "14", 2);
    dx->put("wzsj.co", 7, "13", 2);
    dx->put("wyec.cn", 7, "56", 2);
    dx->put("wyfc.co", 7, "55", 2);
    dx->put("wyjt.cc", 7, "51", 2);
    dx->put("wzcu.cc", 7, "22", 2);
    dx->put("wz08.cn", 7, "29", 2);
    dx->put("wyw.ru", 6, "40", 2);
    dx->put("wzsp.pl", 7, "12", 2);
    dx->put("wxc.co", 6, "83", 2);
    dx->put("wxfg.cn", 7, "80", 2);
    dx->put("wyhr.co", 7, "52", 2);
    dx->put("wzt8.cn", 7, "11", 2);
    dx->put("wzcn.cn", 7, "23", 2);
    dx->put("wztv.cn", 7, "9", 1);
    dx->put("wz.sk", 5, "31", 2);
    dx->put("wzww.co", 7, "7", 1);
    dx->put("wxhr.co", 7, "78", 2);
    dx->put("wzjn.cn", 7, "19", 2);
    dx->put("wz.de", 5, "32", 2);
    dx->put("wzta.cn", 7, "10", 2);
    dx->put("wxs.nl", 6, "72", 2);
    dx->put("wzyw.cn", 7, "4", 1);
    dx->put("wysj.co", 7, "45", 2);
    dx->put("wx4.cc", 6, "86", 2);
    dx->put("wzp.pl", 6, "17", 2);
    dx->put("wzzf.co", 7, "2", 1);
    dx->put("wzxx.co", 7, "5", 1);

    dx->put("Hello", 5, "World", 5);
    dx->put("Nice", 4, "Place", 5);

    dx->put("arrogant", 8, "35", 2);
    dx->put("arrogance", 9, "36", 2);
    dx->put("aroma", 5, "37", 2); // cmp_rel = 1, case 3
    dx->put("ape", 3, "38", 2); // cmp_rel = 2, case 1
    dx->put("arrange", 7, "39", 2); // cmp_rel = 1, case 2

    dx->put("single", 6, "24", 2);
    dx->put("singular", 8, "25", 2);
    dx->put("sine", 4, "27", 2); // cmp_rel = 2, case 4
    dx->put("shuteye", 7, "26", 2); // cmp_rel = 2, case 2
    dx->put("sick", 4, "28", 2); // cmp_rel = 1, case 1

    dx->put("inhabitation", 12, "19", 2);
    dx->put("inhabitant", 10, "20", 2);
    dx->put("inhibition", 10, "21", 2); // cmp_rel = 0, case 3
    dx->put("intent", 6, "22", 2); // cmp_rel = 0, case 4
    dx->put("inhale", 6, "23", 2); // cmp_rel = 0, case 2

    dx->put("yourself", 8, "30", 2);
    dx->put("yourselves", 10, "31", 2);
    dx->put("yousuf", 6, "32", 2); // cmp_rel = 2, case 3
    dx->put("yonder", 6, "33", 2); // cmp_rel = 1, case 4
    dx->put("yvette", 6, "34", 2); // cmp_rel = 0, case 1

    dx->put("resin", 5, "3", 1);
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

    dx->put("lastminutecarinsurancedeals.com", 31, "5668", 4);
    dx->put("youforgottorenewyourhosting.com", 31, "7680", 4);
    dx->put("xn--fiq53l90e917afrv.xn--fiqs8s", 31, "9561", 4);
    dx->put("genericsildenafilcitrateusa.com", 31, "12533", 5);
    dx->put("sildenafilonlinepharmacyusa.com", 31, "12608", 5);
    dx->put("christianlouboutinoutlet.com.co", 31, "13117", 5);
    dx->put("christianlouboutinoutlet.net.co", 31, "14732", 5);
    dx->put("meltzeraccountingtaxservice.com", 31, "15785", 5);
    dx->put("palestinesolidaritycampaign.com", 31, "15814", 5);
    dx->put("serviciointegraldeproyectos.com", 31, "17248", 5);
    dx->put("xn--rhq57s5w8ak9pmvf.xn--fiqs8s", 31, "17617", 5);
    dx->put("xn--wlqw1swrj75p8i1b.xn--fiqs8s", 31, "17637", 5);
    dx->put("xn--w9qq0rwzdb98dqqb.xn--fiqs8s", 31, "17724", 5);
    dx->put("xn--4gq4c290dklat89r.xn--fiqs8s", 31, "17761", 5);
    dx->put("xn--fiq73fnvbuz3e5ml.xn--fiqs8s", 31, "17770", 5);
    dx->put("xn--pss89jtpoqv9ac2d.xn--fiqs8s", 31, "17784", 5);
    dx->put("xn--w2xsoi54a3zbz85a.xn--fiqs8s", 31, "17810", 5);
    dx->put("xn--jvrw50bmntulxqua.xn--fiqs8s", 31, "17844", 5);
    dx->put("xn--vhqr0kjvfmp0cnce.xn--fiqs8s", 31, "17850", 5);
    dx->put("xn--pzsz6axqj76i11za.xn--fiqs8s", 31, "17864", 5);
    dx->put("xn--vsq68jfp8byfvt0a.xn--fiqs8s", 31, "17874", 5);
    dx->put("xn--viq48k2xcftdms1j.xn--fiqs8s", 31, "17923", 5);
    dx->put("xn--nfv35ar44brnrmgf.xn--fiqs8s", 31, "17954", 5);
    dx->put("xn--gmqr9jnzfr46aesx.xn--fiqs8s", 31, "17969", 5);
    dx->put("xn--wlq644avriuwq9ww.xn--fiqs8s", 31, "17977", 5);
    dx->put("xn--rhqu0vt3az3dd35c.xn--fiqs8s", 31, "17979", 5);
    dx->put("xn--tbt34wol4a8sdcxl.xn--fiqs8s", 31, "18000", 5);
    dx->put("xn--49s41u3xvlibw66c.xn--fiqs8s", 31, "18013", 5);
    dx->put("xn--kprp99c1wfz8vngo.xn--fiqs8s", 31, "18026", 5);
    dx->put("xn--z-6k4bn77a89ss9i.xn--fiqs8s", 31, "18033", 5);
    dx->put("xn--vhqd20yzukf0cwzb.xn--fiqs8s", 31, "18065", 5);
    dx->put("xn--v6qr1dq5gzrcw46f.xn--fiqs8s", 31, "18086", 5);
    dx->put("xn--ekr7b865arptsy0c.xn--fiqs8s", 31, "18101", 5);
    dx->put("xn--48sx1dw7wunan48n.xn--fiqs8s", 31, "18111", 5);
    dx->put("somethingsweetcandywrappers.com", 31, "20941", 5);
    dx->put("viagra6withoutprescription6.top", 31, "21002", 5);
    dx->put("gabrielandodonovansfunerals.com", 31, "21167", 5);
    dx->put("genericsildenafilviagrameds.com", 31, "21539", 5);
    dx->put("christian-louboutinshoes.com.co", 31, "21733", 5);
    dx->put("portcullispropertylawyers.co.uk", 31, "22083", 5);
    dx->put("datenschutzbeauftragter-info.de", 31, "24316", 5);
    dx->put("sildenafilcitrateviagrameds.com", 31, "24636", 5);
    dx->put("sustainablecitiescollective.com", 31, "25770", 5);
    dx->put("coachfactoryoutletonline.com.co", 31, "29395", 5);
    dx->put("xn--ooru3l66bewopi7d.xn--fiqs8s", 31, "29632", 5);
    dx->put("xn--fcs1bv97dkiam47a.xn--fiqs8s", 31, "29803", 5);
    dx->put("coachfactoryonlineoutlet.com.co", 31, "30185", 5);
    dx->put("cialistadalafillowest-price.com", 31, "30250", 5);
    dx->put("cialis-tadalafillowestprice.com", 31, "30515", 5);
    dx->put("digital-photography-school.com", 30, "4815", 4);
    dx->put("xn----8sbwgbwgd2ahr6g.xn--p1ai", 30, "8099", 4);
    dx->put("xn--09st2hjis03i62p.xn--fiqs8s", 30, "9613", 4);
    dx->put("insuranceofmiddletennessee.com", 30, "12106", 5);
    dx->put("igniteideasandinspirations.com", 30, "12588", 5);
    dx->put("sildenafilonlinepharmacyus.com", 30, "12887", 5);
    dx->put("genericsildenafilonlinewww.com", 30, "13430", 5);
    dx->put("wwwgenericsildenafilonline.com", 30, "13569", 5);
    dx->put("coachoutletstore-online.com.co", 30, "14492", 5);
    dx->put("wwwviagraonlinepharmacyusa.com", 30, "15784", 5);
    dx->put("louisvuitton-outlet-online.org", 30, "17241", 5);
    dx->put("xn--rhqy5t94sljpn2v.xn--fiqs8s", 30, "17608", 5);
    dx->put("xn--9kqp7incw25ov8m.xn--fiqs8s", 30, "17685", 5);
    dx->put("xn--zfvq32ahhv8ptzv.xn--fiqs8s", 30, "17747", 5);
    dx->put("xn--fct52f10hm3r1mw.xn--fiqs8s", 30, "17753", 5);
    dx->put("xn--fct52f30hhwj28i.xn--fiqs8s", 30, "17754", 5);
    dx->put("xn--8mry53h3a90gq8g.xn--fiqs8s", 30, "17819", 5);
    dx->put("xn--z4qs0xvstokk3gq.xn--fiqs8s", 30, "17949", 5);
    dx->put("xn--zcr16c427ca484z.xn--fiqs8s", 30, "17964", 5);
    dx->put("xn--tkv464fnb89xcxl.xn--fiqs8s", 30, "18002", 5);
    dx->put("xn--1lt2t40q3v2a3xx.xn--fiqs8s", 30, "18036", 5);
    dx->put("xn--m7r38vf9dp1ww9h.xn--fiqs8s", 30, "18060", 5);
    dx->put("xn--oms0s37c7xuo35c.xn--fiqs8s", 30, "18102", 5);
    dx->put("canadianonlinepharmacymeds.com", 30, "18799", 5);
    dx->put("sildenafilcitrateviagrawww.com", 30, "19735", 5);
    dx->put("michaelkors-outletonline.co.uk", 30, "20371", 5);
    dx->put("mulberry-handbagsoutlet.org.uk", 30, "21701", 5);
    dx->put("abercrombie-fitch-hollister.fr", 30, "21711", 5);
    dx->put("canadianonlinepharmacyhome.com", 30, "21794", 5);
    dx->put("footballcentralreflections.com", 30, "22580", 5);
    dx->put("www.turystyka.wrotapodlasia.pl", 30, "23247", 5);
    dx->put("canadianonlinepharmacysafe.com", 30, "23591", 5);
    dx->put("burberry-handbagsoutlet.com.co", 30, "23686", 5);
    dx->put("centralfloridaweddinggroup.com", 30, "23988", 5);
    dx->put("automotivedigitalmarketing.com", 30, "24306", 5);
    dx->put("coach-factoryyoutletonline.net", 30, "24469", 5);
    dx->put("paradiseboatrentalskeywest.com", 30, "24730", 5);
    dx->put("canadianonlinedrugstorewww.com", 30, "24822", 5);
    dx->put("yorkshirecottageservices.co.uk", 30, "25173", 5);
    dx->put("cheapnfljerseysonlinestore.com", 30, "27197", 5);
    dx->put("primemovergovernorservices.com", 30, "27386", 5);
    dx->put("michaelkorsoutletonline.com.co", 30, "27619", 5);
    dx->put("canadianonlinepharmacycare.com", 30, "28543", 5);
    dx->put("bestlowestpriceviagra100mg.com", 30, "28738", 5);
    dx->put("ristorantelafontevillapiana.it", 30, "29213", 5);
    dx->put("xn--wlqwmw8rs0lrs4c.xn--fiqs8s", 30, "29760", 5);
    dx->put("xn--fiqs8ssrh2zv92m.xn--fiqs8s", 30, "29832", 5);
    dx->put("canadianonlinepharmacyhere.com", 30, "29904", 5);
    dx->put("phoenixfeatherscalligraphy.com", 30, "29908", 5);
    dx->put("contentmarketinginstitute.com", 29, "4934", 4);
    dx->put("detail.chiebukuro.yahoo.co.jp", 29, "5627", 4);
    dx->put("informationclearinghouse.info", 29, "6743", 4);
    dx->put("suicidepreventionlifeline.org", 29, "7134", 4);
    dx->put("michaelkorshandbags-uk.org.uk", 29, "9042", 4);
    dx->put("hollister-abercrombiefitch.fr", 29, "9146", 4);
    dx->put("xn--czrx92avj3aruk.xn--fiqs8s", 29, "9556", 4);
    dx->put("xn--9krt00a31ab15f.xn--fiqs8s", 29, "9635", 4);
    dx->put("xn--chq51t4m0ajf2a.xn--fiqs8s", 29, "9693", 4);
    dx->put("cheapsildenafilcitrateusa.com", 29, "13037", 5);
    dx->put("genericonlineviagracanada.com", 29, "13730", 5);
    dx->put("louisvuitton-outlet-store.com", 29, "14187", 5);
    dx->put("canadianonlinepharmacywww.com", 29, "14246", 5);
    dx->put("genericviagraonlinecanada.net", 29, "14327", 5);
    dx->put("louis-vuitton-handbags.org.uk", 29, "15441", 5);
    dx->put("wwwgenericviagraonlineusa.com", 29, "15866", 5);
    dx->put("chaussurelouboutin-pascher.fr", 29, "16042", 5);
    dx->put("michael-kors-australia.com.au", 29, "16350", 5);
    dx->put("tommy-hilfiger-online-shop.de", 29, "16475", 5);
    dx->put("intranetcompass-mexico.com.mx", 29, "16708", 5);
    dx->put("mulberryhandbags-outlet.co.uk", 29, "17112", 5);
    dx->put("cheapnfljerseysstorechina.com", 29, "17441", 5);
    dx->put("xn--1lq90iiu2azf1b.xn--fiqs8s", 29, "17648", 5);
    dx->put("xn--b0tz14bjlf070a.xn--fiqs8s", 29, "17649", 5);
    dx->put("xn--7orv88cw7a551i.xn--fiqs8s", 29, "17750", 5);
    dx->put("xn--jvr951bd2h5i2a.xn--fiqs8s", 29, "17764", 5);
    dx->put("xn--jvrr31fczfyl1a.xn--fiqs8s", 29, "17871", 5);
    dx->put("xn--djro74bl9hqu6a.xn--fiqs8s", 29, "17884", 5);
    dx->put("xn--cpq868aqt8b3df.xn--fiqs8s", 29, "17891", 5);
    dx->put("xn--w9qt40a1nak39j.xn--fiqs8s", 29, "17896", 5);
    dx->put("xn--vuq96as62aph5d.xn--fiqs8s", 29, "17929", 5);
    dx->put("xn--fiqp93a8zsrr1a.xn--fiqs8s", 29, "17972", 5);
    dx->put("xn--tqqy82al5c2z8b.xn--fiqs8s", 29, "17975", 5);
    dx->put("xn--pad-ub9dn4u9uu.xn--fiqs8s", 29, "18034", 5);
    dx->put("xn--wlr572ag4a1z5e.xn--fiqs8s", 29, "18035", 5);
    dx->put("xglartesaniasecuatorianas.com", 29, "18559", 5);
    dx->put("ghd-hair-straighteners.org.uk", 29, "19470", 5);
    dx->put("chaussureslouboutin-soldes.fr", 29, "19939", 5);
    dx->put("universalstudioshollywood.com", 29, "20599", 5);
    dx->put("zitzlofftrainingresources.com", 29, "21098", 5);
    dx->put("www.rolexwatchesoutlet.us.com", 29, "21246", 5);
    dx->put("nike-mercurial-superfly.co.uk", 29, "21264", 5);
    dx->put("wwwcanadianonlinepharmacy.com", 29, "21499", 5);
    dx->put("michaelkors-outlet-online.net", 29, "21566", 5);
    dx->put("jornalfolhacondominios.com.br", 29, "21818", 5);
    dx->put("truereligionjeansoutlet.co.uk", 29, "22163", 5);
    dx->put("expressonlinepharmacymeds.com", 29, "22169", 5);
    dx->put("canadianpharmacyonlinewww.com", 29, "22730", 5);
    dx->put("getcarinsuranceratesonline.pw", 29, "23122", 5);
    dx->put("tiffanyjewellery-outlet.co.uk", 29, "23431", 5);
    dx->put("programmers.stackexchange.com", 29, "24017", 5);
    dx->put("topcanadianpharmacyonline.com", 29, "24237", 5);
    dx->put("homelandinsuranceservices.com", 29, "25388", 5);
    dx->put("designerhandbagsoutlet.net.co", 29, "25467", 5);
    dx->put("thenewcivilrightsmovement.com", 29, "25744", 5);
    dx->put("churchtelemessagingsystem.com", 29, "25894", 5);
    dx->put("alquilerdemontacargas7x24.com", 29, "26416", 5);
    dx->put("xn--80aaggsdegjfnhhi.xn--p1ai", 29, "26529", 5);
    dx->put("canadianwwwonlinepharmacy.com", 29, "27217", 5);
    dx->put("coachoutletstoreonline.com.co", 29, "27236", 5);
    dx->put("pittsburghsteelersjerseys.com", 29, "28319", 5);

    print(dx, "wwwl.co", 7);
    print(dx, "wwzw.co", 7);
    print(dx, "wx4.com", 7);
    print(dx, "wxcn.cc", 7);
    print(dx, "wxcs.cn", 7);
    print(dx, "wwwf.co", 7);
    print(dx, "wxjm.cc", 7);
    print(dx, "wxpx.co", 7);
    print(dx, "wxr.cc", 6);
    print(dx, "wx.com", 6);
    print(dx, "wxsj.co", 7);
    print(dx, "wxsf.co", 7);
    print(dx, "wxv.pl", 6);
    print(dx, "wxxx.co", 7);
    print(dx, "wxzp.co", 7);
    print(dx, "wxzq.co", 7);
    print(dx, "wy.edu", 6);
    print(dx, "wy.gov", 6);
    print(dx, "wy.us", 5);
    print(dx, "wy13.cn", 7);
    print(dx, "wycu.cc", 7);
    print(dx, "wygk.cn", 7);
    print(dx, "wyo.gov", 7);
    print(dx, "wyrc.co", 7);
    print(dx, "wyqc.co", 7);
    print(dx, "wysf.co", 7);
    print(dx, "wysp.ws", 7);
    print(dx, "wz07.cn", 7);
    print(dx, "wyww.co", 7);
    print(dx, "wzwp.pw", 7);
    print(dx, "wyw.it", 6);
    print(dx, "wxmr.co", 7);
    print(dx, "wxww.co", 7);
    print(dx, "wyzf.co", 7);
    print(dx, "wyw.cn", 6);
    print(dx, "wxrc.co", 7);
    print(dx, "wyzp.co", 7);
    print(dx, "wxas.cn", 7);
    print(dx, "wyzq.co", 7);
    print(dx, "wy.cn", 5);
    print(dx, "wyw.hu", 6);
    print(dx, "wzxn.cc", 7);
    print(dx, "wz.cz", 5);
    print(dx, "wxxw.co", 7);
    print(dx, "wz09.cn", 7);
    print(dx, "wz3.cc", 6);
    print(dx, "wxh.cc", 6);
    print(dx, "wz7.cc", 6);
    print(dx, "wyh.cc", 6);
    print(dx, "wzhr.co", 7);
    print(dx, "wzc.cn", 6);
    print(dx, "wzzz.co", 7);
    print(dx, "wylo.cn", 7);
    print(dx, "wzdr.cn", 7);
    print(dx, "wyyw.co", 7);
    print(dx, "wzrd.ru", 7);
    print(dx, "wzmr.co", 7);
    print(dx, "wzyy.co", 7);
    print(dx, "wzpy.cn", 7);
    print(dx, "wzzq.co", 7);
    print(dx, "wyy.cn", 6);
    print(dx, "wxwm.co", 7);
    print(dx, "wzb.eu", 6);
    print(dx, "wzsf.co", 7);
    print(dx, "wzsj.co", 7);
    print(dx, "wyec.cn", 7);
    print(dx, "wyfc.co", 7);
    print(dx, "wyjt.cc", 7);
    print(dx, "wzcu.cc", 7);
    print(dx, "wz08.cn", 7);
    print(dx, "wyw.ru", 6);
    print(dx, "wzsp.pl", 7);
    print(dx, "wxc.co", 6);
    print(dx, "wxfg.cn", 7);
    print(dx, "wyhr.co", 7);
    print(dx, "wzt8.cn", 7);
    print(dx, "wzcn.cn", 7);
    print(dx, "wztv.cn", 7);
    print(dx, "wz.sk", 5);
    print(dx, "wzww.co", 7);
    print(dx, "wxhr.co", 7);
    print(dx, "wzjn.cn", 7);
    print(dx, "wz.de", 5);
    print(dx, "wzta.cn", 7);
    print(dx, "wxs.nl", 6);
    print(dx, "wzyw.cn", 7);
    print(dx, "wysj.co", 7);
    print(dx, "wx4.cc", 6);
    print(dx, "wzp.pl", 6);
    print(dx, "wzzf.co", 7);
    print(dx, "wzxx.co", 7);

    print(dx, "Hello", 5);
    print(dx, "Nice", 4);

    print(dx, "arrogant", 8);
    print(dx, "arrogance", 9);
    print(dx, "aroma", 5);
    print(dx, "ape", 3);
    print(dx, "arrange", 7);

    print(dx, "single", 6);
    print(dx, "singular", 8);
    print(dx, "sine", 4);
    print(dx, "shuteye", 7);
    print(dx, "sick", 4);

    print(dx, "inhabitation", 12);
    print(dx, "inhabitant", 10);
    print(dx, "inhibition", 10);
    print(dx, "intent", 6);
    print(dx, "inhale", 6);

    print(dx, "yourself", 8);
    print(dx, "yourselves", 10);
    print(dx, "yousuf", 6);
    print(dx, "yonder", 6);
    print(dx, "yvette", 6);

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

    print(dx, "lastminutecarinsurancedeals.com", 31);
    print(dx, "youforgottorenewyourhosting.com", 31);
    print(dx, "xn--fiq53l90e917afrv.xn--fiqs8s", 31);
    print(dx, "genericsildenafilcitrateusa.com", 31);
    print(dx, "sildenafilonlinepharmacyusa.com", 31);
    print(dx, "christianlouboutinoutlet.com.co", 31);
    print(dx, "christianlouboutinoutlet.net.co", 31);
    print(dx, "meltzeraccountingtaxservice.com", 31);
    print(dx, "palestinesolidaritycampaign.com", 31);
    print(dx, "serviciointegraldeproyectos.com", 31);
    print(dx, "xn--rhq57s5w8ak9pmvf.xn--fiqs8s", 31);
    print(dx, "xn--wlqw1swrj75p8i1b.xn--fiqs8s", 31);
    print(dx, "xn--w9qq0rwzdb98dqqb.xn--fiqs8s", 31);
    print(dx, "xn--4gq4c290dklat89r.xn--fiqs8s", 31);
    print(dx, "xn--fiq73fnvbuz3e5ml.xn--fiqs8s", 31);
    print(dx, "xn--pss89jtpoqv9ac2d.xn--fiqs8s", 31);
    print(dx, "xn--w2xsoi54a3zbz85a.xn--fiqs8s", 31);
    print(dx, "xn--jvrw50bmntulxqua.xn--fiqs8s", 31);
    print(dx, "xn--vhqr0kjvfmp0cnce.xn--fiqs8s", 31);
    print(dx, "xn--pzsz6axqj76i11za.xn--fiqs8s", 31);
    print(dx, "xn--vsq68jfp8byfvt0a.xn--fiqs8s", 31);
    print(dx, "xn--viq48k2xcftdms1j.xn--fiqs8s", 31);
    print(dx, "xn--nfv35ar44brnrmgf.xn--fiqs8s", 31);
    print(dx, "xn--gmqr9jnzfr46aesx.xn--fiqs8s", 31);
    print(dx, "xn--wlq644avriuwq9ww.xn--fiqs8s", 31);
    print(dx, "xn--rhqu0vt3az3dd35c.xn--fiqs8s", 31);
    print(dx, "xn--tbt34wol4a8sdcxl.xn--fiqs8s", 31);
    print(dx, "xn--49s41u3xvlibw66c.xn--fiqs8s", 31);
    print(dx, "xn--kprp99c1wfz8vngo.xn--fiqs8s", 31);
    print(dx, "xn--z-6k4bn77a89ss9i.xn--fiqs8s", 31);
    print(dx, "xn--vhqd20yzukf0cwzb.xn--fiqs8s", 31);
    print(dx, "xn--v6qr1dq5gzrcw46f.xn--fiqs8s", 31);
    print(dx, "xn--ekr7b865arptsy0c.xn--fiqs8s", 31);
    print(dx, "xn--48sx1dw7wunan48n.xn--fiqs8s", 31);
    print(dx, "somethingsweetcandywrappers.com", 31);
    print(dx, "viagra6withoutprescription6.top", 31);
    print(dx, "gabrielandodonovansfunerals.com", 31);
    print(dx, "genericsildenafilviagrameds.com", 31);
    print(dx, "christian-louboutinshoes.com.co", 31);
    print(dx, "portcullispropertylawyers.co.uk", 31);
    print(dx, "datenschutzbeauftragter-info.de", 31);
    print(dx, "sildenafilcitrateviagrameds.com", 31);
    print(dx, "sustainablecitiescollective.com", 31);
    print(dx, "coachfactoryoutletonline.com.co", 31);
    print(dx, "xn--ooru3l66bewopi7d.xn--fiqs8s", 31);
    print(dx, "xn--fcs1bv97dkiam47a.xn--fiqs8s", 31);
    print(dx, "coachfactoryonlineoutlet.com.co", 31);
    print(dx, "cialistadalafillowest-price.com", 31);
    print(dx, "cialis-tadalafillowestprice.com", 31);
    print(dx, "digital-photography-school.com", 30);
    print(dx, "xn----8sbwgbwgd2ahr6g.xn--p1ai", 30);
    print(dx, "xn--09st2hjis03i62p.xn--fiqs8s", 30);
    print(dx, "insuranceofmiddletennessee.com", 30);
    print(dx, "igniteideasandinspirations.com", 30);
    print(dx, "sildenafilonlinepharmacyus.com", 30);
    print(dx, "genericsildenafilonlinewww.com", 30);
    print(dx, "wwwgenericsildenafilonline.com", 30);
    print(dx, "coachoutletstore-online.com.co", 30);
    print(dx, "wwwviagraonlinepharmacyusa.com", 30);
    print(dx, "louisvuitton-outlet-online.org", 30);
    print(dx, "xn--rhqy5t94sljpn2v.xn--fiqs8s", 30);
    print(dx, "xn--9kqp7incw25ov8m.xn--fiqs8s", 30);
    print(dx, "xn--zfvq32ahhv8ptzv.xn--fiqs8s", 30);
    print(dx, "xn--fct52f10hm3r1mw.xn--fiqs8s", 30);
    print(dx, "xn--fct52f30hhwj28i.xn--fiqs8s", 30);
    print(dx, "xn--8mry53h3a90gq8g.xn--fiqs8s", 30);
    print(dx, "xn--z4qs0xvstokk3gq.xn--fiqs8s", 30);
    print(dx, "xn--zcr16c427ca484z.xn--fiqs8s", 30);
    print(dx, "xn--tkv464fnb89xcxl.xn--fiqs8s", 30);
    print(dx, "xn--1lt2t40q3v2a3xx.xn--fiqs8s", 30);
    print(dx, "xn--m7r38vf9dp1ww9h.xn--fiqs8s", 30);
    print(dx, "xn--oms0s37c7xuo35c.xn--fiqs8s", 30);
    print(dx, "canadianonlinepharmacymeds.com", 30);
    print(dx, "sildenafilcitrateviagrawww.com", 30);
    print(dx, "michaelkors-outletonline.co.uk", 30);
    print(dx, "mulberry-handbagsoutlet.org.uk", 30);
    print(dx, "abercrombie-fitch-hollister.fr", 30);
    print(dx, "canadianonlinepharmacyhome.com", 30);
    print(dx, "footballcentralreflections.com", 30);
    print(dx, "www.turystyka.wrotapodlasia.pl", 30);
    print(dx, "canadianonlinepharmacysafe.com", 30);
    print(dx, "burberry-handbagsoutlet.com.co", 30);
    print(dx, "centralfloridaweddinggroup.com", 30);
    print(dx, "automotivedigitalmarketing.com", 30);
    print(dx, "coach-factoryyoutletonline.net", 30);
    print(dx, "paradiseboatrentalskeywest.com", 30);
    print(dx, "canadianonlinedrugstorewww.com", 30);
    print(dx, "yorkshirecottageservices.co.uk", 30);
    print(dx, "cheapnfljerseysonlinestore.com", 30);
    print(dx, "primemovergovernorservices.com", 30);
    print(dx, "michaelkorsoutletonline.com.co", 30);
    print(dx, "canadianonlinepharmacycare.com", 30);
    print(dx, "bestlowestpriceviagra100mg.com", 30);
    print(dx, "ristorantelafontevillapiana.it", 30);
    print(dx, "xn--wlqwmw8rs0lrs4c.xn--fiqs8s", 30);
    print(dx, "xn--fiqs8ssrh2zv92m.xn--fiqs8s", 30);
    print(dx, "canadianonlinepharmacyhere.com", 30);
    print(dx, "phoenixfeatherscalligraphy.com", 30);
    print(dx, "contentmarketinginstitute.com", 29);
    print(dx, "detail.chiebukuro.yahoo.co.jp", 29);
    print(dx, "informationclearinghouse.info", 29);
    print(dx, "suicidepreventionlifeline.org", 29);
    print(dx, "michaelkorshandbags-uk.org.uk", 29);
    print(dx, "hollister-abercrombiefitch.fr", 29);
    print(dx, "xn--czrx92avj3aruk.xn--fiqs8s", 29);
    print(dx, "xn--9krt00a31ab15f.xn--fiqs8s", 29);
    print(dx, "xn--chq51t4m0ajf2a.xn--fiqs8s", 29);
    print(dx, "cheapsildenafilcitrateusa.com", 29);
    print(dx, "genericonlineviagracanada.com", 29);
    print(dx, "louisvuitton-outlet-store.com", 29);
    print(dx, "canadianonlinepharmacywww.com", 29);
    print(dx, "genericviagraonlinecanada.net", 29);
    print(dx, "louis-vuitton-handbags.org.uk", 29);
    print(dx, "wwwgenericviagraonlineusa.com", 29);
    print(dx, "chaussurelouboutin-pascher.fr", 29);
    print(dx, "michael-kors-australia.com.au", 29);
    print(dx, "tommy-hilfiger-online-shop.de", 29);
    print(dx, "intranetcompass-mexico.com.mx", 29);
    print(dx, "mulberryhandbags-outlet.co.uk", 29);
    print(dx, "cheapnfljerseysstorechina.com", 29);
    print(dx, "xn--1lq90iiu2azf1b.xn--fiqs8s", 29);
    print(dx, "xn--b0tz14bjlf070a.xn--fiqs8s", 29);
    print(dx, "xn--7orv88cw7a551i.xn--fiqs8s", 29);
    print(dx, "xn--jvr951bd2h5i2a.xn--fiqs8s", 29);
    print(dx, "xn--jvrr31fczfyl1a.xn--fiqs8s", 29);
    print(dx, "xn--djro74bl9hqu6a.xn--fiqs8s", 29);
    print(dx, "xn--cpq868aqt8b3df.xn--fiqs8s", 29);
    print(dx, "xn--w9qt40a1nak39j.xn--fiqs8s", 29);
    print(dx, "xn--vuq96as62aph5d.xn--fiqs8s", 29);
    print(dx, "xn--fiqp93a8zsrr1a.xn--fiqs8s", 29);
    print(dx, "xn--tqqy82al5c2z8b.xn--fiqs8s", 29);
    print(dx, "xn--pad-ub9dn4u9uu.xn--fiqs8s", 29);
    print(dx, "xn--wlr572ag4a1z5e.xn--fiqs8s", 29);
    print(dx, "xglartesaniasecuatorianas.com", 29);
    print(dx, "ghd-hair-straighteners.org.uk", 29);
    print(dx, "chaussureslouboutin-soldes.fr", 29);
    print(dx, "universalstudioshollywood.com", 29);
    print(dx, "zitzlofftrainingresources.com", 29);
    print(dx, "www.rolexwatchesoutlet.us.com", 29);
    print(dx, "nike-mercurial-superfly.co.uk", 29);
    print(dx, "wwwcanadianonlinepharmacy.com", 29);
    print(dx, "michaelkors-outlet-online.net", 29);
    print(dx, "jornalfolhacondominios.com.br", 29);
    print(dx, "truereligionjeansoutlet.co.uk", 29);
    print(dx, "expressonlinepharmacymeds.com", 29);
    print(dx, "canadianpharmacyonlinewww.com", 29);
    print(dx, "getcarinsuranceratesonline.pw", 29);
    print(dx, "tiffanyjewellery-outlet.co.uk", 29);
    print(dx, "programmers.stackexchange.com", 29);
    print(dx, "topcanadianpharmacyonline.com", 29);
    print(dx, "homelandinsuranceservices.com", 29);
    print(dx, "designerhandbagsoutlet.net.co", 29);
    print(dx, "thenewcivilrightsmovement.com", 29);
    print(dx, "churchtelemessagingsystem.com", 29);
    print(dx, "alquilerdemontacargas7x24.com", 29);
    print(dx, "xn--80aaggsdegjfnhhi.xn--p1ai", 29);
    print(dx, "canadianwwwonlinepharmacy.com", 29);
    print(dx, "coachoutletstoreonline.com.co", 29);
    print(dx, "pittsburghsteelersjerseys.com", 29);

    dx->print_stats(24);
    dx->print_num_levels();
    cout << "Trie size: " << (int) dx->get_trie_len() << endl;
    cout << "Filled size: " << (int) dx->filled_size() << endl;
    return 0;
}

int main3() {
    int num_keys = 10;
    util::generate_bit_counts();
    //basix *dx = new basix();
    //dfqx *dx = new dfqx();
    //dfqx *dx = new dfqx();
    bfos *dx = new bfos();
    char k[5] = "\001\001\001\000";
    for (int i = 1; i < num_keys; i++) {
        char v[5];
        k[3] = i;
        sprintf(v, "%d", i);
        dx->put(k, 4, v, strlen(v));
    }
    for (int i = 1; i < num_keys; i++) {
        char v[5];
        k[3] = i;
        sprintf(v, "%d", i);
        //print(dx, (char *) k, 4);
    }
    return 0;
}

int main4() {
    util::generate_bit_counts();
    //basix *dx = new basix();
    basix3 *dx = new basix3(512, 512);
    //octp *dx = new octp();
    //bft *dx = new bft();
    //dft *dx = new dft();
    //rb_tree *dx = new rb_tree();
    //dfqx *dx = new dfqx();
    //linex *dx = new linex();

    dx->put("skasilet", 8, "saks", 4);
    dx->put("cjkantxy", 8, "akjc", 4);
    dx->put("xiwzmriu", 8, "zwix", 4);
    dx->put("fzfbbvgs", 8, "bfzf", 4);
    dx->put("gorfakxm", 8, "frog", 4);
    dx->put("elggdrfy", 8, "ggle", 4);
    dx->put("lqzrwrwi", 8, "rzql", 4);
    dx->put("hprciake", 8, "crph", 4);
    dx->put("avjfrjzx", 8, "fjva", 4);
    dx->put("jjdwlwke", 8, "wdjj", 4);
    dx->put("pgdnwlcz", 8, "ndgp", 4);
    dx->put("npqdrzvv", 8, "dqpn", 4);
    dx->put("qqtfndgo", 8, "ftqq", 4);
    dx->put("bhmeaulq", 8, "emhb", 4);
    dx->put("slaxkang", 8, "xals", 4);
    dx->put("hwcnzlti", 8, "ncwh", 4);
    dx->put("ousjtkbv", 8, "jsuo", 4);
    dx->put("mvtizdnf", 8, "itvm", 4);
    dx->put("jdrgtwmg", 8, "grdj", 4);
    dx->put("llvjhpfe", 8, "jvll", 4);
    dx->put("rpxnjjno", 8, "nxpr", 4);
    dx->put("bsymdbsf", 8, "mysb", 4);
    dx->put("kyduqsmw", 8, "udyk", 4);
    dx->put("ohkapuiy", 8, "akho", 4);
    dx->put("ydgkyyty", 8, "kgdy", 4);
    dx->put("pdmnicaz", 8, "nmdp", 4);
    dx->put("fejfuhdj", 8, "fjef", 4);
    dx->put("dhcwohyb", 8, "wchd", 4);
    dx->put("dthoheek", 8, "ohtd", 4);
    dx->put("xdchcpim", 8, "hcdx", 4);
    dx->put("tybnhnms", 8, "nbyt", 4);
    dx->put("nqdofydp", 8, "odqn", 4);
    dx->put("xgrdxckx", 8, "drgx", 4);
    dx->put("ulmppvgs", 8, "pmlu", 4);
    dx->put("djoddccd", 8, "dojd", 4);
    dx->put("avlreioo", 8, "rlva", 4);
    dx->put("istsewde", 8, "stsi", 4);
    dx->put("boiaokzx", 8, "aiob", 4);
    dx->put("fnpouwdj", 8, "opnf", 4);
    dx->put("qrzhmttw", 8, "hzrq", 4);
    dx->put("wnygwvqd", 8, "gynw", 4);
    dx->put("mjkpujqd", 8, "pkjm", 4);

    print(dx, "skasilet", 8);
    print(dx, "cjkantxy", 8);
    print(dx, "xiwzmriu", 8);
    print(dx, "fzfbbvgs", 8);
    print(dx, "gorfakxm", 8);
    print(dx, "elggdrfy", 8);
    print(dx, "lqzrwrwi", 8);
    print(dx, "hprciake", 8);
    print(dx, "avjfrjzx", 8);
    print(dx, "jjdwlwke", 8);
    print(dx, "pgdnwlcz", 8);
    print(dx, "npqdrzvv", 8);
    print(dx, "qqtfndgo", 8);
    print(dx, "bhmeaulq", 8);
    print(dx, "slaxkang", 8);
    print(dx, "hwcnzlti", 8);
    print(dx, "ousjtkbv", 8);
    print(dx, "mvtizdnf", 8);
    print(dx, "jdrgtwmg", 8);
    print(dx, "llvjhpfe", 8);
    print(dx, "rpxnjjno", 8);
    print(dx, "bsymdbsf", 8);
    print(dx, "kyduqsmw", 8);
    print(dx, "ohkapuiy", 8);
    print(dx, "ydgkyyty", 8);
    print(dx, "pdmnicaz", 8);
    print(dx, "fejfuhdj", 8);
    print(dx, "dhcwohyb", 8);
    print(dx, "dthoheek", 8);
    print(dx, "xdchcpim", 8);
    print(dx, "tybnhnms", 8);
    print(dx, "nqdofydp", 8);
    print(dx, "xgrdxckx", 8);
    print(dx, "ulmppvgs", 8);
    print(dx, "djoddccd", 8);
    print(dx, "avlreioo", 8);
    print(dx, "istsewde", 8);
    print(dx, "boiaokzx", 8);
    print(dx, "fnpouwdj", 8);
    print(dx, "qrzhmttw", 8);
    print(dx, "wnygwvqd", 8);
    print(dx, "mjkpujqd", 8);

    dx->print_stats(NUM_ENTRIES);
    dx->print_num_levels();
    std::cout << "Size:" << dx->size() << endl;
    //std::cout << "Trie Size:" << (int) dx->get_trie_len() << endl;
    return 0;
}

int main6() {
    art_tree at;
    art_tree_init(&at);
    art_insert(&at, (const uint8_t *) "arun", 5, (void *) "Hello");
    art_insert(&at, (const uint8_t *) "aruna", 6, (void *) "World");
    //art_insert(&at, (const uint8_t *) "absorb", 6, (void *) "ART", 5);
    int len;
    cout << (char*) art_search(&at, (uint8_t *) "arun", 5) << endl;
    cout << (char*) art_search(&at, (uint8_t *) "aruna", 6) << endl;
    //art_search(&at, (uint8_t *) "absorb", 6, &len);
    return 1;
}

int main7() {
    util::generate_bit_counts();
    //basix *dx = new basix(512, 512, 4, "test.ix");
    bfos *dx = new bfos(512, 512, 2, "test.ix");
    //octp *dx = new octp();
    //bft *dx = new bft();
    //dft *dx = new dft();
    //rb_tree *dx = new rb_tree();
    //dfqx *dx = new dfqx();
    //linex *dx = new linex();

    dx->put("Hello", 5, "World", 5);
    dx->put("Nice", 4, "Place", 5);

    print(dx, "Hello", 5);
    print(dx, "Nice", 4);

    delete dx;

    dx->print_stats(NUM_ENTRIES);
    dx->print_num_levels();
    std::cout << "Size:" << dx->size() << endl;
    std::cout << "Trie Size:" << (int) dx->get_trie_len() << endl;
    return 0;
}

void check_value(const char *key, int key_len, const char *val, int val_len,
        const char *returned_value, int returned_len, int& null_ctr, int& cmp) {
    if (returned_value == null) {
        null_ctr++;
    } else {
        int d = util::compare((const uint8_t *) val, val_len, (const uint8_t *) returned_value, returned_len);
        if (d != 0) {
            cmp++;
            printf("cmp: %.*s==========%.*s--------->%.*s\n", key_len, key, val_len, val, returned_len, returned_value);
            //cout << cmp << ":" << (char *) key << "=========="
            //        << val << "----------->" << returned_value << endl;
        }
    }
}

int test_sqlite(const char *file_name) {
    sqlite *lx = new sqlite(2, 1, "key, value", "imain", LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, 10, file_name);
    int val_len = 2000;
    char val[2000];
    if (lx->get("wfdkbhtmlhsntkkvheyqqheajmlppdghlqeuafxr", 40, &val_len, val)) {
        val[val_len] = 0;
        cout << "Val: [" << val << "]" << endl;
    }
    delete lx;
    return 0;
}

int test(const char *file_name) {
    //bfos *lx = new bfos(32768, 32768);
    //bft *lx = new bft(32768, 32768);
    //bft *lx = new bft(512, 512);
    //dfos *lx = new dfos(512, 512);
    bfox *lx = new bfox(32768, 32768);
    //sqlite *lx = new sqlite(2, 1, "key, value", "imain", LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, 10, file_name);
    int val_len = 4;
    char val[200];
    const char *data[] = {"bon", "bon"
                        , "January", "good"
                        , "good", "private"
                        , "price", "prose"
                        , "private", "reality"
                        , "pea", "real"
                        , "prose", "rinse"
                        , "really", "ride"
                        , "reality", "rid"
                        , "realise", "rick"
                        , "real", "hide"
                        , "resin", "hello"
                        , "rinse", "March"
                        , "rickshaw", "May"
                        , "ride", "July"
                        , "rider", "September"
                        , "rid", "November"
                        , "rice", "arrogant"
                        , "rick", "aroma"
                        , "hike", "arrange"
                        , "hide", "singular"
                        , "hi", "shuteye"
                        , "hello", "inhabitation"
                        , "February", "inhibition"
                        , "March", "inhale"
                        , "April", "yourselves"
                        , "May", "yonder"
                        , "June", "aruna"
                        , "July", "world"
                        , "August", "are"
                        , "September", "hundred"
                        , "October", "boat"
                        , "November", "buoy"
                        , "December", "Sunday"
                        , "arrogant", "Tuesday"
                        , "arrogance", "Thursday"
                        , "aroma", "Saturday"
                        , "ape", "young"
                        , "arrange", "additional"
                        , "single", "add"
                        , "singular", "act"
                        , "sine", "agreement"
                        , "shuteye", "aircraft"
                        , "sick", "adequate"
                        , "inhabitation", "accident"
                        , "inhabitant", "abroad"
                        , "inhibition", "aim"
                        , "intent", "afterwards"
                        , "inhale", "advertise"
                        , "yourself", "advert"
                        , "yourselves", "acid"
                        , "yousuf", "advanced"
                        , "yonder", "admire"
                        , "yvette", "aggressive"
                        , "aruna", "access"
                        , "Hello", "afford"
                        , "world", "ability"
                        , "how", "advertising"
                        , "are", "action"
                        , "you", "account"
                        , "hundred", "accompany"
                        , "boy", "ago"
                        , "boat", "alarmed"
                        , "thousand", "accurately"
                        , "buoy", "a"
                        , "boast", "advantage"
                        , "Sunday", "actual"
                        , "Monday", "actress"
                        , "Tuesday", "able"
                        , "Wednesday", "airport"
                        , "Thursday", "across"
                        , "Friday", "advance"
                        , "Saturday", "adult"
                        , "casa", "aged"
                        , "young", "admiration"
                        , "youth", "adjust"
                        , "additional", "about"
                        , "absorb", "after"
                        , "add", "accent"
                        , "accuse", "admit"
                        , "act", "abandon"
                        , "adopt", "affection"
                        , "agreement", "adventure"
                        , "actively", "advise"
                        , "aircraft", "achievement"
                        , "agree", "bad"
                        , "adequate", "bij"
                        , "afraid", "cut"
                        , "accident", "beste"
                        , "activity", "bin"
                        , "abroad", "borscht"
                        , "acquire", "chaschs"
                        , "aim", "baba"
                        , "absence", "bang"
                        , "afterwards", "being"
                        , "advice", "black"
                        , "advertise", "botrk"
                        , "advertisement", "briskly"
                        , "advert", "building"
                        , "absolute", "but"
                        , "acid", "chamber"
                        , "address", "cleaver"
                        , "advanced", "closely"
                        , "absolutely", "consider"
                        , "admire", "cousins"
                        , "agency", "cspx"
                        , "aggressive", "cuz"
                        , "affair", "center"
                        , "access", "color"
                        , "afternoon", "baban"
                        , "afford", "ban"
                        , "acceptable", "bay"
                        , "ability", "baba"
                        , "again", "bile"
                        , "advertising", "bir"
                        , "accidental", "biri"
                        , "action", "boş"
                        , "academic", "bunu"
                        , "account", "bəs"
                        , "above", "bəyənmirəm"
                        , "accompany", "commentdə"
                        , "adapt", "country"
                        , "ago", "cəhətdən"
                        , "actually", "black"
                        , "alarmed", "borderlands"
                        , "abuse", "btw"
                        , "accurately", "care"
                        , "accurate", "chess"
                        , "a", "com"
                        , "ahead", "counter"
                        , "advantage", "cure"
                        , "accommodation", "behind"
                        , "actual", "bhai"
                        , "acknowledge", "country"
                        , "actress", "berit"
                        , "abandoned", "bohot"
                        , "able", "c'mon"
                        , "absent", "chalte"
                        , "airport", "bencin"
                        , "against", "bolje"
                        , "across", "barrier"
                        , "aid", "bias"
                        , "advance", "bingo"
                        , "accept", "bitxh"
                        , "adult", "blood"
                        , "ad", "bola"
                        , "aged", "but"
                        , "alarm", "cada"
                        , "admiration", "calls"
                        , "affect", "cantat"
                        , "adjust", "caram"
                        , "accidentally", "cat"
                        , "about", "català"
                        , "actor", "child"
                        , "after", "clues"
                        , "air", "collier's"
                        , "accent", "complicado"
                        , "agent", "compraré"
                        , "admit", "comtacs"
                        , "adequately", "continue"
                        , "abandon", "country"
                        , "addition", "crec"
                        , "affection", "cutemouse"
                        , "achieve", "b00m4pxp9m"
                        , "adventure", "badtrip"
                        , "active", "baldurs"
                        , "advise", "ban"
                        , "age", "bato"
                        , "achievement", "beaten"
                        , "baar", "becoming"
                        , "bad", "benefits"
                        , "begter", "better"
                        , "bij", "bhenchod"
                        , "blourp", "blazin'"
                        , "cut", "blogs"
                        , "bern", "bophadese"
                        , "beste", "bothhh"
                        , "biker", "bridge"
                        , "bin", "bruhgpanther"
                        , "bisher", "buhay"
                        , "borscht", "call"
                        , "chas", "cameras"
                        , "chaschs", "candy"
                        , "chli", "capitalise"
                        , "baba", "cash"
                        , "bait", "caves"
                        , "bang", "cci"
                        , "barakatuh", "certainly"
                        , "being", "channel"
                        , "between", "child"
                        , "black", "chunk"
                        , "bless", "claveria"
                        , "botrk", "cobblestone"
                        , "braging", "combat"
                        , "briskly", "commonly"
                        , "bro", "confidence"
                        , "building", "congested"
                        , "bukhari", "connect"
                        , "but", "constitution"
                        , "call", "conversation"
                        , "chamber", "counselor"
                        , "clearly", "courts"
                        , "cleaver", "cowabunga"
                        , "click", "creatures"
                        , "closely", "crisistextline"
                        , "computers", "cult"
                        , "consider", "curing"
                        , "counter-clockwise", "babiš"
                        , "cousins", "barevný"
                        , "cover", "batch"
                        , "cspx", "bateriÍ"
                        , "cube-shaped", "bavit"
                        , "cuz", "bdrstv"
                        , "brahma", "became"
                        , "center", "becherovku"
                        , "cib", "bejt"
                        , "color", "bekr's"
                        , "baban", "beton"
                        , "bacarmayıb", "bily"
                        , "ban", "bitva"
                        , "bax", "blbej"
                        , "bayılmışsındır", "blow"
                        , "başqa", "bludů"
                        , "başına", "bodu"
                        , "belə", "bodů"
                        , "bile", "bohatí"
                        , "bilmərəm", "bohužel"
                        , "bir", "bojí"
                        , "biraz", "bonai"
                        , "biri", "bordel"
                        , "bombası", "boty"
                        , "boş", "brala"
                        , "bugünə", "braník"
                        , "bunu", "brother"
                        , "bütün", "brutál"
                        , "bəs", "buck"
                        , "bəyənmirlər", "budeme"
                        , "bəyənmirəm", "budou"
                        , "cavab", "bug"
                        , "commentdə", "buranském"
                        , "contraversial", "but"
                        , "country", "bych"
                        , "cəbhəcilər", "bydlet"
                        , "cəhətdən", "byla"
                        , "better", "bylo"
                        , "blizzard", "byty"
                        , "borderlands", "bába"
                        , "brain", "bílkoviny"
                        , "btw", "být"
                        , "cancer", "canonem"
                        , "care", "cara"
                        , "chaos", "cases"
                        , "chess", "cca"
                        , "collectors", "celej"
                        , "com", "celou"
                        , "competition", "celý"
                        , "counter", "cenu"
                        , "creed", "ceskenoviny"
                        , "cure", "cestování"
                        , "because", "charge"
                        , "behind", "chceš"
                        , "bhai", "china"
                        , "comfort", "chodil"
                        , "crap", "chroničtí"
                        , "berit", "chtěj"
                        , "bet", "chtěla"
                        , "bohot", "chudobou"
                        , "c'mon", "chybí"
                        , "chalta", "cim"
                        , "chalte", "citroën"
                        , "cop", "clanek"
                        , "bencin", "cmll"
                        , "bil", "cokoliv"
                        , "bolje", "comes"
                        , "brat", "condo"
                        , "barrier", "contains"
                        , "berapa", "corn"
                        , "bias", "covidem"
                        , "bigger", "což"
                        , "bingo", "cranky"
                        , "bio", "cvoka"
                        , "bitxh", "cyklů"
                        , "bitxhes", "bah"
                        , "blood", "become"
                        , "bloody", "being"
                        , "bola", "bill"
                        , "bullying", "bo'o"
                        , "butt", "can"
                        , "cada", "collaborative"
                        , "calls", "concrete"
                        , "cambrera", "cracking"
                        , "cantat", "cross"
                        , "cap", "cyma"
                        , "caram", "cymru"
                        , "cas", "baby"
                        , "cat", "bachelor"
                        , "catalunya", "bacon"
                        , "català", "bae"
                        , "chad", "bagefter"
                        , "child", "baggrunden"
                        , "citar", "baggård"
                        , "clues", "bagud"
                        , "collects", "bait"
                        , "collier's", "balderet"
                        , "comercials", "bamses"
                        , "complicado", "bands"
                        , "comprar-los", "baner"
                        , "compraré", "bank"
                        , "comprensible", "banker"
                        , "comtacs", "bankforhold"
                        , "comtext", "banned"
                        , "continue", "barberet"
                        , "conventional", "barn"
                        , "counts", "barnlig"
                        , "crec", "barsel"
                        , "creus", "base"
                        , "cutemouse", "basere"
                        , "bicho", "baseret"
                        , "b00m4pxp9m", "basically"
                        , "b7ar", "batterier"
                        , "badtrip", "bazar"
                        , "bakuna", "bebyggelse"
                        , "baldurs", "bede"
                        , "baliw", "bedrag"
                        , "bathroom", "bedsteforældre"
                        , "bato", "bedt"
                        , "beat", "befolkning"
                        , "beaten", "begge"
                        , "become", "begrundelse"
                        , "becoming", "begrænset"
                        , "benefits", "begyndt"
                        , "bent", "begynner"
                        , "bgc", "behandles"
                        , "bhenchod", "behandlingen"
                        , "bisaya", "behold"
                        , "blazin'", "behov"
                        , "blog", "behåret"
                        , "blogs", "behøver"
                        , "books", "bekomme"
                        , "bophadese", "bekræfte"
                        , "both", "bekymrende"
                        , "bothhh", "bekymret"
                        , "break", "bekymringer"
                        , "bridge", "bella"
                        , "bridgestorecovery", "beløb"
                        , "bruhgpanther", "bemærket"
                        , "buff", "benny"
                        , "buhay", "beregnede"
                        , "came", "besidde"
                        , "cameras", "besitter"
                        , "camp", "beskeder"
                        , "candy", "beskraldet"
                        , "cannot", "beskrivelse"
                        , "capitalise", "beskriver"
                        , "card", "beskyttelsen"
                        , "cash", "beskyttes"
                        , "catching", "beslutninger"
                        , "caves", "beste"
                        , "cbt", "bestemmelserne"
                        , "cci", "bestemt"
                        , "cebu", "bestilles"
                        , "certainly", "bestå"
                        , "chance", "består"
                        , "channel", "besvarelse"
                        , "charisma", "besøg"
                        , "choufou", "betaling"
                        , "chunk", "betalingsmulighed"
                        , "clarification", "betalt"
                        , "claveria", "betegnelse"
                        , "closure", "betegnet"
                        , "cobblestone", "betjente"
                        , "code", "betragtede"
                        , "combat", "betragtet"
                        , "come", "betrække"
                        , "commonly", "bette"
                        , "compared", "betyde"
                        , "confidence", "betydning"
                        , "confirmed", "bevares"
                        , "congested", "bevis"
                        , "conjunction", "bevise"
                        , "connect", "bevist"
                        , "connected", "bevæge"
                        , "constitution", "bevæger"
                        , "consular", "bevæggrunde"
                        , "conversation", "bidrag"
                        , "cost", "bidragende"
                        , "counselor", "big"
                        , "count", "bil"
                        , "courts", "bilen"
                        , "covid", "bilister"
                        , "cowabunga", "billeder"
                        , "crafting_shaped", "billet"
                        , "creatures", "billig"
                        , "crisis", "billigste"
                        , "crisistextline", "bimodalfordeling"
                        , "cryo", "biograf"
                        , "cult", "biologien"
                        , "cups", "biologiske"
                        , "curing", "bit"
                        , "custom", "bitte"
                        , "babiš", "bizart"
                        , "banda", "blandt"
                        , "barevný", "blatant"
                        , "basics", "blevet"
                        , "batch", "blindt"
                        , "baterie", "bliv"
                        , "bateriÍ", "bliver"
                        , "batteries", "blokering"
                        , "bavit", "blomsterne"
                        , "bavíme", "blottere"
                        , "bdrstv", "blå"
                        , "became", "bobler"
                        , "becherovku", "bodylab"
                        , "been", "bodyshame"
                        , "bejt", "bodystocking"
                        , "bekr", "boede"
                        , "bekr's", "boet"
                        , "benevolentní", "bogstaveligt"
                        , "bereš", "boks"
                        , "berou", "boldgade"
                        , "beton", "boliger"
                        , "bezďáci", "boligmangel"
                        , "bily", "boligproblemet"
                        , "birthdays", "bondegårdsferie"
                        , "bitva", "booke"
                        , "blbce", "bopæl"
                        , "blbej", "bord"
                        , "blbě", "borger"
                        , "blow", "borgeren"
                        , "bludy", "borgernes"
                        , "bludů", "borislaursen"
                        , "bmw", "borte"
                        , "bodu", "bosat"
                        , "body", "boston"
                        , "bodů", "botox"
                        , "bohaté", "bransjen"
                        , "bohatí", "bremse"
                        , "bohatší", "brevsprækken"
                        , "bohužel", "bro"
                        , "bojujeme", "broer"
                        , "bojí", "brokke"
                        , "bolševici", "bror"
                        , "bonai", "brt"
                        , "borcem", "bruden"
                        , "bordel", "brudt"
                        , "botchy", "bruge"
                        , "boty", "brugere"
                        , "boško", "brugerne"
                        , "brala", "brugt"
                        , "branik", "bruhhhh"
                        , "braník", "bruke"
                        , "brez", "bruuns"
                        , "brother", "bryder"
                        , "bruh", "bryllup"
                        , "brutál", "bryst"
                        , "brýlaté", "bræk"
                        , "buck", "brætspil"
                        , "bude", "bud"
                        , "budeme", "budskabet"
                        , "budeš", "build"
                        , "budou", "bukke"
                        , "budu", "bukserne"
                        , "bug", "buler"
                        , "burani", "bulk"
                        , "buranském", "bumle"
                        , "business", "bump"
                        , "buď", "bursts"
                        , "bych", "bussen"
                        , "bydlení", "butik"
                        , "bydlet", "butikkerne"
                        , "byl", "butthurt"
                        , "byla", "bweeeerrrrrrruhhhher"
                        , "byli", "byer"
                        , "bylo", "byggemarkedernes"
                        , "byly", "bygger"
                        , "byt", "bygget"
                        , "bytu", "bymidten"
                        , "byty", "bypass"
                        , "byznys", "byrum"
                        , "bába", "bytta"
                        , "bájo", "byzonen"
                        , "bílkoviny", "bålsted"
                        , "bílé", "båse"
                        , "být", "bænk"
                        , "cakes", "bænkene"
                        , "canonem", "bødeforlægget"
                        , "canounu", "bøder"
                        , "cara", "bølgepap"
                        , "caristico", "bør"
                        , "cases", "børn"
                        , "cause", "børnene"
                        , "cca", "børnesweater"
                        , "cdc", "caféer"
                        , "celej", "can't"
                        , "celkem", "cannibal"
                        , "celou", "carlsberg"
                        , "celéjo", "cashmere"
                        , "celý", "café"
                        , "cena", "cathrine"
                        , "cenu", "cbc"
                        , "cením", "cbs-typerne"
                        , "ceskenoviny", "celex"
                        , "cesta", "celofan"
                        , "cestování", "censur"
                        , "cestu", "censureret"
                        , "charge", "chance"
                        , "chce", "chat"
                        , "chceš", "checke"
                        , "chci", "chef"
                        , "cheng-morality", "chefer"
                        , "chief", "chill"
                        , "china", "chrishell"
                        , "chlubím", "christine"
                        , "chodil", "cirka"
                        , "chodili", "cis-trans"
                        , "chodí", "ciskøn"
                        , "chromový", "ciskønnet"
                        , "chroničtí", "civiliseret"
                        , "chrání", "clairvoyanter"
                        , "chtěj", "cmon"
                        , "chtěl", "cold"
                        , "chtěla", "comments"
                        , "chtělo", "computerarm"
                        , "chudobou", "computerspilnørd"
                        , "chudáky", "contender"
                        , "chudí", "cool"
                        , "chvíli", "corolla"
                        , "chybí", "coronalån"
                        , "chřipečka", "coronatiden"
                        , "cim", "cream"
                        , "cinta", "creepy"
                        , "citroën", "crispy"
                        , "cizí", "crusher"
                        , "clanek", "csgo"
                        , "clarify", "curious"
                        , "cmll", "current"
                        , "coincindences", "family"
                        , "cokoliv", "rahmatullahi"
                        , "colour", "race"
                        , "comes", "raditi"
                        , "compare", "facebook"
                        , "condo", "favorite"
                        , "conrad", "radium"
                        , "contains", "fact"
                        , "copak", "faith"
                        , "corn", "far"
                        , "covid-19", "rally"
                        , "covidem", "rate"
                        , "covidu", "rather"
                        , "což", "fabie"
                        , "co☺", "facebook"
                        , "cranky", "faith"
                        , "cute", "fakt"
                        , "cvoka", "faktor"
                        , "cykl", "fanoušci"
                        , "cyklů", "fastflash"
                        , "czech", "radost"
                        , "bah", "rainer"
                        , "basis", "raver"
                        , "believe", "facon"
                        , "bill", "facts"
                        , "bills", "fadøl"
                        , "bo'o", "fagbevægelsen"
                        , "bring", "fagforbund"
                        , "built", "fags"
                        , "bureaucracy", "fair"
                        , "can", "faktisk"
                        , "collaborations", "faktum"
                        , "collaborative", "falde"
                        , "commend", "faldet"
                        , "commissioner", "faldt"
                        , "commissions", "familiebesøg"
                        , "concrete", "familien"
                        , "country's", "fan"
                        , "cracking", "fandeme"
                        , "creative", "fandme"
                        , "cross", "fange"
                        , "currently", "fanget"
                        , "cyma", "fant"
                        , "cymraeg", "far"
                        , "cymru", "faren"
                        , "babe", "farligere"
                        , "baby", "farm"
                        , "babyface", "fart"
                        , "bachelor", "farver"
                        , "back", "fascinerer"
                        , "bacon", "fast"
                        , "badekaret", "fastlåse"
                        , "bae", "fatsvage"
                        , "bag", "fattet"
                        , "bagefter", "favorabel"
                        , "baggrund", "rabat"
                        , "baggrunden", "racercykel"
                        , "baggrundshistorien", "radius"
                        , "baggård", "ramme"
                        , "baghjul", "ramt"
                        , "bagud", "random"
                        , "bagvedliggende", "ranker"
                        , "bakke", "raps"
                        , "balderet", "rapsolie"
                        , "ballade", "rart"
                        , "bamses", "rask"
                        , "banalt", "raskt"
                        , "bands", "rationalt"
                        , "banen", "rawanda"
                        , "baner", "fab"
                        , "bange", "face"
                        , "bank", "facelift"
                        , "banken", "facettenreich"
                        , "banker", "fachabi"
                        , "bankes", "fachartzlicher"
                        , "bankforhold", "fachbereich"
                        , "banklånet", "fachfremd"
                        , "banned", "fachgeschäften"
                        , "bar", "fachinformatiker"
                        , "barberet", "fachkräfte"
                        , "bare", "fachlich"
                        , "barn", "fachmann"
                        , "barndom", "fachspezifisches"
                        , "barnebarn", "fachärzten"
                        , "barnet", "fact"
                        , "barnlig", "factor"
                        , "barnligt", "facts"
                        , "barsel", "faden"
                        , "basal", "fading"
                        , "base", "faeser"
                        , "based", "fahne"
                        , "basere", "fahrbahn"
                        , "baserede", "fahrbereit"
                        , "baseret", "fahren"
                        , "basic", "fahrenheit"
                        , "basically", "fahrerflucht"
                        , "basketbane", "fahrgast"
                        , "batterier", "fahrgastrechten"
                        , "bay", "fahrgäste"
                        , "bazar", "fahrkarte"
                        , "beboere", "fahrn"
                        , "bebyggelse", "fahrprüfungen"
                        , "bed", "fahrrad-lastenanh"
                        , "bede", "fahrraddiebe"
                        , "bedere", "fahrradgarage"
                        , "bedrag", "fahrradklingel"
                        , "bedre", "fahrradtouren"
                        , "bedst", "fahrradwerkstatt"
                        , "bedste", "fahrschein"
                        , "bedsteforældre", "fahrt"
                        , "bedsteforældrene", "fahrtrichtung"
                        , "bedt", "fahrzeug"
                        , "befolkning", "fahrzeughalter"
                        , "befolkningen", "fahrzeugstörungen"
                        , "begge", "fair"
                        , "begrebet", "fairerweise"
                        , "begrundelse", "fairness"
                        , "begrænses", "fake"
                        , "begrænset", "fake-rechner"
                        , "begrænsning", "fakten"
                        , "begyndelsen", "faktisk"
                        , "begynder", "faktoren"
                        , "begyndt", "faktum"
                        , "begyndte", "falcons"
                        , "begynner", "falle"
                        , "begå", "fallenden"
                        , "begået", "fallenlassen"
                        , "behagelig", "fallschirmjäger"
                        , "behandles", "fallzahlen"
                        , "behandling", "falschberatung"
                        , "behandlingen", "falschen"
                        , "behandlinh", "falsches"
                        , "behold", "false-positives"
                        , "beholde", "falten"
                        , "behov", "fam"
                        , "behovet", "familien"
                        , "behåret", "familienfreundlich"
                        , "behøvede", "familienleben"
                        , "behøver", "familienrabatt"
                        , "beklager", "familienurlaub"
                        , "bekomme", "familiziden"
                        , "bekostning", "familiäre"
                        , "bekræfte", "famous"
                        , "bekymre", "fan"
                        , "bekymrende", "fanbase"
                        , "bekymrer", "fand"
                        , "bekymret", "fandest"
                        , "bekymring", "fandst"
                        , "bekymringer", "fange"
                        , "belgium", "fanpost"
                        , "bella", "fanseven"
                        , "belåning", "fantasie"
                        , "beløb", "fantasies"
                        , "bemærkelsesværdigt", "fantasy"
                        , "bemærket", "fao"
                        , "ben", "farbdsrstellung"
                        , "benny", "farben"
                        , "benytte", "farblich"
                        , "beregnede", "farcry"
                        , "beretning", "farm"
                        , "berettiget", "farrahhill"
                        , "berkshire", "faschiertes"
                        , "berlingske", "faschisten"
                        , "besat", "faschos"
                        , "besidde", "fassaden"
                        , "besidder", "fassung"
                        , "besitter", "fassungslosigkeit"
                        , "besked", "faste"
                        , "beskeder", "fastenbrechen"
                        , "beskidt", "fastentage"
                        , "beskraldet", "faster"
                        , "beskrevet", "fastet"
                        , "beskrivelse", "faszinierend"
                        , "beskrivelsen", "fasziniert"
                        , "beskriver", "fatal"
                        , "beskytte", "fatemas"
                        , "beskyttelsen", "father's"
                        , "beskytter", "fatter"
                        , "beskyttes", "fatwa"
                        , "beslut", "faul"
                        , "beslutninger", "fauler"
                        , "beslutningerne", "fault"
                        , "bestemme", "fav"
                        , "bestemmelserne", "favorite"
                        , "bestemmer", "favourite"
                        , "bestemt", "faz"
                        , "bestemte", "rabattcodes"
                        , "bestilles", "rabbit"
                        , "bestillinger", "rabbits'"
                        , "bestå", "rabenmutter"
                        , "bestået", "racial"
                        , "består", "rad-weg-initiative"
                        , "besvarede", "radagons"
                        , "besvarelse", "radarr"
                        , "besyv", "radfahren"
                        , "besøg", "radfahrern"
                        , "betal", "radiatoren"
                        , "betale", "radikale"
                        , "betaler", "radikalisiert"
                        , "betaling", "radioactive"
                        , "betalingskrav", "radlager"
                        , "betalingsmulighed", "radweg"
                        , "betalingsservice", "ragastan"
                        , "betalt", "ragen"
                        , "betalte", "rahaa"
                        , "betegnelse", "rahmen"
                        , "betegnelsen", "rahmenlage"
                        , "betegnet", "raider"
                        , "betjent", "raiding"
                        , "betjente", "rainbow-colored"
                        , "betjenten", "rainy"
                        , "betragtede", "rakete"
                        , "betragtes", "rakurei"
                        , "betragtet", "ram"
                        , "betragtning", "ramadan-beginn"
                        , "betrække", "ramadu"
                        , "betrækket", "ramadī"
                        , "bette", "ramaḍān"
                        , "betyde", "rammen"
                        , "betyder", "rammstein"
                        , "betydning", "ran"
                        , "betydningsfulle", "rand"
                        , "bevares", "randgebiet"
                        , "bevidst", "randomized"
                        , "bevis", "randomly"
                        , "bevisbyrden", "rang"
                        , "bevise", "range"
                        , "bevismateriale", "rangehen"
                        , "bevist", "rangers"
                        , "bevisthed", "rangeschaft"
                        , "bevæge", "ranging"
                        , "bevægelse", "ranholt"
                        , "bevæger", "ranked"
                        , "bevæget", "rant"
                        , "bevæggrunde", "ranzig"
                        , "bibelske", "rape"
                        , "bidrag", "rapist"
                        , "bidrage", "rapper"
                        , "bidragende", "rar"
                        , "bidrager", "rarted"
                        , "big", "rascher"
                        , "biks", "rasieren"
                        , "bildet", "rassistische"
                        , "bilen", "rastest"
                        , "biler", "rate"
                        , "bilister", "rates"
                        , "billede", "rather"
                        , "billeder", "rationale"
                        , "billedet", "rationalität"
                        , "billet", "ratsamer"
                        , "billetter", "ratsvorsitzenden"
                        , "billig", "ratten"
                        , "billigere", "rattiger"
                        , "billigste", "rauch"
                        , "billigt", "rauchen"
                        , "bimodalfordeling", "raucht"
                        , "bingeing", "rauer"
                        , "biograf", "raufgegangen"
                        , "biologi", "raufgezogen"
                        , "biologien", "raufzuklettern"
                        , "biologisk", "raum"
                        , "biologiske", "raumtemperatur"
                        , "bipper", "raus"
                        , "bit", "rausdrehen"
                        , "bitre", "rausfindet"
                        , "bitte", "rausgefunden"
                        , "bitter", "rausgehauen"
                        , "bizart", "rausgeht"
                        , "blande", "rausgeschnitten"
                        , "blandt", "rausgeworfen"
                        , "blanket", "raushauen"
                        , "blatant", "rausholen"
                        , "blev", "rauskommt"
                        , "blevet", "rausnehmen"
                        , "blinde", "rausrücken"
                        , "blindt", "rausschlagen"
                        , "blir", "rausschneiden✌️"
                        , "bliv", "rauswillst"
                        , "blive", "rauszufinden"
                        , "bliver", "rauszuhalten"
                        , "blokere", "rauszulesen"
                        , "blokering", "raven"
                        , "blomster", "ravioli"
                        , "blomsterne", "rayciss"
                        , "blot", "face"
                        , "blottere", "fair"
                        , "blues", "rae"
                        , "blå", "rant"
                        , "blåøjet", "fa'art"
                        , "blæseren", "fa's"
                        , "blæst", "fa-ex"
                        , "bobler", "fa-fyl"
                        , "bod", "fa05b12"
                        , "bodde", "fa1tality"
                        , "bodega", "fa20"
                        , "bodylab", "fa69"
                        , "bodyman", "faa's"
                        , "bodyshame", "faa2"
                        , "bodyshamer", "faaa164f6e28509c-64"
                        , "bodystocking", "faaaaaaaaaaaaake"
                        , "bodystockingen", "faaaaaaaair"
                        , "boede", "faaaaaaith"
                        , "boende", "faaaaaar"
                        , "boet", "faaaaaded"
                        , "bogstavelige", "faaaaairrr"
                        , "bogstaveligt", "faaaaamily"
                        , "bogstaveligtalt", "faaaaatttt"
                        , "boks", "faaaaboolous"
                        , "boktittelen", "faaaack"
                        , "boldgade", "faaaaiiirr"
                        , "bolig", "faaaamily"
                        , "boliger", "faaaark"
                        , "boliglån", "faaaassst"
                        , "boligmangel", "faaacts"
                        , "boligmarkedet", "faaakkkeee"
                        , "boligproblemet", "faaall"
                        , "boligsøgende", "faaamily"
                        , "bondegårdsferie", "faaarrr"
                        , "bone", "faaav"
                        , "booke", "faad"
                        , "booty", "faado"
                        , "bopæl", "faafo"
                        , "bor", "faag"
                        , "bord", "faakin"
                        , "bordet", "faalale"
                        , "borger", "faalele"
                        , "borgere", "faaltu"
                        , "borgeren", "faang-tier"
                        , "borgerne", "faanng"
                        , "borgernes", "faarke"
                        , "boris", "faate"
                        , "borislaursen", "fab"
                        , "bort", "fab-5000"
                        , "borte", "fabaulous"
                        , "bortset", "fabbbbb"
                        , "bosat", "fabbri"
                        , "boss", "fabcurate"
                        , "boston", "fabdellavalle"
                        , "botanisk", "fabel"
                        , "botox", "faber"
                        , "bra", "faber-castell"
                        , "bransjen", "fabergé"
                        , "bremse", "fabianksi"
                        , "bremser", "fabians"
                        , "brevsprækken", "fabien"
                        , "brikker", "fabindia"
                        , "brobyggerne", "fabio'"
                        , "broer", "fabiola"
                        , "brok", "fabius"
                        , "brokke", "fable"
                        , "bronuts", "fablehaven"
                        , "bror", "fabless"
                        , "browserbaseret", "fabletics"
                        , "brt", "faboulus"
                        , "brud", "fabreeze"
                        , "bruden", "fabri"
                        , "brudeparret", "fabriano"
                        , "brudt", "fabric'"
                        , "brug", "fabric-anima"
                        , "bruge", "fabric-biome-api-v1"
                        , "bruger", "fabric-containers-v0"
                        , "brugere", "fabric-item-api-v1"
                        , "brugeren", "fabric-models-v0"
                        , "brugerne", "fabric-particles-v1"
                        , "bruges", "fabric-rendering-v1"
                        , "brugt", "fabric-textures-v0"
                        , "brugte", "fabricate"
                        , "bruhhhh", "fabricates"
                        , "bruk", "fabricating"
                        , "bruke", "fabrications"
                        , "brukes", "fabricator's"
                        , "bruuns", "fabrice"
                        , "bryde", "fabricmc"
                        , "bryder", "fabrika"
                        , "brydes", "fabrikat"
                        , "bryllup", "fabrisexual"
                        , "bryllupsgæst", "fabrizioromano"
                        , "bryst", "fabski"
                        , "brædder", "fabuland"
                        , "bræk", "fabulous"
                        , "brækket", "fabulously"
                        , "brætspil", "fabuously"
                        , "bucks", "fac_working_papers"
                        , "bud", "facade"
                        , "budapest", "facas"
                        , "budskabet", "faccion"
                        , "buh-huh", "face'"
                        , "build", "face--it's"
                        , "buket", "face-blind"
                        , "bukke", "face-cards"
                        , "bukser", "face-eating"
                        , "bukserne", "face-freezingly"
                        , "bule", "face-le"
                        , "buler", "face-meltingly"
                        , "bulgaria", "face-punches"
                        , "bulk", "face-tank"
                        , "bullshit", "face-tanks"
                        , "bumle", "face-to-two-face"
                        , "bummer", "face-turn"
                        , "bump", "face-up"
                        , "bunden", "face6so"
                        , "bundet", "faceai"
                        , "burde", "faceas"
                        , "bursts", "faceblind"
                        , "bus", "faceboo"
                        , "bussen", "facebook's"
                        , "butik", "facebook-worthy"
                        , "butikker", "facebookmail"
                        , "butikkerne", "facebookos"
                        , "buttermilk", "facebooks"
                        , "butthurt", "facebook—it's"
                        , "buttplug", "facebreaker"
                        , "bweeeerrrrrrruhhhher", "facebusters"
                        , "byen", "facecamp"
                        , "byer", "facecampers"
                        , "byggede", "facecards"
                        , "byggemarkedernes", "facecrack"
                        , "byggemarkedet", "facedancer"
                        , "bygger", "faceee"
                        , "bygges", "facefixer"
                        , "bygget", "facefucked"
                        , "bygning", "faceful"
                        , "bymidten", "facegen"
                        , "bymiljøet", "facehugger"
                        , "bypass", "faceid"
                        , "byplanlægning", "faceless"
                        , "byrum", "facelift"
                        , "byrummet", "facelifts"
                        , "bytta", "facemagic"
                        , "bytur", "facemasks"
                        , "byzonen", "facemoji"
                        , "både", "faceoffs"
                        , "bålsted", "facepaint"
                        , "bås", "facepalmed"
                        , "båse", "facepalms"
                        , "bælter", "faceplant"
                        , "bænk", "faceplate"
                        , "bænke", "faceplates"
                        , "bænkene", "faceprint"
                        , "bøde", "facepuncher"
                        , "bødeforlægget", "facere"
                        , "bøden", "faceroll"
                        , "bøder", "facers"
                        , "bøjer", "facescan"
                        , "bølgepap", "faceshit"
                        , "bønnen", "facesitting"
                        , "bør", "faceswap"
                        , "børe", "facetaker"
                        , "børn", "facetanks"
                        , "børnehaver", "faceters"
                        , "børnene", "facetexture"
                        , "børnenes", "faceticious"
                        , "børnesweater", "facetimed"
                        , "bøvle", "facetiming"
                        , "caféer", "facetious"
                        , "calturin", "facets"
                        , "can't", "facetune"
                        , "canelos", "facetuning"
                        , "cannibal", "facewash"
                        , "capio", "facey"
                        , "carlsberg", "face☹️"
                        , "cashmere", "fachada"
                        , "casper", "fachgebiet"
                        , "café", "fachists"
                        , "caster", "fachsprache"
                        , "cathrine", "faci"
                        , "causal", "facial"
                        , "cbc", "facialed"
                        , "cbd", "facials"
                        , "cbs-typerne", "facielongus"
                        , "cekic", "faciiatis"
                        , "celex", "facile"
                        , "celler", "facilisi"
                        , "celofan", "facilitate"
                        , "cement", "facilitates"
                        , "censur", "facilitation"
                        , "censurere", "facilitators"
                        , "censureret", "facilities"
                        , "centos", "facility's"
                        , "chancen", "facinator"
                        , "chat", "facing"
                        , "check", "facings"
                        , "checke", "faciois"
                        , "checker", "facism"
                        , "chef", "facists"
                        , "chefen", "fack"
                        , "chefer", "facked"
                        , ""
                        };
    int len = 10000;
    for (int i = 0; i < len; i++) {
        const char *key = data[i * 2];
        if (strlen(key) == 0)
            break;
        if (data[(i + 1) * 2][0] == 0)
            cout << "Last entry" << endl;
        const char *val = data[i * 2 + 1];
        lx->put(key, strlen(key), val, strlen(val));
    }
    bool all_ok = true;
    for (int i = 0; i < len; i++) {
        const char *key = data[i * 2];
        if (strlen(key) == 0)
            break;
        if (lx->get(key, strlen(key), &val_len, val)) {
            val[val_len] = 0;
            if (memcmp(val, data[i * 2 + 1], strlen(data[i * 2 + 1])) != 0) {
                cout << key << " not matching" << endl;
                all_ok = false;
            } else
                cout << "Key: " << key << ", val: [" << val << "]" << endl;
        } else {
            cout << key << " not found" << endl;
            all_ok = false;
        }
    }
    // bft_iterator_status bft_iter(lx->trie, 0, lx->get_trie_len());
    // uint8_t key[100];
    // int ptr;
    // do {
    //     ptr = lx->next_ptr(bft_iter, key, len);
    //     if (ptr) {
    //         memcpy(key + len, lx->current_block + ptr + 1, lx->current_block[ptr]);
    //         len += lx->current_block[ptr];
    //         key[len] = 0;
    //         cout << "Key: " << key << endl;
    //     }
    // } while (ptr);
    lx->print_stats(lx->size());
    if (all_ok)
        cout << "All ok" << endl;
    else
        cout << "ALL NOT OK" << endl;
    delete lx;
    return 0;
}

/// ./build/imain 1000000 2 16 8 4096 4096 10000 out.ix
int main(int argc, char *argv[]) {

    if (argc == 1) {
        cout << "Load file:" << endl;
        cout << "  imain <IMPORT_FILE> [KEY_LEN] [USE_HASH_TABLE] [LEAF_PAGE_SIZE] [PARENT_PAGE_SIZE] [CACHE_SIZE] [FILE]" << endl;
        cout << "Gen entries:" << endl;
        cout << "  imain <NUM_ENTRIES> [CHAR_SET] [KEY_LEN] [VAL_LEN] [LEAF_PAGE_SIZE] [PARENT_PAGE_SIZE] [CACHE_SIZE] [FILE]" << endl;
        return 0;
    }

    char out_file1[100];
    char out_file2[100];
    if (argc > 1) {
        if ((argv[1][0] >= '0' && argv[1][0] <= '9') || argv[1][0] == '-') {
            NUM_ENTRIES = atol(argv[1]);
            if (NUM_ENTRIES < 0) {
                NUM_ENTRIES = -NUM_ENTRIES;
                KEY_VALUE_VAR_LEN = 1;
            }
        } else {
            IMPORT_FILE = argv[1];
            if (argc > 2)
                KEY_LEN = atoi(argv[2]);
            if (argc > 3 && (argv[3][0] == '0' || argv[3][0] == '1'))
                USE_HASHTABLE = atoi(argv[3]);
            if (argc > 4)
                LEAF_PAGE_SIZE = atoi(argv[4]);
            if (argc > 5)
                PARENT_PAGE_SIZE = atoi(argv[5]);
            if (argc > 6)
                CACHE_SIZE = atoi(argv[6]);
            if (argc > 7) {
                OUT_FILE1 = out_file1;
                strcpy(OUT_FILE1, "f1_");
                strcat(OUT_FILE1, argv[7]);
                OUT_FILE2 = out_file2;
                strcpy(OUT_FILE2, "f2_");
                strcat(OUT_FILE2, argv[7]);
            }
        }
    }
    if (IMPORT_FILE == NULL) {
        if (argc > 2)
            CHAR_SET = atoi(argv[2]);
        if (argc > 3)
            KEY_LEN = atoi(argv[3]);
        if (argc > 4)
            VALUE_LEN = atoi(argv[4]);
        if (argc > 5)
            LEAF_PAGE_SIZE = atoi(argv[5]);
        if (argc > 6)
            PARENT_PAGE_SIZE = atoi(argv[6]);
        if (argc > 7)
            CACHE_SIZE = atoi(argv[7]);
        if (argc > 8) {
            OUT_FILE1 = out_file1;
            strcpy(OUT_FILE1, "f1_");
            strcat(OUT_FILE1, argv[8]);
            OUT_FILE2 = out_file2;
            strcpy(OUT_FILE2, "f2_");
            strcat(OUT_FILE2, argv[8]);
        }
        if (NUM_ENTRIES == 0) {
            return test(OUT_FILE1);
        }
    }

    int64_t data_alloc_sz = (IMPORT_FILE == NULL ? (KEY_LEN + VALUE_LEN + 8) * NUM_ENTRIES // including \0 at end of key
            : get_import_file_size() + 110000000); // extra 30mb for 7 + key_len + value_len + \0 for max 11 mil entries
    cout << "Data size: " << data_alloc_sz / 1000 << "kb" << endl;
    uint8_t *data_buf = (uint8_t *) malloc(data_alloc_sz);
    int64_t data_sz = 0;
    char value_buf[VALUE_LEN + 1];

    unordered_map<string, string> m;
    uint32_t start, stop;
    start = get_time_val();
    if (IMPORT_FILE == NULL)
        data_sz = insert(m, data_buf);
    else {
        cout << "Loading:" << IMPORT_FILE << "..." << endl;
        NUM_ENTRIES = 0;
        data_sz = load_file(m, data_buf);
        for (int i = 3; i < argc; i++) {
            if (argv[3][0] != '0' && argv[3][0] != '1') {
                IMPORT_FILE = argv[i];
                cout << "Loading:" << IMPORT_FILE << "..." << endl;
                data_sz += load_file(m, data_buf + data_sz);
            }
        }
    }
    stop = get_time_val();

    unordered_map<string, string>::iterator it;
    int null_ctr = 0;
    int cmp = 0;

    /*
     //stx::btree_map<string, string> m1;
     map<string, string> m1;
     start = get_time_val();
     it = m.begin();
     for (; it != m.end(); ++it)
     m1.insert(pair<string, string>(it->first, it->second));
     stop = get_time_val();
     cout << "RB Tree insert time:" << timedifference(start, stop) << endl;
     it = m.begin();
     start = get_time_val();
     for (; it != m.end(); ++it) {
     string value = m1[it->first.c_str()];
     char v[100];
     if (value.length() == 0) {
     null_ctr++;
     } else {
     int d = util::compare(it->second.c_str(), it->second.length(),
     value.c_str(), value.length());
     if (d != 0) {
     cmp++;
     strncpy(v, value.c_str(), value.length());
     v[it->first.length()] = 0;
     cout << cmp << ":" << it->first.c_str() << "=========="
     << it->second.c_str() << "----------->" << v << endl;
     }
     }
     ctr++;
     }
     stop = get_time_val();
     cout << "RB Tree get time:" << timedifference(start, stop) << endl;
     cout << "Null:" << null_ctr << endl;
     cout << "Cmp:" << cmp << endl;
     cout << "RB Tree size:" << m1.size() << endl;
     */

    unordered_map<string, string>::iterator it1;

//    ctr = 0;
//    it = m.begin();
//    for (; it != m.end(); ++it) {
//        cout << "\"" << it->first.c_str() << "\", " << it->first.length()
//                << ", \"" << it->second.c_str() << "\", " << it->second.length() << endl;
//        if (ctr++ > 300)
//            break;
//    }
//
//    ctr = 0;
//    it = m.begin();
//    for (; it != m.end(); ++it) {
//        cout << "\"" << it->first.c_str() << "\", " << it->first.length() << endl;
//        if (ctr++ > 300)
//            break;
//    }

    art_tree at;
    if (TEST_ART)
    {
    ctr = 0;
    art_tree_init(&at);
    start = get_time_val();
    if (USE_HASHTABLE) {
        it1 = m.begin();
        for (; it1 != m.end(); ++it1) {
            //cout << it1->first.c_str() << endl; //<< ":" << it1->second.c_str() << endl;
            art_insert(&at, (unsigned char*) it1->first.c_str(),
                    it1->first.length(), (void *) it1->second.c_str());
            ctr++;
        }
    } else {
        for (int64_t pos = 0; pos < data_sz; pos++) {
            int8_t vlen;
            uint32_t key_len = read_vint32(data_buf + pos, &vlen);
            pos += vlen;
            uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);
            art_insert(&at, data_buf + pos, key_len, data_buf + pos + key_len + vlen + 1);
            pos += key_len + value_len + vlen + 1;
            ctr++;
        }
    }
    stop = get_time_val();
    cout << "ART Insert Time:" << timedifference(start, stop) << endl;
    //getchar();
    }

    if (TEST_HASHTABLE) {
        start = get_time_val();
        for (int64_t pos = 0; pos < data_sz; pos++) {
            int8_t vlen;
            uint32_t key_len = read_vint32(data_buf + pos, &vlen);
            pos += vlen;
            uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);
            m.insert(pair<string, string>((char *) data_buf + pos, (char *) data_buf + pos + key_len + vlen + 1));
            pos += key_len + value_len + vlen + 1;
            ctr++;
        }
        stop = get_time_val();
        cout << "Hash_map insert time:" << timedifference(start, stop) << ", ";
        cout << "Number of entries: " << NUM_ENTRIES << ", ";
        cout << "Hash_map size:" << m.size() << endl;
        //getchar();
    }

    squeezed::builder sb;
    sb.set_print_enabled(true);
    if (TEST_SQZD)
    {
    it1 = m.begin();
    start = get_time_val();
    uint8_t dummy[9];
    //cout << "Ptr size:" << util::ptr_toBytes((unsigned long) lx->root_block, dummy) << endl;
    if (USE_HASHTABLE) {
        it1 = m.begin();
        for (; it1 != m.end(); ++it1) {
            //cout << it1->first.c_str() << endl; //<< ":" << it1->second.c_str() << endl;
            sb.insert((const uint8_t *) it1->first.c_str(), it1->first.length(), (const uint8_t *) it1->second.c_str(),
                    it1->second.length());
            ctr++;
        }
    } else {
        for (int64_t pos = 0; pos < data_sz; pos++) {
            int8_t vlen;
            uint32_t key_len = read_vint32(data_buf + pos, &vlen);
            pos += vlen;
            uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);
            sb.insert(data_buf + pos, key_len, data_buf + pos + key_len + vlen + 1, value_len);
            pos += key_len + value_len + vlen + 1;
            ctr++;
        }
    }
    if (OUT_FILE1 == NULL)
        sb.build(string("test.rst"));
    else
        sb.build(string(OUT_FILE1) + ".rst");
    stop = get_time_val();
    cout << "Squeezed builder insert+build time:" << timedifference(start, stop) << endl;
    }

    basix *lx;
    if (TEST_IDX1)
    {
    ctr = 0;
    
    //lx = new linex(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);    // staging not working
    lx = new basix(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);
    //lx = new basix3(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);
    //lx = new stager(OUT_FILE1, CACHE_SIZE); // not working
    //lx = new sqlite(2, 1, "key, value", "imain", LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);
    //lx = new bft(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);    // staging not working
    //lx = new dft(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);
    //lx = new bfos(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);
    //lx = new bfox(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);
    //lx = new bfqs(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);
    //lx = new dfqx(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);
    //lx = new dfox(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);
    //lx = new dfos(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);
    //lx = new rb_tree(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE1);  // not working
    it1 = m.begin();
    start = get_time_val();
    uint8_t dummy[9];
    //cout << "Ptr size:" << util::ptr_toBytes((unsigned long) lx->root_block, dummy) << endl;
    if (USE_HASHTABLE) {
        it1 = m.begin();
        for (; it1 != m.end(); ++it1) {
            //cout << it1->first.c_str() << endl; //<< ":" << it1->second.c_str() << endl;
            lx->put(it1->first.c_str(), it1->first.length(), it1->second.c_str(),
                    it1->second.length());
            ctr++;
        }
    } else {
        for (int64_t pos = 0; pos < data_sz; pos++) {
            int8_t vlen;
            uint32_t key_len = read_vint32(data_buf + pos, &vlen);
            pos += vlen;
            uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);
            lx->put((char *) data_buf + pos, key_len, (char *) data_buf + pos + key_len + vlen + 1, value_len);
            pos += key_len + value_len + vlen + 1;
            ctr++;
        }
    }
    stop = get_time_val();
    cout << "Index1 insert time:" << timedifference(start, stop) << endl;
    //getchar();
    }

    bfos *dx;
    if (TEST_IDX2)
    {
    ctr = 0;
    //dx = new linex(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2); // working
    //dx = new basix(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2); // working
    //dx = new basix3(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2); // working
    //dx = new stager(OUT_FILE2, CACHE_SIZE);
    //dx = new sqlite(2, 1, "key, value", "imain", LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2);
    //dx = new bft(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2);
    //dx = new dft(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2);
    dx = new bfos(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2); // working only with int16_t
    //dx = new bfox(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2); // working only with int16_t
    //dx = new bfqs(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2); // working
    //dx = new dfqx(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2); // working
    //dx = new dfox(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2);
    //dx = new dfos(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2); // working
    //dx = new rb_tree(LEAF_PAGE_SIZE, PARENT_PAGE_SIZE, CACHE_SIZE, OUT_FILE2);
    it1 = m.begin();
    start = get_time_val();
    if (USE_HASHTABLE) {
        it1 = m.begin();
        for (; it1 != m.end(); ++it1) {
            //cout << it1->first.c_str() << endl; //<< ":" << it1->second.c_str() << endl;
            dx->put(it1->first.c_str(), it1->first.length(), it1->second.c_str(),
                    it1->second.length());
            ctr++;
        }
    } else {
        for (int64_t pos = 0; pos < data_sz; pos++) {
            int8_t vlen;
            uint32_t key_len = read_vint32(data_buf + pos, &vlen);
            pos += vlen;
            uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);
            dx->put((char *) data_buf + pos, key_len, (char *) data_buf + pos + key_len + vlen + 1, value_len);
            pos += key_len + value_len + vlen + 1;
            ctr++;
        }
    }
    stop = get_time_val();
    cout << "Index2 insert time:" << timedifference(start, stop) << endl;
    //getchar();
    }

    if (TEST_ART)
    {
    cmp = 0;
    ctr = 0;
    null_ctr = 0;
    it1 = m.begin();
    start = get_time_val();
    if (USE_HASHTABLE) {
        for (; it1 != m.end(); ++it1) {
            int len;
            char *value = (char *) art_search(&at,
                    (unsigned char*) it1->first.c_str(), it1->first.length());
            check_value(it1->first.c_str(), it1->first.length(),
                    it1->second.c_str(), it1->second.length(), value, it1->second.length(), null_ctr, cmp);
            ctr++;
        }
    } else {
        for (int64_t pos = 0; pos < data_sz; pos++) {
            int len;
            int8_t vlen;
            uint32_t key_len = read_vint32(data_buf + pos, &vlen);
            pos += vlen;
            uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);
            char *value = (char *) art_search(&at, data_buf + pos, key_len);
            len = strlen(value);
            check_value((char *) data_buf + pos, key_len,
                    (char *) data_buf + pos + key_len + vlen + 1, value_len, value, len, null_ctr, cmp);
            pos += key_len + value_len + vlen + 1;
            ctr++;
        }
    }
    stop = get_time_val();
    cout << "ART Get Time:" << timedifference(start, stop) << ", ";
    cout << "Null:" << null_ctr << ", Cmp:" << cmp << ", ";
    cout << "ART Size:" << art_size(&at) << endl;
    //getchar();
    }

    if (TEST_HASHTABLE) {
        cmp = 0;
        ctr = 0;
        null_ctr = 0;
        start = get_time_val();
        for (int64_t pos = 0; pos < data_sz; pos++) {
            int len;
            int8_t vlen;
            uint32_t key_len = read_vint32(data_buf + pos, &vlen);
            pos += vlen;
            uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);
            it = m.find((char *) (data_buf + pos));
            char *value = (char *) it->second.c_str();
            len = value_len;
            check_value((char *) data_buf + pos, key_len,
                    (char *) data_buf + pos + key_len + vlen + 1, value_len, value, len, null_ctr, cmp);
            pos += key_len + value_len + vlen + 1;
            ctr++;
        }
        stop = get_time_val();
        cout << "Hashtable Get Time:" << timedifference(start, stop) << ", ";
        cout << "Null:" << null_ctr << ", Cmp:" << cmp << ", ";
        cout << "Size:" << ctr << endl;
    }

    if (TEST_SQZD)
    {
    squeezed::static_dict sd;
    std::string rst_outfile;
    if (OUT_FILE1 == NULL)
        rst_outfile = "test.rst";
    else {
        rst_outfile = OUT_FILE1;
        rst_outfile += ".rst";
    }
    sd.load(rst_outfile.c_str());
    cmp = 0;
    ctr = 0;
    null_ctr = 0;
    it1 = m.begin();
    start = get_time_val();
    if (USE_HASHTABLE) {
        for (; it1 != m.end(); ++it1) {
            int len = VALUE_LEN;
            bool is_found = sd.get((const uint8_t *) it1->first.c_str(), it1->first.length(), &len, (uint8_t *) value_buf);
            check_value(it1->first.c_str(), it1->first.length() + 1,
                    it1->second.c_str(), it1->second.length(), value_buf, len, null_ctr, cmp);
            ctr++;
        }
    } else {
        for (int64_t pos = 0; pos < data_sz; pos++) {
            int len = VALUE_LEN;
            int8_t vlen;
            uint32_t key_len = read_vint32(data_buf + pos, &vlen);
            pos += vlen;
            uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);
            bool is_found = sd.get(data_buf + pos, key_len, &len, (uint8_t *) value_buf);
            check_value((char *) data_buf + pos, key_len,
                    (char *) data_buf + pos + key_len + vlen + 1, value_len, value_buf, len, null_ctr, cmp);
            pos += key_len + value_len + vlen + 1;
            ctr++;
        }
    }
    stop = get_time_val();
    cout << "Squeezed Get Time:" << timedifference(start, stop) << ", ";
    cout << "Null:" << null_ctr << ", Cmp:" << cmp << endl;
    }

    if (TEST_IDX1)
    {
    cmp = 0;
    ctr = 0;
    null_ctr = 0;
    it1 = m.begin();
    start = get_time_val();
    if (USE_HASHTABLE) {
        for (; it1 != m.end(); ++it1) {
            int len = VALUE_LEN;
            bool is_found = lx->get((const char *) it1->first.c_str(), it1->first.length(), &len, value_buf);
            check_value(it1->first.c_str(), it1->first.length() + 1,
                    it1->second.c_str(), it1->second.length(), value_buf, len, null_ctr, cmp);
            ctr++;
        }
    } else {
        for (int64_t pos = 0; pos < data_sz; pos++) {
            int len = VALUE_LEN;
            int8_t vlen;
            uint32_t key_len = read_vint32(data_buf + pos, &vlen);
            pos += vlen;
            uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);
            bool is_found = lx->get((char *) data_buf + pos, key_len, &len, value_buf);
            check_value((char *) data_buf + pos, key_len,
                    (char *) data_buf + pos + key_len + vlen + 1, value_len, value_buf, len, null_ctr, cmp);
            pos += key_len + value_len + vlen + 1;
            ctr++;
        }
    }
    stop = get_time_val();
    cout << "Index1 Get Time:" << timedifference(start, stop) << ", ";
    cout << "Null:" << null_ctr << ", Cmp:" << cmp << endl;
    lx->print_stats(NUM_ENTRIES);
    lx->print_num_levels();
    //lx->print_counts();
    cout << "Root filled size:" << lx->filled_size() << endl;
/*
    if ((null_ctr > 0 || cmp > 0) && (null_ctr + cmp) < 20) {
        if (USE_HASHTABLE) {
            for (; it1 != m.end(); ++it1) {
                int len;
                const char *value = lx->get(it1->first.c_str(), it1->first.length(), &len, value_buf);
                if (value == NULL)
                    printf("Null key: %.*s\n", (int) it1->first.length(), it1->first.c_str());
                else if (memcmp(value_buf, it1->second.c_str(), it1->second.length()) != 0) {
                    printf("Cmp fail: %.*s\nexp: %.*s\nact: %.*s\n", (int) it1->first.length(), it1->first.c_str(), 
                        (int) it1->second.length(), it1->second.c_str(), (int) len, value_buf);
                }
                ctr++;
            }
        } else {
            for (int64_t pos = 0; pos < data_sz; pos++) {
                int len;
                int8_t vlen;
                uint32_t key_len = read_vint32(data_buf + pos, &vlen);
                pos += vlen;
                uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);  
                const char *value = (const char *) lx->get((const char *) data_buf + pos, key_len, &len, value_buf);
                if (value == NULL)
                    printf("Null key: %.*s\n", (int) key_len, data_buf + pos);
                else if (memcmp(value_buf, data_buf + pos + key_len + vlen, value_len) != 0) {
                    printf("Cmp fail: %.*s\nexp: %.*s\nact: %.*s\n", (int) key_len, data_buf + pos, 
                        (int) value_len, data_buf + pos + key_len + vlen + 1, (int) len, value_buf);
                }
                pos += key_len + value_len + vlen + 1;
                ctr++;
            }
            for (int64_t pos = 0; pos < data_sz; pos++) {
                int len;
                int8_t vlen;
                uint32_t key_len = read_vint32(data_buf + pos, &vlen);
                pos += vlen;
                uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);
                printf("Key: %.*s, val: %.*s\n", (int) key_len, data_buf + pos, 
                        (int) value_len, data_buf + pos + key_len + vlen + 1);
                pos += key_len + value_len + vlen + 1;
                ctr++;
            }
        }
    }
*/
    }

    if (TEST_IDX2)
    {
    cmp = 0;
    ctr = 0;
    null_ctr = 0;
    //bfos::count = 0;
    it1 = m.begin();
    start = get_time_val();
    __builtin_prefetch(util::bit_count2x, 0, 3);
    __builtin_prefetch(util::bit_count2x + 64, 0, 3);
    __builtin_prefetch(util::bit_count2x + 128, 0, 3);
    __builtin_prefetch(util::bit_count2x + 192, 0, 3);
    if (USE_HASHTABLE) {
        for (; it1 != m.end(); ++it1) {
            int len = VALUE_LEN;
            bool is_found = dx->get(it1->first.c_str(), it1->first.length(), &len, value_buf);
            check_value(it1->first.c_str(), it1->first.length() + 1,
                    it1->second.c_str(), it1->second.length(), value_buf, len, null_ctr, cmp);
            ctr++;
        }
    } else {
        for (int64_t pos = 0; pos < data_sz; pos++) {
            int len = VALUE_LEN;
            int8_t vlen;
            uint32_t key_len = read_vint32(data_buf + pos, &vlen);
            pos += vlen;
            uint32_t value_len = read_vint32(data_buf + pos + key_len + 1, &vlen);
            bool is_found = dx->get((const char *) data_buf + pos, key_len, &len, value_buf);
            check_value((char *) data_buf + pos, key_len,
                    (char *) data_buf + pos + key_len + vlen + 1, value_len, value_buf, len, null_ctr, cmp);
            pos += key_len + value_len + vlen + 1;
            ctr++;
        }
    }
    stop = get_time_val();
    cout << "Index2 get time:" << timedifference(start, stop) << ", ";
    cout << "Null:" << null_ctr << ", Cmp:" << cmp << endl;
    dx->print_stats(NUM_ENTRIES);
    dx->print_num_levels();
    //dx->print_counts();
    //cout << "Root filled size:" << (int) dx->root_data[MAX_PTR_BITMAP_BYTES+2] << endl;
    //getchar();
    }

    if (TEST_ART)
        art_tree_destroy(&at);
    if (TEST_IDX1)
        delete lx;
    if (TEST_IDX2)
        delete dx;

    free(data_buf);
    return 0;

}
