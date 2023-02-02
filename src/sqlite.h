#ifndef SQLITE_H
#define SQLITE_H
#ifndef ARDUINO
#include <cstdio>
#include <cstring>
#include <iostream>
#endif
#include "bplus_tree_handler.h"

using namespace std;

// TODO: decide whether needed
#define page_resv_bytes 5
#define CHKSUM_LEN 3
#define LEN_OF_REC_LEN 3
#define LEN_OF_HDR_LEN 2

const int8_t col_data_lens[] = {0, 1, 2, 3, 4, 6, 8, 8};

enum {SQLT_TYPE_NULL = 0, SQLT_TYPE_INT8, SQLT_TYPE_INT16, SQLT_TYPE_INT24, SQLT_TYPE_INT32, SQLT_TYPE_INT48, SQLT_TYPE_INT64,
        SQLT_TYPE_REAL, SQLT_TYPE_INT0, SQLT_TYPE_INT1, SQLT_TYPE_TEXT = 12, SQLT_TYPE_BLOB = 13};

enum {SQLT_RES_OK = 0, SQLT_RES_ERR = -1, SQLT_RES_INV_PAGE_SZ = -2, 
  SQLT_RES_TOO_LONG = -3, SQLT_RES_WRITE_ERR = -4, SQLT_RES_FLUSH_ERR = -5};

enum {SQLT_RES_SEEK_ERR = -6, SQLT_RES_READ_ERR = -7,
  SQLT_RES_INVALID_SIG = -8, SQLT_RES_MALFORMED = -9,
  SQLT_RES_NOT_FOUND = -10, SQLT_RES_NOT_FINALIZED = -11,
  SQLT_RES_TYPE_MISMATCH = -12, SQLT_RES_INV_CHKSUM = -13,
  SQLT_RES_NEED_1_PK = -14, SQLT_RES_NO_SPACE = -15};

// CRTP see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
class sqlite : public bplus_tree_handler<sqlite> {

    private:
        // Returns how many bytes the given integer will
        // occupy if stored as a variable integer
        int8_t get_vlen_of_uint16(uint16_t vint) {
            return vint > 16383 ? 3 : (vint > 127 ? 2 : 1);
        }

        // Returns how many bytes the given integer will
        // occupy if stored as a variable integer
        int8_t get_vlen_of_uint32(uint32_t vint) {
            return vint > 268435455 ? 5 : (vint > 2097151 ? 4 
                    : (vint > 16383 ? 3 : (vint > 127 ? 2 : 1)));
        }

        // Stores the given uint8_t in the given location
        // in big-endian sequence
        void write_uint8(uint8_t *ptr, uint8_t input) {
            ptr[0] = input;
        }

        // Stores the given uint16_t in the given location
        // in big-endian sequence
        void write_uint16(uint8_t *ptr, uint16_t input) {
            ptr[0] = input >> 8;
            ptr[1] = input & 0xFF;
        }

        // Stores the given uint32_t in the given location
        // in big-endian sequence
        void write_uint32(uint8_t *ptr, uint32_t input) {
            int i = 4;
            while (i--)
                *ptr++ = (input >> (8 * i)) & 0xFF;
        }

        // Stores the given uint64_t in the given location
        // in big-endian sequence
        void write_uint64(uint8_t *ptr, uint64_t input) {
            int i = 8;
            while (i--)
                *ptr++ = (input >> (8 * i)) & 0xFF;
        }

        // Stores the given uint16_t in the given location
        // in variable integer format
        int write_vint16(uint8_t *ptr, uint16_t vint) {
            int len = get_vlen_of_uint16(vint);
            for (int i = len - 1; i > 0; i--)
                *ptr++ = 0x80 + ((vint >> (7 * i)) & 0x7F);
            *ptr = vint & 0x7F;
            return len;
        }

        // Stores the given uint32_t in the given location
        // in variable integer format
        int write_vint32(uint8_t *ptr, uint32_t vint) {
            int len = get_vlen_of_uint32(vint);
            for (int i = len - 1; i > 0; i--)
                *ptr++ = 0x80 + ((vint >> (7 * i)) & 0x7F);
            *ptr = vint & 0x7F;
            return len;
        }

        // Reads and returns big-endian uint8_t
        // at a given memory location
        uint8_t read_uint8(const uint8_t *ptr) {
            return *ptr;
        }

        // Reads and returns big-endian uint16_t
        // at a given memory location
        uint16_t read_uint16(const uint8_t *ptr) {
            return (*ptr << 8) + ptr[1];
        }

        // Reads and returns big-endian uint32_t
        // at a given memory location
        uint32_t read_uint32(const uint8_t *ptr) {
            uint32_t ret;
            ret = ((uint32_t)*ptr++) << 24;
            ret += ((uint32_t)*ptr++) << 16;
            ret += ((uint32_t)*ptr++) << 8;
            ret += *ptr;
            return ret;
        }

        // Reads and returns big-endian uint48_t :)
        // at a given memory location
        uint64_t read_uint48(const uint8_t *ptr) {
            uint64_t ret = 0;
            int len = 6;
            while (len--)
                ret += (*ptr++ << (8 * len));
            return ret;
        }

        // Reads and returns big-endian uint64_t
        // at a given memory location
        uint64_t read_uint64(const uint8_t *ptr) {
            uint64_t ret = 0;
            int len = 8;
            while (len--)
                ret += (*ptr++ << (8 * len));
            return ret;
        }

        // Reads and returns variable integer
        // from given location as uint16_t
        // Also returns the length of the varint
        uint16_t read_vint16(const uint8_t *ptr, int8_t *vlen) {
            uint16_t ret = 0;
            int8_t len = 3; // read max 3 bytes
            do {
                ret <<= 7;
                ret += *ptr & 0x7F;
                len--;
            } while ((*ptr++ & 0x80) == 0x80 && len);
            if (vlen)
                *vlen = 3 - len;
            return ret;
        }

        // Reads and returns variable integer
        // from given location as uint32_t
        // Also returns the length of the varint
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

        // Returns type of column based on given value and length
        // See https://www.sqlite.org/fileformat.html#record_format
        uint32_t derive_col_type_or_len(int type, const void *val, int len) {
            uint32_t col_type_or_len = type;
            if (type > 11)
                col_type_or_len = len * 2 + type;
            return col_type_or_len;    
        }

        // Writes Record length, Row ID and Header length
        // at given location
        // No corruption checking because no unreliable source
        void write_rec_len_rowid_hdr_len(uint8_t *ptr, uint16_t rec_len,
                uint32_t rowid, uint16_t hdr_len) {
            // write record len
            ptr += write_vint32(ptr, rec_len);
            // write row id
            ptr += write_vint32(ptr, rowid);
            // write header len
            *ptr++ = 0x80 + (hdr_len >> 7);
            *ptr = hdr_len & 0x7F;
        }

        // Writes given value at given pointer in Sqlite format
        uint16_t write_data(uint8_t *data_ptr, int type, const void *val, uint16_t len) {
            if (val == NULL)
                return 0;
            if ((type >= SQLT_TYPE_INT8 && type <= SQLT_TYPE_INT64)
                    || type == SQLT_TYPE_INT0 || type == SQLT_TYPE_INT1) {
                switch (len) {
                case 1:
                    write_uint8(data_ptr, *((int8_t *) val));
                    break;
                case 2:
                    write_uint16(data_ptr, *((int16_t *) val));
                    break;
                case 4:
                    write_uint32(data_ptr, *((int32_t *) val));
                    break;
                case 8:
                    write_uint64(data_ptr, *((int64_t *) val));
                    break;
                }
            } else
            if (type == SQLT_TYPE_REAL && len == 4) {
                // Assumes float is represented in IEEE-754 format
                uint64_t bytes64 = float_to_double(val);
                write_uint64(data_ptr, bytes64);
                len = 8;
            } else
            if (type == SQLT_TYPE_REAL && len == 8) {
                // Assumes double is represented in IEEE-754 format
                uint64_t bytes = *((uint64_t *) val);
                write_uint64(data_ptr, bytes);
            } else
                memcpy(data_ptr, val, len);
            return len;
        }

        // Attempts to locate a column using given index
        // Returns position of column in header area, position of column
        // in data area, record length and header length
        // See https://www.sqlite.org/fileformat.html#record_format
        uint8_t *locate_column(uint8_t *rec_ptr, int col_idx, uint8_t **pdata_ptr, 
                    uint16_t *prec_len, uint16_t *phdr_len, uint16_t limit) {
            int8_t vint_len;
            uint8_t *hdr_ptr = rec_ptr;
            *prec_len = read_vint16(hdr_ptr, &vint_len);
            hdr_ptr += vint_len;
            read_vint32(hdr_ptr, &vint_len);
            hdr_ptr += vint_len;
            if (*prec_len + (hdr_ptr - rec_ptr) > limit)
                return NULL; // corruption
            *phdr_len = read_vint16(hdr_ptr, &vint_len);
            if (*phdr_len > limit)
                return NULL; // corruption
            *pdata_ptr = hdr_ptr + *phdr_len;
            uint8_t *data_start_ptr = *pdata_ptr; // re-position to check for corruption below
            hdr_ptr += vint_len;
            for (int i = 0; i < col_idx; i++) {
                uint32_t col_type_or_len = read_vint32(hdr_ptr, &vint_len);
                hdr_ptr += vint_len;
                (*pdata_ptr) += derive_data_len(col_type_or_len);
                if (hdr_ptr >= data_start_ptr)
                return NULL; // corruption or column not found
                if (*pdata_ptr - rec_ptr > limit)
                return NULL; // corruption
            }
            return hdr_ptr;
        }

        // See .h file for API description
        uint32_t derive_data_len(uint32_t col_type_or_len) {
            if (col_type_or_len >= 12) {
                if (col_type_or_len % 2)
                    return (col_type_or_len - 13)/2;
                return (col_type_or_len - 12)/2; 
            } else if (col_type_or_len < 8)
                return col_data_lens[col_type_or_len];
            return 0;
        }

        // Initializes the buffer as a B-Tree Leaf Index
        void init_bt_idx_leaf(uint8_t *ptr) {
            ptr[0] = 10; // Leaf index b-tree page
            write_uint16(ptr + 1, 0); // No freeblocks
            write_uint16(ptr + 3, 0); // No records yet
            write_uint16(ptr + 5, (leaf_block_size == 65536 ? 0 : leaf_block_size - page_resv_bytes)); // No records yet
            write_uint8(ptr + 7, 0); // Fragmented free bytes
            write_uint32(ptr + 8, 0); // right-most pointer
        }

        // Initializes the buffer as a B-Tree Leaf Table
        void init_bt_tbl_leaf(uint8_t *ptr) {
            ptr[0] = 13; // Leaf Table b-tree page
            write_uint16(ptr + 1, 0); // No freeblocks
            write_uint16(ptr + 3, 0); // No records yet
            write_uint16(ptr + 5, (leaf_block_size == 65536 ? 0 : leaf_block_size - page_resv_bytes)); // No records yet
            write_uint8(ptr + 7, 0); // Fragmented free bytes
        }

        int get_offset() {
            return (current_block[0] == 2 || current_block[0] == 5 || current_block[0] == 10 || current_block[0] == 13 ? 0 : 100);
        }

        int write_new_rec(int pos, int col_count, const void *values[], const size_t value_lens[], const uint8_t types[]) {            
            int data_len = 0;
            for (int i = 0; i < col_count; i++)
                data_len += value_lens[i];
            int hdr_len = 0;
            for (int i = 0; i < col_count; i++)
                hdr_len += get_vlen_of_uint16(value_lens[i]);
            int hdr_len_vlen = get_vlen_of_uint32(hdr_len);
            hdr_len += hdr_len_vlen;
            int rec_len = hdr_len + data_len;
            int rec_len_vlen = get_vlen_of_uint32(rec_len);
            int last_pos = read_uint16(current_block + 5);
            if (last_pos == 0)
                last_pos = 65536;
            int ptr_len = read_uint16(current_block + 3) << 1;
            int offset = get_offset();
            if (offset + blk_hdr_len + ptr_len + rec_len + rec_len_vlen >= last_pos)
                return SQLT_RES_NO_SPACE;
            last_pos -= rec_len;
            last_pos -= rec_len_vlen;
            uint8_t *ptr = current_block + last_pos;
            ptr += write_vint32(ptr, rec_len);
            ptr += write_vint32(ptr, hdr_len);
            for (int i = 0; i < col_count; i++) {
                int col_len_in_hdr = (types[i] == SQLT_TYPE_TEXT || types[i] == SQLT_TYPE_BLOB)
                        ? value_lens[i] * 2 + (types[i] == SQLT_TYPE_BLOB ? 13 : 12)
                        : col_data_lens[types[i]];
                ptr += write_vint32(ptr, col_len_in_hdr);
            }
            for (int i = 0; i < col_count; i++) {
                if (value_lens[i] > 0)
                    ptr += write_data(ptr, types[i], values[i], value_lens[i]);
            }
            if (pos >= 0)
                write_uint16(current_block + offset + blk_hdr_len + pos * 2, last_pos);
            return last_pos;
        }

        // Writes data into buffer to form first page of Sqlite db
        int write_page0(FILE *fp, int total_col_count, int pk_col_count, const uint8_t types[],
            const char *col_names[] = NULL, const char *table_name = NULL) {

            int leaf_block_size = leaf_block_size;
            if (leaf_block_size % 512 || leaf_block_size < 512 || leaf_block_size > 65536)
                throw SQLT_RES_INV_PAGE_SZ;

            master_block = (uint8_t *) malloc(leaf_block_size);
            current_block = master_block;

            // 100 uint8_t header - refer https://www.sqlite.org/fileformat.html
            memcpy(current_block, "SQLite format 3\0", 16);
            write_uint16(current_block + 16, leaf_block_size == 65536 ? 1 : (uint16_t) leaf_block_size);
            current_block[18] = 1;
            current_block[19] = 1;
            current_block[20] = 5; // page_resv_bytes;
            current_block[21] = 64;
            current_block[22] = 32;
            current_block[23] = 32;
            //write_uint32(current_block + 24, 0);
            //write_uint32(current_block + 28, 0);
            //write_uint32(current_block + 32, 0);
            //write_uint32(current_block + 36, 0);
            //write_uint32(current_block + 40, 0);
            memset(current_block + 24, '\0', 20); // Set to zero, above 5
            write_uint32(current_block + 28, 2); // TODO: Update during finalize
            write_uint32(current_block + 44, 4);
            //write_uint16(current_block + 48, 0);
            //write_uint16(current_block + 52, 0);
            memset(current_block + 48, '\0', 8); // Set to zero, above 2
            write_uint32(current_block + 56, 1);
            // User version initially 0, set to table leaf count
            // used to locate last leaf page for binary search
            // and move to last page.
            write_uint32(current_block + 60, 0);
            write_uint32(current_block + 64, 0);
            // App ID - set to 0xA5xxxxxx where A5 is signature
            // till it is implemented
            write_uint32(current_block + 68, 0xA5000000);
            memset(current_block + 72, '\0', 20); // reserved space
            write_uint32(current_block + 92, 105);
            write_uint32(current_block + 96, 3016000);
            memset(current_block + 100, '\0', leaf_block_size - 100); // Set remaining page to zero

            // master table b-tree
            init_bt_tbl_leaf(current_block + 100);

            // write table script record
            const char *default_table_name = "idx1";
            if (table_name == NULL)
                table_name = default_table_name;
            // write table script record
            int col_count = 5;
            // if (table_script) {
            //     uint16_t script_len = strlen(table_script);
            //     if (script_len > leaf_block_size - 100 - page_resv_bytes - 8 - 10)
            //         return SQLT_RES_TOO_LONG;
            //     set_col_val(4, SQLT_TYPE_TEXT, table_script, script_len);
            // } else {
                int table_name_len = strlen(table_name);
                int script_len = 13 + table_name_len + 2 + 14 + 15;
                for (int i = 0; i < total_col_count; i++)
                    script_len += strlen(col_names[i]);
                script_len += total_col_count; // commas
                for (int i = 0; i < pk_col_count; i++)
                    script_len += strlen(col_names[i]);
                script_len += pk_col_count; // commas
                if (script_len > leaf_block_size - 100 - page_resv_bytes - 8 - 10)
                    return SQLT_RES_TOO_LONG;
                uint8_t *script_loc = current_block + leaf_block_size - current_block[20] - script_len;
                uint8_t *script_pos = script_loc;
                memcpy(script_pos, "CREATE TABLE ", 13);
                script_pos += 13;
                memcpy(script_pos, table_name, table_name_len);
                script_pos += table_name_len;
                *script_pos++ = ' ';
                *script_pos++ = '(';
                for (int i = 0; i < total_col_count; ) {
                    i++;
                    size_t str_len = strlen(col_names[i]);
                    memcpy(script_pos, col_names[i], str_len);
                    script_pos += str_len;
                    *script_pos++ = ',';
                }
                memcpy(script_pos, " PRIMARY KEY (", 14);
                script_pos += 14;
                for (int i = 0; i < pk_col_count; ) {
                    i++;
                    size_t str_len = strlen(col_names[i]);
                    memcpy(script_pos, col_names[i], str_len);
                    script_pos += str_len;
                    *script_pos++ = (i == pk_col_count ? ')' : ',');
                }
                memcpy(script_pos, ") WITHOUT ROWID", 15);
                script_pos += 15;
            // }
            int32_t root_page_no = 2;
            int res = write_new_rec(0, 5, (const void *[]) {"table", table_name, table_name, &root_page_no, script_loc},
                    (const size_t[]) {5, strlen(table_name), strlen(table_name), sizeof(root_page_no), script_len},
                    (const uint8_t[]) {SQLT_TYPE_TEXT, SQLT_TYPE_TEXT, SQLT_TYPE_TEXT, SQLT_TYPE_INT32, SQLT_TYPE_TEXT});
            if (res < 0)
                throw res;
            res = fwrite(current_block, 1, leaf_block_size, fp);
            if (res != leaf_block_size)
                return SQLT_RES_WRITE_ERR;

            return SQLT_RES_OK;

        }

        // Converts float to Sqlite's Big-endian double
        int64_t float_to_double(const void *val) {
            uint32_t bytes = *((uint32_t *) val);
            uint8_t exp8 = (bytes >> 23) & 0xFF;
            uint16_t exp11 = exp8;
            if (exp11 != 0) {
                if (exp11 < 127)
                exp11 = 1023 - (127 - exp11);
                else
                exp11 = 1023 + (exp11 - 127);
            }
            return ((int64_t)(bytes >> 31) << 63) 
                | ((int64_t)exp11 << 52)
                | ((int64_t)(bytes & 0x7FFFFF) << (52-23) );
        }

        double read_double(const uint8_t *ptr) {
            return (double) *ptr; // TODO: assuming little endian?
        }

        int64_t cvt_to_int64(const uint8_t *ptr, int type) {
            switch (type) {
                case SQLT_TYPE_NULL:
                case SQLT_TYPE_INT0:
                    return 0;
                case SQLT_TYPE_INT1:
                    return 1;
                case SQLT_TYPE_INT8:
                    return ptr[0];
                case SQLT_TYPE_INT16:
                    return read_uint16(ptr);
                case SQLT_TYPE_INT32:
                    return read_uint32(ptr);
                case SQLT_TYPE_INT48:
                    return read_uint48(ptr);
                case SQLT_TYPE_INT64:
                    return read_uint64(ptr);
                case SQLT_TYPE_REAL:
                    return read_double(ptr);
                case SQLT_TYPE_TEXT:
                case SQLT_TYPE_BLOB:
                    return 0; // TODO: do atol?
            }
            return -1; // should not reach here
        }

        // TODO: sqlite seems checking INT_MAX and INT_MIN when converting integers
        double cvt_to_dbl(const uint8_t *ptr, int type) {
            switch (type) {
                case SQLT_TYPE_REAL:
                    return read_double(ptr);
                case SQLT_TYPE_NULL:
                case SQLT_TYPE_INT0:
                    return 0;
                case SQLT_TYPE_INT1:
                    return 1;
                case SQLT_TYPE_INT8:
                    return ptr[0];
                case SQLT_TYPE_INT16:
                    return read_uint16(ptr);
                case SQLT_TYPE_INT32:
                    return read_uint32(ptr);
                case SQLT_TYPE_INT48:
                    return read_uint48(ptr);
                case SQLT_TYPE_INT64:
                    return read_uint64(ptr);
                case SQLT_TYPE_TEXT:
                case SQLT_TYPE_BLOB:
                    return 0; // TODO: do atol?
            }
            return -1; // should not reach here
        }

        int compare_col(const uint8_t *col1, int col_len1, int col_type1,
                            const uint8_t *col2, int col_len2, int col_type2) {
            switch (col_type1) {
                case SQLT_TYPE_TEXT:
                case SQLT_TYPE_BLOB:
                    if (col_type2 == SQLT_TYPE_TEXT || col_type2 == SQLT_TYPE_BLOB)
                        return util::compare(col1, col_len1, col2, col_len2);
                    if (col_type2 == SQLT_TYPE_NULL)
                        return 1;
                    return -1; // incompatible types
                case SQLT_TYPE_REAL: {
                    double col1_dbl = read_double(col1);
                    double col2_dbl = cvt_to_dbl(col2, col_type2);
                    return (col1_dbl < col2_dbl ? -1 : (col1_dbl > col2_dbl ? 1 : 0));
                    }
                case SQLT_TYPE_INT0:
                case SQLT_TYPE_INT1:
                case SQLT_TYPE_INT8:
                case SQLT_TYPE_INT16:
                case SQLT_TYPE_INT32:
                case SQLT_TYPE_INT48:
                case SQLT_TYPE_INT64: {
                    int64_t col1_int64 = cvt_to_int64(col1, col_type1);
                    int64_t col2_int64 = cvt_to_int64(col2, col_type2);
                    return (col1_int64 < col2_int64 ? -1 : (col1_int64 > col2_int64 ? 1 : 0));
                    }
                case SQLT_TYPE_NULL:
                    if (col_type2 == SQLT_TYPE_NULL)
                        return 0;
                    return -1; // NULL is less than any other type?
            }
            return -1; // should not be reached
        }

        int compare_keys(const uint8_t *rec1, int rec1_len, const uint8_t *rec2, int rec2_len) {
            int8_t vlen;
            const uint8_t *ptr1 = rec1;
            int hdr1_len = read_vint32(ptr1, &vlen);
            const uint8_t *data_ptr1 = ptr1 + hdr1_len;
            ptr1 += vlen;
            const uint8_t *ptr2 = rec2;
            int hdr2_len = read_vint32(ptr2, &vlen);
            const uint8_t *data_ptr2 = ptr2 + hdr2_len;
            ptr2 += vlen;
            int cols_compared = 0;
            int cmp = -1;
            for (int i = 0; i < hdr1_len; i++) {
                int col_len1 = read_vint32(ptr1, &vlen);
                int col_type1 = col_len1;
                if (col_len1 >= 12) {
                    if (col_len1 % 2) {
                        col_len1 = (col_len1 - 12) / 2;
                        col_type1 = SQLT_TYPE_TEXT;
                    } else {
                        col_len1 = (col_len1 - 13) / 2;
                        col_type1 = SQLT_TYPE_BLOB;
                    }
                } else
                    col_len1 = col_data_lens[col_len1];
                ptr1 += vlen;
                int col_len2 = read_vint32(ptr2, &vlen);
                int col_type2 = col_len2;
                if (col_len2 >= 12) {
                    if (col_len2 % 2) {
                        col_len2 = (col_len2 - 12) / 2;
                        col_type2 = SQLT_TYPE_TEXT;
                    } else {
                        col_len2 = (col_len2 - 13) / 2;
                        col_type2 = SQLT_TYPE_BLOB;
                    }
                } else
                    col_len2 = col_data_lens[col_len2];
                ptr2 += vlen;
                cmp = compare_col(data_ptr1, col_len1, col_type1,
                            data_ptr2, col_len2, col_type2);
                if (cmp != 0)
                    return cmp;
                cols_compared++;
                if (cols_compared == pk_count)
                    return 0;
                data_ptr1 += col_len1;
                data_ptr2 += col_len2;
            }
            return cmp;
        }

    public:
        uint8_t *master_block;
        int16_t pos;
        int pk_count;
        int col_count;
        const char **column_names;
        const char *table_name;
        int blk_hdr_len;
        sqlite(int total_col_count, int pk_col_count, const uint8_t types[],
                const char *col_names[] = NULL, const char *tbl_name = NULL,
                uint16_t leaf_block_sz = DEFAULT_LEAF_BLOCK_SIZE,
                uint16_t parent_block_sz = DEFAULT_PARENT_BLOCK_SIZE, int cache_sz = 0,
                const char *fname = NULL) : pk_count (pk_col_count),
                    col_count (total_col_count), column_names (col_names),
                    table_name (tbl_name) {
            if (cache_sz > 0) {
                FILE *fp = fopen(fname, "r+b");
                if (fp == NULL) {
                    fp = fopen(fname, "wb");
                    if (fp == NULL)
                        throw errno;
                    fclose(fp);
                    fp = fopen(fname, "r+b");
                    if (fp == NULL)
                        throw errno;
                    write_page0(fp, total_col_count, pk_col_count, types, col_names, table_name);
                }
            }
            bplus_tree_handler<sqlite>(leaf_block_sz, parent_block_sz, cache_sz, fname, 1);
        }

        sqlite(uint32_t block_sz, uint8_t *block) :
        bplus_tree_handler<sqlite>(block_sz, block) {
            init_stats();
        }

        ~sqlite() {
            if (cache_size > 0) {
                write_uint32(master_block + 28, cache->file_page_count);
                cache->write_page(master_block, 0, leaf_block_size);
            }
            free(master_block);
        }

        inline void setCurrentBlockRoot() {
            setCurrentBlock(root_block);
        }

        inline void setCurrentBlock(uint8_t *m) {
            current_block = m;
            blk_hdr_len = (current_block[0] == 10 || current_block[0] == 13 ? 8 : 12);
        }

        inline int16_t searchCurrentBlock() {
            int middle, first, filled_size, cmp;
            first = 0;
            filled_size = read_uint16(current_block + 3);
            while (first < filled_size) {
                middle = (first + filled_size) >> 1;
                key_at = current_block + read_uint16(current_block + blk_hdr_len + middle * 2);
                key_at_len = read_vint32(key_at, NULL);
                key_at += key_at_len;
                if (key[0] == 0 && value_len == 0) {
                    cmp = compare_keys(key_at, key_at_len, key + 1, key_len);
                } else {
                    int8_t hdr_vlen;
                    uint8_t *given_key = key_at + read_vint32(key_at, &hdr_vlen);
                    cmp = util::compare(given_key, (read_vint32(key_at + hdr_vlen, NULL) - 12) / 2,
                                        key, key_len);
                }
                if (cmp < 0)
                    first = middle + 1;
                else if (cmp > 0)
                    filled_size = middle;
                else
                    return middle;
            }
            return ~filled_size;
        }

        inline uint8_t *getChildPtrPos(int16_t search_result) {
            if (search_result < 0) {
                search_result++;
                search_result = ~search_result;
            }
            return current_block + getPtr(search_result);
        }

        inline uint8_t *getPtrPos() {
            return current_block + blk_hdr_len; // TODO: fix
        }

        inline int getHeaderSize() {
            return blk_hdr_len;
        }

        void remove_entry(int16_t pos) {
            delPtr(pos);
        }

        void makeSpace() {
            int block_size = (isLeaf() ? leaf_block_size : parent_block_size);
            int lvl = current_block[0] & 0x1F;
            const uint16_t data_size = block_size - getKVLastPos();
            uint8_t data_buf[data_size];
            uint16_t new_data_len = 0;
            int16_t new_idx;
            int16_t orig_filled_size = filledSize();
            for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
                uint16_t src_idx = getPtr(new_idx);
                uint16_t kv_len = current_block[src_idx];
                kv_len++;
                kv_len += current_block[src_idx + kv_len];
                kv_len++;
                new_data_len += kv_len;
                memcpy(data_buf + data_size - new_data_len, current_block + src_idx, kv_len);
                setPtr(new_idx, block_size - new_data_len);
            }
            uint16_t new_kv_last_pos = block_size - new_data_len;
            memcpy(current_block + new_kv_last_pos, data_buf + data_size - new_data_len, new_data_len);
            //printf("%d, %d\n", data_size, new_data_len);
            setKVLastPos(new_kv_last_pos);
            searchCurrentBlock();
        }

        bool isFull(int16_t search_result) { // TODO: fix
            int16_t ptr_size = filledSize() + 1;
            if (getKVLastPos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2)) {
                makeSpace();
                if (getKVLastPos() <= (BLK_HDR_SIZE + ptr_size + key_len + value_len + 2))
                    return true;
            }
            return false;
        }

        uint8_t *split(uint8_t *first_key, int16_t *first_len_ptr) {
            int16_t orig_filled_size = filledSize();
            uint32_t BASIX_NODE_SIZE = isLeaf() ? leaf_block_size : parent_block_size;
            int lvl = current_block[0] & 0x1F;
            uint8_t *b = allocateBlock(BASIX_NODE_SIZE, isLeaf(), lvl);
            sqlite new_block(this->leaf_block_size, b);
            new_block.BPT_MAX_KEY_LEN = BPT_MAX_KEY_LEN;
            uint16_t kv_last_pos = getKVLastPos();
            uint16_t halfKVLen = BASIX_NODE_SIZE - kv_last_pos + 1;
            halfKVLen /= 2;

            int16_t brk_idx = -1;
            uint16_t brk_kv_pos;
            uint16_t tot_len;
            brk_kv_pos = tot_len = 0;
            // Copy all data to new block in ascending order
            int16_t new_idx;
            for (new_idx = 0; new_idx < orig_filled_size; new_idx++) {
                uint16_t src_idx = getPtr(new_idx);
                uint16_t kv_len = current_block[src_idx];
                kv_len++;
                kv_len += current_block[src_idx + kv_len];
                kv_len++;
                tot_len += kv_len;
                memcpy(new_block.current_block + kv_last_pos, current_block + src_idx, kv_len);
                new_block.insPtr(new_idx, kv_last_pos);
                kv_last_pos += kv_len;
                if (brk_idx == -1) {
                    if (tot_len > halfKVLen || new_idx == (orig_filled_size / 2)) {
                        brk_idx = new_idx + 1;
                        brk_kv_pos = kv_last_pos;
                        uint16_t first_idx = getPtr(new_idx + 1);
                        if (isLeaf()) {
                            int len = 0;
                            while (current_block[first_idx + len + 1] == current_block[src_idx + len + 1])
                                len++;
                            *first_len_ptr = len + 1;
                            memcpy(first_key, current_block + first_idx + 1, *first_len_ptr);
                        } else {
                            *first_len_ptr = current_block[first_idx];
                            memcpy(first_key, current_block + first_idx + 1, *first_len_ptr);
                        }
                    }
                }
            }
            //memset(current_block + BLK_HDR_SIZE, '\0', BASIX_NODE_SIZE - BLK_HDR_SIZE);
            kv_last_pos = getKVLastPos();
            uint16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
            memcpy(current_block + BASIX_NODE_SIZE - old_blk_new_len, new_block.current_block + kv_last_pos,
                    old_blk_new_len); // Copy back first half to old block
            //memset(new_block.current_block + kv_last_pos, '\0', old_blk_new_len);
            int diff = (BASIX_NODE_SIZE - brk_kv_pos);
            for (new_idx = 0; new_idx <= brk_idx; new_idx++) {
                setPtr(new_idx, new_block.getPtr(new_idx) + diff);
            } // Set index of copied first half in old block

            {
                uint16_t old_blk_new_len = brk_kv_pos - kv_last_pos;
                memcpy(current_block + BASIX_NODE_SIZE - old_blk_new_len,
                        new_block.current_block + kv_last_pos, old_blk_new_len); // Copy back first half to old block
                setKVLastPos(BASIX_NODE_SIZE - old_blk_new_len);
                setFilledSize(brk_idx);
            }

            {
                int16_t new_size = orig_filled_size - brk_idx;
                uint8_t *block_ptrs = new_block.current_block + blk_hdr_len;
                new_block.setKVLastPos(brk_kv_pos);
                new_block.setFilledSize(new_size);
            }

            return b;
        }

        void addData(int16_t search_result) {

            uint16_t kv_last_pos = getKVLastPos();
            if (key[0] == 0 && value_len == 0) {
                kv_last_pos -= key_len;
                kv_last_pos -= 2;
                setKVLastPos(kv_last_pos);
                uint8_t *ptr = current_block + kv_last_pos;
                memcpy(ptr, key + 1, key_len);
            } else {
                int rec_len = (key_len + value_len + 2);
                int key_len_vlen = get_vlen_of_uint32(key_len * 2 + 12);
                int value_len_vlen = get_vlen_of_uint32(value_len * 2 + 12);
                int hdr_len = key_len_vlen + value_len_vlen;
                int hdr_len_vlen = get_vlen_of_uint32(hdr_len);
                hdr_len += hdr_len_vlen;
                rec_len += hdr_len;
                kv_last_pos -= rec_len;
                setKVLastPos(kv_last_pos);
                uint8_t *ptr = current_block + kv_last_pos;
                ptr += write_vint32(ptr, rec_len);
                ptr += write_vint32(ptr, hdr_len);
                ptr += write_vint32(ptr, key_len_vlen);
                ptr += write_vint32(ptr, value_len_vlen);
                memcpy(ptr, key, key_len);
                memcpy(ptr, value, value_len);
            }

            int16_t filled_size = read_uint16(current_block + 3);
            uint8_t *kv_idx = current_block + blk_hdr_len + search_result * 2;
            memmove(kv_idx + 2, kv_idx, (filled_size - search_result) * 2);
            write_uint16(current_block + 3, filled_size + 1);
            write_uint16(kv_idx, kv_last_pos);
            // if (BPT_MAX_KEY_LEN < key_len)
            //     BPT_MAX_KEY_LEN = key_len;

        }

        void addFirstData() {
            addData(0);
        }

        uint8_t *findSplitSource(int16_t search_result) {
            return NULL;
        }

};

#endif
