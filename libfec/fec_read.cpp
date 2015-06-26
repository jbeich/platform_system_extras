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

#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>

extern "C" {
    #include <fec.h>
}

#include "fec_private.h"

typedef std::unique_ptr<void, decltype(&free_rs_char)> rs_unique_ptr;

/* prints a hexdump of the given buffer using warn */
static void dump(const char *name, uint64_t value, const uint8_t *data,
        size_t size)
{
    const int bytes_per_line = 16;
    char hex[bytes_per_line * 3 + 1];
    char prn[bytes_per_line + 1];

    warn("%s (%" PRIu64 ") (%zu bytes):", name ? name : "", value, size);

    if (!data) {
        warn("    (null)");
        return;
    }

    for (size_t n = 0; n < size; n += bytes_per_line) {
        memset(hex, 0, sizeof(hex));
        memset(prn, 0, sizeof(prn));

        for (size_t m = 0; m < bytes_per_line; ++m) {
            if (n + m < size) {
                sprintf(&hex[m * 3], "%02x ", data[n + m]);

                if (isprint(data[n + m])) {
                    prn[m] = data[n + m];
                } else {
                    prn[m] = '.';
                }
            } else {
                strcpy(&hex[m * 3], "   ");
            }
        }

        warn("    %04zu   %s  %s", n, hex, prn);
    }
}

static inline bool is_erasure(fec_handle *f, uint64_t interleaved,
        uint8_t *data)
{
    /* ideally, we would like to know if a specific byte on this block has
       been corrupted, but knowing whether any of them is can be useful as
       well, because often the entire block is corrupted */

    uint64_t n = interleaved / FEC_BLOCKSIZE;

    return !verity_check_block(f, n, &f->verity.hash[n * SHA256_DIGEST_SIZE],
                data);
}

static int __ecc_read(fec_handle *f, void *rs, uint8_t *dest, uint64_t offset,
        bool use_erasures, uint8_t *ecc_data, size_t *errors)
{
    check(offset % FEC_BLOCKSIZE == 0);
    ecc_info *e = &f->ecc;

    /* reverse interleaving: calculate the RS block that includes the requested
       offset */
    uint64_t rsb = offset - (offset / (e->rounds * FEC_BLOCKSIZE)) *
                        e->rounds * FEC_BLOCKSIZE;
    int data_index = -1;
    int erasures[e->rsn];
    int neras = 0;

    /* verity is required to check for erasures */
    check(!use_erasures || f->verity.hash);

    for (int i = 0; i < e->rsn; ++i) {
        uint64_t interleaved = fec_ecc_interleave(rsb * e->rsn + i, e->rsn,
                                    e->rounds);

        if (interleaved == offset) {
            data_index = i;
        }

        /* copy raw data to reconstruct the RS block */
        uint8_t bbuf[FEC_BLOCKSIZE];

        if (unlikely(interleaved >= e->start - FEC_BLOCKSIZE)) {
            memset(bbuf, 0, FEC_BLOCKSIZE);
        } else {
            if (!raw_pread(f, bbuf, FEC_BLOCKSIZE, interleaved)) {
                error("failed to read: %s", strerror(errno));
                return -1;
            }

            if (use_erasures && neras <= e->roots &&
                    interleaved < f->verity.start &&
                    is_erasure(f, interleaved, bbuf)) {
                erasures[neras++] = i;
            }
        }

        for (int j = 0; j < FEC_BLOCKSIZE; ++j) {
            ecc_data[j * FEC_RSM + i] = bbuf[j];
        }
    }

    check(data_index >= 0);

    size_t nerrs = 0;
    uint8_t copy[FEC_RSM];

    for (int i = 0; i < FEC_BLOCKSIZE; ++i) {
        /* copy parity data */
        if (!raw_pread(f, &ecc_data[i * FEC_RSM + e->rsn], e->roots,
                e->start + (i + rsb) * e->roots)) {
            error("failed to read ecc data: %s", strerror(errno));
            return -1;
        }

        /* for debugging decoding failures, because decode_rs_char can mangle
           ecc_data */
        if (unlikely(use_erasures)) {
            memcpy(copy, &ecc_data[i * FEC_RSM], FEC_RSM);
        }

        /* decode */
        int rc = decode_rs_char(rs, &ecc_data[i * FEC_RSM], erasures, neras);

        if (unlikely(rc < 0)) {
            if (use_erasures) {
                error("RS block %" PRIu64 ": decoding failed (%d erasures)",
                    rsb, neras);
                dump("raw RS block", rsb, copy, FEC_RSM);
            } else if (!f->verity.hash) {
                warn("RS block %" PRIu64 ": decoding failed", rsb);
            } else {
                debug("RS block %" PRIu64 ": decoding failed", rsb);
            }

            errno = EIO;
            return -1;
        } else if (unlikely(rc > 0)) {
            check(rc <= (use_erasures ? e->roots : e->roots / 2));
            nerrs += rc;
        }

        dest[i] = ecc_data[i * FEC_RSM + data_index];
    }

    if (nerrs) {
        warn("RS block %" PRIu64 ": corrected %zu errors", rsb, nerrs);
        *errors += nerrs;
    }

    return FEC_BLOCKSIZE;
}

static int ecc_init(fec_handle *f, rs_unique_ptr& rs,
        std::unique_ptr<uint8_t[]>& ecc_data)
{
    check(f);

    rs.reset(init_rs_char(FEC_PARAMS(f->ecc.roots)));

    if (unlikely(!rs)) {
        error("failed to initialize RS");
        errno = ENOMEM;
        return -1;
    }

    ecc_data.reset(new (std::nothrow) uint8_t[FEC_RSM * FEC_BLOCKSIZE]);

    if (unlikely(!ecc_data)) {
        error("failed to allocate ecc buffer");
        errno = ENOMEM;
        return -1;
    }

    return 0;
}

static ssize_t ecc_read(fec_handle *f, uint8_t *dest, size_t count,
        uint64_t offset, size_t *errors)
{
    check(f);
    check(dest);
    check(offset < f->data_size);
    check(offset + count <= f->data_size);
    check(errors);

    debug("[%" PRIu64 ", %" PRIu64 ")", offset, offset + count);

    rs_unique_ptr rs(NULL, free_rs_char);
    std::unique_ptr<uint8_t[]> ecc_data;

    if (ecc_init(f, rs, ecc_data) == -1) {
        return -1;
    }

    uint64_t curr = offset / FEC_BLOCKSIZE;
    size_t coff = (size_t)(offset - curr * FEC_BLOCKSIZE);
    size_t left = count;

    uint8_t data[FEC_BLOCKSIZE];

    while (left > 0) {
        if (__ecc_read(f, rs.get(), data, curr * FEC_BLOCKSIZE, false,
                ecc_data.get(), errors) == -1) {
            return -1;
        }

        size_t copy = FEC_BLOCKSIZE - coff;

        if (copy > left) {
            copy = left;
        }

        memcpy(dest, &data[coff], copy);

        dest += copy;
        left -= copy;
        coff = 0;
        ++curr;
    }

    return count;
}

static ssize_t verity_read(fec_handle *f, uint8_t *dest, size_t count,
        uint64_t offset, size_t *errors)
{
    check(f);
    check(dest);
    check(offset < f->data_size);
    check(offset + count <= f->data_size);
    check(f->verity.hash);
    check(errors);

    debug("[%" PRIu64 ", %" PRIu64 ")", offset, offset + count);

    rs_unique_ptr rs(NULL, free_rs_char);
    std::unique_ptr<uint8_t[]> ecc_data;

    if (f->ecc.start && ecc_init(f, rs, ecc_data) == -1) {
        return -1;
    }

    uint64_t curr = offset / FEC_BLOCKSIZE;
    size_t coff = (size_t)(offset - curr * FEC_BLOCKSIZE);
    size_t left = count;
    uint8_t data[FEC_BLOCKSIZE];

    while (left > 0) {
        /* copy raw data without error correction */
        if (!raw_pread(f, data, FEC_BLOCKSIZE, curr * FEC_BLOCKSIZE)) {
            error("failed to read: %s", strerror(errno));
            return -1;
        }

        check(curr * SHA256_DIGEST_SIZE <=
            f->verity.hash_data_blocks * FEC_BLOCKSIZE - SHA256_DIGEST_SIZE);

        if (likely(verity_check_block(f, curr,
                &f->verity.hash[curr * SHA256_DIGEST_SIZE], data))) {
            goto valid;
        }

        if (!f->ecc.start) {
            /* fatal error without ecc */
            error("[%" PRIu64 ", %" PRIu64 "): corrupted block %" PRIu64,
                offset, offset + count, curr);
            return -1;
        } else {
            debug("[%" PRIu64 ", %" PRIu64 "): corrupted block %" PRIu64,
                offset, offset + count, curr);
        }

        /* try to correct without erasures first, because checking for
           erasure locations is slower */
        if (__ecc_read(f, rs.get(), data, curr * FEC_BLOCKSIZE, false,
                ecc_data.get(), errors) == FEC_BLOCKSIZE &&
            verity_check_block(f, VERITY_NO_CACHE,
                &f->verity.hash[curr * SHA256_DIGEST_SIZE], data)) {
            goto corrected;
        }

        /* try to correct with erasures */
        if (__ecc_read(f, rs.get(), data, curr * FEC_BLOCKSIZE, true,
                ecc_data.get(), errors) == FEC_BLOCKSIZE &&
            verity_check_block(f, VERITY_NO_CACHE,
                &f->verity.hash[curr * SHA256_DIGEST_SIZE], data)) {
            goto corrected;
        }

        error("[%" PRIu64 ", %" PRIu64 "): corrupted block %" PRIu64
            " (offset %" PRIu64 ") cannot be recovered",
            offset, offset + count, curr, curr * FEC_BLOCKSIZE);
        dump("decoded block", curr, data, FEC_BLOCKSIZE);

        errno = EIO;
        return -1;

corrected:
        /* update the corrected block to the file if we are in r/w mode */
        if (f->mode & O_RDWR) {
            if (!raw_pwrite(f, data, FEC_BLOCKSIZE,
                    curr * FEC_BLOCKSIZE)) {
                error("failed to write: %s", strerror(errno));
                return -1;
            }
        }

valid:
        size_t copy = FEC_BLOCKSIZE - coff;

        if (copy > left) {
            copy = left;
        }

        memcpy(dest, &data[coff], copy);

        dest += copy;
        left -= copy;
        coff = 0;
        ++curr;
    }

    return count;
}

int fec_seek(struct fec_handle *f, int64_t offset, int whence)
{
    check(f);

    if (whence == SEEK_SET) {
        if (offset < 0) {
            errno = EOVERFLOW;
            return -1;
        }

        f->pos = offset;
    } else if (whence == SEEK_CUR) {
        if (offset < 0 && f->pos < (uint64_t)-offset) {
            errno = EOVERFLOW;
            return -1;
        } else if (offset > 0 && (uint64_t)offset > UINT64_MAX - f->pos) {
            errno = EOVERFLOW;
            return -1;
        }

        f->pos += offset;
    } else if (whence == SEEK_END) {
        if (offset >= 0) {
            errno = ENXIO;
            return -1;
        } else if ((uint64_t)-offset > f->size) {
            errno = EOVERFLOW;
            return -1;
        }

        f->pos = f->size + offset;
    } else {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

ssize_t fec_read(struct fec_handle *f, void *buf, size_t count)
{
    ssize_t rc = fec_pread(f, buf, count, f->pos);

    if (rc > 0) {
        check(f->pos < UINT64_MAX - rc);
        f->pos += rc;
    }

    return rc;
}

static inline size_t get_max_count(uint64_t offset, size_t count, uint64_t max)
{
    if (offset >= max) {
        return 0;
    } else if (offset + count > max) {
        return (size_t)(max - offset);
    }

    return count;
}

bool raw_pread(fec_handle *f, void *buf, size_t count, uint64_t offset)
{
    check(f);
    check(buf);

    uint8_t *p = (uint8_t *)buf;
    size_t remaining = count;

    while (remaining > 0) {
        ssize_t n = TEMP_FAILURE_RETRY(pread64(f->fd, p, remaining, offset));

        if (n <= 0) {
            return false;
        }

        p += n;
        remaining -= n;
        offset += n;
    }

    return true;
}

bool raw_pwrite(fec_handle *f, const void *buf, size_t count, uint64_t offset)
{
    check(f);
    check(buf);

    const uint8_t *p = (const uint8_t *)buf;
    size_t remaining = count;

    while (remaining > 0) {
        ssize_t n = TEMP_FAILURE_RETRY(pwrite64(f->fd, p, remaining, offset));

        if (n <= 0) {
            return false;
        }

        p += n;
        remaining -= n;
        offset += n;
    }

    return true;
}

ssize_t fec_pread(struct fec_handle *f, void *buf, size_t count,
        uint64_t offset)
{
    check(f);
    check(buf);

    if (unlikely(offset > UINT64_MAX - count)) {
        errno = EOVERFLOW;
        return -1;
    }

    if (f->verity.hash) {
        check(f->verity.start < f->size);

        return process(f, (uint8_t *)buf,
                    get_max_count(offset, count, f->verity.start), offset,
                    verity_read);
    } else if (f->ecc.start) {
        check(f->ecc.start < f->size);

        count = get_max_count(offset, count, f->data_size);
        ssize_t rc = process(f, (uint8_t *)buf, count, offset, ecc_read);

        if (rc >= 0) {
            return rc;
        }

        /* return raw data if pure ecc read fails; due to interleaving
           the specific blocks the caller wants may still be fine */
    } else {
        count = get_max_count(offset, count, f->size);
    }

    if (raw_pread(f, buf, count, offset)) {
        return count;
    }

    return -1;
}
