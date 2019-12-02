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

int hashtree_info::get_hash(const uint8_t *block, uint8_t *hash) {
    if (nid == NID_sha256) {
        return get_sha256_hash(block, hash, salt);
    }
    return get_sha1_hash(block, hash, salt);
}

int hashtree_info::intialize(uint64_t hash_start, uint64_t data_blocks,
                             const std::vector<uint8_t> &salt, int nid) {
    check(nid == NID_sha256 || nid == NID_sha1);

    this->hash_start = hash_start;
    this->data_blocks = data_blocks;
    this->salt = std::move(salt);
    this->nid = nid;

    digest_length_ = nid == NID_sha1 ? SHA_DIGEST_LENGTH : SHA256_DIGEST_LENGTH;
    // The padded digest size for both sha256 and sha1 are 256 bytes.
    padded_digest_length_ = SHA256_DIGEST_LENGTH;

    return 0;
}

bool hashtree_info::check_block_hash(const uint8_t *expected,
                                     const uint8_t *block) {
    check(block);
    std::vector<uint8_t> hash(digest_length_, 0);

    if (unlikely(get_hash(block, hash.data()) == -1)) {
        error("failed to hash");
        return false;
    }

    check(expected);
    return !memcmp(expected, hash.data(), digest_length_);
}

bool hashtree_info::check_block_hash_with_index(uint64_t index,
                                                const uint8_t *block) {
    error("%zu, %zu", (size_t)index, (size_t)data_blocks);
    check(index < data_blocks) const uint8_t *expected =
        &hash_data[index * padded_digest_length_];
    return check_block_hash(expected, block);
}

// Reads the hash and the corresponding data block using error correction, if
// available.
bool hashtree_info::ecc_read_hashes(fec_handle *f, uint64_t hash_offset,
                                    uint8_t *hash, uint64_t data_offset,
                                    uint8_t *data) {
    check(f);

    if (hash &&
        fec_pread(f, hash, digest_length_, hash_offset) != digest_length_) {
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
int hashtree_info::verify_tree(const fec_handle *f, const uint8_t *root) {
    check(f);
    check(root);

    uint8_t data[FEC_BLOCKSIZE];

    uint32_t levels = 0;

    /* calculate the size and the number of levels in the hash tree */
    uint64_t hash_size =
        verity_get_size(data_blocks * FEC_BLOCKSIZE, &levels, NULL);

    check(hash_start < UINT64_MAX - hash_size);
    check(hash_start + hash_size <= f->data_size);

    uint64_t hash_offset = hash_start;
    uint64_t data_offset = hash_offset + FEC_BLOCKSIZE;

    uint64_t hash_data_offset_ = data_offset;

    /* validate the root hash */
    if (!raw_pread(f->fd, data, FEC_BLOCKSIZE, hash_offset) ||
        !check_block_hash(root, data)) {
        /* try to correct */
        if (!ecc_read_hashes(const_cast<fec_handle *>(f), 0, nullptr,
                             hash_offset, data) ||
            !check_block_hash(root, data)) {
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

    verity_get_size(data_blocks * FEC_BLOCKSIZE, NULL, hashes);

    /* calculate the size and offset for the data hashes */
    for (uint32_t i = 1; i < levels; ++i) {
        uint32_t blocks = hashes[levels - i];
        debug("%u hash blocks on level %u", blocks, levels - i);

        hash_data_offset_ = data_offset;
        hash_data_blocks = blocks;

        data_offset += blocks * FEC_BLOCKSIZE;
    }

    check(hash_data_blocks);
    check(hash_data_blocks <= hash_size / FEC_BLOCKSIZE);

    check(hash_data_offset_);
    check(hash_data_offset_ <= UINT64_MAX - (hash_data_blocks * FEC_BLOCKSIZE));
    check(hash_data_offset_ < f->data_size);
    check(hash_data_offset_ + hash_data_blocks * FEC_BLOCKSIZE <= f->data_size);

    /* copy data hashes to memory in case they are corrupted, so we don't
       have to correct them every time they are needed */
    std::vector<uint8_t> data_hashes(hash_data_blocks * FEC_BLOCKSIZE, 0);

    /* validate the rest of the hash tree */
    data_offset = hash_offset + FEC_BLOCKSIZE;

    std::vector<uint8_t> buffer(padded_digest_length_, 0);
    for (uint32_t i = 1; i < levels; ++i) {
        uint32_t blocks = hashes[levels - i];

        for (uint32_t j = 0; j < blocks; ++j) {
            /* ecc reads are very I/O intensive, so read raw hash tree and do
               error correcting only if it doesn't validate */
            if (!raw_pread(f->fd, buffer.data(), padded_digest_length_,
                           hash_offset + j * padded_digest_length_) ||
                !raw_pread(f->fd, data, FEC_BLOCKSIZE,
                           data_offset + j * FEC_BLOCKSIZE)) {
                error("failed to read hashes: %s", strerror(errno));
                return -1;
            }

            if (!check_block_hash(buffer.data(), data)) {
                /* try to correct */
                if (!ecc_read_hashes(const_cast<fec_handle *>(f),
                                     hash_offset + j * padded_digest_length_,
                                     buffer.data(),
                                     data_offset + j * FEC_BLOCKSIZE, data) ||
                    !check_block_hash(buffer.data(), data)) {
                    error("invalid hash tree: hash_offset %" PRIu64
                          ", "
                          "data_offset %" PRIu64 ", block %u",
                          hash_offset, data_offset, j);
                    return -1;
                }

                /* update the corrected blocks to the file if we are in r/w
                   mode */
                if (f->mode & O_RDWR) {
                    if (!raw_pwrite(f->fd, buffer.data(), padded_digest_length_,
                                    hash_offset + j * padded_digest_length_) ||
                        !raw_pwrite(f->fd, data, FEC_BLOCKSIZE,
                                    data_offset + j * FEC_BLOCKSIZE)) {
                        error("failed to write hashes: %s", strerror(errno));
                        return -1;
                    }
                }
            }

            if (blocks == hash_data_blocks) {
                std::copy(data, data + FEC_BLOCKSIZE,
                          data_hashes.begin() + j * FEC_BLOCKSIZE);
            }
        }

        hash_offset = data_offset;
        data_offset += blocks * FEC_BLOCKSIZE;
    }

    debug("valid");

    hash_data = std::move(data_hashes);

    std::vector<uint8_t> zero_block(FEC_BLOCKSIZE, 0);
    zero_hash.resize(padded_digest_length_, 0);
    if (get_hash(zero_block.data(), zero_hash.data()) == -1) {
        error("failed to hash");
        return -1;
    }
    return 0;
}
