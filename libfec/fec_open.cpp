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

#include <linux/fs.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

extern "C" {
    #include <squashfs_utils.h>
    #include <ext4_sb.h>
}

#include "fec_private.h"

typedef uint64_t (*size_func)(uint64_t size, int roots);

static int find_offset(uint64_t file_size, int roots, uint64_t *offset,
        size_func get_appr_size, size_func get_real_size)
{
    check(offset);
    check(get_appr_size);
    check(get_real_size);

    if (file_size % FEC_BLOCKSIZE) {
        /* must be a multiple of block size */
        error("file size not multiple of " stringify(FEC_BLOCKSIZE));
        errno = EINVAL;
        return -1;
    }

    uint64_t mi = get_appr_size(file_size, roots);
    uint64_t lo = file_size - mi * 2;
    uint64_t hi = file_size - mi / 2;

    while (lo < hi) {
        mi = ((hi + lo) / (2 * FEC_BLOCKSIZE)) * FEC_BLOCKSIZE;
        uint64_t total = mi + get_real_size(mi, roots);

        if (total < file_size) {
            lo = mi + FEC_BLOCKSIZE;
        } else if (total > file_size) {
            hi = mi;
        } else {
            *offset = mi;
            debug("file_size = %" PRIu64 " -> offset = %" PRIu64, file_size,
                mi);
            return 0;
        }
    }

    warn("could not determine offset");
    errno = ERANGE;
    return -1;
}

static uint64_t get_appr_ecc_size(uint64_t total_size, int roots)
{
    /* very close, but nearly always slightly too small estimate */
    return fec_round_up((total_size / (FEC_RSM - roots)) * roots,
                FEC_BLOCKSIZE);
}

static int find_ecc_offset(fec_handle *f, uint64_t *offset)
{
    check(f);
    check(offset);

    return find_offset(f->size, f->ecc.roots, offset, get_appr_ecc_size,
                fec_ecc_get_size);
}

static uint64_t get_verity_size(uint64_t size, int)
{
    return VERITY_METADATA_SIZE + verity_get_size(size, NULL, NULL);
}

static int find_verity_offset(fec_handle *f, uint64_t *offset)
{
    check(f);
    check(offset);

    uint64_t size = f->size;

    if (f->ecc.start) {
        size = f->ecc.start - FEC_BLOCKSIZE;
    }

    return find_offset(size, 0, offset, get_verity_size, get_verity_size);
}

static int parse_ecc_header(fec_handle *f, uint64_t offset)
{
    fec_header header;

    check(f);
    check(offset < f->size - sizeof(fec_header));
    check(f->ecc.rsn > 0 && f->ecc.rsn < FEC_RSM);

    debug("offset = %" PRIu64, offset);

    /* there's obviously no ecc data at this point, so there is no need to
       call fec_pread to access this data */
    if (!raw_pread(f, &header, sizeof(fec_header), offset)) {
        error("failed to read: %s", strerror(errno));
        return -1;
    }

    if (header.magic != FEC_MAGIC) {
        return -1;
    }
    if (header.version != FEC_VERSION) {
        error("unsupported ecc version: %u", header.version);
        return -1;
    }
    if (header.size != sizeof(fec_header)) {
        error("unexpected ecc header size: %u", header.size);
        return -1;
    }
    if (header.roots == 0 || header.roots >= FEC_RSM) {
        error("invalid ecc roots: %u", header.roots);
        return -1;
    }
    if (f->ecc.roots != (int)header.roots) {
        error("unexpected number of roots: %d vs %u", f->ecc.roots,
            header.roots);
        return -1;
    }
    if (header.fec_size % header.roots ||
            header.fec_size % FEC_BLOCKSIZE) {
        error("inconsistent ecc size %u", header.fec_size);
        return -1;
    }
    if (offset < header.inp_size ||
            offset + sizeof(fec_header) > header.inp_size + FEC_BLOCKSIZE) {
        error("unexpected input size: %" PRIu64 " vs %" PRIu64, offset,
            header.inp_size);
        return -1;
    }

    if (f->size < header.inp_size + header.fec_size + FEC_BLOCKSIZE) {
        error("file too short for ecc data");
        return -1;
    }

    f->data_size = header.inp_size;
    f->ecc.blocks = fec_div_round_up(f->data_size, FEC_BLOCKSIZE);
    f->ecc.rounds = fec_div_round_up(f->ecc.blocks, f->ecc.rsn);

    if (header.fec_size != f->ecc.rounds * f->ecc.roots * FEC_BLOCKSIZE) {
        error("inconsistent ecc size %u", header.fec_size);
        return -1;
    }

    f->ecc.size = header.fec_size;
    f->ecc.start = FEC_BLOCKSIZE + header.inp_size;

    /* validate encoding data; caller may opt not to use it if invalid */
    SHA256_CTX ctx;
    SHA256_init(&ctx);

    uint8_t buf[FEC_BLOCKSIZE];
    uint32_t n = 0;
    uint32_t len = FEC_BLOCKSIZE;

    while (n < f->ecc.size) {
        if (len > f->ecc.size - n) {
            len = f->ecc.size - n;
        }

        if (!raw_pread(f, buf, len, f->ecc.start + n)) {
            error("failed to read ecc: %s", strerror(errno));
            return -1;
        }

        SHA256_update(&ctx, buf, len);
        n += len;
    }

    f->ecc.valid = !memcmp(SHA256_final(&ctx), header.hash,
                        SHA256_DIGEST_SIZE);

    if (!f->ecc.valid) {
        warn("ecc data not valid");
    }

    return 0;
}

static int parse_ecc(fec_handle *f, uint64_t offset)
{
    check(f);
    check(offset % FEC_BLOCKSIZE == 0);

    /* check the primary header at the beginning of the block */
    if (parse_ecc_header(f, offset) == 0) {
        return 0;
    }

    /* check the backup header at the end of the block */
    if (parse_ecc_header(f, offset + FEC_BLOCKSIZE - sizeof(fec_header)) == 0) {
        warn("using backup ecc header");
        return 0;
    }

    return -1;
}

static int get_squashfs_size(fec_handle *f, uint64_t *offset)
{
    squashfs_info sq;
    size_t sb_size = squashfs_get_sb_size();
    uint8_t buffer[sb_size];

    check(f);
    check(offset);

    if (fec_pread(f, buffer, sizeof(buffer), 0) != (ssize_t)sb_size) {
        error("failed to read superblock: %s", strerror(errno));
        return -1;
    }

    if (squashfs_parse_sb_buffer(buffer, &sq) >= 0) {
        *offset = sq.bytes_used_4K_padded;
        return 0;
    }

    return -1;
}

static int get_ext4_size(fec_handle *f, uint64_t *offset)
{
    check(f);
    check(f->size > 1024 + sizeof(ext4_super_block));
    check(offset);

    ext4_super_block sb;

    if (fec_pread(f, &sb, sizeof(sb), 1024) != sizeof(sb)) {
        error("failed to read superblock: %s", strerror(errno));
        return -1;
    }

    fs_info info;
    info.len = 0;  /* only len is set to 0 to ask the device for real size. */

    if (ext4_parse_sb(&sb, &info) != 0) {
        errno = EINVAL;
        return -1;
    }

    *offset = info.len;
    return 0;
}

static int get_fs_size(fec_handle *f, uint64_t *offset)
{
    int rc = -1;

    check(f);
    check(offset);

    if (f->flags & FEC_FS_EXT4) {
        rc = get_ext4_size(f, offset);
    } else if (f->flags & FEC_FS_SQUASH) {
        rc = get_squashfs_size(f, offset);
    } else {
        /* try both */
        rc = get_ext4_size(f, offset);

        if (rc == 0) {
            debug("found ext4fs");
        } else {
            rc = get_squashfs_size(f, offset);

            if (rc == 0) {
                debug("found squashfs");
            }
        }
    }

    return rc;
}

static int load_verity(fec_handle *f)
{
    check(f);
    debug("flags = %d", f->flags);

    uint64_t offset = 0;

    /* best case: we can locate verity metadata without relying on content */
    if (find_verity_offset(f, &offset) == 0 &&
            verity_parse_header(f, offset) == 0) {
        debug("found at %" PRIu64, offset);
        return 0;
    }

    debug("verity not at %" PRIu64, offset);

    /* worse case: we need to rely on fs superblock not being corrupted beyond
       our error correction ability */
    int rc = get_fs_size(f, &offset);

    if (rc == 0) {
        debug("file system size = %" PRIu64, offset);
        rc = verity_parse_header(f, offset);

        if (rc == 0) {
            debug("found at %" PRIu64, offset);
        }
    }

    return rc;
}

static int load_ecc(fec_handle *f)
{
    check(f);
    debug("size = %" PRIu64, f->size);

    uint64_t offset = 0;

    /* best case: we can locate ecc without relying on content */
    if (find_ecc_offset(f, &offset) == 0 && parse_ecc(f, offset) == 0) {
        debug("found at %" PRIu64, offset);
        return 0;
    }

    debug("ecc not at %" PRIu64, offset);

    /* worse case: we need to rely on fs superblock not being corrupted */
    int rc = get_fs_size(f, &offset);

    if (rc == 0) {
        debug("file system size = %" PRIu64, offset);

        if (parse_ecc(f, offset) == 0) {
            debug("found after fs at %" PRIu64, offset);
            return 0;
        }

        /* if verity metadata exists, ecc data starts after it, give it
           a try */
        offset = offset + get_verity_size(offset, 0);

        if (parse_ecc(f, offset) == 0) {
            debug("found after verity at %" PRIu64, offset);
            return 0;
        }
    }

    /* worst case: we could scan from the end of file until ecc data is found,
       but this seems rarely worthwhile, so give up */
    errno = ENODATA;
    return -1;
}

static int get_size(fec_handle *f)
{
    check(f);

    struct stat st;

    if (fstat(f->fd, &st) == -1) {
        error("fstat failed: %s", strerror(errno));
        return -1;
    }

    if (S_ISBLK(st.st_mode)) {
        debug("block device");

        if (ioctl(f->fd, BLKGETSIZE64, &f->size) == -1) {
            error("ioctl failed: %s", strerror(errno));
            return -1;
        }
    } else if (S_ISREG(st.st_mode)) {
        debug("file");
        f->size = st.st_size;
    } else {
        error("unsupported type %d", (int)st.st_mode);
        errno = EACCES;
        return -1;
    }

    return 0;
}

static void init_handle(fec_handle *f)
{
    memset(&f->ecc, 0, sizeof(f->ecc));
    f->fd = -1;
    f->flags = 0;
    f->mode = 0;
    f->errors = 0;
    f->data_size = 0;
    f->pos = 0;
    f->size = 0;
    memset(&f->verity, 0, sizeof(f->verity));
}

int fec_close(struct fec_handle *f)
{
    if (!f) {
        return 0;
    }

    if (f->fd != -1) {
        if (f->mode & O_RDWR) {
            if (fdatasync(f->fd) == -1) {
                warn("fdatasync failed: %s", strerror(errno));
            }
        }

        TEMP_FAILURE_RETRY(close(f->fd));
    }

    f->cache.clear();
    f->lru.clear();

    if (f->verity.hash) {
        delete[] f->verity.hash;
    }
    if (f->verity.salt) {
        delete[] f->verity.salt;
    }
    if (f->verity.table) {
        delete[] f->verity.table;
    }

    pthread_mutex_destroy(&f->mutex);
    init_handle(f);

    delete f;
    return 0;
}

int fec_verity_get_metadata(struct fec_handle *f, struct fec_verity_metadata *data)
{
    check(f);
    check(data);

    if (!f->verity.start) {
        return -1;
    }

    check(f->data_size < f->size);
    check(f->data_size == f->verity.start);
    check(f->verity.table);

    data->disabled = f->verity.disabled;
    data->data_size = f->verity.start;
    memcpy(data->signature, f->verity.header.signature, sizeof(data->signature));
    data->table = f->verity.table;
    data->table_length = f->verity.header.length;

    return 0;
}

int fec_ecc_get_metadata(struct fec_handle *f, struct fec_ecc_metadata *data)
{
    check(f);
    check(data);

    if (!f->ecc.start) {
        return -1;
    }

    check(f->data_size < f->size);
    check(f->ecc.start >= f->data_size);
    check(f->ecc.start < f->size);
    check(f->ecc.start % FEC_BLOCKSIZE == 0)

    data->valid = f->ecc.valid;
    data->roots = f->ecc.roots;
    data->blocks = f->ecc.blocks;
    data->rounds = f->ecc.rounds;
    data->start = f->ecc.start;

    return 0;
}

int fec_get_status(struct fec_handle *f, struct fec_status *s)
{
    check(f);
    check(s);

    s->flags = f->flags;
    s->mode = f->mode;
    s->errors = f->errors;
    s->data_size = f->data_size;
    s->size = f->size;

    return 0;
}

int fec_open(struct fec_handle **fret, const char *path, int mode, int flags,
        int roots)
{
    check(path);
    check(fret);
    check(roots > 0 && roots < FEC_RSM);

    debug("path = %s, mode = %d, flags = %d, roots = %d", path, mode, flags,
        roots);

    if (mode & (O_CREAT | O_TRUNC | O_EXCL | O_WRONLY)) {
        /* only reading and updating existing files is supported */
        errno = EACCES;
        return -1;
    }

    fec_handle *f = new fec_handle;

    if (unlikely(!f)) {
        errno = ENOMEM;
        return -1;
    }

    init_handle(f);
    *fret = f;

    f->mode = mode;
    f->ecc.roots = roots;
    f->ecc.rsn = FEC_RSM - roots;
    f->flags = flags;

    if (unlikely(pthread_mutex_init(&f->mutex, NULL) != 0)) {
        goto err;
    }

    f->fd = TEMP_FAILURE_RETRY(open(path, mode | O_CLOEXEC));

    if (f->fd == -1) {
        goto err;
    }

    if (get_size(f) == -1) {
        goto err;
    }

    f->data_size = f->size; /* until ecc and/or verity has been loaded */

    if (load_ecc(f) == -1) {
        warn("error-correcting codes not found; cannot recover from data "
            "corruption");
    }

    /* verity metadata is not mandatory for error correction, but being
       able to locate erasures doubles the effectiveness and multiplies
       the performance, so we attempt to locate and load it */

    if (load_verity(f) == -1) {
        warn("verity metadata not found; I/O performance will be slow");
    }

    return 0;

err:
    error("failed: %s", strerror(errno));
    fec_close(f);
    return -1;
}
