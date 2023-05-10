#ifndef SIARA_IDX_RSRCH_COMMON
#define SIARA_IDX_RSRCH_COMMON

#include <string>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <snappy.h>
#include <brotli/encode.h>
#include <brotli/decode.h>
#include <lz4.h>

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
        static size_t compress(int8_t type, const uint8_t *input_block, size_t sz, uint8_t *out_buf) {
            size_t c_size = 65536;
            switch (type) {
                case CMPR_TYPE_SNAPPY:
                    snappy::RawCompress((const char *) input_block, sz, (char *) out_buf, &c_size);
                    return c_size;
                case CMPR_TYPE_BROTLI:
                    if (BrotliEncoderCompress(BROTLI_DEFAULT_QUALITY, BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE,
                                        sz, input_block, &c_size, out_buf)) {
                            return c_size;
                    } else {
                        std::cout << "Brotli compress return 0" << std::endl;
                    }
                    break;
                case CMPR_TYPE_LZ4:
                    c_size = LZ4_compress_default((const char *) input_block, (char *) out_buf, sz, c_size);
                    return c_size;
            }
            return 0;
        }

        static size_t decompress(int8_t type, const uint8_t *input_str, size_t sz, uint8_t *out_buf, int out_size) {
            size_t c_size;
            switch (type) {
                case CMPR_TYPE_SNAPPY:
                    c_size = 0;
                    if (!snappy::GetUncompressedLength((const char *) input_str, sz, &c_size)) {
                    	std::cout << "Snappy GetUncompressedLength failure" << std::endl;
                        return 0;
                    }
                    if (c_size != out_size) {
                    	std::cout << "Snappy GetUncompressedLength mismatch: " << c_size << ", " << out_size << std::endl;
                        return 0;
                    }
                    if (snappy::RawUncompress((const char *) input_str, sz, (char *) out_buf))
                        return c_size;
                    else
                    	std::cout << "Snappy uncompress failure" << std::endl;
                    break;
                case CMPR_TYPE_BROTLI:
                    c_size = out_size;
                    if (BrotliDecoderDecompress(sz, input_str, &c_size, out_buf)) {
                        return c_size;
                    } else {
                        std::cout << "Brotli decompress return 0" << std::endl;
                    }
                    break;
                 case CMPR_TYPE_LZ4:
                    c_size = out_size;
                    LZ4_decompress_safe((const char *) input_str, (char *) out_buf, sz, c_size);
                    return c_size;
            }
            return 0;
        }

};

#endif
