/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <stdint.h>

#include <string>
#include <vector>

#include <openssl/obj_mac.h>
#include <openssl/sha.h>

#include "fec_private.h"

// Computes a SHA-256 salted with 'salt' from a FEC_BLOCKSIZE byte buffer
// 'block', and copies the hash to 'hash'.
static int get_sha256_hash(const uint8_t *block, uint8_t *hash,
             const std::vector<uint8_t> &salt) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    check(!salt.empty());
    SHA256_Update(&ctx, salt.data(), salt.size());

    check(block);
    SHA256_Update(&ctx, block, FEC_BLOCKSIZE);

    check(hash);
    SHA256_Final(hash, &ctx);
    return 0;
}

// Computes a SHA1 salted with 'salt' from a FEC_BLOCKSIZE byte buffer
// 'block', and copies the hash to 'hash'.
static int get_sha1_hash(const uint8_t *block, uint8_t *hash,
             const std::vector<uint8_t> &salt) {
    SHA_CTX ctx;
    SHA1_Init(&ctx);

    check(!salt.empty());
    SHA1_Update(&ctx, salt.data(), salt.size());

    check(block);
    SHA1_Update(&ctx, block, FEC_BLOCKSIZE);

    check(hash);
    SHA1_Final(hash, &ctx);
    return 0;
}

static int get_hash(const uint8_t *block, uint8_t *hash,
             const std::vector<uint8_t> &salt, int nid) {
    check(nid == NID_sha256 || nid == NID_sha1);
    if (nid == NID_sha256) {
      return get_sha256_hash(block, hash, salt);
    }
    return get_sha1_hash(block, hash, salt);
}

bool check_block_hash(const uint8_t *expected, const uint8_t *block,
                      const std::vector<uint8_t> &salt, int nid) {
    check(block);
    check(nid == NID_sha256 || nid == NID_sha1);
    uint32_t hash_length = nid == NID_sha256 ? SHA256_DIGEST_LENGTH : SHA_DIGEST_LENGTH;
    uint8_t hash[hash_length];

    if (unlikely(get_hash(block, hash, salt, nid) == -1)) {
        error("failed to hash");
        return false;
    }

    check(expected);
    return !memcmp(expected, hash, hash_length);
}

// Reads the hash and the corresponding data block using error correction, if
// available.
static bool ecc_read_hashes(fec_handle *f, uint64_t hash_offset, uint8_t *hash,
                     uint64_t data_offset, uint8_t *data, int nid) {
    check(f);

    check(nid == NID_sha256 || nid == NID_sha1);
    uint32_t hash_length = nid == NID_sha256 ? SHA256_DIGEST_LENGTH : SHA_DIGEST_LENGTH;
    if (hash && fec_pread(f, hash, hash_length, hash_offset) !=
                    hash_length) {
        error("failed to read hash tree: offset %" PRIu64 ": %s", hash_offset,
              strerror(errno));
        return false;
    }

    check(data);

    if (fec_pread(f, data, FEC_BLOCKSIZE, data_offset) != FEC_BLOCKSIZE) {
        error("failed to read hash tree: data_offset %" PRIu64 ": %s",
              data_offset, strerror(errno));
        return false;
    }

    return true;
}

/* computes the size of verity hash tree for `file_size' bytes and returns the
   number of hash tree levels in `verity_levels,' and the number of hashes per
   level in `level_hashes', if the parameters are non-NULL */
uint64_t verity_get_size(uint64_t file_size, uint32_t *verity_levels,
                         uint32_t *level_hashes) {
    // we assume a known metadata size, 4 KiB block size, and SHA-256 or SHA1 to
    // avoid relying on disk content. The padded digest size for both sha256 and
    // sha1 are 256 bytes.

    uint32_t level = 0;
    uint64_t total = 0;
    uint64_t hashes = file_size / FEC_BLOCKSIZE;

    do {
        if (level_hashes) {
            level_hashes[level] = hashes;
        }

        hashes = fec_div_round_up(hashes * SHA256_DIGEST_LENGTH, FEC_BLOCKSIZE);
        total += hashes;

        ++level;
    } while (hashes > 1);

    if (verity_levels) {
        *verity_levels = level;
    }

    return total * FEC_BLOCKSIZE;
}

// TODO(xunchang) support hashtree computed with SHA1.
int verify_tree(hashtree_info *hashtree, const fec_handle *f,
                const uint8_t *root, int nid) {
    check(hashtree);
    check(f);
    check(root);
    check(nid == NID_sha256 || nid == NID_sha1);

    // The padded digest size for both sha256 and sha1 are 256 bytes.
    uint32_t padded_hash_length = SHA256_DIGEST_LENGTH;
    uint8_t data[FEC_BLOCKSIZE];
    uint8_t hash[padded_hash_length];


    uint32_t levels = 0;

    /* calculate the size and the number of levels in the hash tree */
    hashtree->hash_size =
        verity_get_size(hashtree->data_blocks * FEC_BLOCKSIZE, &levels, NULL);

    check(hashtree->hash_start < UINT64_MAX - hashtree->hash_size);
    check(hashtree->hash_start + hashtree->hash_size <= f->data_size);

    uint64_t hash_offset = hashtree->hash_start;
    uint64_t data_offset = hash_offset + FEC_BLOCKSIZE;

    hashtree->hash_data_offset = data_offset;

    /* validate the root hash */
    if (!raw_pread(f->fd, data, FEC_BLOCKSIZE, hash_offset) ||
        !check_block_hash(root, data, hashtree->salt, nid)) {
        /* try to correct */
        if (!ecc_read_hashes(const_cast<fec_handle *>(f), 0,
                             nullptr, hash_offset,
                             data, nid) ||
            !check_block_hash(root, data, hashtree->salt, nid)) {
            error("root hash invalid");
            return -1;
        } else if (f->mode & O_RDWR &&
                   !raw_pwrite(f->fd, data, FEC_BLOCKSIZE, hash_offset)) {
            error("failed to rewrite the root block: %s", strerror(errno));
            return -1;
        }
    }

    debug("root hash valid");

    /* calculate the number of hashes on each level */
    uint32_t hashes[levels];

    verity_get_size(hashtree->data_blocks * FEC_BLOCKSIZE, NULL, hashes);

    /* calculate the size and offset for the data hashes */
    for (uint32_t i = 1; i < levels; ++i) {
        uint32_t blocks = hashes[levels - i];
        debug("%u hash blocks on level %u", blocks, levels - i);

        hashtree->hash_data_offset = data_offset;
        hashtree->hash_data_blocks = blocks;

        data_offset += blocks * FEC_BLOCKSIZE;
    }

    check(hashtree->hash_data_blocks);
    check(hashtree->hash_data_blocks <= hashtree->hash_size / FEC_BLOCKSIZE);

    check(hashtree->hash_data_offset);
    check(hashtree->hash_data_offset <=
          UINT64_MAX - (hashtree->hash_data_blocks * FEC_BLOCKSIZE));
    check(hashtree->hash_data_offset < f->data_size);
    check(hashtree->hash_data_offset +
              hashtree->hash_data_blocks * FEC_BLOCKSIZE <=
          f->data_size);

    /* copy data hashes to memory in case they are corrupted, so we don't
       have to correct them every time they are needed */
    std::vector<uint8_t> data_hashes(hashtree->hash_data_blocks * FEC_BLOCKSIZE,
                                     0);

    /* validate the rest of the hash tree */
    data_offset = hash_offset + FEC_BLOCKSIZE;

    for (uint32_t i = 1; i < levels; ++i) {
        uint32_t blocks = hashes[levels - i];

        for (uint32_t j = 0; j < blocks; ++j) {
            /* ecc reads are very I/O intensive, so read raw hash tree and do
               error correcting only if it doesn't validate */
            if (!raw_pread(f->fd, hash, padded_hash_length,
                           hash_offset + j * padded_hash_length) ||
                !raw_pread(f->fd, data, FEC_BLOCKSIZE,
                           data_offset + j * FEC_BLOCKSIZE)) {
                error("failed to read hashes: %s", strerror(errno));
                return -1;
            }

            if (!check_block_hash(hash, data, hashtree->salt, nid)) {
                /* try to correct */
                if (!ecc_read_hashes(const_cast<fec_handle *>(f),
                                     hash_offset + j * padded_hash_length,
                                     hash, data_offset + j * FEC_BLOCKSIZE,
                                     data, nid) ||
                    !check_block_hash(hash, data, hashtree->salt, nid)) {
                    error("invalid hash tree: hash_offset %" PRIu64
                          ", "
                          "data_offset %" PRIu64 ", block %u",
                          hash_offset, data_offset, j);
                    return -1;
                }

                /* update the corrected blocks to the file if we are in r/w
                   mode */
                if (f->mode & O_RDWR) {
                    if (!raw_pwrite(f->fd, hash, padded_hash_length,
                                    hash_offset + j * padded_hash_length) ||
                        !raw_pwrite(f->fd, data, FEC_BLOCKSIZE,
                                    data_offset + j * FEC_BLOCKSIZE)) {
                        error("failed to write hashes: %s", strerror(errno));
                        return -1;
                    }
                }
            }

            if (blocks == hashtree->hash_data_blocks) {
                std::copy(data, data + FEC_BLOCKSIZE,
                          data_hashes.begin() + j * FEC_BLOCKSIZE);
            }
        }

        hash_offset = data_offset;
        data_offset += blocks * FEC_BLOCKSIZE;
    }

    debug("valid");

    hashtree->hash = std::move(data_hashes);

    std::vector<uint8_t> zero_block(FEC_BLOCKSIZE, 0);
    hashtree->zero_hash.assign(nid ==  NID_sha256 ? SHA256_DIGEST_LENGTH : SHA_DIGEST_LENGTH , 0);
    if (get_hash(zero_block.data(), hashtree->zero_hash.data(),
                 hashtree->salt, nid) == -1) {
        error("failed to hash");
        return -1;
    }
    return 0;
}
