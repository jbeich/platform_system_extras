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

#ifndef ___FEC_IO_H___
#define ___FEC_IO_H___

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <mincrypt/sha256.h>
#include <mincrypt/rsa.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FEC_BLOCKSIZE 4096
#define FEC_DEFAULT_ROOTS 2

#define FEC_MAGIC 0xFECFECFE
#define FEC_VERSION 0

struct fec_header {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t roots;
    uint32_t fec_size;
    uint64_t inp_size;
    uint8_t hash[SHA256_DIGEST_SIZE];
};

struct fec_status {
    int flags;
    int mode;
    uint64_t errors;
    uint64_t data_size;
    uint64_t size;
};

struct fec_ecc_metadata {
    bool valid;
    uint32_t roots;
    uint64_t blocks;
    uint64_t rounds;
    uint64_t start;
};

struct fec_verity_metadata {
    bool disabled;
    uint64_t data_size;
    uint8_t signature[RSANUMBYTES];
    const char *table;
    uint32_t table_length;
};

enum {
    FEC_FS_EXT4 = 1 << 0,
    FEC_FS_SQUASH = 1 << 1,
    FEC_VERITY_DISABLE = 1 << 8
};

struct fec_handle;

/* file access */
extern int fec_open(struct fec_handle **f, const char *path, int mode,
        int flags, int roots);

extern int fec_close(struct fec_handle *f);

extern int fec_verity_get_metadata(struct fec_handle *f,
        struct fec_verity_metadata *data);

extern int fec_ecc_get_metadata(struct fec_handle *f,
        struct fec_ecc_metadata *data);

extern int fec_get_status(struct fec_handle *f, struct fec_status *s);

extern int fec_seek(struct fec_handle *f, int64_t offset, int whence);

extern ssize_t fec_read(struct fec_handle *f, void *buf, size_t count);

extern ssize_t fec_pread(struct fec_handle *f, void *buf, size_t count,
        uint64_t offset);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ___FEC_IO_H___ */
