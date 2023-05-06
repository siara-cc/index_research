#ifndef SIARA_IDX_RSRCH_COMMON
#define SIARA_IDX_RSRCH_COMMON

#include <string>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <snappy.h>
#include <brotli/encode.h>
#include <brotli/decode.h>

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

        #define CMPR_TYPE_NONE 0
        #define CMPR_TYPE_SNAPPY 1
        #define CMPR_TYPE_LZ4 2
        #define CMPR_TYPE_DEFLATE 3
        #define CMPR_TYPE_BROTLI 4
        #define CMPR_TYPE_LZMA 5
        #define CMPR_TYPE_BZ2 6
        static size_t compress(int8_t type, uint8_t *input_block, size_t sz, std::string& compressed_str) {
            size_t c_size;
            switch (type) {
                case CMPR_TYPE_SNAPPY:
                    return snappy::Compress((char *) input_block, sz, &compressed_str);
                case CMPR_TYPE_BROTLI:
                    c_size = sz * 2;
                    compressed_str.resize(c_size);
                    if (BrotliEncoderCompress(BROTLI_DEFAULT_QUALITY, BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE,
                        sz, input_block, &c_size, (uint8_t *) compressed_str.c_str())) {
                            compressed_str.resize(c_size);
                            return c_size;
                    } else {
                        std::cout << "Brotli compress return 0" << std::endl;
                        compressed_str.resize(0);
                    }
                    break;
            }
            return 0;
        }

        static size_t decompress(int8_t type, uint8_t *input_str, size_t sz, std::string& out_str, int out_size) {
            size_t c_size;
            switch (type) {
                case CMPR_TYPE_SNAPPY:
                    return snappy::Uncompress((char *) input_str, sz, &out_str) ? out_str.length() : 0;
                case CMPR_TYPE_BROTLI:
                    out_str.resize(out_size);
                    c_size = out_size;
                    if (BrotliDecoderDecompress(sz, input_str, &c_size, (uint8_t *) out_str.c_str())) {
                        out_str.resize(c_size);
                        return c_size;
                    } else {
                        std::cout << "Brotli decompress return 0" << std::endl;
                        out_str.resize(0);
                    }
            }
            return 0;
        }

};

#endif
