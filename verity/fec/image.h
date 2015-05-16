/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __FEC_H__
#define __FEC_H__

#include <fec/io.h>
#include <fec/ecc.h>

#define IMAGE_MIN_THREADS     1
#define IMAGE_MAX_THREADS     128

#define INFO(x...) \
    fprintf(stderr, x);
#define FATAL(x...) { \
    fprintf(stderr, x); \
    exit(1); \
}

#define unlikely(x)    __builtin_expect(!!(x), 0)

struct image {
    bool inplace;
    bool mmap;
    bool sparse;
    bool verbose;
    const char *fec_filename;
    int fec_fd;
    int inp_fd;
    int roots;
    int rs_n;
    int threads;
    uint32_t fec_size;
    uint64_t blocks;
    uint64_t inp_size;
    uint64_t pos;
    uint64_t rounds;
    uint64_t rv;
    uint8_t *fec;
    uint8_t *fec_mmap_addr;
    uint8_t *input;
    uint8_t *output;
};

typedef void (*image_proc_func)(struct image_proc_ctx *);

struct image_proc_ctx {
    image_proc_func func;
    int id;
    struct image *ctx;
    uint64_t rv;
    uint64_t fec_pos;
    uint64_t start;
    uint64_t end;
};

extern bool image_load(const char *filename, struct image *ctx,
        bool output_needed);
extern bool image_save(const char *filename, struct image *ctx);

extern bool image_ecc_new(const char *filename, struct image *ctx);
extern bool image_ecc_load(const char *filename, struct image *ctx);
extern bool image_ecc_save(struct image *ctx);

extern bool image_process(image_proc_func f, struct image *ctx);

extern void image_init(struct image *ctx);
extern void image_free(struct image *ctx);

inline uint8_t image_get_interleaved_byte(uint64_t i, struct image *ctx)
{
    uint64_t offset = fec_ecc_interleave(i, ctx->rs_n, ctx->rounds);

    if (unlikely(offset >= ctx->inp_size)) {
        return 0;
    }

    return ctx->input[offset];
}

inline void image_set_interleaved_byte(uint64_t i, struct image *ctx,
        uint8_t value)
{
    uint64_t offset = fec_ecc_interleave(i, ctx->rs_n, ctx->rounds);

    if (unlikely(offset >= ctx->inp_size)) {
        assert(value == 0);
    } else if (ctx->output && ctx->output[offset] != value) {
        ctx->output[offset] = value;
    }
}

#endif // __FEC_H__
