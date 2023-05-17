#ifndef SIARA_IDX_RSRCH_COMMON
#define SIARA_IDX_RSRCH_COMMON

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <snappy.h>
#include <brotli/encode.h>
#include <brotli/decode.h>
#include <lz4.h>
#include <zlib.h>

#define _FILE_OFFSET_BITS 64
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include <fcntl.h>

#ifdef __APPLE__
    #define O_LARGEFILE 0
    #define POSIX_FADV_RANDOM 4
    #define POSIX_FADV_WILLNEED 3
#endif

class common {

    public:

        static int open_fd(const char *filename) {
            int fd = open(filename, O_RDWR | O_CREAT | O_LARGEFILE, 0644);
            if (fd == -1)
                throw errno;
            return fd;
        }

        static void close_fd(int fd) {
            if (fd != -1)
                close(fd);
        }

        static size_t read_bytes(int fd, uint8_t *block, off_t file_pos, size_t bytes) {
            //printf("Read: %lld, %lu\n", file_pos, bytes);
            size_t read_count = pread(fd, block, bytes, file_pos);
            return read_count;
        }

        static size_t write_bytes(int fd, const uint8_t block[], off_t file_pos, size_t bytes) {
            //printf("Write: %lld, %lu\n", file_pos, bytes);
            size_t write_count = pwrite(fd, block, bytes, file_pos);
            if (write_count != bytes) {
                printf("Write mismatch: %lu, %lu\n", write_count, bytes);
                throw EIO;
            }
            return write_count;
        }

        static void set_fadvise(int fd, int advise) {
#ifdef __APPLE__
            switch (advise) {
                case POSIX_FADV_RANDOM:
                    fcntl(fd, F_RDAHEAD, advise);
                    break;
                case POSIX_FADV_WILLNEED:
                    fcntl(fd, F_RDADVISE, (void*)0);
                    break;
            }
#else
            posix_fadvise(fd, 0, 0, advise);
#endif
        }

        #define CMPR_TYPE_NONE 0
        #define CMPR_TYPE_SNAPPY 1
        #define CMPR_TYPE_LZ4 2
        #define CMPR_TYPE_DEFLATE 3
        #define CMPR_TYPE_BROTLI 4
        #define CMPR_TYPE_LZMA 5
        #define CMPR_TYPE_BZ2 6
        static size_t compress_block(int8_t type, const uint8_t *input_block, size_t sz, uint8_t *out_buf) {
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
                case CMPR_TYPE_DEFLATE:
                    int result = compress(out_buf, &c_size, input_block, sz);
                    if (result != Z_OK) {
                        std::cout << "Uncompress failure: " << result << std::endl;
                        return 0;
                    }
                    return c_size;
            }
            return 0;
        }

        static size_t decompress_block(int8_t type, const uint8_t *input_str, size_t sz, uint8_t *out_buf, int out_size) {
            size_t c_size = out_size;
            switch (type) {
                case CMPR_TYPE_SNAPPY:
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
                    if (BrotliDecoderDecompress(sz, input_str, &c_size, out_buf)) {
                        return c_size;
                    } else {
                        std::cout << "Brotli decompress return 0" << std::endl;
                    }
                    break;
                 case CMPR_TYPE_LZ4:
                    LZ4_decompress_safe((const char *) input_str, (char *) out_buf, sz, c_size);
                    return c_size;
                case CMPR_TYPE_DEFLATE:
                    int result = uncompress(out_buf, &c_size, input_str, sz);
                    if (result != Z_OK) {
                        std::cout << "Uncompress failure: " << result << std::endl;
                        return 0;
                    }
                    return c_size;
            }
            return 0;
        }

};

#endif
