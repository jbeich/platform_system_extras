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

extern "C" {
    #include <fec.h>
}

#undef NDEBUG

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <base/file.h>
#include <fec/io.h>
#include <fec/ecc.h>
#include "image.h"

enum {
    MODE_NONE,
    MODE_ENCODE,
    MODE_DECODE,
    MODE_PRINTSIZE
};

static void encode_rs(struct image_proc_ctx *ctx)
{
    struct image *fcx = ctx->ctx;
    int j;
    uint8_t data[fcx->rs_n];
    uint64_t i;

    for (i = ctx->start; i < ctx->end; i += fcx->rs_n) {
        for (j = 0; j < fcx->rs_n; ++j) {
            data[j] = image_get_interleaved_byte(i + j, fcx);
        }

        encode_rs_char(ctx->rs, data, &fcx->fec[ctx->fec_pos]);
        ctx->fec_pos += fcx->roots;
    }
}

static void decode_rs(struct image_proc_ctx *ctx)
{
    struct image *fcx = ctx->ctx;
    int j, rv;
    uint8_t data[fcx->rs_n + fcx->roots];
    uint64_t i;

    assert(sizeof(data) == 255);

    for (i = ctx->start; i < ctx->end; i += fcx->rs_n) {
        for (j = 0; j < fcx->rs_n; ++j) {
            data[j] = image_get_interleaved_byte(i + j, fcx);
        }

        memcpy(&data[fcx->rs_n], &fcx->fec[ctx->fec_pos], fcx->roots);
        rv = decode_rs_char(ctx->rs, data, NULL, 0);

        if (rv < 0) {
            FATAL("failed to recover [%" PRIu64 ", %" PRIu64 ")\n",
                i, i + fcx->rs_n);
        } else if (rv > 0) {
            /* copy corrected data to output */
            for (j = 0; j < fcx->rs_n; ++j) {
                image_set_interleaved_byte(i + j, fcx, data[j]);
            }

            ctx->rv += rv;
        }

        ctx->fec_pos += fcx->roots;
    }
}

static void usage(void)
{
    printf("usage: fec <mode> [ <options> ] <data> <fec> [ <output> ]\n"
           "mode:\n"
           "  -e  --encode                      encode (default)\n"
           "  -d  --decode                      decode\n"
           "  -s, --print-fec-size=<data size>  print FEC size\n"
           "options:\n"
           "  -h                                show this help\n"
           "  -v                                enable verbose logging\n"
           "  -r, --roots=<bytes>               number of parity bytes\n"
           "  -m, --mmap                        use memory mapping\n"
           "  -j, --threads=<threads>           number of threads to use\n"
           "  -S                                treat data as a sparse file\n"
           "decoding options:\n"
           "  -i, --inplace                     correct <data> in place\n"
        );
}

static uint64_t parse_arg(const char *arg, const char *name, uint64_t maxval)
{
    char* endptr;
    errno = 0;

    unsigned long long int value = strtoull(arg, &endptr, 0);

    if (arg[0] == '\0' || *endptr != '\0' ||
            (errno == ERANGE && value == ULLONG_MAX)) {
        FATAL("invalid value of %s\n", name);
    }
    if (value > maxval) {
        FATAL("value of roots too large (max. %" PRIu64 ")\n", maxval);
    }

    return (uint64_t)value;
}

int main(int argc, char **argv)
{
    char *fec_filename;
    char *inp_filename;
    char *out_filename;
    int mode = MODE_NONE;
    struct image ctx;

    image_init(&ctx);
    ctx.roots = FEC_DEFAULT_ROOTS;

    while (1) {
        const static struct option long_options[] = {
            {"help", no_argument, 0, 'h'},
            {"encode", no_argument, 0, 'e'},
            {"decode", no_argument, 0, 'd'},
            {"sparse", no_argument, 0, 'S'},
            {"roots", required_argument, 0, 'r'},
            {"inplace", no_argument, 0, 'i'},
            {"mmap", no_argument, 0, 'm'},
            {"threads", required_argument, 0, 'j'},
            {"fec-size", required_argument, 0, 's'},
            {"verbose", no_argument, 0, 'v'},
            {NULL, 0, 0, 0}
        };
        int c = getopt_long(argc, argv, "hedSr:imj:s:v", long_options, NULL);
        if (c < 0) {
            break;
        }

        switch (c) {
        case 'h':
            usage();
            return 1;
        case 'S':
            ctx.sparse = true;
            break;
        case 'e':
            if (mode == MODE_NONE) {
                mode = MODE_ENCODE;
            } else {
                usage();
                return 1;
            }
            break;
        case 'd':
            if (mode == MODE_NONE) {
                mode = MODE_DECODE;
            } else {
                usage();
                return 1;
            }
            break;
        case 'r':
            ctx.roots = (int)parse_arg(optarg, "roots", 255);
            break;
        case 'i':
            ctx.inplace = true;
            break;
        case 'm':
            ctx.mmap = true;
            break;
        case 'j':
            ctx.threads = (int)parse_arg(optarg, "threads", IMAGE_MAX_THREADS);
            break;
        case 's':
            ctx.inp_size = parse_arg(optarg, "print-fec-size", UINT64_MAX);

            if (mode == MODE_NONE) {
                mode = MODE_PRINTSIZE;
            } else {
                usage();
                return 1;
            }
            break;
        case 'v':
            ctx.verbose = true;
            break;
        case '?':
            usage();
            return 1;
        default:
            abort();
        }
    }

    argc -= optind;
    argv += optind;

    assert(ctx.roots > 0 && ctx.roots < 255);

    if (mode == MODE_PRINTSIZE) {
        /* output size including header */
        printf("%" PRIu64 "\n", fec_ecc_get_size(ctx.inp_size, ctx.roots));
        return 0;
    }

    if (mode == MODE_NONE) {
        mode = MODE_ENCODE;
    }

    if (argc < 2) {
        usage();
        return 1;
    }

    inp_filename = argv[0];
    fec_filename = argv[1];

    if (argc > 2) {
        if (mode != MODE_DECODE || ctx.inplace) {
            usage();
            return 1;
        }

        out_filename = argv[2];
    } else {
        out_filename = NULL;
    }

    if (mode == MODE_ENCODE) {
        if (ctx.inplace) {
            FATAL("invalid parameters: inplace can only used when decoding\n");
        }

        if (!image_load(inp_filename, &ctx, false)) {
            FATAL("failed to read input\n");
        }

        if (!image_ecc_new(fec_filename, &ctx)) {
            FATAL("failed to allocate ecc\n");
        }

        INFO("encoding RS(255, %d) for '%s' to '%s'\n", ctx.rs_n, inp_filename,
            fec_filename);

        if (ctx.verbose) {
            INFO("\traw fec size: %u\n", ctx.fec_size);
            INFO("\tblocks: %" PRIu64 "\n", ctx.blocks);
            INFO("\trounds: %" PRIu64 "\n", ctx.rounds);
        }

        if (!image_process(encode_rs, &ctx)) {
            FATAL("failed to process input\n");
        }

        if (!image_ecc_save(&ctx)) {
            FATAL("failed to write output\n");
        }
    } else {
        if (ctx.inplace && ctx.sparse) {
            FATAL("invalid parameters: inplace cannot be used with sparse "
                "files\n");
        }

        if (!image_ecc_load(fec_filename, &ctx) ||
                !image_load(inp_filename, &ctx, !!out_filename)) {
            FATAL("failed to read input\n");
        }

        if (ctx.inplace) {
            INFO("correcting '%s' using RS(255, %d) from '%s'\n", inp_filename,
                ctx.rs_n, fec_filename);

            out_filename = inp_filename;
        } else {
            INFO("decoding '%s' to '%s' using RS(255, %d) from '%s'\n",
                inp_filename, out_filename ? out_filename : "<none>", ctx.rs_n,
                fec_filename);
        }

        if (ctx.verbose) {
            INFO("\traw fec size: %u\n", ctx.fec_size);
            INFO("\tblocks: %" PRIu64 "\n", ctx.blocks);
            INFO("\trounds: %" PRIu64 "\n", ctx.rounds);
        }

        if (!image_process(decode_rs, &ctx)) {
            FATAL("failed to process input\n");
        }

        if (ctx.rv) {
            INFO("corrected %" PRIu64 " errors\n", ctx.rv);
        } else {
            INFO("no errors found\n");
        }

        if (out_filename && !image_save(out_filename, &ctx)) {
            FATAL("failed to write output\n");
        }
    }

    image_free(&ctx);

    return 0;
}
