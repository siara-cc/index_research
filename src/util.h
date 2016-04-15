#ifndef UTIL_H
#define UTIL_H

typedef unsigned char byte;
#define null 0

class util {
public:
    static int getInt(byte *pos) {
        int ret = *pos * 256;
        pos++;
        ret += *pos;
        return ret;
    }

    static void setInt(byte *pos, int val) {
        *pos = val / 256;
        pos++;
        *pos = val % 256;
    }

    static void ptrToFourBytes(unsigned long addr_num, char *addr) {
        addr[3] = (addr_num & 0xFF);
        addr_num >>= 8;
        addr[2] = (addr_num & 0xFF);
        addr_num >>= 8;
        addr[1] = (addr_num & 0xFF);
        addr_num >>= 8;
        addr[0] = addr_num;
        addr[4] = 0;
    }

    static unsigned long fourBytesToPtr(byte *addr) {
        unsigned long ret = 0;
        ret = addr[0];
        ret <<= 8;
        ret |= addr[1];
        ret <<= 8;
        ret |= addr[2];
        ret <<= 8;
        ret |= addr[3];
        return ret;
    }

    static int compare(const char *v1, int len1, const char *v2, int len2) {
        int lim = len1;
        if (len2 < len1)
            lim = len2;
        int k = 0;
        while (k < lim) {
            char c1 = v1[k];
            char c2 = v2[k];
            if (c1 < c2) {
                return -1-k;
            } else if (c1 > c2) {
                return k+1;
            }
            k++;
        }
//        if (lim == len1)
//            return lim;
//        else
//            return -lim;
        return len1 - len2;
    }

    static int min(int x, int y) {
        return (x > y ? y : x);
    }

};

#endif
