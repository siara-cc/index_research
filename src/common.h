#ifndef SIARA_COMMON
#define SIARA_COMMON

#include <string>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <snappy.h>
//#include <brotli/encode.h>

class common {

    public:

        static int read_page(FILE *fp, uint8_t *block, off_t file_pos, size_t bytes) {
            if (!fseek(fp, file_pos, SEEK_SET)) {
                int read_count = fread(block, 1, bytes, fp);
                //printf("read_count: %d, %d, %d, %d, %ld\n", read_count, page_size, disk_page, (int) page_cache[page_size * cache_pos], ftell(fp));
                if (read_count != bytes) {
                    perror("read");
                }
                return read_count;
            } else {
                std::cout << "file_pos: " << file_pos << errno << std::endl;
            }
            return 0;
        }

        static void write_page(FILE *fp, const uint8_t block[], off_t file_pos, size_t bytes) {
            if (fseek(fp, file_pos, SEEK_SET))
                fseek(fp, 0, SEEK_END);
            int write_count = fwrite(block, 1, bytes, fp);
            if (write_count != bytes) {
                printf("Short write: %d\n", write_count);
                throw EIO;
            }
        }

        static FILE *open_fp(const char *filename) {
            FILE *fp = fopen(filename, "r+b");
            if (fp == NULL) {
                fp = fopen(filename, "wb");
                if (fp == NULL)
                    throw errno;
                fclose(fp);
                fp = fopen(filename, "r+b");
                if (fp == NULL)
                    throw errno;
            }
            return fp;
        }

        static void close_fp(FILE *fp) {
            if (fp != NULL)
                fclose(fp);
        }

        #define CMPR_TYPE_SNAPPY 1
        #define CMPR_TYPE_LZ4 2
        #define CMPR_TYPE_DEFLATE 3
        static size_t compress(int8_t type, uint8_t *input_block, size_t sz, std::string& compressed_str) {
            switch (type) {
                case CMPR_TYPE_SNAPPY:
                    return snappy::Compress((char *) input_block, sz, &compressed_str);
            }
            return 0;
        }

        static size_t decompress(int8_t type, uint8_t *input_str, size_t sz, std::string& out_str) {
            switch (type) {
                case CMPR_TYPE_SNAPPY:
                    return snappy::Uncompress((char *) input_str, sz, &out_str) ? out_str.length() : 0;
            }
            return 0;
        }

};

#endif
