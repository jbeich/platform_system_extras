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

#include <ctype.h>
#include <stdlib.h>
#include <base/strings.h>
#include "fec_private.h"

static inline int hextobin(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else {
        errno = EINVAL;
        return -1;
    }
}

static int parse_hex(uint8_t *dst, uint32_t size, const char *src)
{
    int l, h;

    check(dst);
    check(src);
    check(size == strlen(src) / 2);

    while (size--) {
        h = hextobin(tolower(*src++));
        l = hextobin(tolower(*src++));

        check(l >= 0);
        check(h >= 0);

        *dst++ = (h << 4) | l;
    }

    return 0;
}

static int parse_uint64(const char *src, uint64_t maxval, uint64_t *dst)
{
    char *end;
    unsigned long long int value;

    check(src);
    check(dst);

    errno = 0;
    value = strtoull(src, &end, 0);

    if (*src == '\0' || *end != '\0' ||
            (errno == ERANGE && value == ULLONG_MAX)) {
        return -1;
    }

    if (maxval && value > maxval) {
        errno = EINVAL;
        return -1;
    }

   *dst = (uint64_t)value;
    return 0;
}

uint64_t verity_get_size(uint64_t file_size, uint32_t *verity_levels,
        uint32_t *level_hashes)
{
    /* we assume a known metadata size, 4 KiB block size, and SHA-256 to avoid
       relying on disk content */

    uint32_t level = 0;
    uint64_t total = 0;
    uint64_t hashes = file_size / FEC_BLOCKSIZE;

    do {
        if (level_hashes) {
            level_hashes[level] = hashes;
        }

        hashes = fec_div_round_up(hashes * SHA256_DIGEST_SIZE, FEC_BLOCKSIZE);
        total += hashes;

        ++level;
    } while (hashes > 1);

    if (verity_levels) {
        *verity_levels = level;
    }

    return total * FEC_BLOCKSIZE;
}

bool verity_check_block(fec_handle *f, uint64_t index, const uint8_t *expected,
        const uint8_t *block)
{
    if (index != VERITY_NO_CACHE) {
        pthread_mutex_lock(&f->mutex);
        auto cached = f->cache.find(index);

        if (cached != f->cache.end()) {
            verity_block_info vbi = *(cached->second);

            f->lru.erase(cached->second);
            f->lru.push_front(vbi);
            f->cache[index] = f->lru.begin();

            pthread_mutex_unlock(&f->mutex);
            return vbi.valid;
        }

        pthread_mutex_unlock(&f->mutex);
    }

    SHA256_CTX ctx;
    SHA256_init(&ctx);
    SHA256_update(&ctx, f->verity.salt, f->verity.salt_size);
    SHA256_update(&ctx, block, FEC_BLOCKSIZE);

    bool valid = !memcmp(expected, SHA256_final(&ctx), SHA256_DIGEST_SIZE);

    if (index != VERITY_NO_CACHE) {
        pthread_mutex_lock(&f->mutex);

        verity_block_info vbi;
        vbi.index = index;
        vbi.valid = valid;

        if (f->lru.size() >= VERITY_CACHE_BLOCKS) {
            f->cache.erase(f->lru.rbegin()->index);
            f->lru.pop_back();
        }

        f->lru.push_front(vbi);
        f->cache[index] = f->lru.begin();
        pthread_mutex_unlock(&f->mutex);
    }

    return valid;
}

static bool ecc_read_hashes(fec_handle *f, uint64_t hash_offset,
        uint8_t *hash, uint64_t data_offset, uint8_t *data)
{
    if (hash && fec_pread(f, hash, SHA256_DIGEST_SIZE, hash_offset) !=
                    SHA256_DIGEST_SIZE) {
        error("failed to read hash tree: offset %" PRIu64 ": %s", hash_offset,
            strerror(errno));
        return false;
    }

    if (fec_pread(f, data, FEC_BLOCKSIZE, data_offset) != FEC_BLOCKSIZE) {
        error("failed to read hash tree: data_offset %" PRIu64 ": %s",
            data_offset, strerror(errno));
        return false;
    }

    return true;
}

static int verify_tree(fec_handle *f, const uint8_t *root)
{
    uint8_t data[FEC_BLOCKSIZE];
    uint8_t hash[SHA256_DIGEST_SIZE];

    check(f);
    check(root);

    verity_info *v = &f->verity;
    uint32_t levels = 0;

    /* calculate the size and the number of levels in the hash tree */
    v->hash_size =
        verity_get_size(v->data_blocks * FEC_BLOCKSIZE, &levels, NULL);

    check(v->hash_start + v->hash_size <= f->data_size);

    uint64_t hash_offset = v->hash_start;
    uint64_t data_offset = hash_offset + FEC_BLOCKSIZE;

    v->hash_data_offset = data_offset;

    /* validate the root hash */
    if (!raw_pread(f, data, SHA256_DIGEST_SIZE, hash_offset) ||
            !verity_check_block(f, VERITY_NO_CACHE, root, data)) {
        /* try to correct */
        if (!ecc_read_hashes(f, 0, NULL, hash_offset, data) ||
                !verity_check_block(f, VERITY_NO_CACHE, root, data)) {
            error("root hash invalid");
            return -1;
        }
    }

    debug("root hash valid");

    /* calculate the number of hashes on each level */
    uint32_t hashes[levels];

    verity_get_size(v->data_blocks * FEC_BLOCKSIZE, NULL, hashes);

    /* calculate the size and offset for the data hashes */
    for (uint32_t i = 1; i < levels; ++i) {
        uint32_t blocks = hashes[levels - i];
        debug("%u hash blocks on level %u", blocks, levels - i);

        v->hash_data_offset = data_offset;
        v->hash_data_blocks = blocks;

        data_offset += blocks * FEC_BLOCKSIZE;
    }

    /* copy data hashes to memory in case they are corrupted, so we don't
       have to correct them every time they are needed */
    uint8_t *data_hashes =
        new uint8_t[f->verity.hash_data_blocks * FEC_BLOCKSIZE];

    if (!data_hashes) {
        errno = ENOMEM;
        goto err;
    }

    /* validate the rest of the hash tree */
    data_offset = hash_offset + FEC_BLOCKSIZE;

    for (uint32_t i = 1; i < levels; ++i) {
        uint32_t blocks = hashes[levels - i];

        for (uint32_t j = 0; j < blocks; ++j) {
            /* ecc reads are very I/O intensive, so read raw hash tree and do
               error correcting only if it doesn't validate */
            if (!raw_pread(f, hash, SHA256_DIGEST_SIZE,
                    hash_offset + j * SHA256_DIGEST_SIZE) ||
                !raw_pread(f, data, FEC_BLOCKSIZE,
                    data_offset + j * FEC_BLOCKSIZE)) {
                error("failed to read hashes: %s", strerror(errno));
                goto err;
            }

            if (!verity_check_block(f, VERITY_NO_CACHE, hash, data)) {
                /* try to correct */
                if (!ecc_read_hashes(f,
                        hash_offset + j * SHA256_DIGEST_SIZE, hash,
                        data_offset + j * FEC_BLOCKSIZE, data) ||
                    !verity_check_block(f, VERITY_NO_CACHE, hash, data)) {
                    error("invalid hash tree: hash_offset %" PRIu64 ", "
                        "data_offset %" PRIu64 ", block %u",
                        hash_offset, data_offset, j);
                    goto err;
                }

                /* update the corrected blocks to the file if we are in r/w
                   mode */
                if (f->mode & O_RDWR) {
                    if (!raw_pwrite(f, hash, SHA256_DIGEST_SIZE,
                            hash_offset + j * SHA256_DIGEST_SIZE) ||
                        !raw_pwrite(f, data, FEC_BLOCKSIZE,
                            data_offset + j * FEC_BLOCKSIZE)) {
                        error("failed to write hashes: %s", strerror(errno));
                        goto err;
                    }
                }
            }

            if (blocks == v->hash_data_blocks) {
                memcpy(&data_hashes[j * FEC_BLOCKSIZE], data, FEC_BLOCKSIZE);
            }
        }

        hash_offset = data_offset;
        data_offset += blocks * FEC_BLOCKSIZE;
    }

    debug("valid");

    v->hash = data_hashes;
    return 0;

err:
    delete[] data_hashes;
    return -1;
}

static int parse_table(fec_handle *f, uint64_t offset,
        uint32_t size)
{
    check(f);
    debug("offset = %" PRIu64 ", size = %u", offset, size);

    verity_info *v = &f->verity;
    v->table = new char[size + 1];

    if (!v->table) {
        errno = ENOMEM;
        return -1;
    }

    if (fec_pread(f, v->table, size, offset) != (ssize_t)size) {
        error("failed to read verity table: %s", strerror(errno));
        delete[] v->table;
        v->table = NULL;
        return -1;
    }

    v->table[size] = '\0';
    debug("verity table: '%s'", v->table);

    int i = 0;
    uint8_t root[SHA256_DIGEST_SIZE];

    auto tokens = android::base::Split(v->table, " ");

    for (const auto token : tokens) {
        switch (i++) {
        case 0: /* version */
            if (token != stringify(VERITY_TABLE_VERSION)) {
                error("unsupported verity table version: %s", token.c_str());
                goto err;
            }
            break;
        case 3: /* data_block_size */
        case 4: /* hash_block_size */
            /* assume 4 KiB block sizes for everything */
            if (token != stringify(FEC_BLOCKSIZE)) {
                error("unsupported verity block size: %s", token.c_str());
                goto err;
            }
            break;
        case 5: /* num_data_blocks */
            if (parse_uint64(token.c_str(), f->data_size / FEC_BLOCKSIZE,
                    &v->data_blocks) == -1) {
                error("invalid number of verity data blocks: %s",
                    token.c_str());
                goto err;
            }
            break;
        case 6: /* hash_start_block */
            if (parse_uint64(token.c_str(), f->data_size / FEC_BLOCKSIZE,
                    &v->hash_start) == -1) {
                error("invalid verity hash start block: %s", token.c_str());
                goto err;
            }

            v->hash_start *= FEC_BLOCKSIZE;
            break;
        case 7: /* algorithm */
            if (token != "sha256") {
                error("unsupported verity hash algorithm: %s", token.c_str());
                goto err;
            }
            break;
        case 8: /* digest */
            if (parse_hex(root, sizeof(root), token.c_str()) == -1) {
                error("invalid verity root hash: %s", token.c_str());
                goto err;
            }
            break;
        case 9: /* salt */
            v->salt_size = token.size();
            check(v->salt_size % 2 == 0);
            v->salt_size /= 2;

            v->salt = new uint8_t[v->salt_size];

            if (!v->salt) {
                errno = ENOMEM;
                goto err;
            }

            if (parse_hex(v->salt, v->salt_size, token.c_str()) == -1) {
                error("invalid verity salt: %s", token.c_str());
                goto err;
            }
            break;
        default:
            break;
        }
    }

    if (i < VERITY_TABLE_ARGS) {
        error("not enough arguments in verity table: %d; expected at least "
            stringify(VERITY_TABLE_ARGS), i);
        goto err;
    }

    check(v->hash_start < f->data_size);
    check(v->data_blocks * FEC_BLOCKSIZE == v->start);
    check(v->salt);

    if (!(f->flags & FEC_VERITY_DISABLE)) {
        if (verify_tree(f, root) == -1) {
            goto err;
        }

        check(v->hash_data_offset);
        check(v->hash_data_offset < f->data_size);
        check(v->hash_data_offset + v->hash_data_blocks * FEC_BLOCKSIZE <=
                f->data_size);
        check(v->hash);
    }

    return 0;

err:
    if (v->table) {
        delete[] v->table;
        v->table = NULL;
    }

    if (v->salt) {
        delete[] v->salt;
        v->salt = NULL;
    }
    return -1;
}

int verity_parse_header(fec_handle *f, uint64_t offset)
{
    check(f);
    check(offset < f->data_size - sizeof(verity_header));

    verity_info *v = &f->verity;

    if (fec_pread(f, &v->header, sizeof(v->header), offset) !=
            sizeof(v->header)) {
        error("failed to read verity header: %s", strerror(errno));
        return -1;
    }

    verity_header raw_header;

    if (!raw_pread(f, &raw_header, sizeof(raw_header), offset)) {
        error("failed to read verity header: %s", strerror(errno));
        return -1;
    }

    /* use raw data to check for the alternative magic, because it will
       be error corrected to VERITY_MAGIC otherwise */
    if (raw_header.magic == VERITY_MAGIC_DISABLE) {
        /* this value is not used by us, but can be used by a caller to
           decide whether dm-verity should be enabled */
        v->disabled = true;
    } else if (v->header.magic != VERITY_MAGIC) {
        return -1;
    }

    if (v->header.version != VERITY_VERSION) {
        error("unsupported verity version %u", v->header.version);
        return -1;
    }

    if (v->header.length <  VERITY_MIN_TABLE_SIZE ||
        v->header.length >= VERITY_MAX_TABLE_SIZE) {
        error("invalid verity table size: %u; expected ["
            stringify(VERITY_MIN_TABLE_SIZE) ", "
            stringify(VERITY_MAX_TABLE_SIZE) ")", v->header.length);
        return -1;
    }

    f->verity.start = offset;

    /* signature is skipped, because for our purposes it won't matter from
       where the data originates; the caller of the library is responsible
       for signature verification */

    if (offset > UINT64_MAX - v->header.length) {
        error("invalid verity table length: %u", v->header.length);
        return -1;
    } else if (offset + v->header.length >= f->data_size) {
        error("invalid verity table length: %u", v->header.length);
        return -1;
    }

    if (parse_table(f, offset + sizeof(v->header), v->header.length) == -1) {
        return -1;
    }

    f->data_size = f->verity.start;

    return 0;
}
